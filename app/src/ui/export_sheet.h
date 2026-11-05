#pragma once

#include "core/export_options.h"

#include <QDialog>
#include <QImage>
#include <QSize>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
class QFormLayout;
class QHBoxLayout;
class QVBoxLayout;
class QWidget;

namespace pixelfx::ui {

/// Export (mock export.html): format, quality, size, metadata, folder, file name, and what to
/// do when the file exists. Starts from the remembered choices; returns the chosen ones.
class ExportSheet : public QDialog
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
    [[nodiscard]] QWidget* fitRow(bool fitInside);
    void refreshSize(const QSize& photoSize);
    [[nodiscard]] QWidget* folderRow();
    [[nodiscard]] QWidget* nameRow(const QString& nameTemplate);
    [[nodiscard]] QWidget* ifExistsBox(core::ExportOptions::IfExists ifExists);
    QHBoxLayout* addFooter(QFormLayout* form);
    [[nodiscard]] QWidget* watermarkRow(const QString& current);
    [[nodiscard]] QWidget* metadataRow(const core::ExportOptions& options);

    QString m_source;
    QButtonGroup* m_formats = nullptr;
    QSlider* m_quality = nullptr;
    QLabel* m_qualityValue = nullptr;
    QWidget* m_qualityRow = nullptr;
    QCheckBox* m_sixteen = nullptr;
    QComboBox* m_size = nullptr;
    QSpinBox* m_longEdge = nullptr;
    QWidget* m_fitRow = nullptr;
    class QLabel* m_fitNote = nullptr;
    class QPushButton* m_fill = nullptr;
    class QPushButton* m_fit = nullptr;
    QCheckBox* m_metadata = nullptr;
    QCheckBox* m_location = nullptr;
    QLabel* m_folder = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_extension = nullptr;
    QComboBox* m_ifExists = nullptr;
    QString m_folderPath;
    class WatermarkPicker* m_watermark = nullptr;
};

} // namespace pixelfx::ui
