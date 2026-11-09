#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

namespace pixelfx::core {

/// One open-source component PixelFx ships or is built from (THIRD_PARTY.md), with its licence
/// text built in.
struct Notice
{
    QString name;
    QString licence; ///< SPDX words, as THIRD_PARTY.md gives them
    QString use;     ///< what PixelFx uses it for, in plain words
    QString file;    ///< the licence text, under :/notices/
};

/// Every component, PixelFx and libpixelfx first.
[[nodiscard]] const QList<Notice>& notices();
/// The licence text of `notice`, as the component publishes it.
[[nodiscard]] QString noticeText(const Notice& notice);

/// What a bug report needs to know about this build and system, and nothing personal: no file
/// names, no paths, nothing about accounts.
struct DiagnosticsFacts
{
    QString appVersion;
    QString revision; ///< empty when unknown
    QString libraryVersion;
    QString qtVersion;
    QList<std::pair<QString, QString>> engines; ///< name and version
    QString system;                             ///< "Arch Linux, Linux 7.2.7"
    QString desktop;                            ///< "KDE, Wayland"; empty when unknown
    QString packaging;                          ///< "snap", "Flatpak"; empty when none
    QStringList readExtensions;
    QStringList writeExtensions;
    int threads = 0;
};

/// The revision PixelFx was built from (the application's "revision" property); empty when unknown.
[[nodiscard]] QString buildRevision();
/// The facts of the running build and system; the app passes its own version and revision.
/// How this copy was installed: "snap", "Flatpak" or "AppImage"; empty for a distro package or a
/// build from source.
[[nodiscard]] QString packagingName();
[[nodiscard]] DiagnosticsFacts collectDiagnostics(const QString& appVersion, const QString& revision);
/// `facts` as plain text to paste into a bug report. Pure, tested.
[[nodiscard]] QString diagnosticsText(const DiagnosticsFacts& facts);
/// Format names for file extensions (".jpg", ".cr2" ...): "JPEG", "RAW", in a fixed order;
/// extensions with no known name are left out. Pure, tested.
[[nodiscard]] QStringList formatNames(const QStringList& extensions);

} // namespace pixelfx::core
