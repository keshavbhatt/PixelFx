#include "core/presets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <pfx/document.h>
#include <pfx/registry.h>
#include <pfx/xmp_import.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

const char* const kContext = "PresetUnits";

struct UnitRule
{
    const char* id;
    const char* label;
    std::vector<std::string_view> prefixes;
    std::vector<std::string_view> excludes;
};

const std::vector<UnitRule>& rules()
{
    static const std::vector<UnitRule> list = {
        {"light", QT_TRANSLATE_NOOP("PresetUnits", "Light"), {"light."}, {}},
        {"color", QT_TRANSLATE_NOOP("PresetUnits", "Color"), {"color."}, {"color.wb."}},
        {"wb", QT_TRANSLATE_NOOP("PresetUnits", "White balance"), {"color.wb."}, {}},
        {"mixer", QT_TRANSLATE_NOOP("PresetUnits", "Color mixer"), {"mixer."}, {}},
        {"grading", QT_TRANSLATE_NOOP("PresetUnits", "Color grading"), {"grading."}, {}},
        {"curves", QT_TRANSLATE_NOOP("PresetUnits", "Curves"), {"curves."}, {}},
        {"detail", QT_TRANSLATE_NOOP("PresetUnits", "Detail"), {"detail."}, {}},
        {"effects", QT_TRANSLATE_NOOP("PresetUnits", "Effects"), {"effects."}, {}},
        {"look", QT_TRANSLATE_NOOP("PresetUnits", "Look"), {"look."}, {}},
        {"crop",
         QT_TRANSLATE_NOOP("PresetUnits", "Crop and rotate"),
         {"geometry."},
         {"geometry.perspective."}},
        {"perspective", QT_TRANSLATE_NOOP("PresetUnits", "Perspective"), {"geometry.perspective."}, {}},
    };
    return list;
}

const UnitRule* ruleFor(const QString& id)
{
    for (const UnitRule& r : rules()) {
        if (id == QLatin1StringView(r.id)) {
            return &r;
        }
    }
    return nullptr;
}

bool matches(const UnitRule& rule, std::string_view key)
{
    const auto starts = [key](std::string_view p) { return key.starts_with(p); };
    return std::ranges::any_of(rule.prefixes, starts) && std::ranges::none_of(rule.excludes, starts);
}

bool isDefault(const pfx::ParamSpec& spec, const pfx::Value& value)
{
    if (const auto* d = std::get_if<double>(&value)) {
        return std::abs(*d - spec.defaultValue) < 1e-9;
    }
    if (const auto* b = std::get_if<bool>(&value)) {
        return !*b;
    }
    if (const auto* c = std::get_if<pfx::Curve>(&value)) {
        return c->empty() || *c == pfx::Curve{{0, 0}, {1, 1}};
    }
    return false;
}

QString fileSafe(const QString& name)
{
    QString s = name.trimmed();
    s.replace(QRegularExpression(u"[\\\\/:*?\"<>|\\x00-\\x1F]"_s), u"-"_s);
    while (s.startsWith(u'.')) {
        s.remove(0, 1); // a leading dot would hide the file from the list
    }
    return s.isEmpty() ? u"Preset"_s : s.left(80);
}

} // namespace

const std::vector<PresetUnit>& presetUnits()
{
    static const std::vector<PresetUnit> units = [] {
        std::vector<PresetUnit> out;
        for (const UnitRule& r : rules()) {
            out.push_back({QString::fromLatin1(r.id), QCoreApplication::translate(kContext, r.label)});
        }
        return out;
    }();
    return units;
}

bool keyInUnit(std::string_view key, const QString& unitId)
{
    const UnitRule* rule = ruleFor(unitId);
    return rule != nullptr && matches(*rule, key);
}

QStringList unitsIn(const pfx::Settings& settings)
{
    QStringList out;
    for (const UnitRule& r : rules()) {
        const auto keys = settings.keys();
        if (std::ranges::any_of(keys, [&r](const std::string& k) { return matches(r, k); })) {
            out << QString::fromLatin1(r.id);
        }
    }
    return out;
}

int changedCount(const pfx::Settings& settings, const QString& unitId)
{
    if (unitId == u"look"_s) {
        return settings.text("look.id", "").empty() ? 0 : 1; // a look, or none
    }
    return static_cast<int>(changedKeys(settings, unitId).size());
}

std::vector<std::string> changedKeys(const pfx::Settings& settings, const QString& unitId)
{
    std::vector<std::string> keys;
    const UnitRule* rule = ruleFor(unitId);
    if (rule == nullptr) {
        return keys;
    }
    for (const std::string& key : settings.keys()) {
        if (!matches(*rule, key)) {
            continue;
        }
        const pfx::ParamSpec* spec = pfx::registry().param(key);
        if (spec == nullptr || !isDefault(*spec, *settings.find(key))) {
            keys.push_back(key);
        }
    }
    return keys;
}

namespace {

// Every registry setting of the unit, the photo's value or the default; false when none.
bool addUnit(pfx::Settings& out, const pfx::Settings& settings, const UnitRule& rule)
{
    bool any = false;
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        for (const pfx::ParamSpec& p : op.params) {
            if (!matches(rule, p.key)) {
                continue;
            }
            if (const pfx::Value* v = settings.find(p.key)) {
                out.set(p.key, *v);
            } else if (p.type == pfx::ParamType::Number) {
                out.set(p.key, p.defaultValue);
            } else if (p.type == pfx::ParamType::Boolean) {
                out.set(p.key, false);
            } else {
                continue;
            }
            any = true;
        }
    }
    return any;
}

} // namespace

pfx::Settings presetFrom(const pfx::Settings& settings, const QStringList& units)
{
    pfx::Settings out;
    for (const QString& id : units) {
        const UnitRule* rule = ruleFor(id);
        if (rule == nullptr) {
            continue;
        }
        const bool any = addUnit(out, settings, *rule);
        if (id == u"look"_s) {
            // Not registry settings: the look's id and strength. No look is stored as an empty
            // id, so applying the preset takes a photo's look off, as other units reset theirs.
            out.set("look.id", settings.text("look.id", ""));
            out.set("look.strength", settings.number("look.strength", 100.0));
        }
        if (!any && id == u"curves"_s) {
            out.set("curves.rgb", pfx::Curve{{0, 0}, {1, 1}}); // an included, straight curve
        }
    }
    return out;
}

pfx::Settings applyPreset(const pfx::Settings& current, const pfx::Settings& preset)
{
    const QStringList units = unitsIn(preset);
    pfx::Settings out;
    for (const std::string& key : current.keys()) {
        const bool replaced =
            std::ranges::any_of(units, [&key](const QString& id) { return keyInUnit(key, id); });
        if (!replaced) {
            out.set(key, *current.find(key));
        }
    }
    for (const std::string& key : preset.keys()) {
        const pfx::ParamSpec* spec = pfx::registry().param(key);
        if (spec != nullptr && isDefault(*spec, *preset.find(key))) {
            continue; // the default is the absence of the key
        }
        out.set(key, *preset.find(key));
    }
    return out;
}

QString presetSummary(const pfx::Settings& preset)
{
    QStringList names;
    const QStringList units = unitsIn(preset);
    for (const PresetUnit& u : presetUnits()) {
        if (units.contains(u.id)) {
            names << u.label;
        }
    }
    return names.join(u", "_s);
}

PresetLibrary::PresetLibrary(QString folder)
    : m_folder(std::move(folder))
{}

QString PresetLibrary::defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/presets"_s;
}

std::vector<PresetEntry> PresetLibrary::entries() const
{
    std::vector<PresetEntry> out;
    const QDir dir(m_folder);
    for (const QFileInfo& file : dir.entryInfoList({u"*.pfxpreset"_s}, QDir::Files, QDir::Name)) {
        const auto doc = pfx::loadDocument(file.absoluteFilePath().toStdString());
        if (!doc) {
            continue;
        }
        PresetEntry e;
        e.name = QString::fromStdString(doc.value().name);
        if (e.name.isEmpty()) {
            e.name = file.completeBaseName();
        }
        e.group = doc.value().group.empty() ? myPresetsGroup() : QString::fromStdString(doc.value().group);
        e.path = file.absoluteFilePath();
        e.settings = doc.value().settings;
        out.push_back(std::move(e));
    }
    const QString mine = myPresetsGroup();
    const QString imported = QCoreApplication::translate(kContext, "Imported");
    const auto rank = [&mine, &imported](const QString& group) {
        if (group == mine) {
            return 0;
        }
        return group == imported ? 2 : 1;
    };
    std::ranges::sort(out, [&rank](const PresetEntry& a, const PresetEntry& b) {
        if (rank(a.group) != rank(b.group)) {
            return rank(a.group) < rank(b.group);
        }
        return a.group == b.group ? a.name.localeAwareCompare(b.name) < 0
                                  : a.group.localeAwareCompare(b.group) < 0;
    });
    for (PresetEntry& e : builtIn()) {
        out.push_back(std::move(e));
    }
    return out;
}

QString PresetLibrary::freePath(const QString& name) const
{
    const QString base = m_folder + u'/' + fileSafe(name);
    QString path = base + u".pfxpreset"_s;
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = base + u"-%1.pfxpreset"_s.arg(n);
    }
    return path;
}

QString PresetLibrary::save(const QString& name, const QString& group, const pfx::Settings& settings) const
{
    QDir().mkpath(m_folder);
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Preset;
    doc.name = name.trimmed().toStdString();
    doc.group = group.toStdString();
    doc.settings = settings;
    const QString path = freePath(name);
    return pfx::saveDocument(doc, path.toStdString()).ok() ? path : QString();
}

bool PresetLibrary::update(const QString& path, const pfx::Settings& settings)
{
    auto doc = pfx::loadDocument(path.toStdString());
    if (!doc) {
        return false;
    }
    doc.value().settings = settings;
    return pfx::saveDocument(doc.value(), path.toStdString()).ok();
}

bool PresetLibrary::rename(const QString& path, const QString& name)
{
    auto doc = pfx::loadDocument(path.toStdString());
    if (!doc || name.trimmed().isEmpty()) {
        return false;
    }
    doc.value().name = name.trimmed().toStdString();
    return pfx::saveDocument(doc.value(), path.toStdString()).ok();
}

bool PresetLibrary::remove(const QString& path) const
{
    return QFileInfo(path).absolutePath() == QFileInfo(m_folder).absoluteFilePath() && QFile::remove(path);
}

PresetLibrary::Imported PresetLibrary::importFile(const QString& file) const
{
    Imported out;
    const QString imported = QCoreApplication::translate(kContext, "Imported");
    if (QFileInfo(file).suffix().compare(u"xmp"_s, Qt::CaseInsensitive) == 0) {
        auto xmp = pfx::importXmpPresetFile(file.toStdString());
        if (!xmp) {
            out.error = QString::fromStdString(xmp.error().message);
            return out;
        }
        const QString name = QString::fromStdString(xmp.value().document.name);
        out.path = save(QCoreApplication::translate(kContext, "Lightroom: %1").arg(name), imported,
                        xmp.value().document.settings);
        for (const std::string& key : xmp.value().skipped) {
            out.skipped << QString::fromStdString(key);
        }
    } else {
        pfx::ParseReport report;
        auto doc = pfx::loadDocument(file.toStdString(), &report);
        if (!doc) {
            out.error = QString::fromStdString(doc.error().message);
            return out;
        }
        if (doc.value().kind != pfx::DocumentKind::Preset && doc.value().kind != pfx::DocumentKind::Recipe) {
            out.error = QCoreApplication::translate(kContext, "This file is not a preset.");
            return out;
        }
        const QString name = doc.value().name.empty() ? QFileInfo(file).completeBaseName()
                                                      : QString::fromStdString(doc.value().name);
        out.path = save(name, imported, doc.value().settings);
    }
    if (out.path.isEmpty()) {
        out.error = QCoreApplication::translate(kContext, "The preset folder is not writable.");
    }
    return out;
}

bool PresetLibrary::exportTo(const PresetEntry& entry, const QString& target)
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Preset;
    doc.name = entry.name.toStdString();
    doc.settings = entry.settings;
    return pfx::saveDocument(doc, target.toStdString()).ok();
}

QString PresetLibrary::myPresetsGroup()
{
    return QCoreApplication::translate(kContext, "My presets");
}

std::vector<PresetEntry> PresetLibrary::builtIn()
{
    const QString group = QCoreApplication::translate(kContext, "Built in");
    const auto make = [&group](const char* name, const char* description,
                               std::initializer_list<std::pair<const char*, double>> values) {
        PresetEntry e;
        e.name = QCoreApplication::translate(kContext, name);
        e.description = QCoreApplication::translate(kContext, description);
        e.group = group;
        for (const auto& [key, value] : values) {
            e.settings.set(key, value);
        }
        return e;
    };
    PresetEntry autoEnhance = make(QT_TRANSLATE_NOOP("PresetUnits", "Auto enhance"),
                                   QT_TRANSLATE_NOOP("PresetUnits", "Analyses each image"), {});
    autoEnhance.autoEnhance = true;
    return {
        autoEnhance,
        make(QT_TRANSLATE_NOOP("PresetUnits", "Bright and airy"),
             QT_TRANSLATE_NOOP("PresetUnits", "Exposure, Shadows, Pastel"),
             {{"light.exposure", 0.35},
              {"light.shadows", 35},
              {"light.highlights", -20},
              {"light.contrast", -10},
              {"color.vibrance", 10},
              {"effects.fade", 15}}),
        make(QT_TRANSLATE_NOOP("PresetUnits", "Punchy"),
             QT_TRANSLATE_NOOP("PresetUnits", "Contrast, Vibrance, Clarity"),
             {{"light.contrast", 30}, {"color.vibrance", 30}, {"detail.clarity", 25}, {"light.blacks", -10}}),
        make(QT_TRANSLATE_NOOP("PresetUnits", "High key B&W"),
             QT_TRANSLATE_NOOP("PresetUnits", "Mono, Exposure, Whites"),
             {{"color.saturation", -100},
              {"light.exposure", 0.5},
              {"light.whites", 30},
              {"light.highlights", -15},
              {"light.contrast", 10}})};
}

} // namespace pixelfx::core
