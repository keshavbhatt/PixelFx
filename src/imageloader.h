#ifndef IMAGELOADER_H
#define IMAGELOADER_H

#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImage>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QFileDialog>
#include <QUuid>
#include <QApplication>
#include <QImageWriter>
#include <QScreen>
#include <QFuture>
#include <QtConcurrent>

#include "imageinfowidget.h"

class ImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit ImageLoader(QObject *parent = nullptr);
    ~ImageLoader();

signals:
    void loadedImage(QImage img);
    void imageLoadStarted();
    void imageLoaded();

public slots:
    QString getOriginalPath();
    QString getScaledPath();
    QString getFilteredPath();
    QString getUUID();
    void openFile(QWidget* wid);
    void imageInfo();
    QString getOriginalExetension();
    QString getLocalOriginalPath();
    bool deleteTemporaryLocations();
    QImage writeScaledFile(QImage img);
private slots:
    void writeOriginalFile();
    void loadImage(QString path);
private:
    QString fileName, UUID;
    QString defaultLocation;
    QString scaledLocation,filteredLocation,originalLocation;
    QFile tempFile;
    QSettings settings;
    QImage currentLoadedImage;
};

//The image reader class for asyncImageRead
class ImageReader : public QObject {
public:
    QFuture<QImage> read(const QString &fileName)
    {
        auto readImageWorker = [](const QString &fileName) {
            QImage image;
            image.load(fileName);
            return image;
        };
        return QtConcurrent::run(readImageWorker, fileName);
    }
};
#endif // IMAGELOADER_H
