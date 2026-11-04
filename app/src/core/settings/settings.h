#pragma once

#include <QByteArray>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace pixelfx::core {

/// The one settings facade (CODING_STANDARDS section 7): typed accessors, defaults in one
/// place, a signal per value the UI reacts to. Owned by Application, passed by reference.
class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Settings)

public:
    static constexpr int kMaxRecentFiles = 12;

    /// The application's settings file (identity from QCoreApplication).
    explicit Settings(QObject* parent = nullptr);
    /// A settings file at `fileName` (tests, scratch profiles).
    explicit Settings(const QString& fileName, QObject* parent = nullptr);
    ~Settings() override;

    [[nodiscard]] QString fileName() const;
    void sync();

    [[nodiscard]] QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    [[nodiscard]] QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    /// The folder the open dialog starts in; Pictures until the user picks something.
    [[nodiscard]] QString lastOpenFolder() const;
    void setLastOpenFolder(const QString& folder);

    /// Most recent first, no duplicates, at most kMaxRecentFiles.
    [[nodiscard]] QStringList recentFiles() const;
    void addRecentFile(const QString& path);
    void clearRecentFiles();

    /// Carries the last image folder over from PixelFx 1's settings file, once. Returns true
    /// when something was migrated.
    bool migrateFromVersion1(const QString& legacyFile);
    /// Where PixelFx 1 kept its settings (inside the snap's home when running as a snap).
    [[nodiscard]] static QString legacyFilePath();

Q_SIGNALS:
    void lastOpenFolderChanged(const QString& folder);
    void recentFilesChanged(const QStringList& files);

private:
    std::unique_ptr<QSettings> m_settings;
};

} // namespace pixelfx::core
