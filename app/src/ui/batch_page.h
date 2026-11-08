#pragma once

#include "core/compress_job.h"
#include "core/export_options.h"
#include "core/looks.h"
#include "core/presets.h"
#include "services/batch_runner.h"
#include "services/thumbnailer.h"
#include "ui/file_row.h"
#include "ui/recipe_picker.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include <memory>
#include <utility>

class QButtonGroup;
class QCheckBox;
class QFrame;
class QComboBox;
class QDialog;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

namespace pixelfx::ui {

class PathLabel;
class SliderRow;

/// Batch, in two modes chosen at the top of the right panel (1. What to do).
/// Edit and export: on the left the photos, as a list, added as files or whole folders
/// or dropped, the first one before and after the recipe, and while running the progress (Pause,
/// Stop), then the summary (Retry failed, Show in folder); on the right 2. Recipe (a preset or
/// look, or the settings of the photo open in the editor, then a preset on top, Auto enhance,
/// the edits already made), 3. Resize (Never enlarge), 4. Watermark, 5. Save as (format and
/// quality, More..., folder, name, beside each original), and Save as batch recipe and Process.
/// Compress only: the files with their savings (CompressList), a before
/// and after at 100% (CompressCompare), the totals, and CompressPanel on the right.
/// The window's header shows the count and size and Add photos and Add folder while the page is
/// up.
class BatchPage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BatchPage)

public:
    explicit BatchPage(core::ExportOptions output, core::CompressJob job = {}, bool compressOnly = false,
                       QWidget* parent = nullptr);
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
    /// Where remembered edits are; empty when Remember edits is off (Keep edits is then off too).
    void setSidecarFolder(const QString& folder);
    /// The recipe's first choice: "" (none), "look:<id>", "preset:<name>", or "recipe" (an
    /// opened batch recipe's own steps). Settings from a photo is its own source (fromPhoto).
    void chooseRecipe(const QString& choice);
    /// Convert and resize only (the welcome page's quick job): no look or preset, nothing on top.
    void chooseConvertOnly();
    /// The favourites and recent presets Choose the recipe marks and offers first.
    void setPickerLists(const RecipePicker::Lists& lists) { m_pickerLists = lists; }
    /// Choose the recipe, or (second) Add a preset on top, not yet shown; the caller owns it.
    [[nodiscard]] RecipePicker* newRecipePicker(bool second);
    /// The preset added on top (by name), or none when empty.
    void chooseSecond(const QString& presetName);
    void setFromPhoto(bool on);
    /// The recipe and output as the page shows them.
    [[nodiscard]] services::BatchRecipe recipe() const;
    void process();
    /// Compress only: makes every file smaller; replacing the originals asks first.
    void compress();
    void setCompressOnly(bool on);
    [[nodiscard]] bool isCompressOnly() const { return m_compressOnly; }
    [[nodiscard]] class CompressPanel* compressPanel() const { return m_compressPanel; }
    /// Edit and export's file name (Compress only has its own in compressPanel).
    [[nodiscard]] class NameField* nameField() const { return m_name; }
    [[nodiscard]] class CompressList* compressList() const { return m_compressList; }
    [[nodiscard]] class CompressCompare* compressCompare() const { return m_compare; }
    /// Writes the page as a batch recipe (.pfxbatch) to `path`; false when it cannot.
    [[nodiscard]] bool saveRecipe(const QString& path) const;
    /// Sets the page from a batch recipe, adding the looks and the watermark it carries to the
    /// user's own; the reason when it cannot be read, or what could not be added.
    [[nodiscard]] QString loadRecipe(const QString& path);
    [[nodiscard]] services::BatchRunner& runner() { return m_runner; }
    /// Process and Process the rest ask it first (photos above the free number, Pro items in
    /// the recipe); the Watermark heading gets the Pro mark short of Pro.
    void setProGate(class ProGate* gate);

    /// Whether `path` is a photo PixelFx opens (set by the window: the library's formats).
    static void setPhotoTest(bool (*isPhoto)(const QString&));

Q_SIGNALS:
    /// The output choices changed (remembered like the Export sheet's).
    void outputChanged(const pixelfx::core::ExportOptions& output);
    /// An opened recipe added looks to the user's looks.
    void looksAdded();
    /// The photos changed: the header's "6 photos · 129 MB".
    void summaryChanged(const QString& summary);
    /// Compress only's choices changed, and the mode (both remembered).
    void compressJobChanged(const pixelfx::core::CompressJob& job);
    void compressOnlyChanged(bool on);
    /// A batch started or ended: while one runs, the list cannot change (the header's Add
    /// buttons follow).
    void runningChanged(bool running);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;

private:
    /// The controls of More..., read back once the sheet is accepted.
    struct SaveOptionsControls
    {
        SliderRow* quality = nullptr;
        QCheckBox* deep = nullptr;
        QCheckBox* camera = nullptr;
        QCheckBox* location = nullptr;
        QComboBox* exists = nullptr;
        QButtonGroup* sharpen = nullptr;
        QComboBox* colorSpace = nullptr;
    };

    QWidget* buildList();
    QWidget* buildListHead(QWidget* parent);
    /// "Sort Biggest saving" for Compress only's list.
    QWidget* buildSortBox(QWidget* head);
    void connectCompress();
    QWidget* buildPreview(QWidget* parent);
    /// The empty list: a drop zone with Add photos and Add folder.
    QWidget* buildEmpty(QWidget* parent);
    /// The list, Compress only's list, or the drop zone when there are no photos.
    void refreshView();
    void setDragActive(bool active);
    QWidget* buildStatus(QWidget* parent);
    QWidget* buildPanel();
    QWidget* buildMode(QWidget* parent);
    QWidget* buildEditPanel(QWidget* parent);
    /// The before and after of `path` at 100%, read on a worker.
    void showCompare(const QString& path);
    void showCompressDone(int failed);
    QWidget* buildRecipe(QWidget* parent);
    QWidget* buildRecipeCard(QWidget* box);
    QHBoxLayout* buildSecondRow(QWidget* box);
    static QFormLayout* saveOptionsForm(QDialog* sheet, const core::ExportOptions& output,
                                        SaveOptionsControls& c);
    [[nodiscard]] std::pair<QString, QString> recipeCardText() const;
    /// The card for no look or preset first: Convert and resize only, or what is on top.
    [[nodiscard]] std::pair<QString, QString> convertCardText() const;
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
    /// The large preview: every photo before and after the recipe, as Batch writes it.
    void openLargePreview();
    void renderPreview();
    /// Choose the recipe, or (second) a preset on top.
    void showRecipePicker(bool second);
    void editSaveOptions();
    void outputEdited();
    /// The name previews follow the list and the options.
    void refreshNames();
    /// The photos the recipe would save over their originals (If it exists: Overwrite, the
    /// names their own, in their own folder).
    [[nodiscard]] QStringList overwrittenOriginals() const;
    /// Asks before saving over originals; whether to go ahead.
    [[nodiscard]] bool confirmOverwrites();
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
    RecipePicker::Lists m_pickerLists;
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
    QFrame* m_emptyZone = nullptr;
    QLabel* m_emptyGlyph = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyHint = nullptr;
    QLabel* m_emptyMore = nullptr;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_rowsBox = nullptr;
    std::vector<FileRow> m_fileRows; ///< the list's rows, in the order of m_files
    QWidget* m_status = nullptr;
    QLabel* m_statusIcon = nullptr;
    QLabel* m_statusText = nullptr;
    QLabel* m_statusDetail = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_pause = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_retry = nullptr;
    QPushButton* m_rest = nullptr; ///< "Process the rest" after Stop
    QPushButton* m_showFolder = nullptr;
    QPushButton* m_process = nullptr;
    QPushButton* m_clear = nullptr;
    bool m_wasRunning = false;
    bool m_stopped = false; ///< the last run was stopped (rather than limited to the free number)
    class ProGate* m_gate = nullptr;
    QWidget* m_watermarkMark = nullptr;
    /// Whether the recipe's Pro items (RAW photos, a Lightroom preset, a LUT look, a watermark)
    /// may be saved: always with Pro, otherwise after a prompt.
    [[nodiscard]] bool admitRecipe(const services::BatchRecipe& recipe);
    /// Locks what would change the list or the mode while a batch runs, and what has nothing to
    /// act on while the list is empty.
    void refreshLocks();
    [[nodiscard]] bool canProcess() const;
    /// The status line says what went wrong (a recipe not saved or not read), in danger colour,
    /// with nothing of the last run beside it.
    void showStatusError(const QString& text);
    void setStatusError(bool error);
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
    PathLabel* m_folder = nullptr;
    class NameField* m_name = nullptr;
    QHash<QString, QSize> m_photoSizes; ///< each photo's size, for {w}x{h} in the name preview
    QCheckBox* m_beside = nullptr;
    class WatermarkPicker* m_watermark = nullptr;
    core::CompressJob m_job;
    bool m_compressOnly = false;
    QButtonGroup* m_modes = nullptr;
    QLabel* m_modeNote = nullptr;
    QStackedWidget* m_panels = nullptr;
    class CompressPanel* m_compressPanel = nullptr;
    class CompressList* m_compressList = nullptr;
    class CompressCompare* m_compare = nullptr;
    QLabel* m_listTitle = nullptr;
    QWidget* m_sortBox = nullptr;
    quint64 m_compareGeneration = 0;
};

} // namespace pixelfx::ui
