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
    /// The files of the list, for the name preview.
    void setFiles(const QStringList& files);
    [[nodiscard]] class NameField* nameField() const { return m_name; }
    /// The count on the primary button; none disables it.
    void setFileCount(int count);
    /// While running nothing starts; after a finished run of this list the button compresses
    /// again ("Compress again"), replacing what that run saved.
    void setState(bool running, bool ranBefore);
    [[nodiscard]] QPushButton* compressButton() const { return m_compress; }

Q_SIGNALS:
    void jobChanged(const pixelfx::core::CompressJob& job);
    void compressRequested();

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
    class NameField* m_name = nullptr;
    QCheckBox* m_keep = nullptr;
    QPushButton* m_compress = nullptr;
    int m_count = 0; ///< photos to compress
    bool m_ranBefore = false;
    void refreshButton();
};

} // namespace pixelfx::ui
