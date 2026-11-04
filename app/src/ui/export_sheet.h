#pragma once

#include "core/export_options.h"

#include <QDialog>
#include <QSize>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;
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
    ExportSheet(const QString& source, const QSize& photoSize, const core::ExportOptions& options,
                QWidget* parent = nullptr);
    ~ExportSheet() override;

    [[nodiscard]] core::ExportOptions options() const;

private:
    void updateForFormat();
    void chooseFolder();
    [[nodiscard]] QWidget* formatRow(const QString& current);
    [[nodiscard]] QWidget* qualityRow(int quality);
    [[nodiscard]] QWidget* sizeRow(const QSize& photoSize, int longEdge);
    [[nodiscard]] QWidget* folderRow();
    [[nodiscard]] QWidget* nameRow(const QString& nameTemplate);
    [[nodiscard]] QWidget* ifExistsBox(core::ExportOptions::IfExists ifExists);
    void addFooter(QVBoxLayout* column);

    QString m_source;
    QButtonGroup* m_formats = nullptr;
    QSlider* m_quality = nullptr;
    QLabel* m_qualityValue = nullptr;
    QWidget* m_qualityRow = nullptr;
    QCheckBox* m_sixteen = nullptr;
    QComboBox* m_size = nullptr;
    QSpinBox* m_longEdge = nullptr;
    QCheckBox* m_metadata = nullptr;
    QLabel* m_folder = nullptr;
    QLineEdit* m_name = nullptr;
    QLabel* m_extension = nullptr;
    QComboBox* m_ifExists = nullptr;
    QString m_folderPath;
};

} // namespace pixelfx::ui
