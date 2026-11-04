#pragma once

#include "core/presets.h"
#include "services/document.h"
#include "services/exporter.h"
#include "services/thumbnailer.h"

#include <QMainWindow>

#include <functional>
#include <pfx/settings.h>

class QLabel;
class QStackedWidget;
class QToolButton;
class QTimer;
class QPushButton;

namespace pixelfx::core {
class Settings;
} // namespace pixelfx::core

namespace pixelfx::ui {

class AdjustPanel;
class Canvas;
class BatchPage;
class CropPanel;
class Filmstrip;
class PresetsPanel;
class WelcomePage;

/// The main window (DESIGN section 2). M2: the header bar, the welcome page, and the canvas
/// showing one opened photo. It owns the open Document; it holds no image logic itself.
class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    explicit MainWindow(core::Settings& settings, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Opens the first supported file of `files` (the filmstrip for many arrives with M3).
    void openFiles(const QStringList& files);
    void chooseFiles();
    /// Switches to photo `index` of the filmstrip.
    void showPhoto(int index);
    [[nodiscard]] Filmstrip* filmstrip() const { return m_strip; }
    /// Opens the Export sheet for the open photo and writes it.
    void exportPhoto();
    [[nodiscard]] services::Exporter& exporter() { return m_exporter; }

    [[nodiscard]] services::Document& document() { return m_document; }
    [[nodiscard]] core::Settings& settings() { return m_settings; }
    [[nodiscard]] Canvas* canvas() const { return m_canvas; }
    [[nodiscard]] AdjustPanel* adjustPanel() const { return m_adjust; }
    [[nodiscard]] CropPanel* cropPanel() const { return m_cropPanel; }
    [[nodiscard]] PresetsPanel* presetsPanel() const { return m_presets; }
    /// Re-reads the preset library and redraws the preset pictures.
    void refreshPresets();
    /// Saves the photo's settings as a preset through the Save as preset sheet.
    void savePreset();
    /// Imports PixelFx, Lightroom and Camera Raw presets chosen in a file dialog.
    void importPresets();
    /// Crop mode (C): the crop box over the whole photo and the crop panel.
    void enterCrop();
    /// Leaves Crop mode, keeping the changes as one history step (`apply`) or dropping them.
    void leaveCrop(bool apply);
    [[nodiscard]] bool isCropping() const;
    /// Batch mode (B): the Batch page in place of the photo.
    void enterBatch();
    void leaveBatch();
    [[nodiscard]] bool isBatching() const;
    [[nodiscard]] BatchPage* batchPage() const { return m_batch; }
    /// Shows the photo without its settings while `on` (the eye button, or Space held).
    void showOriginal(bool on);
    /// Before and after on or off (the \\ key and the header button).
    void setCompare(bool on);

    /// Copies the photo's settings (crop excepted) to the clipboard; pastes them as one step.
    void copySettings();
    void pasteSettings();
    /// The Settings sheet (Ctrl+,).
    void openSettings();
    /// Opens the picture on the clipboard, if there is one (Ctrl+V).
    void pasteImage();
    /// Tab: hides or shows the side panels and the filmstrip (menu.html, "Hide panels").
    void setPanelsHidden(bool hidden);
    [[nodiscard]] bool panelsHidden() const;

    /// Whether `path` has an extension PixelFx opens.
    [[nodiscard]] static bool isSupported(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void setupUi();
    void buildHeader();
    void buildEditButtons();
    void buildRail();
    void buildBatch();
    void buildWelcome();
    QWidget* buildCanvasPage();
    void connectCrop();
    void updateCropStrip();
    void setupActions();
    void buildMainMenu();
    void buildRecentMenu(class QMenu* recent);
    void setupPanelActions();
    [[nodiscard]] QAction* namedAction(const QString& name) const;
    void setupEditActions();
    void setupCropActions();
    void setupZoomActions();
    void connectZoom();
    void connectClipboard();
    void connectWhiteBalance();
    void applyGeneralSettings();
    void exportNext();
    void connectPresets();
    void connectLooks();
    void importLooks();
    void requestLookThumbnails();
    void connectFilmstrip();
    /// `change` on the current photo as one step, and on every other selected photo's
    /// remembered edits.
    void applyToSelection(const std::function<pfx::Settings(const pfx::Settings&)>& change,
                          const QString& label);
    void connectPresetActions();
    void requestPresetThumbnails();
    void applyPreset(const core::PresetEntry& entry);
    void connectDocument();
    void updateHistoryButtons();
    void showToast(const QString& text);
    void showHistory();
    void showOpened(const services::DocumentInfo& info);
    void showFailed(const QString& path, const QString& message);
    [[nodiscard]] int previewLongEdge() const;

    core::Settings& m_settings;
    services::Document m_document;
    services::Exporter m_exporter;
    QLabel* m_toast = nullptr;
    QPushButton* m_export = nullptr;
    QStackedWidget* m_stack = nullptr;
    WelcomePage* m_welcome = nullptr;
    Canvas* m_canvas = nullptr;
    AdjustPanel* m_adjust = nullptr;
    CropPanel* m_cropPanel = nullptr;
    PresetsPanel* m_presets = nullptr;
    struct ExportJob
    {
        QString path;
        pfx::Settings settings;
    };
    QList<ExportJob> m_exportQueue;
    core::ExportOptions m_exportOptions;
    int m_exportTotal = 0;
    int m_exportSaved = 0;
    Filmstrip* m_strip = nullptr;
    services::Thumbnailer* m_stripThumbnails = nullptr;
    core::PresetLibrary m_library{core::PresetLibrary::defaultFolder()};
    services::Thumbnailer* m_presetThumbnails = nullptr;
    services::Thumbnailer* m_lookThumbnails = nullptr;
    int m_lookGeneration = 0;
    QTimer* m_thumbnailTimer = nullptr;
    int m_thumbnailGeneration = 0;
    QStackedWidget* m_panels = nullptr;
    QWidget* m_rail = nullptr;
    QToolButton* m_editMode = nullptr;
    QToolButton* m_cropMode = nullptr;
    QToolButton* m_batchMode = nullptr;
    BatchPage* m_batch = nullptr;
    QWidget* m_canvasPage = nullptr;
    QWidget* m_cropStrip = nullptr;
    QLabel* m_cropSize = nullptr;
    pfx::Settings m_cropStart;
    QList<QAction*> m_cropActions;
    QToolButton* m_undo = nullptr;
    QToolButton* m_redo = nullptr;
    QToolButton* m_original = nullptr;
    QToolButton* m_history = nullptr;
    QToolButton* m_compare = nullptr;
    QToolButton* m_menuButton = nullptr;
    QWidget* m_leftDock = nullptr;
    QWidget* m_rightDock = nullptr;
    QWidget* m_stripDock = nullptr;
    QAction* m_hidePanels = nullptr;
    bool m_showingOriginal = false;
    bool m_copying = false; ///< the exporter is copying to the clipboard, not saving
    QLabel* m_docName = nullptr;
    QLabel* m_docMeta = nullptr;
    QWidget* m_header = nullptr;
};

} // namespace pixelfx::ui
