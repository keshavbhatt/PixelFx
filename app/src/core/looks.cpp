#include "core/looks.h"

#include "core/file_names.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <pfx/document.h>
#include <pfx/io.h>
#include <pfx/lut.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

constexpr const char* kContext = "Looks";

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

} // namespace

// The looks shipped with the app: :/looks/<category>/<NN>-<slug>.pfxlook, NN the display order.
const std::vector<LookEntry>& builtInLooks()
{
    static const std::vector<LookEntry> looks = [] {
        std::vector<LookEntry> out;
        for (const char* category : {"color", "film", "bw"}) {
            const QDir dir(u":/looks/"_s + QLatin1StringView(category));
            for (const QFileInfo& file : dir.entryInfoList({u"*.pfxlook"_s}, QDir::Files, QDir::Name)) {
                QFile in(file.absoluteFilePath());
                if (!in.open(QIODevice::ReadOnly)) {
                    continue;
                }
                auto doc = pfx::parseDocument(in.readAll().toStdString());
                if (!doc || doc.value().kind != pfx::DocumentKind::Look ||
                    !doc.value().id.starts_with("builtin:")) {
                    continue;
                }
                out.push_back({QString::fromStdString(doc.value().id),
                               QCoreApplication::translate(kContext, doc.value().name.c_str()),
                               QString::fromStdString(doc.value().look.category), QString(),
                               std::make_shared<const pfx::Look>(std::move(doc.value().look))});
            }
        }
        return out;
    }();
    return looks;
}

const LookEntry* builtInLook(const QString& id)
{
    const auto& looks = builtInLooks();
    const auto it = std::ranges::find(looks, id, &LookEntry::id);
    return it == looks.end() ? nullptr : &*it;
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
