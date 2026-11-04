#include "core/settings/settings.h"

#include "core/logging.h"
#include "core/settings/settings_keys.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

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
    const QString stored = m_settings->value(key(keys::kLastOpenFolder)).toString();
    if (!stored.isEmpty() && QFileInfo(stored).isDir()) {
        return stored;
    }
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.isEmpty() ? QDir::homePath() : pictures;
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
