#ifndef GIFT_H
#define GIFT_H

#include <QObject>
#include <QGraphicsView>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QProcess>
#include <QApplication>
#include <QDebug>
#include <QBuffer>
#include <QDir>

#include "graphicsview.h"
#include "imageloader.h"

class Gift : public QObject
{
     Q_OBJECT
     Q_PROPERTY( bool processing READ isProcessing WRITE setProcessing NOTIFY stateChanged )
public:
    Gift(graphicsView *view = nullptr);
    ~Gift();
signals:
    void resetCoreFilters();
    void stateChanged(bool processing);
public slots:
    void openFile();
    void applyFilter(QString filterChain);
    void reset();
    bool isProcessing();
    void saveFile();
    void compare(bool compare);
    void loadImage(QImage image);
    void imageInfo();
private slots:
    void updateTempImage(const QByteArray &byte);
    void setProcessing(bool processing);
    void applyFilterExportImage(const QString originalFilePath, const QString newFilePath);
private:
    bool processing;

    graphicsView *view = nullptr;
    QSettings settings;
    QProcess *giftProcess;
    QImage scaledImage;
    QImage modifiedImage;

    ImageLoader *imageLoader = nullptr;
};

#endif // GIFT_H