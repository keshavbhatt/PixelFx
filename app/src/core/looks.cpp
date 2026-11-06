#include "core/looks.h"

#include "core/file_names.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include <pfx/document.h>
#include <pfx/io.h>
#include <pfx/lut.h>
#include <pfx/registry.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

constexpr const char* kContext = "Looks";

// A look from settings, as steps in pipeline order: one step per operation that has keys.
std::shared_ptr<const pfx::Look> lookFrom(const pfx::Settings& settings, const char* category, int strength)
{
    auto look = std::make_shared<pfx::Look>();
    look->category = category;
    look->defaultStrength = strength;
    for (const pfx::OpSpec& op : pfx::registry().ops()) {
        pfx::LookStep step;
        step.op = op.id;
        for (const pfx::ParamSpec& param : op.params) {
            if (const pfx::Value* v = settings.find(param.key)) {
                step.params.set(param.key, *v);
            }
        }
        if (!step.params.isEmpty()) {
            look->steps.push_back(std::move(step));
        }
    }
    return look;
}

pfx::Settings values(std::initializer_list<std::pair<const char*, double>> list)
{
    pfx::Settings s;
    for (const auto& [key, value] : list) {
        s.set(key, value);
    }
    return s;
}

pfx::Settings withCurve(pfx::Settings s, pfx::Curve curve)
{
    s.set("curves.rgb", std::move(curve));
    return s;
}

struct Registry
{
    QMutex mutex;
    QString folder;
    bool scanned = false;
    QHash<QString, std::shared_ptr<const pfx::Look>> mine;
};

Registry& lookRegistry()
{
    static Registry r;
    return r;
}

struct LookDef
{
    const char* slug;
    const char* name;
    const char* category;
    int strength;
    pfx::Settings settings;
};

// Draft values: a starting point for the owner to tune (PROGRESS, decisions).
std::vector<LookDef> colourLooks()
{
    return {
        {"vivid", QT_TRANSLATE_NOOP("Looks", "Vivid"), "color", 80,
         values({{"light.contrast", 15}, {"color.vibrance", 35}, {"color.saturation", 5}})},
        {"warm", QT_TRANSLATE_NOOP("Looks", "Warm"), "color", 80,
         values({{"color.wb.temperature", 20}, {"color.vibrance", 8}})},
        {"cool", QT_TRANSLATE_NOOP("Looks", "Cool"), "color", 80,
         values({{"color.wb.temperature", -18}, {"light.highlights", -5}})},
        {"film", QT_TRANSLATE_NOOP("Looks", "Film"), "film", 80,
         withCurve(values({{"color.saturation", -10},
                           {"grading.shadows.hue", 200},
                           {"grading.shadows.amount", 10},
                           {"grading.highlights.hue", 40},
                           {"grading.highlights.amount", 10},
                           {"effects.grain.amount", 20}}),
                   {{0.0, 0.05}, {0.25, 0.22}, {0.75, 0.8}, {1.0, 0.96}})},
        {"faded", QT_TRANSLATE_NOOP("Looks", "Faded"), "film", 80,
         values({{"light.contrast", -15}, {"color.saturation", -15}, {"effects.fade", 35}})},
        {"golden-hour", QT_TRANSLATE_NOOP("Looks", "Golden hour"), "color", 72,
         values({{"color.wb.temperature", 28},
                 {"light.highlights", -15},
                 {"color.vibrance", 15},
                 {"grading.shadows.hue", 25},
                 {"grading.shadows.amount", 8},
                 {"grading.highlights.hue", 40},
                 {"grading.highlights.amount", 25}})},
        {"teal-and-orange", QT_TRANSLATE_NOOP("Looks", "Teal and orange"), "color", 75,
         values({{"light.contrast", 10},
                 {"color.vibrance", 10},
                 {"grading.shadows.hue", 190},
                 {"grading.shadows.amount", 30},
                 {"grading.highlights.hue", 30},
                 {"grading.highlights.amount", 25}})},
    };
}

std::vector<LookDef> moreLooks()
{
    return {
        {"matte", QT_TRANSLATE_NOOP("Looks", "Matte"), "film", 80,
         withCurve(values({{"light.contrast", -10}, {"color.saturation", -8}}),
                   {{0.0, 0.08}, {0.5, 0.5}, {1.0, 0.94}})},
        {"dramatic", QT_TRANSLATE_NOOP("Looks", "Dramatic"), "color", 70,
         values({{"light.contrast", 35},
                 {"light.highlights", -30},
                 {"light.shadows", 15},
                 {"detail.clarity", 30},
                 {"color.saturation", -10},
                 {"effects.vignette.amount", -25}})},
        {"mono", QT_TRANSLATE_NOOP("Looks", "Mono"), "bw", 100,
         values({{"color.saturation", -100}, {"light.contrast", 10}})},
        {"noir", QT_TRANSLATE_NOOP("Looks", "Noir"), "bw", 100,
         values({{"color.saturation", -100},
                 {"light.contrast", 45},
                 {"light.blacks", -20},
                 {"detail.clarity", 20},
                 {"effects.vignette.amount", -30}})},
        {"sepia", QT_TRANSLATE_NOOP("Looks", "Sepia"), "bw", 90,
         values({{"color.saturation", -100},
                 {"grading.shadows.hue", 30},
                 {"grading.shadows.amount", 30},
                 {"grading.highlights.hue", 40},
                 {"grading.highlights.amount", 20},
                 {"effects.fade", 10}})},
        {"pastel", QT_TRANSLATE_NOOP("Looks", "Pastel"), "color", 80,
         values({{"light.exposure", 0.15},
                 {"light.highlights", -20},
                 {"light.shadows", 25},
                 {"color.vibrance", -15},
                 {"grading.highlights.hue", 330},
                 {"grading.highlights.amount", 8},
                 {"effects.fade", 25}})},
    };
}

} // namespace

const std::vector<LookEntry>& builtInLooks()
{
    static const std::vector<LookEntry> looks = [] {
        std::vector<LookDef> defs = colourLooks();
        for (LookDef& d : moreLooks()) {
            defs.push_back(std::move(d));
        }
        std::vector<LookEntry> out;
        out.reserve(defs.size());
        for (const LookDef& d : defs) {
            out.push_back({u"builtin:"_s + QLatin1StringView(d.slug),
                           QCoreApplication::translate(kContext, d.name), QLatin1StringView(d.category),
                           QString(), lookFrom(d.settings, d.category, d.strength)});
        }
        return out;
    }();
    return looks;
}

LookLibrary::LookLibrary(QString folder)
    : m_folder(std::move(folder))
{}

QString LookLibrary::defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/looks"_s;
}

std::vector<LookEntry> LookLibrary::entries() const
{
    std::vector<LookEntry> out;
    for (const QFileInfo& file : QDir(m_folder).entryInfoList({u"*.pfxlook"_s}, QDir::Files)) {
        auto doc = pfx::loadDocument(file.absoluteFilePath().toStdString());
        if (!doc || doc.value().kind != pfx::DocumentKind::Look || doc.value().id.empty()) {
            continue;
        }
        const QString name = QString::fromStdString(doc.value().name);
        out.push_back({QString::fromStdString(doc.value().id),
                       name.isEmpty() ? file.completeBaseName() : name,
                       QString::fromStdString(doc.value().look.category), file.absoluteFilePath(),
                       std::make_shared<const pfx::Look>(std::move(doc.value().look))});
    }
    std::ranges::sort(
        out, [](const LookEntry& a, const LookEntry& b) { return a.name.localeAwareCompare(b.name) < 0; });
    for (const LookEntry& e : builtInLooks()) {
        out.push_back(e);
    }
    return out;
}

QString LookLibrary::importLut(const QString& file, QString* error) const
{
    const auto fail = [error](const QString& why) {
        if (error != nullptr) {
            *error = why;
        }
        return QString();
    };
    pfx::Result<pfx::Lut> lut = pfx::Error{};
    if (file.endsWith(u".cube"_s, Qt::CaseInsensitive)) {
        lut = pfx::loadCube(file.toStdString());
    } else {
        auto picture = pfx::load(file.toStdString());
        if (!picture) {
            return fail(QString::fromStdString(picture.error().message));
        }
        lut = pfx::lutFromHald(picture.value().image);
    }
    if (!lut) {
        return fail(QString::fromStdString(lut.error().message));
    }
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Look;
    doc.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    doc.name = QFileInfo(file).completeBaseName().toStdString();
    doc.look.category = "color";
    doc.look.defaultStrength = 100;
    pfx::LookStep step;
    step.op = "lut";
    step.params.set("resource", std::string("lut"));
    step.params.set("mix", 100.0);
    doc.look.steps.push_back(std::move(step));
    doc.look.resources.emplace("lut", std::move(lut).value());
    if (!QDir().mkpath(m_folder)) {
        return fail(QCoreApplication::translate(kContext, "The looks folder could not be created."));
    }
    const QString base = m_folder + u'/' + fileSafeName(QFileInfo(file).completeBaseName(), u"Look"_s);
    QString path = base + u".pfxlook"_s;
    for (int n = 2; QFileInfo::exists(path); ++n) {
        path = base + u"-%1.pfxlook"_s.arg(n);
    }
    if (!pfx::saveDocument(doc, path.toStdString()).ok()) {
        return fail(QCoreApplication::translate(kContext, "The look could not be saved."));
    }
    lookFolderChanged();
    return QString::fromStdString(doc.id);
}

QString LookLibrary::save(const QString& name, const pfx::Look& look, const QString& path,
                          QString* savedPath) const
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Look;
    if (!path.isEmpty()) {
        if (auto existing = pfx::loadDocument(path.toStdString());
            existing && existing.value().kind == pfx::DocumentKind::Look) {
            doc = std::move(existing).value();
        }
    }
    if (doc.id.empty()) {
        doc.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    }
    doc.name = name.trimmed().toStdString();
    doc.look = look;
    QString target = path;
    if (target.isEmpty()) {
        if (!QDir().mkpath(m_folder)) {
            return {};
        }
        const QString base = m_folder + u'/' + fileSafeName(name, u"Look"_s);
        target = base + u".pfxlook"_s;
        for (int n = 2; QFileInfo::exists(target); ++n) {
            target = base + u"-%1.pfxlook"_s.arg(n);
        }
    }
    if (!pfx::saveDocument(doc, target.toStdString()).ok()) {
        return {};
    }
    if (savedPath != nullptr) {
        *savedPath = std::move(target);
    }
    lookFolderChanged();
    return QString::fromStdString(doc.id);
}

bool LookLibrary::exportTo(const LookEntry& entry, const QString& target)
{
    pfx::Document doc;
    if (entry.isMine()) {
        auto existing = pfx::loadDocument(entry.path.toStdString());
        if (!existing) {
            return false;
        }
        doc = std::move(existing).value();
    } else {
        doc.kind = pfx::DocumentKind::Look;
        doc.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        doc.name = entry.name.toStdString();
        doc.look = *entry.look;
    }
    return pfx::saveDocument(doc, target.toStdString()).ok();
}

bool LookLibrary::remove(const QString& path) const
{
    const bool removed =
        QFileInfo(path).absolutePath() == QFileInfo(m_folder).absoluteFilePath() && QFile::remove(path);
    lookFolderChanged();
    return removed;
}

void setLookFolder(const QString& folder)
{
    Registry& r = lookRegistry();
    const QMutexLocker lock(&r.mutex);
    r.folder = folder;
    r.scanned = false;
    r.mine.clear();
}

QString lookFolder()
{
    Registry& r = lookRegistry();
    const QMutexLocker lock(&r.mutex);
    return r.folder.isEmpty() ? LookLibrary::defaultFolder() : r.folder;
}

void lookFolderChanged()
{
    Registry& r = lookRegistry();
    const QMutexLocker lock(&r.mutex);
    r.scanned = false;
    r.mine.clear();
}

std::shared_ptr<const pfx::Look> findLook(const QString& id)
{
    if (id.isEmpty()) {
        return nullptr;
    }
    if (id.startsWith(u"builtin:"_s)) {
        for (const LookEntry& e : builtInLooks()) {
            if (e.id == id) {
                return e.look;
            }
        }
        return nullptr;
    }
    Registry& r = lookRegistry();
    const QMutexLocker lock(&r.mutex);
    if (!r.scanned) {
        const QString folder = r.folder.isEmpty() ? LookLibrary::defaultFolder() : r.folder;
        for (const LookEntry& e : LookLibrary(folder).entries()) {
            if (e.isMine()) {
                r.mine.insert(e.id, e.look);
            }
        }
        r.scanned = true;
    }
    return r.mine.value(id);
}

pfx::Result<pfx::Image> renderPhoto(pfx::Image&& input, const pfx::Settings& settings,
                                    pfx::RenderOptions options)
{
    const std::shared_ptr<const pfx::Look> look =
        findLook(QString::fromStdString(settings.text("look.id", "")));
    if (look) {
        options.look = look.get();
        options.lookStrength =
            static_cast<float>(std::clamp(settings.number("look.strength", 100.0), 0.0, 100.0) / 100.0);
    }
    return pfx::render(std::move(input), settings, options);
}

pfx::Result<pfx::Image> renderPhoto(const pfx::Image& input, const pfx::Settings& settings,
                                    pfx::RenderOptions options)
{
    return renderPhoto(pfx::Image(input), settings, options);
}

QString lookName(const QString& id)
{
    if (id.isEmpty()) {
        return {};
    }
    const QString folder = [] {
        Registry& r = lookRegistry();
        const QMutexLocker lock(&r.mutex);
        return r.folder.isEmpty() ? LookLibrary::defaultFolder() : r.folder;
    }();
    for (const LookEntry& e : LookLibrary(folder).entries()) {
        if (e.id == id) {
            return e.name;
        }
    }
    return {};
}

pfx::Settings withLook(const pfx::Settings& settings, const QString& id, double strength)
{
    pfx::Settings out = settings;
    if (id.isEmpty()) {
        out.erase("look.id");
        out.erase("look.strength");
    } else {
        out.set("look.id", id.toStdString());
        out.set("look.strength", std::clamp(strength, 0.0, 100.0));
    }
    return out;
}

} // namespace pixelfx::core
