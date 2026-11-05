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
    o.keepLocation = m_settings->value(key(keys::kExportKeepLocation), o.keepLocation).toBool();
    o.sizePreset = m_settings->value(key(keys::kExportSizePreset)).toString();
    o.fitInside = m_settings->value(key(keys::kExportFitInside), false).toBool();
    o.watermark = m_settings->value(key(keys::kExportWatermark)).toString();
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
    m_settings->setValue(key(keys::kExportKeepLocation), options.keepLocation);
    m_settings->setValue(key(keys::kExportSizePreset), options.sizePreset);
    m_settings->setValue(key(keys::kExportFitInside), options.fitInside);
    m_settings->setValue(key(keys::kExportWatermark), options.watermark);
    m_settings->setValue(key(keys::kExportFolder), options.folder);
    m_settings->setValue(key(keys::kExportNameTemplate), options.nameTemplate);
    m_settings->setValue(key(keys::kExportIfExists), ifExistsKey(options.ifExists));
}

CompressJob Settings::compressJob() const
{
    CompressJob j;
    const auto value = [this](const char* name, const QVariant& fallback) {
        return m_settings->value(key(name), fallback);
    };
    j.codec.jpeg = value(keys::kCompressJpeg, u"lossless"_s).toString() == u"quality"_s
                       ? pfx::JpegCompression::Quality
                       : pfx::JpegCompression::Lossless;
    j.codec.jpegQuality = std::clamp(value(keys::kCompressJpegQuality, j.codec.jpegQuality).toInt(), 1, 100);
    j.codec.png = value(keys::kCompressPng, u"lossless"_s).toString() == u"fewerColours"_s
                      ? pfx::PngCompression::FewerColours
                      : pfx::PngCompression::Lossless;
    j.codec.pngQuality = std::clamp(value(keys::kCompressPngQuality, j.codec.pngQuality).toInt(), 40, 100);
    j.codec.removeExtras = value(keys::kCompressRemoveExtras, j.codec.removeExtras).toBool();
    j.folder = value(keys::kCompressFolder, defaultCompressFolder()).toString();
    j.nameTemplate = value(keys::kCompressNameTemplate, j.nameTemplate).toString();
    j.keepIfNotSmaller = value(keys::kCompressKeepIfNotSmaller, j.keepIfNotSmaller).toBool();
    return j;
}

void Settings::setCompressJob(const CompressJob& job)
{
    m_settings->setValue(key(keys::kCompressJpeg),
                         job.codec.jpeg == pfx::JpegCompression::Quality ? u"quality"_s : u"lossless"_s);
    m_settings->setValue(key(keys::kCompressJpegQuality), job.codec.jpegQuality);
    m_settings->setValue(key(keys::kCompressPng), job.codec.png == pfx::PngCompression::FewerColours
                                                      ? u"fewerColours"_s
                                                      : u"lossless"_s);
    m_settings->setValue(key(keys::kCompressPngQuality), job.codec.pngQuality);
    m_settings->setValue(key(keys::kCompressRemoveExtras), job.codec.removeExtras);
    m_settings->setValue(key(keys::kCompressFolder), job.folder);
    m_settings->setValue(key(keys::kCompressNameTemplate), job.nameTemplate);
    m_settings->setValue(key(keys::kCompressKeepIfNotSmaller), job.keepIfNotSmaller);
}

bool Settings::batchCompressOnly() const
{
    return m_settings->value(key(keys::kBatchCompressOnly), false).toBool();
}

void Settings::setBatchCompressOnly(bool on)
{
    m_settings->setValue(key(keys::kBatchCompressOnly), on);
}

int Settings::batchAtOnce() const
{
    const int n = m_settings->value(key(keys::kBatchAtOnce), 0).toInt();
    return (n == 1 || n == 2 || n == 4 || n == 8) ? n : 0;
}

void Settings::setBatchAtOnce(int count)
{
    if (count != batchAtOnce()) {
        m_settings->setValue(key(keys::kBatchAtOnce), count);
        Q_EMIT batchAtOnceChanged(batchAtOnce());
    }
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

Settings::Theme Settings::theme() const
{
    const int value = m_settings->value(key(keys::kTheme), static_cast<int>(Theme::Dark)).toInt();
    return value >= 0 && value <= static_cast<int>(Theme::Light) ? static_cast<Theme>(value) : Theme::Dark;
}

void Settings::setTheme(Theme theme)
{
    m_settings->setValue(key(keys::kTheme), static_cast<int>(theme));
}

Settings::CanvasBackground Settings::canvasBackground() const
{
    const int value =
        m_settings->value(key(keys::kCanvasBackground), static_cast<int>(CanvasBackground::DarkGray)).toInt();
    return value >= 0 && value <= static_cast<int>(CanvasBackground::White)
               ? static_cast<CanvasBackground>(value)
               : CanvasBackground::DarkGray;
}

void Settings::setCanvasBackground(CanvasBackground background)
{
    if (background != canvasBackground()) {
        m_settings->setValue(key(keys::kCanvasBackground), static_cast<int>(background));
        Q_EMIT canvasBackgroundChanged(background);
    }
}

bool Settings::rememberEdits() const
{
    return m_settings->value(key(keys::kRememberEdits), true).toBool();
}

void Settings::setRememberEdits(bool on)
{
    if (on != rememberEdits()) {
        m_settings->setValue(key(keys::kRememberEdits), on);
        Q_EMIT rememberEditsChanged(on);
    }
}

bool Settings::openAsLeft() const
{
    return m_settings->value(key(keys::kOpenAsLeft), true).toBool();
}

void Settings::setOpenAsLeft(bool on)
{
    if (on != openAsLeft()) {
        m_settings->setValue(key(keys::kOpenAsLeft), on);
        Q_EMIT openAsLeftChanged(on);
    }
}

bool Settings::clippingWarnings() const
{
    return m_settings->value(key(keys::kClippingWarnings), false).toBool();
}

void Settings::setClippingWarnings(bool on)
{
    if (on != clippingWarnings()) {
        m_settings->setValue(key(keys::kClippingWarnings), on);
        Q_EMIT clippingWarningsChanged(on);
    }
}

std::optional<QStringList> Settings::openAdjustGroups() const
{
    const QString name = key(keys::kOpenAdjustGroups);
    if (!m_settings->contains(name)) {
        return std::nullopt;
    }
    return m_settings->value(name).toStringList();
}

void Settings::setOpenAdjustGroups(const QStringList& ids)
{
    m_settings->setValue(key(keys::kOpenAdjustGroups), ids);
}

QStringList Settings::openAdjustMore() const
{
    return m_settings->value(key(keys::kOpenAdjustMore)).toStringList();
}

void Settings::setOpenAdjustMore(const QStringList& ids)
{
    m_settings->setValue(key(keys::kOpenAdjustMore), ids);
}

bool Settings::oneGroupAtATime() const
{
    return m_settings->value(key(keys::kOneGroupAtATime), false).toBool();
}

void Settings::setOneGroupAtATime(bool on)
{
    m_settings->setValue(key(keys::kOneGroupAtATime), on);
}

QStringList Settings::favoritePresets() const
{
    return m_settings->value(key(keys::kFavoritePresets)).toStringList();
}

void Settings::setFavoritePresets(const QStringList& ids)
{
    m_settings->setValue(key(keys::kFavoritePresets), ids);
}

QStringList Settings::recentPresets() const
{
    return m_settings->value(key(keys::kRecentPresets)).toStringList();
}

void Settings::notePresetUsed(const QString& id)
{
    constexpr qsizetype kRecent = 12;
    QStringList recent = recentPresets();
    recent.removeAll(id);
    recent.prepend(id);
    m_settings->setValue(key(keys::kRecentPresets), recent.mid(0, kRecent));
}

QStringList Settings::closedPresetCategories() const
{
    return m_settings->value(key(keys::kClosedPresetCategories)).toStringList();
}

void Settings::setClosedPresetCategories(const QStringList& names)
{
    m_settings->setValue(key(keys::kClosedPresetCategories), names);
}

bool Settings::showWhatsNew() const
{
    return m_settings->value(key(keys::kShowWhatsNew), true).toBool();
}

void Settings::setShowWhatsNew(bool on)
{
    m_settings->setValue(key(keys::kShowWhatsNew), on);
}

QString Settings::whatsNewSeen() const
{
    return m_settings->value(key(keys::kWhatsNewSeen)).toString();
}

void Settings::setWhatsNewSeen(const QString& version)
{
    m_settings->setValue(key(keys::kWhatsNewSeen), version);
}

} // namespace pixelfx::core
