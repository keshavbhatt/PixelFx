#include "core/about.h"

#include <QFile>
#include <QGuiApplication>
#include <QSet>
#include <QSysInfo>
#include <QThread>

#include <array>
#include <pfx/io.h>
#include <pfx/log.h>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

const QList<Notice>& notices()
{
    // Keep in step with THIRD_PARTY.md (tst_about checks every shipped row is here).
    static const QList<Notice> list = {
        {u"PixelFx"_s, u"GPL-3.0-or-later"_s, QObject::tr("This app"), u"pixelfx.txt"_s},
        {u"libpixelfx"_s, u"Apache-2.0"_s, QObject::tr("The image engine"), u"libpixelfx.txt"_s},
        {u"Qt"_s, u"LGPL-3.0-only"_s, QObject::tr("The windows and controls"), u"qt.txt"_s},
        {u"libvips"_s, u"LGPL-2.1-or-later"_s, QObject::tr("Reading, writing and resizing photos"),
         u"libvips.txt"_s},
        {u"Little CMS"_s, u"MIT"_s, QObject::tr("Colour management"), u"lcms.txt"_s},
        {u"libheif"_s, u"LGPL-3.0-or-later"_s, QObject::tr("HEIC and AVIF photos"), u"libheif.txt"_s},
        {u"libde265"_s, u"LGPL-3.0-or-later"_s, QObject::tr("HEIC decoding"), u"libde265.txt"_s},
        {u"dav1d"_s, u"BSD-2-Clause"_s, QObject::tr("AVIF decoding"), u"dav1d.txt"_s},
        {u"libjxl"_s, u"BSD-3-Clause"_s, QObject::tr("JPEG XL photos"), u"libjxl.txt"_s},
        {u"highway"_s, u"Apache-2.0 or BSD-3-Clause"_s, QObject::tr("Fast maths for libjxl"),
         u"highway.txt"_s},
        {u"brotli"_s, u"MIT"_s, QObject::tr("Compression in JPEG XL"), u"brotli.txt"_s},
        {u"LibRaw"_s, u"LGPL-2.1-only or CDDL-1.0"_s, QObject::tr("Camera RAW photos"), u"libraw.txt"_s},
        {u"nlohmann/json"_s, u"MIT"_s, QObject::tr("Reading and writing presets and looks"),
         u"nlohmann-json.txt"_s},
        {u"Lucide icons"_s, u"ISC"_s, QObject::tr("The icons"), u"lucide.txt"_s},
    };
    return list;
}

QString noticeText(const Notice& notice)
{
    QFile file(u":/notices/"_s + notice.file);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

namespace {

QStringList fromStd(const std::vector<std::string>& list)
{
    QStringList out;
    for (const std::string& s : list) {
        out.append(QString::fromStdString(s));
    }
    return out;
}

QString desktopName()
{
    QStringList parts;
#ifdef Q_OS_LINUX
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP").section(u':', 0, 0);
    if (!desktop.isEmpty()) {
        parts.append(desktop);
    }
#endif
    const QString platform = QGuiApplication::platformName();
    if (platform == u"wayland"_s) {
        parts.append(u"Wayland"_s);
    } else if (platform == u"xcb"_s) {
        parts.append(u"X11"_s);
    }
    return parts.join(u", "_s);
}

QString packagingName()
{
    if (qEnvironmentVariableIsSet("SNAP")) {
        return u"snap"_s;
    }
    if (qEnvironmentVariableIsSet("FLATPAK_ID")) {
        return u"Flatpak"_s;
    }
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        return u"AppImage"_s;
    }
    return {};
}

} // namespace

DiagnosticsFacts collectDiagnostics(const QString& appVersion, const QString& revision)
{
    DiagnosticsFacts f;
    f.appVersion = appVersion;
    f.revision = revision == u"unknown"_s ? QString() : revision;
    f.libraryVersion = QString::fromLatin1(pfx::versionString());
    f.qtVersion = QString::fromLatin1(qVersion());
    for (const pfx::EngineVersion& e : pfx::engineVersions()) {
        f.engines.append({QString::fromStdString(e.name), QString::fromStdString(e.version)});
    }
    f.system = QSysInfo::prettyProductName();
#ifdef Q_OS_LINUX
    f.system += u", Linux "_s + QSysInfo::kernelVersion(); // distributions differ in their kernels
#endif
    f.desktop = desktopName();
    f.packaging = packagingName();
    f.readExtensions = fromStd(pfx::readableExtensions());
    f.writeExtensions = fromStd(pfx::writableExtensions());
    f.threads = QThread::idealThreadCount();
    return f;
}

QStringList formatNames(const QStringList& extensions)
{
    struct Format
    {
        const char* name;
        std::array<const char*, 4> extensions;
    };
    static const std::array<Format, 10> kFormats = {{
        {"JPEG", {".jpg", ".jpeg", nullptr, nullptr}},
        {"PNG", {".png", nullptr, nullptr, nullptr}},
        {"WebP", {".webp", nullptr, nullptr, nullptr}},
        {"TIFF", {".tif", ".tiff", nullptr, nullptr}},
        {"HEIC", {".heic", ".heif", nullptr, nullptr}},
        {"AVIF", {".avif", nullptr, nullptr, nullptr}},
        {"JPEG XL", {".jxl", nullptr, nullptr, nullptr}},
        {"GIF", {".gif", nullptr, nullptr, nullptr}},
        {"BMP", {".bmp", nullptr, nullptr, nullptr}},
        {"RAW", {".dng", ".cr2", ".nef", ".arw"}},
    }};
    const QSet<QString> present(extensions.begin(), extensions.end());
    QStringList names;
    for (const Format& format : kFormats) {
        for (const char* ext : format.extensions) {
            if (ext != nullptr && present.contains(QString::fromLatin1(ext))) {
                names.append(QString::fromLatin1(format.name));
                break;
            }
        }
    }
    return names;
}

QString diagnosticsText(const DiagnosticsFacts& f)
{
    QStringList lines;
    lines << u"PixelFx %1%2, libpixelfx %3"_s.arg(
        f.appVersion, f.revision.isEmpty() ? QString() : u" (%1)"_s.arg(f.revision), f.libraryVersion);
    QStringList engines{u"Qt %1"_s.arg(f.qtVersion)};
    for (const auto& [name, version] : f.engines) {
        engines << u"%1 %2"_s.arg(name, version);
    }
    lines << engines.join(u", "_s);
    QString system = f.system;
    if (!f.desktop.isEmpty()) {
        system += u" (%1)"_s.arg(f.desktop);
    }
    if (!f.packaging.isEmpty()) {
        system += u", "_s + f.packaging;
    }
    lines << system;
    lines << u"Reads: "_s + formatNames(f.readExtensions).join(u", "_s);
    lines << u"Writes: "_s + formatNames(f.writeExtensions).join(u", "_s);
    lines << u"Threads: %1"_s.arg(f.threads);
    return lines.join(u'\n');
}

} // namespace pixelfx::core
