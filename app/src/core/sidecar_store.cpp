#include "core/sidecar_store.h"

#include "core/logging.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <pfx/document.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {
constexpr qint64 kChunk = qint64{64} * 1024;
} // namespace

QString fileFingerprint(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const qint64 size = file.size();
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArray::number(size));
    hash.addData(file.read(kChunk));
    if (size > kChunk) {
        file.seek(std::max(kChunk, size - kChunk));
        hash.addData(file.read(kChunk));
    }
    return u"sha1:"_s + QString::fromLatin1(hash.result().toHex().left(16));
}

SidecarStore::SidecarStore(QString folder)
    : m_folder(std::move(folder))
{}

QString SidecarStore::pathFor(const QString& fingerprint) const
{
    QString name = fingerprint;
    name.replace(u':', u'-');
    return QDir(m_folder).filePath(name + u".pfx"_s);
}

std::optional<pfx::Settings> SidecarStore::read(const QString& fingerprint) const
{
    if (fingerprint.isEmpty()) {
        return std::nullopt;
    }
    const QString path = pathFor(fingerprint);
    if (!QFileInfo::exists(path)) {
        return std::nullopt;
    }
    const auto doc = pfx::loadDocument(path.toStdString());
    if (!doc || doc.value().kind != pfx::DocumentKind::Recipe) {
        qCWarning(lcCore) << "ignored an unreadable edit file" << path;
        return std::nullopt;
    }
    return doc.value().settings;
}

bool SidecarStore::write(const QString& photo, const QString& fingerprint,
                         const pfx::Settings& settings) const
{
    if (fingerprint.isEmpty()) {
        return false;
    }
    const QString path = pathFor(fingerprint);
    if (settings.isEmpty()) {
        return !QFileInfo::exists(path) || QFile::remove(path);
    }
    if (!QDir().mkpath(m_folder)) {
        return false;
    }
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Recipe;
    doc.sourceName = QFileInfo(photo).fileName().toStdString();
    doc.sourceSize = static_cast<std::uint64_t>(QFileInfo(photo).size());
    doc.sourceFingerprint = fingerprint.toStdString();
    doc.settings = settings;
    QSaveFile file(path); // written in full or not at all
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QByteArray::fromStdString(pfx::serializeDocument(doc)));
    return file.commit();
}

QString SidecarStore::defaultFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/edits"_s;
}

} // namespace pixelfx::core
