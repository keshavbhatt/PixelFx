#pragma once

#include "core/watermark.h"
#include "ui/sheet.h"

#include <QImage>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QLayout;
class QLineEdit;
class QPushButton;
class QSlider;
class QStackedWidget;
class QToolButton;

namespace pixelfx::ui {

class MessageLine;

/// The photo with the mark on it, as the export will lay it.
/// A press or drag places the mark freely.
class WatermarkPreview : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WatermarkPreview)

public:
    enum class Shape
    {
        Photo,
        Portrait,
        Square,
    };
    explicit WatermarkPreview(QWidget* parent = nullptr);
    ~WatermarkPreview() override;

    void setPhoto(const QImage& photo);
    void setShape(Shape shape);
    void setWatermark(const core::Watermark& w);
    /// Where the photo is drawn, in widget pixels.
    [[nodiscard]] QRect photoRect() const;
    /// The photo as shown with the mark, at the preview's size (tests).
    [[nodiscard]] QImage composed() const;

Q_SIGNALS:
    /// The mark's centre as shares of the photo's width and height.
    void placed(QPointF centre);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    void rebuild();
    void place(const QPointF& at);

    QImage m_photo;
    QImage m_shown; ///< the photo in the chosen shape, at the widget's size, with the mark
    Shape m_shape = Shape::Photo;
    core::Watermark m_mark;
};

/// Watermark, opened from Export: make, pick or change a saved watermark.
class WatermarkSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WatermarkSheet)

public:
    /// `photo` is what the preview shows; `current` the saved watermark to start from (a new
    /// one when empty); `folder` where watermarks are kept.
    WatermarkSheet(const QImage& photo, const QString& current, const QString& folder,
                   QWidget* parent = nullptr);
    ~WatermarkSheet() override;

    [[nodiscard]] const core::Watermark& watermark() const { return m_mark; }
    /// After Use watermark: the saved file the export should use.
    [[nodiscard]] const QString& chosenPath() const { return m_path; }
    [[nodiscard]] WatermarkPreview* preview() const { return m_preview; }
    /// Saves under the watermark's name and closes (Use watermark).
    bool useWatermark();
    /// Photos of the batch for "Preview on: Batch photos"; none hides it.
    void setBatchPhotos(const QStringList& paths);

private:
    QWidget* buildHeader();
    QLayout* buildFooter();
    QWidget* buildKindRow(QWidget* parent);
    QWidget* buildLogoPage();
    QWidget* buildControls();
    QWidget* buildPreview();
    QWidget* buildColorRow();
    QWidget* buildPositionRow();
    void refreshList(const QString& select);
    void load(const core::Watermark& w, const QString& path);
    void sync();
    void saveAs();
    void showNextBatchPhoto();

    core::WatermarkLibrary m_library;
    core::Watermark m_mark;
    QString m_path;
    bool m_loading = false;
    QComboBox* m_list = nullptr;
    QButtonGroup* m_kind = nullptr;
    QStackedWidget* m_kindPages = nullptr;
    QLineEdit* m_text = nullptr;
    QLabel* m_logoName = nullptr;
    MessageLine* m_message = nullptr;
    QFontComboBox* m_font = nullptr;
    QButtonGroup* m_colors = nullptr;
    QToolButton* m_custom = nullptr;
    QSlider* m_size = nullptr;
    QSlider* m_opacity = nullptr;
    QSlider* m_margin = nullptr;
    QButtonGroup* m_anchors = nullptr;
    QCheckBox* m_repeat = nullptr;
    QCheckBox* m_shadow = nullptr;
    QWidget* m_angleRow = nullptr;
    QSlider* m_angle = nullptr;
    WatermarkPreview* m_preview = nullptr;
    QImage m_photo;
    QStringList m_batchPhotos;
    int m_batchIndex = -1;
    QPushButton* m_batchButton = nullptr;
};

/// The Watermark row of Export and Batch: None, the saved watermarks, New watermark..., and
/// Edit... for the chosen one.
class WatermarkPicker : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WatermarkPicker)

public:
    /// `current`: the remembered watermark's file, empty for None.
    explicit WatermarkPicker(const QString& current, QWidget* parent = nullptr);
    ~WatermarkPicker() override;

    /// Where saved watermarks are kept (tests use their own folder).
    void setFolder(const QString& folder);
    /// What the watermark sheet previews on.
    void setPreview(const QImage& preview) { m_preview = preview; }
    /// The batch's photos, for the sheet's "Preview on: Batch photos".
    void setBatchPhotos(const QStringList& paths) { m_batchPhotos = paths; }
    /// The chosen watermark's file, empty for None.
    [[nodiscard]] QString path() const;
    /// Lists the folder again and chooses the watermark at `file` (empty: None).
    void setPath(const QString& file);
    [[nodiscard]] const QString& folder() const { return m_folder; }
    [[nodiscard]] QStringList names() const;

Q_SIGNALS:
    void changed();

private:
    void refresh(const QString& select);
    void edit(const QString& file);

    QComboBox* m_box = nullptr;
    QPushButton* m_edit = nullptr;
    QString m_folder;
    QString m_remembered;
    QImage m_preview;
    QStringList m_batchPhotos;
    int m_last = 0;
};

} // namespace pixelfx::ui
