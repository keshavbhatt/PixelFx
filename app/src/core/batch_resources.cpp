#include "core/batch_resources.h"

#include "core/looks.h"
#include "core/watermark.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

constexpr qint64 kMaxLogoBytes = qint64{8} * 1024 * 1024;
const auto kLogoData = u"logoData"_s;

QByteArray fileBytes(const QString& path)
{
    QFile file(path);
    if (file.size() > kMaxLogoBytes || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void embedLooks(pfx::BatchBody& body)
{
    for (const pfx::BatchStep& step : body.stack) {
        const std::string id = step.settings.text("look.id", "");
        if (id.empty() || id.starts_with("builtin:") ||
            std::ranges::any_of(body.looks, [&](const pfx::EmbeddedLook& e) { return e.id == id; })) {
            continue;
        }
        const QString qid = QString::fromStdString(id);
        if (const auto look = findLook(qid)) {
            body.looks.push_back({id, lookName(qid).toStdString(), *look});
        }
    }
}

void embedWatermark(pfx::BatchBody& body, const QString& path)
{
    const auto w = WatermarkLibrary::load(path);
    if (!w) {
        return;
    }
    QJsonObject json = QJsonDocument::fromJson(watermarkToJson(*w)).object();
    if (w->kind == Watermark::Kind::Logo && !w->logo.isEmpty()) {
        const QByteArray logo = fileBytes(w->logo);
        if (logo.isEmpty()) {
            return; // a logo mark without its logo would draw nothing
        }
        json.insert(kLogoData, QString::fromLatin1(logo.toBase64()));
    }
    body.watermark = QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString();
}

// A look's file name from its id: letters, digits, '-' and '_' only.
QString fileNameFor(const std::string& id)
{
    static const QRegularExpression kUnsafe(u"[^A-Za-z0-9_-]"_s);
    QString name = QString::fromStdString(id);
    name.replace(kUnsafe, u"-"_s);
    return name.left(80) + u".pfxlook"_s;
}

void adoptLooks(const pfx::BatchBody& body, const QString& folder, AdoptedResources& out)
{
    if (body.looks.empty()) {
        return;
    }
    const std::vector<LookEntry> mine = LookLibrary(folder).entries();
    bool added = false;
    for (const pfx::EmbeddedLook& e : body.looks) {
        const QString id = QString::fromStdString(e.id);
        if (std::ranges::any_of(mine, [&](const LookEntry& m) { return m.isMine() && m.id == id; })) {
            continue; // already here (the same file, or the same look shared before)
        }
        pfx::Document doc;
        doc.kind = pfx::DocumentKind::Look;
        doc.id = e.id;
        doc.name = e.name;
        doc.look = e.look;
        const QString name = e.name.empty() ? QObject::tr("A look") : QString::fromStdString(e.name);
        if (!QDir().mkpath(folder) ||
            !pfx::saveDocument(doc, QString(folder + u'/' + fileNameFor(e.id)).toStdString()).ok()) {
            out.problems << QObject::tr("The look %1 could not be added to your looks.").arg(name);
            continue;
        }
        out.addedLooks << name;
        added = true;
    }
    if (added) {
        lookFolderChanged();
    }
}

// The same watermark, apart from where its logo file is; logos compared by their bytes.
bool sameWatermark(const Watermark& a, const QByteArray& aLogo, const WatermarkLibrary::Entry& b)
{
    Watermark left = a;
    Watermark right = b.watermark;
    left.logo.clear();
    right.logo.clear();
    left.name = left.name.trimmed();
    if (!(left == right)) {
        return false;
    }
    return a.kind != Watermark::Kind::Logo || fileBytes(b.watermark.logo) == aLogo;
}

void adoptWatermark(const pfx::BatchBody& body, const QString& folder, AdoptedResources& out)
{
    if (body.watermark.empty()) {
        return;
    }
    QJsonObject json = QJsonDocument::fromJson(QByteArray::fromStdString(body.watermark)).object();
    const QByteArray logo = QByteArray::fromBase64(json.take(kLogoData).toString().toLatin1());
    auto w = watermarkFromJson(QJsonDocument(json).toJson());
    if (!w || (w->kind == Watermark::Kind::Logo && logo.isEmpty())) {
        out.problems << QObject::tr("The recipe's watermark could not be read.");
        return;
    }
    const WatermarkLibrary library(folder);
    for (const WatermarkLibrary::Entry& e : library.entries()) {
        if (sameWatermark(*w, logo, e)) {
            out.watermarkPath = e.path;
            return;
        }
    }
    const QTemporaryDir scratch;
    if (w->kind == Watermark::Kind::Logo) {
        // Saving copies the logo in from where it is; give it a file with the logo's own suffix.
        const QString suffix = QFileInfo(w->logo).suffix().toLower();
        const QString file = scratch.filePath(u"logo."_s + (suffix == u"svg"_s ? suffix : u"png"_s));
        QFile f(file);
        if (!scratch.isValid() || !f.open(QIODevice::WriteOnly) || f.write(logo) != logo.size()) {
            out.problems << QObject::tr("The recipe's watermark could not be added.");
            return;
        }
        f.close();
        w->logo = file;
    }
    if (w->name.trimmed().isEmpty()) {
        w->name = QObject::tr("Batch watermark");
    }
    out.watermarkPath = library.save(*w);
    if (out.watermarkPath.isEmpty()) {
        out.problems << QObject::tr("The recipe's watermark could not be added.");
    }
}

} // namespace

void embedBatchResources(pfx::BatchBody& body, const QString& watermarkPath)
{
    embedLooks(body);
    body.watermark.clear();
    if (!watermarkPath.isEmpty()) {
        embedWatermark(body, watermarkPath);
    }
}

AdoptedResources adoptBatchResources(const pfx::BatchBody& body, const QString& lookFolder,
                                     const QString& watermarkFolder)
{
    AdoptedResources out;
    adoptLooks(body, lookFolder, out);
    adoptWatermark(body, watermarkFolder, out);
    return out;
}

} // namespace pixelfx::core
