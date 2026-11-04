#pragma once

#include "core/export_options.h"
#include "core/presets.h"
#include "services/batch_runner.h"

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QHBoxLayout;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
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
    /// Adds photos; folders bring every photo inside them, subfolders included.
    void addPaths(const QStringList& paths);
    [[nodiscard]] const QStringList& files() const { return m_files; }
    /// Where remembered edits are, for "Keep edits already made"; empty: none.
    void setSidecarFolder(const QString& folder) { m_sidecarFolder = folder; }
    /// The recipe and output as the page shows them.
    [[nodiscard]] services::BatchRecipe recipe() const;
    void process();
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
    QWidget* buildPanel();
    QWidget* buildRecipe(QWidget* parent);
    QWidget* buildOutput(QWidget* parent);
    void rebuildRows();
    void refreshRow(int index);
    void refreshStatus();

    QStringList m_files;
    std::vector<core::PresetEntry> m_presets;
    core::ExportOptions m_output;
    QString m_sidecarFolder;
    services::BatchRunner m_runner;
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
};

} // namespace pixelfx::ui
