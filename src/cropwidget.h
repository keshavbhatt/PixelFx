#ifndef CROPWIDGET_H
#define CROPWIDGET_H

#include <QWidget>
#include <QMessageBox>
#include "lib/ImageCropper/imagecropperlabel.h"

namespace Ui {
class CropWidget;
}

class CropWidget : public QWidget
{
    Q_OBJECT
signals:
    void croppedImage(QPixmap pix);

public:
    explicit CropWidget(QWidget *parent = nullptr,QPixmap pixmap = QPixmap());
    ~CropWidget();

private slots:
    void init();
    void onUpdatePreview();
    void onOutputShapeChanged(int idx);
    void onCropperShapeChanged(int idx);
    void onOpacityChanged(int val);
    void onFixedWidthChanged(int width);
    void onFixedHeightChanged(int height);
    void onSaveCroppedImage();
    void onCropperRectGeometryChanged(QRect rect);
private:
    Ui::CropWidget *ui;

    ImageCropperLabel* imgCropperLabel;
};

#endif // CROPWIDGET_H
