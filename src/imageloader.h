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

class ImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit ImageLoader(QObject *parent = nullptr);

signals:
    void loadedImage(QImage img);

public slots:
    QString getOriginalPath();
    QString getScaledPath();
    QString getFilteredPath();
    QString getUUID();
    void openFile(QWidget* wid);
    void imageInfo();
private slots:
    QImage writeScaledFile();
private:
    QString fileName, UUID;
    QString defaultLocation;
    QString scaledLocation,filteredLocation;
    QFile tempFile,scaledFile;
    QSettings settings;
};

#endif // IMAGELOADER_H
