#pragma once

#include "core/export_options.h"
#include "core/looks.h"
#include "core/presets.h"
#include "services/batch_runner.h"
#include "services/thumbnailer.h"

#include <QElapsedTimer>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include <memory>
#include <utility>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialog;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace pixelfx::ui {

class SliderRow;

/// Batch. Left: the photos, as a list
/// or a grid, added as files or whole folders or dropped, the first one before and after the
/// recipe, and while running the progress (Pause, Stop), then the summary (Retry failed, Show in
/// folder). Right: 1. Recipe (a preset or look, or the settings of the photo open in the
/// editor, then a preset on top, Auto enhance, the edits already made), 2. Resize (Never
/// enlarge), 3. Watermark, 4. Save as (format and quality, More..., folder, name, beside each
/// original), and Save as batch recipe and Process at the foot. The window's header shows the
/// count and size and Add photos and Add folder while the page is up.
class BatchPage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BatchPage)

public:
    explicit BatchPage(core::ExportOptions output, QWidget* parent = nullptr);
    ~BatchPage() override;

    /// The presets the recipe can use (the library's list).
    void setPresets(const std::vector<core::PresetEntry>& entries);
    /// Looks offered as the recipe ("Preset or look").
    void setLooks(const std::vector<core::LookEntry>& entries);
    /// The photo open in the editor, for "Settings from a photo"; an empty name when none is.
    void setPhotoSettings(const QString& name, const pfx::Settings& settings);
    /// Adds photos; folders bring every photo inside them, subfolders included.
    void addPaths(const QStringList& paths);
    /// Add photos and Add folder (the header's buttons): a file dialog each.
    void chooseFiles();
    void chooseFolder();
    [[nodiscard]] const QStringList& files() const { return m_files; }
    /// "6 photos · 129 MB", for the header.
    [[nodiscard]] QString summary() const;
    /// Where remembered edits are, for "Keep edits already made"; empty: none.
    void setSidecarFolder(const QString& folder) { m_sidecarFolder = folder; }
    /// The recipe's first choice: "" (none), "look:<id>", "preset:<name>", or "recipe" (an
    /// opened batch recipe's own steps). Settings from a photo is its own source (fromPhoto).
    void chooseRecipe(const QString& choice);
    /// The preset added on top (by name), or none when empty.
    void chooseSecond(const QString& presetName);
    void setFromPhoto(bool on);
    /// The recipe and output as the page shows them.
    [[nodiscard]] services::BatchRecipe recipe() const;
    void process();
    /// Writes the page as a batch recipe (.pfxbatch) to `path`; false when it cannot.
    [[nodiscard]] bool saveRecipe(const QString& path) const;
    /// Sets the page from a batch recipe, adding the looks and the watermark it carries to the
    /// user's own; the reason when it cannot be read, or what could not be added.
    [[nodiscard]] QString loadRecipe(const QString& path);
    [[nodiscard]] services::BatchRunner& runner() { return m_runner; }

    /// Whether `path` is a photo PixelFx opens (set by the window: the library's formats).
    static void setPhotoTest(bool (*isPhoto)(const QString&));

Q_SIGNALS:
    /// The output choices changed (remembered like the Export sheet's).
    void outputChanged(const pixelfx::core::ExportOptions& output);
    /// An opened recipe added looks to the user's looks.
    void looksAdded();
    /// The photos changed: the header's "6 photos · 129 MB".
    void summaryChanged(const QString& summary);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    /// The controls of More..., read back once the sheet is accepted.
    struct SaveOptionsControls
    {
        SliderRow* quality = nullptr;
        QCheckBox* deep = nullptr;
        QCheckBox* camera = nullptr;
        QCheckBox* location = nullptr;
        QComboBox* exists = nullptr;
    };

    QWidget* buildList();
    QWidget* buildListHead(QWidget* parent);
    QWidget* buildPreview(QWidget* parent);
    QWidget* buildStatus(QWidget* parent);
    QWidget* buildPanel();
    QWidget* buildRecipe(QWidget* parent);
    QWidget* buildRecipeCard(QWidget* box);
    QHBoxLayout* buildSecondRow(QWidget* box);
    static QFormLayout* saveOptionsForm(QDialog* sheet, const core::ExportOptions& output,
                                        SaveOptionsControls& c);
    [[nodiscard]] std::pair<QString, QString> recipeCardText() const;
    void addFirstChoice(services::BatchRecipe& r) const;
    [[nodiscard]] static pfx::Settings photoPreset(const pfx::Settings& settings);
    void showRunning(int done, int total);
    void showDone(int saved, int failed);
    QWidget* buildResize(QWidget* parent);
    QWidget* buildSaveAs(QWidget* parent);
    QWidget* buildFoot(QWidget* parent);
    static QComboBox* buildSizes(QWidget* parent);
    void rebuildRows();
    /// Chooses the size preset `sizePreset`, else the long edge, else Original size.
    void showSize(const QString& sizePreset, int longEdge);
    QWidget* makeRow(const QString& file);
    void refreshRow(int index);
    void refreshStatus();
    void refreshRecipeCard();
    void refreshFormats();
    void schedulePreview();
    void renderPreview();
    void showRecipeMenu(QWidget* anchor, bool second);
    void editSaveOptions();
    void outputEdited();
    [[nodiscard]] QString recipeTitle() const;

    QStringList m_files;
    std::vector<core::PresetEntry> m_presets;
    std::vector<core::LookEntry> m_looks;
    QString m_first;  ///< see chooseRecipe
    QString m_second; ///< a preset's name
    bool m_fromPhoto = false;
    QString m_photoName;
    pfx::Settings m_photoSettings;
    std::vector<pfx::BatchStep> m_loadedStack; ///< the steps of an opened recipe
    QString m_loadedName;
    core::ExportOptions m_output;
    QString m_sidecarFolder;
    services::BatchRunner m_runner;
    services::Thumbnailer* m_thumbnails = nullptr;
    QTimer* m_previewTimer = nullptr;
    QElapsedTimer m_clock;
    QLabel* m_before = nullptr;
    QLabel* m_after = nullptr;
    QPixmap m_afterPixmap;
    QWidget* m_preview = nullptr;
    QString m_previewPath;                            ///< the photo the small copy is of
    std::shared_ptr<const pfx::Image> m_previewImage; ///< the first photo, small
    quint64 m_previewGeneration = 0;
    QStackedWidget* m_views = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_rowsBox = nullptr;
    QListWidget* m_grid = nullptr;
    QButtonGroup* m_viewModes = nullptr;
    QWidget* m_status = nullptr;
    QLabel* m_statusIcon = nullptr;
    QLabel* m_statusText = nullptr;
    QLabel* m_statusDetail = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_pause = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_retry = nullptr;
    QPushButton* m_showFolder = nullptr;
    QPushButton* m_process = nullptr;
    QButtonGroup* m_sources = nullptr;
    QLabel* m_cardThumb = nullptr;
    QLabel* m_cardName = nullptr;
    QLabel* m_cardDetail = nullptr;
    QPushButton* m_cardChange = nullptr;
    QCheckBox* m_secondOn = nullptr;
    QLabel* m_secondName = nullptr;
    QPushButton* m_secondChange = nullptr;
    QCheckBox* m_auto = nullptr;
    QCheckBox* m_keepEdits = nullptr;
    QComboBox* m_size = nullptr;
    QCheckBox* m_noEnlarge = nullptr;
    QComboBox* m_format = nullptr;
    QLabel* m_folder = nullptr;
    QLineEdit* m_name = nullptr;
    QCheckBox* m_beside = nullptr;
    class WatermarkPicker* m_watermark = nullptr;
};

} // namespace pixelfx::ui
