#pragma once

#include "ui/sheet.h"

#include <functional>

class QFormLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace pixelfx::core {
struct ExportOptions;
class Settings;
} // namespace pixelfx::core

namespace pixelfx::ui {

/// Settings: a sheet with a sidebar of pages where every row writes straight to the settings,
/// no OK button (`settings.html`).
class SettingsSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsSheet)

public:
    enum Page
    {
        GeneralPage,
        EditorPage,
        ExportPage,
        LooksPage,
        PerformancePage,
        AdvancedPage,
    };

    /// `looksFolder` and `presetsFolder` hold the user's looks and presets.
    SettingsSheet(core::Settings& settings, QString looksFolder, QString presetsFolder,
                  QWidget* parent = nullptr);
    ~SettingsSheet() override;

    void showPage(Page page);
    [[nodiscard]] Page page() const;
    /// Counts the user's looks and presets again (after an import).
    void refreshCounts();

Q_SIGNALS:
    /// Import presets and looks was clicked; the window imports, then calls refreshCounts().
    void importRequested();

private:
    QWidget* buildGeneral();
    QWidget* buildEditor();
    QWidget* buildExport();
    QWidget* exportFolderRow(const QString& path, QWidget* page);
    QWidget* exportMetadataBox(const core::ExportOptions& options, QWidget* page);
    /// Changes the remembered export choices with `edit`.
    void changeExport(const std::function<void(core::ExportOptions&)>& edit);
    QWidget* buildLooks();
    QWidget* buildPerformance();
    QWidget* buildAdvanced();
    /// Fills the stack with every page, built from the settings as they are now.
    void buildPages();
    void restoreDefaults();

    core::Settings& m_settings;
    QString m_looksFolder;
    QString m_presetsFolder;
    QListWidget* m_pages;
    QStackedWidget* m_stack;
    QLabel* m_lookCount = nullptr;
    QLabel* m_presetCount = nullptr;
    QLabel* m_recentCount = nullptr;
    QPushButton* m_clearRecent = nullptr;
};

} // namespace pixelfx::ui
