#ifndef IMAGEINFOWIDGET_H
#define IMAGEINFOWIDGET_H

#include <QWidget>
#include "lib/EasyExif/exif.h"
#include <QImageReader>
#include <QDebug>
#include <QListWidgetItem>


namespace Ui {
class ImageInfoWidget;
}

class ImageInfoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageInfoWidget(QWidget *parent = nullptr);
    ~ImageInfoWidget();

public slots:
    void fillMetaData(QString fileName);
private slots:
    QString MetaDataEntry(QString name, QString val);
    QString makeEntry(int tagID, const QExifValue &value);
    QListWidgetItem *createHeaderItem(bool header, QString title, QListWidget *listWidget);
private:
    Ui::ImageInfoWidget *ui;
    QStringList basicData, imageTagsData, extendedTagsData, gpsTagsData;
};

#endif // IMAGEINFOWIDGET_H
