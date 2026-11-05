#pragma once

#include "core/compress_job.h"

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QLineEdit;
class QPushButton;

namespace pixelfx::ui {

class PathLabel;
class SliderRow;

/// Batch's Compress only choices (batch-compress.html): 2. Compression (JPEG, PNG, the other
/// formats, location and previews), 3. Save (into a folder or replacing the originals, the name,
/// keeping the original when the result is not smaller), and the foot (Run again with these
/// settings, Compress N files).
class CompressPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CompressPanel)

public:
    explicit CompressPanel(core::CompressJob job, QWidget* parent = nullptr);
    ~CompressPanel() override;

    [[nodiscard]] const core::CompressJob& job() const { return m_job; }
    void setJob(const core::CompressJob& job);
    /// The count on the primary button; none disables it.
    void setFileCount(int count);
    /// While running nothing starts; Run again needs a finished run of this list.
    void setState(bool running, bool canRunAgain);
    [[nodiscard]] QPushButton* compressButton() const { return m_compress; }
    [[nodiscard]] QPushButton* runAgainButton() const { return m_again; }

Q_SIGNALS:
    void jobChanged(const pixelfx::core::CompressJob& job);
    void compressRequested();
    void runAgainRequested();

private:
    QWidget* buildCompression(QWidget* parent);
    QWidget* buildSave(QWidget* parent);
    QWidget* buildFoot(QWidget* parent);
    void showJob();
    void edited();

    core::CompressJob m_job;
    QButtonGroup* m_jpeg = nullptr;
    QButtonGroup* m_png = nullptr;
    SliderRow* m_jpegQuality = nullptr;
    SliderRow* m_pngQuality = nullptr;
    QCheckBox* m_extras = nullptr;
    QButtonGroup* m_where = nullptr;
    QWidget* m_folderRow = nullptr;
    PathLabel* m_folder = nullptr;
    QLineEdit* m_name = nullptr;
    QCheckBox* m_keep = nullptr;
    QPushButton* m_again = nullptr;
    QPushButton* m_compress = nullptr;
};

} // namespace pixelfx::ui
