#pragma once

#include <QString>

#include <optional>
#include <pfx/settings.h>

namespace pixelfx::core {

/// A photo's file fingerprint: its size plus a SHA-1 of its first and last 64 KiB
/// (DOCS/presets.md section 4). Survives renames and moves; reads at most 128 KiB.
/// Empty when the file cannot be read.
[[nodiscard]] QString fileFingerprint(const QString& path);

/// Where each photo's edits are kept (FEATURES E17, PLAN D18): one recipe file per photo in the
/// app's library folder, named by the photo's fingerprint. Plain file work; callers run it off
/// the GUI thread (ADR-006).
class SidecarStore
{
public:
    explicit SidecarStore(QString folder);

    [[nodiscard]] const QString& folder() const { return m_folder; }
    [[nodiscard]] QString pathFor(const QString& fingerprint) const;

    /// The settings stored for this fingerprint, if any.
    [[nodiscard]] std::optional<pfx::Settings> read(const QString& fingerprint) const;
    /// Stores `settings` for the photo; empty settings remove its file. Returns success.
    [[nodiscard]] bool write(const QString& photo, const QString& fingerprint,
                             const pfx::Settings& settings) const;

    /// The default folder: <AppDataLocation>/edits.
    [[nodiscard]] static QString defaultFolder();

private:
    QString m_folder;
};

} // namespace pixelfx::core
