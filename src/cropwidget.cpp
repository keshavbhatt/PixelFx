#include "cropwidget.h"
#include "ui_cropwidget.h"

CropWidget::CropWidget(QWidget *parent, QPixmap pixmap) :
    QWidget(parent),
    ui(new Ui::CropWidget)
{
    ui->setupUi(this);
    init();
    this->setAttribute(Qt::WA_DeleteOnClose, true);
    this->setWindowTitle(QApplication::applicationName()+" | "+tr("Crop Dialog"));

    foreach (QWidget *widget, this->findChildren<QWidget*>()) {
        if( widget->inherits("QGroupBox")&&widget->objectName().contains("CoreGroupBox")){
            widget->setStyleSheet("QGroupBox#"+widget->objectName()+"{border:none;background-color: qlineargradient(spread:reflect, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(17, 18, 20, 240), stop:0.5 rgba(17, 18, 20, 96), stop:1 rgba(17, 18, 20, 240));}");
        }
        if((widget->inherits("QLabel") || widget->inherits("QSlider"))
                && !objectName().contains("ignoredLabel"))
        {
            widget->setStyleSheet("background-color:transparent;");
        }
    }

    //load image
    if (pixmap.isNull()) {
        QMessageBox::critical(this, "Error", "Image loading failed", QMessageBox::Ok);
        return;
    }else{
        imgCropperLabel->setOriginalImage(pixmap);
        imgCropperLabel->update();
        ui->editCropperFixedHeight->setRange(0,pixmap.size().height());
        ui->editCropperFixedWidth->setRange(0,pixmap.size().width());
        onUpdatePreview();
    }
}

inline void setLayoutVisible(QLayoutItem *item, bool visible)
{
  if (auto widget = item->widget())
    return widget->setVisible(visible);
  if (auto layout = item->layout())
    for (int i = 0; i < layout->count(); ++i)
      setLayoutVisible(layout->itemAt(i), visible);
}

/**
 * @brief CropWidget::init
 * init core widgets
 */
void CropWidget::init()
{
    //main cropper label programatically
    imgCropperLabel = new ImageCropperLabel(600, 500, this);
    imgCropperLabel->setObjectName("ignoredLabel_imgCropper");
    ui->imageCoreGroupBox->layout()->addWidget(imgCropperLabel);
    imgCropperLabel->setAlignment(Qt::AlignCenter);
    imgCropperLabel->setRectCropper();
    imgCropperLabel->enableOpacity(true);
    imgCropperLabel->setShowDragSquare(true);

    //opacity
    ui->sliderOpacity->setRange(0, 100);
    ui->sliderOpacity->setValue(60);
    connect(ui->sliderOpacity, &QSlider::valueChanged,
            this, &CropWidget::onOpacityChanged);


    //preview label from UI
    ui->labelPreviewImage->setFixedSize(200, 200);
    ui->labelPreviewImage->setAlignment(Qt::AlignCenter);
    connect(imgCropperLabel, &ImageCropperLabel::croppedImageChanged,
            this, &CropWidget::onUpdatePreview);
    connect(imgCropperLabel, SIGNAL(cropperRectGeometryChanged(QRect)),this,SLOT(onCropperRectGeometryChanged(QRect)));

    //cropper fixed width lineedits
    ui->editCropperFixedWidth->setEnabled(false);
    ui->editCropperFixedHeight->setEnabled(false);

    //cropper output shape
    //hiding output shape selector as we have coppuled it with cropper shape
    setLayoutVisible(ui->shapeHorizontalLayout,false);
    connect(ui->comboOutputShape, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onOutputShapeChanged(int)));
    //cropper shape
    connect(ui->comboCropperShape, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onCropperShapeChanged(int)));
    //fixed height width
    connect(ui->editCropperFixedWidth,SIGNAL(valueChanged(int)),
            this, SLOT(onFixedWidthChanged(int)));
    connect(ui->editCropperFixedHeight,SIGNAL(valueChanged(int)),
            this, SLOT(onFixedHeightChanged(int)));

    //save cancel buttons
    connect(ui->btnSavePreview, &QPushButton::clicked,
            this, &CropWidget::onSaveCroppedImage);
    connect(ui->btnQuit, &QPushButton::clicked,
            this, &CropWidget::close);
}




/*****************************************************************************
 *
 *    slots
 *
*****************************************************************************/


void CropWidget::onSaveCroppedImage()
{
    const QPixmap* pixmap = ui->labelPreviewImage->pixmap();
    if (!pixmap) {
        QMessageBox::information(this, "Error", "There is no cropped image to load.", QMessageBox::Ok);
        return ;
    }
    //cropped image
    emit croppedImage(imgCropperLabel->getCroppedImage());
}

void CropWidget::onUpdatePreview()
{
    QPixmap preview = imgCropperLabel->getCroppedImage();
    preview = preview.scaled(ui->labelPreviewImage->width(), ui->labelPreviewImage->height(),
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ui->labelPreviewImage->setPixmap(preview);

}

void CropWidget::onCropperRectGeometryChanged(QRect rect)
{
    ui->label_width->setText(QString::number(rect.width()));
    ui->label_height->setText(QString::number(rect.height()));
    ui->label_x->setText(QString::number(rect.x()));
    ui->label_y->setText(QString::number(rect.y()));
}

void CropWidget::onOutputShapeChanged(int idx)
{
    // Output: Rectangular
    if (idx == 0)
        imgCropperLabel->setOutputShape(OutputShape::RECT);
    else
        imgCropperLabel->setOutputShape(OutputShape::ELLIPSE);
    onUpdatePreview();
}

void CropWidget::onCropperShapeChanged(int idx)
{
    switch (CropperShape(idx + 1)) {
    case CropperShape::RECT: {
        imgCropperLabel->setRectCropper();
        ui->editCropperFixedWidth->setEnabled(false);
        ui->editCropperFixedHeight->setEnabled(false);
        onOutputShapeChanged(0);
        break;
    }
    case CropperShape::SQUARE: {
        imgCropperLabel->setSquareCropper();
        ui->editCropperFixedWidth->setEnabled(false);
        ui->editCropperFixedHeight->setEnabled(false);
        onOutputShapeChanged(0);
        break;
    }
    case CropperShape::FIXED_RECT: {
        imgCropperLabel->setFixedRectCropper(QSize(64, 64));
        ui->editCropperFixedWidth->setEnabled(true);
        ui->editCropperFixedHeight->setEnabled(true);
        ui->editCropperFixedWidth->setValue(64);
        ui->editCropperFixedHeight->setValue(64);
        onOutputShapeChanged(0);
        break;
    }
    case CropperShape::ELLIPSE: {
        imgCropperLabel->setEllipseCropper();
        ui->editCropperFixedWidth->setEnabled(false);
        ui->editCropperFixedHeight->setEnabled(false);
        onOutputShapeChanged(1);
        break;
    }
    case CropperShape::CIRCLE: {
        imgCropperLabel->setCircleCropper();
        ui->editCropperFixedWidth->setEnabled(false);
        ui->editCropperFixedHeight->setEnabled(false);
        onOutputShapeChanged(1);
        break;
    }
    case CropperShape::FIXED_ELLIPSE:
        imgCropperLabel->setFixedEllipseCropper(QSize(64, 64));
        ui->editCropperFixedWidth->setEnabled(true);
        ui->editCropperFixedHeight->setEnabled(true);
        ui->editCropperFixedWidth->setValue(64);
        ui->editCropperFixedHeight->setValue(64);
        onOutputShapeChanged(1);
        break;
    case CropperShape::UNDEFINED:
        break;
    }

    imgCropperLabel->update();
    onUpdatePreview();
}

void CropWidget::onFixedWidthChanged(int width)
{
    imgCropperLabel->setCropperFixedWidth(width);
    imgCropperLabel->update();
    onUpdatePreview();
}

void CropWidget::onFixedHeightChanged(int height)
{
    imgCropperLabel->setCropperFixedHeight(height);
    imgCropperLabel->update();
    onUpdatePreview();
}

void CropWidget::onOpacityChanged(int val)
{
    imgCropperLabel->setOpacity(val / 100.0);
    imgCropperLabel->update();
}

CropWidget::~CropWidget()
{
    delete ui;
}
