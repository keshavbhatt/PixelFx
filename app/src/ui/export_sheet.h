#pragma once

#include "core/export_options.h"
#include "ui/sheet.h"

#include <QImage>
#include <QSize>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QFormLayout;
class QFrame;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;

namespace pixelfx::ui {

class PathLabel;
class SliderRow;

/// The quality of a lossy format as a slider row ("Smaller file · Better image", 1 to 100,
/// 90 by default), for Export and Batch.
[[nodiscard]] SliderRow* qualitySlider(int quality, QWidget* parent);
/// Sharpen for (None, Screen, Print) as segments in `group`, for Export and Batch.
[[nodiscard]] QFrame* sharpenSegments(pfx::OutputSharpen sharpen, QButtonGroup* group, QWidget* parent);
/// Color space (sRGB, Display P3, Adobe RGB, Same as original), for Export and Batch.
[[nodiscard]] QComboBox* colorSpaceList(pfx::OutputSpace space, QWidget* parent);

/// Export: format, quality, size, metadata, folder, file name, and what to
/// do when the file exists. Starts from the remembered choices; returns the chosen ones.
class ExportSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExportSheet)

public:
    /// The result of exec() when Copy to clipboard was pressed (Accepted is Export).
    static constexpr int kCopyToClipboard = 2;

    ExportSheet(const QString& source, const QSize& photoSize, const core::ExportOptions& options,
                QWidget* parent = nullptr);
    ~ExportSheet() override;

    [[nodiscard]] core::ExportOptions options() const;
    /// Several photos at once: the title and button say "Export 4 photos" (apply-to-selected.html).
    void setPhotoCount(int count);
    /// What the watermark sheet previews on (the photo as edited).
    void setPreview(const QImage& preview);
    /// Where saved watermarks are kept (tests use their own folder).
    void setWatermarkFolder(const QString& folder);
    /// The sizes kept under Mine; customSizes() has any added with Custom size.
    void setCustomSizes(const std::vector<core::CustomSize>& sizes);
    [[nodiscard]] const std::vector<core::CustomSize>& customSizes() const { return m_mySizes; }
    [[nodiscard]] bool customSizesChanged() const { return m_mySizesChanged; }
    [[nodiscard]] QComboBox* sizeList() const { return m_size; }
    [[nodiscard]] class FramePicker* framePicker() const { return m_picker; }
    /// The note under the choices: the estimated file size once it is known.
    [[nodiscard]] QLabel* note() const { return m_note; }

private:
    void updateForFormat();
    class QLabel* m_title = nullptr;
    class QLabel* m_file = nullptr;
    class QPushButton* m_exportButton = nullptr;
    class QPushButton* m_copyButton = nullptr;
    void chooseFolder();
    [[nodiscard]] QWidget* formatRow(const QString& current);
    [[nodiscard]] QWidget* qualityRow(int quality);
    [[nodiscard]] QWidget* sizeRow(const QSize& photoSize, const core::ExportOptions& options);
    [[nodiscard]] QWidget* framingCard(const core::ExportOptions& options);
    /// The size list: Original, the presets by group, Mine, the custom size in use, Custom size...
    void fillSizes(const QString& chosen);
    void refreshSize();
    /// The size part of the options, as the list and the frame show it.
    [[nodiscard]] core::ExportOptions sizeOptions() const;
    /// Asks for a custom size (width, height, resolution, kept under Mine); false when cancelled.
    bool askCustomSize();
    struct CustomSizeControls
    {
        QSpinBox* width = nullptr;
        QSpinBox* height = nullptr;
        QSpinBox* dpi = nullptr;
        QCheckBox* keep = nullptr;
        QLineEdit* name = nullptr;
    };
    /// The Custom size sheet's body; `use` is enabled while the size is complete.
    QVBoxLayout* customSizeForm(Sheet& sheet, CustomSizeControls& c, class QPushButton* use) const;
    /// Puts a format from More on the More button.
    void showRareFormat(const QString& extension);
    void chooseRareFormat(const QString& extension);
    [[nodiscard]] QWidget* folderRow();
    [[nodiscard]] QWidget* nameRow(const QString& nameTemplate);
    [[nodiscard]] QWidget* ifExistsBox(core::ExportOptions::IfExists ifExists);
    QHBoxLayout* addFooter(QFormLayout* form);
    [[nodiscard]] QWidget* watermarkRow(const QString& current);
    [[nodiscard]] QWidget* metadataRow(const core::ExportOptions& options);
    [[nodiscard]] QWidget* sharpenRow(pfx::OutputSharpen sharpen);
    [[nodiscard]] QWidget* colorSpaceBox(pfx::OutputSpace space);
    /// Estimates the file size again after a short pause; changes in the meantime restart it.
    void scheduleEstimate();
    void estimate();
    void showEstimate(qint64 bytes);

    QString m_source;
    QButtonGroup* m_formats = nullptr;
    SliderRow* m_quality = nullptr;
    QWidget* m_qualityRow = nullptr;
    QCheckBox* m_sixteen = nullptr;
    QComboBox* m_size = nullptr;
    int m_longEdge = 0;
    int m_sizeIndex = 0;
    class QPushButton* m_more = nullptr;
    QSize m_photoSize;
    core::CustomSize m_custom;
    std::vector<core::CustomSize> m_mySizes;
    bool m_mySizesChanged = false;
    QFrame* m_framing = nullptr;
    class QLabel* m_frameNote = nullptr;
    class FramePicker* m_picker = nullptr;
    class QPushButton* m_fill = nullptr;
    class QPushButton* m_fit = nullptr;
    QCheckBox* m_metadata = nullptr;
    QCheckBox* m_location = nullptr;
    PathLabel* m_folder = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_extension = nullptr;
    QComboBox* m_ifExists = nullptr;
    QString m_folderPath;
    QString m_pendingSize; ///< a Mine size to choose once the list has it
    class WatermarkPicker* m_watermark = nullptr;
    QButtonGroup* m_sharpen = nullptr;
    QComboBox* m_colorSpace = nullptr;
    QLabel* m_note = nullptr;
    class QTimer* m_estimateTimer = nullptr;
    QImage m_preview;
    int m_photoCount = 1;
    int m_estimates = 0; ///< estimates started, so only the latest is shown
};

} // namespace pixelfx::ui
