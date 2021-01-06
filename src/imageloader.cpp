#include "imageloader.h"
#include <QDebug>

ImageLoader::ImageLoader(QObject *parent) : QObject(parent)
{
    QDir d;
    scaledLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            +QDir::separator()+QApplication::applicationName()+QDir::separator()+
            "scaled"+QDir::separator();
    d.mkpath(scaledLocation);

    originalLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            +QDir::separator()+QApplication::applicationName()+QDir::separator()+
            "original"+QDir::separator();
    d.mkpath(originalLocation);

    filteredLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            +QDir::separator()+QApplication::applicationName()+QDir::separator()+
            "filtered"+QDir::separator();
    d.mkpath(filteredLocation);

    defaultLocation = settings.value("last_image_dir_path").isValid() ?
                settings.value("last_image_dir_path").toString() :
                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
}

ImageLoader::~ImageLoader()
{
   qDebug()<<"Deleted tempLocations:"<<deleteTemporaryLocations();
}

bool ImageLoader::deleteTemporaryLocations()
{
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            +QDir::separator()+QApplication::applicationName());
    return tempDir.removeRecursively();
}


void ImageLoader::openFile(QWidget *wid)
{
    fileName.clear();
    fileName = QFileDialog::getOpenFileName(wid,
         QObject::tr("Open Image"), defaultLocation,
         QObject::tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.webp *.wbmp *mng)"));
    if(fileName.isEmpty() == false){
        QFileInfo fileInfo(fileName);
        settings.setValue("last_image_dir_path",fileInfo.absoluteDir().path());
        defaultLocation = settings.value("last_image_dir_path").toString();
        //init image
        UUID = QUuid::createUuid().toString(QUuid::Id128);
        writeOriginalFile();

        loadImage(getOriginalPath());
    }
}

void ImageLoader::writeOriginalFile()
{
    QFile file(fileName);
    file.copy(originalLocation+QDir::separator()+getUUID()+"."+getOriginalExetension());
}

void ImageLoader::imageInfo()
{
    ImageInfoWidget* infoDialog = new ImageInfoWidget(nullptr);
    infoDialog->setWindowFlags(Qt::Dialog | infoDialog->windowFlags());
    infoDialog->setWindowModality(Qt::WindowModal);
    infoDialog->fillMetaData(fileName);
    infoDialog->show();
}

//load image in non gui thread
void ImageLoader::loadImage(QString path)
{
    emit imageLoadStarted(); // show the loader
    ImageReader reader;
    QFuture<QImage> future = reader.read(path);
    QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
    connect(watcher, &QFutureWatcher<QImage>::finished,
            [future,this]() {
        currentLoadedImage = future.result();
        emit loadedImage( writeScaledFile(currentLoadedImage) );
        emit imageLoaded(); // hide the loader
    });
    watcher->setFuture(future);
}


//returns currently loadedimage via loadImage func.
QImage ImageLoader::getCurrentLoadedImage()
{
    if(currentLoadedImage.isNull()==false)
        return currentLoadedImage;
    else
        return QImage();
}

QImage ImageLoader::writeScaledFile(QImage img)
{
    scaledFile.setFileName(getScaledPath());
    scaledFile.open(QIODevice::ReadWrite | QIODevice::Truncate);

    if(img.isNull() == false){
        QScreen *scr = QApplication::primaryScreen();
        qreal dpr;
        if(scr == nullptr){
           dpr = 1.0;
        }
        dpr = scr->devicePixelRatio();
        QImageReader image(getOriginalPath());
        if(image.size().height()>768)
        img = QImage(img.scaled( 1000 * dpr,768 * dpr,
                     Qt::KeepAspectRatio, Qt::SmoothTransformation));
        img.setDevicePixelRatio(dpr);

        QImageWriter imgwritter;
        imgwritter.setFileName(getScaledPath());
        imgwritter.setCompression(0);
        imgwritter.setQuality(100);
        imgwritter.setFormat("PNG");
        imgwritter.write(img);
    }
    QImage scaledImage(getScaledPath());
    return scaledImage;
}

QString ImageLoader::getLocalOriginalPath()
{
    return originalLocation+QDir::separator()+getUUID()+"."+getOriginalExetension();
}

// the real file name.
QString ImageLoader::getOriginalPath()
{
    return fileName;
}

QString ImageLoader::getOriginalExetension()
{
    QFileInfo info(getOriginalPath());
    return info.suffix().isEmpty() ? "png" : info.suffix();
}

QString  ImageLoader::getScaledPath()
{
    return scaledLocation+QDir::separator()+getUUID()+".png";
}

QString  ImageLoader::getFilteredPath()
{
    return filteredLocation+QDir::separator()+getUUID()+".png";
}

QString ImageLoader::getUUID()
{
    return UUID;
}
