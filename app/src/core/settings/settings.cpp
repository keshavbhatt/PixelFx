#include "core/settings/settings.h"

#include "core/logging.h"
#include "core/settings/settings_keys.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

QString key(const char* name)
{
    return QString::fromLatin1(name);
}

} // namespace

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_settings(std::make_unique<QSettings>())
{}

Settings::Settings(const QString& fileName, QObject* parent)
    : QObject(parent)
    , m_settings(std::make_unique<QSettings>(fileName, QSettings::IniFormat))
{}

Settings::~Settings() = default;

QString Settings::fileName() const
{
    return m_settings->fileName();
}

void Settings::sync()
{
    m_settings->sync();
}

QByteArray Settings::windowGeometry() const
{
    return m_settings->value(key(keys::kWindowGeometry)).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    m_settings->setValue(key(keys::kWindowGeometry), geometry);
}

QByteArray Settings::windowState() const
{
    return m_settings->value(key(keys::kWindowState)).toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    m_settings->setValue(key(keys::kWindowState), state);
}

QString Settings::lastOpenFolder() const
{
    QString stored = m_settings->value(key(keys::kLastOpenFolder)).toString();
    if (!stored.isEmpty() && QFileInfo(stored).isDir()) {
        return stored;
    }
    // Pictures only when it exists (a fresh account or a CI machine may have none).
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return !pictures.isEmpty() && QFileInfo(pictures).isDir() ? pictures : QDir::homePath();
}

void Settings::setLastOpenFolder(const QString& folder)
{
    if (folder == m_settings->value(key(keys::kLastOpenFolder)).toString()) {
        return;
    }
    m_settings->setValue(key(keys::kLastOpenFolder), folder);
    Q_EMIT lastOpenFolderChanged(folder);
}

QStringList Settings::recentFiles() const
{
    return m_settings->value(key(keys::kRecentFiles)).toStringList();
}

void Settings::addRecentFile(const QString& path)
{
    QStringList files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > kMaxRecentFiles) {
        files.removeLast();
    }
    m_settings->setValue(key(keys::kRecentFiles), files);
    Q_EMIT recentFilesChanged(files);
}

void Settings::clearRecentFiles()
{
    m_settings->remove(key(keys::kRecentFiles));
    Q_EMIT recentFilesChanged({});
}

ExportOptions Settings::exportOptions() const
{
    ExportOptions o;
    o.extension = m_settings->value(key(keys::kExportExtension), o.extension).toString();
    o.quality = std::clamp(m_settings->value(key(keys::kExportQuality), o.quality).toInt(), 1, 100);
    o.bitsPerSample = m_settings->value(key(keys::kExportBits), o.bitsPerSample).toInt() == 16 ? 16 : 8;
    o.longEdge = std::max(0, m_settings->value(key(keys::kExportLongEdge), o.longEdge).toInt());
    o.keepMetadata = m_settings->value(key(keys::kExportKeepMetadata), o.keepMetadata).toBool();
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString defaultFolder = (pictures.isEmpty() ? QDir::homePath() : pictures) + u"/PixelFx"_s;
    o.folder = m_settings->value(key(keys::kExportFolder), defaultFolder).toString();
    o.nameTemplate = m_settings->value(key(keys::kExportNameTemplate), o.nameTemplate).toString();
    o.ifExists = ifExistsFromKey(m_settings->value(key(keys::kExportIfExists)).toString());
    return o;
}

void Settings::setExportOptions(const ExportOptions& options)
{
    m_settings->setValue(key(keys::kExportExtension), options.extension);
    m_settings->setValue(key(keys::kExportQuality), options.quality);
    m_settings->setValue(key(keys::kExportBits), options.bitsPerSample);
    m_settings->setValue(key(keys::kExportLongEdge), options.longEdge);
    m_settings->setValue(key(keys::kExportKeepMetadata), options.keepMetadata);
    m_settings->setValue(key(keys::kExportFolder), options.folder);
    m_settings->setValue(key(keys::kExportNameTemplate), options.nameTemplate);
    m_settings->setValue(key(keys::kExportIfExists), ifExistsKey(options.ifExists));
}

bool Settings::migrateFromVersion1(const QString& legacyFile)
{
    if (m_settings->value(key(keys::kLegacyMigrated), false).toBool()) {
        return false;
    }
    m_settings->setValue(key(keys::kLegacyMigrated), true);
    if (!QFileInfo::exists(legacyFile)) {
        return false;
    }
    const QSettings legacy(legacyFile, QSettings::IniFormat);
    const QString folder = legacy.value(key(keys::kLegacyLastFolder)).toString();
    if (folder.isEmpty() || !QFileInfo(folder).isDir() || m_settings->contains(key(keys::kLastOpenFolder))) {
        return false;
    }
    setLastOpenFolder(folder);
    qCInfo(lcCore) << "carried the last image folder over from PixelFx 1:" << folder;
    return true;
}

QString Settings::legacyFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           u"/org.keshavnrj.ubuntu/pixelFx.conf"_s;
}

} // namespace pixelfx::core
