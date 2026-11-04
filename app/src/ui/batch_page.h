#pragma once

#include "core/export_options.h"
#include "core/looks.h"
#include "core/presets.h"
#include "services/batch_runner.h"
#include "services/thumbnailer.h"

#include <QStringList>
#include <QWidget>

#include <memory>

class QCheckBox;
class QHBoxLayout;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;
class QVBoxLayout;

namespace pixelfx::ui {

/// Batch (mocks batch.html, batch-running.html, batch-done.html): photos on the left (added as
/// files or whole folders, or dropped), the recipe on the right (a preset, a second one on top,
/// Auto enhance first, the edits already made), resize and Save as, then Process; progress per
/// photo, Pause and Stop, and a summary with Retry failed and Show in folder. Still to come from
/// the mocks: the before and after preview, the watermark, saving the page as a batch recipe.
class BatchPage : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BatchPage)

public:
    explicit BatchPage(core::ExportOptions output, QWidget* parent = nullptr);
    ~BatchPage() override;

    /// The presets the recipe can use (the library's list).
    void setPresets(const std::vector<core::PresetEntry>& entries);
    /// Looks offered in the first choice of the recipe ("Preset or look").
    void setLooks(const std::vector<core::LookEntry>& entries);
    /// Adds photos; folders bring every photo inside them, subfolders included.
    void addPaths(const QStringList& paths);
    [[nodiscard]] const QStringList& files() const { return m_files; }
    /// Where remembered edits are, for "Keep edits already made"; empty: none.
    void setSidecarFolder(const QString& folder) { m_sidecarFolder = folder; }
    /// The recipe and output as the page shows them.
    [[nodiscard]] services::BatchRecipe recipe() const;
    void process();
    /// Writes the page as a batch recipe (.pfxbatch) to `path`; false when it cannot.
    [[nodiscard]] bool saveRecipe(const QString& path) const;
    /// Sets the page from a batch recipe; the reason when it cannot be read.
    [[nodiscard]] QString loadRecipe(const QString& path);
    [[nodiscard]] services::BatchRunner& runner() { return m_runner; }

    /// Whether `path` is a photo PixelFx opens (set by the window: the library's formats).
    static void setPhotoTest(bool (*isPhoto)(const QString&));

Q_SIGNALS:
    /// The output choices changed (remembered like the Export sheet's).
    void outputChanged(const pixelfx::core::ExportOptions& output);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QWidget* buildList();
    QHBoxLayout* buildStatus(QWidget* box);
    void buildPreview(QWidget* box);
    QWidget* buildPanel();
    QWidget* buildRecipe(QWidget* parent);
    QWidget* buildOutput(QWidget* parent);
    void rebuildRows();
    QWidget* makeRow(const QString& file);
    void refreshRow(int index);
    void refreshStatus();
    void schedulePreview();
    void renderPreview();

    QStringList m_files;
    std::vector<core::PresetEntry> m_presets;
    std::vector<core::LookEntry> m_looks;
    void fillRecipeChoices();
    std::vector<pfx::BatchStep> m_loadedStack; ///< the presets of an opened recipe
    core::ExportOptions m_output;
    QString m_sidecarFolder;
    services::BatchRunner m_runner;
    services::Thumbnailer* m_thumbnails = nullptr;
    QTimer* m_previewTimer = nullptr;
    QLabel* m_before = nullptr;
    QLabel* m_after = nullptr;
    QWidget* m_preview = nullptr;
    QString m_previewPath;                            ///< the photo the small copy is of
    std::shared_ptr<const pfx::Image> m_previewImage; ///< the first photo, small
    quint64 m_previewGeneration = 0;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_rowsBox = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_pause = nullptr;
    QPushButton* m_stop = nullptr;
    QPushButton* m_retry = nullptr;
    QPushButton* m_showFolder = nullptr;
    QPushButton* m_process = nullptr;
    QComboBox* m_preset = nullptr;
    QComboBox* m_second = nullptr;
    QCheckBox* m_auto = nullptr;
    QCheckBox* m_keepEdits = nullptr;
    QComboBox* m_size = nullptr;
    QComboBox* m_format = nullptr;
    QSpinBox* m_quality = nullptr;
    QLabel* m_folder = nullptr;
    QCheckBox* m_beside = nullptr;
    class WatermarkPicker* m_watermark = nullptr;
};

} // namespace pixelfx::ui
