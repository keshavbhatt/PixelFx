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
    bool useNativeDialog = settings.value("useNativeFileDialog",true).toBool();
    if(useNativeDialog){
        fileName = QFileDialog::getOpenFileName(wid,
             QObject::tr("Open Image"), defaultLocation,
             QObject::tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.webp *.wbmp *mng)"));

    }else{
        fileName = QFileDialog::getOpenFileName(wid,
             QObject::tr("Open Image"), defaultLocation,
             QObject::tr("Image Files (*.png *.jpg *.jpeg *.bmp *.tiff *.webp *.wbmp *mng)"),nullptr,
                                                QFileDialog::DontUseNativeDialog);

    }

    if(fileName.isEmpty() == false){
        QFileInfo fileInfo(fileName);
        settings.setValue("last_image_dir_path",fileInfo.dir().path());
        defaultLocation = settings.value("last_image_dir_path").toString();
        //init image
        UUID = QUuid::createUuid().toString(QUuid::Id128);
        writeOriginalFile();

        loadImage(getOriginalPath());
    }
}

void ImageLoader::writeOriginalFile()
{
    if(QString::compare("png",getOriginalExetension(),Qt::CaseInsensitive) == 0){
        QFile file(fileName);
        file.copy(originalLocation+QDir::separator()+getUUID()+"."+getOriginalExetension());
    }else{
        //we saving all local originals in PNG formats to prevent non transparent bg for jpg src imgs cropped to circle or ellipse shaped png's
        QImage image(getOriginalPath());
        //image.save(originalLocation+QDir::separator()+getUUID()+".png","PNG",-1); //this method seems slower

        QImageWriter imgwritter;
        imgwritter.setFileName(originalLocation+QDir::separator()+getUUID()+".png");
        //imgwritter.setQuality(10);
        imgwritter.setFormat("PNG");
        imgwritter.write(image);
    }
}

void ImageLoader::imageInfo()
{
    ImageInfoWidget* infoDialog = new ImageInfoWidget(nullptr);
    infoDialog->setWindowFlags(Qt::Dialog | infoDialog->windowFlags());
    infoDialog->setWindowModality(Qt::WindowModal);
    infoDialog->setAttribute(Qt::WA_DeleteOnClose);
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
            [future,this,watcher]() {
        emit loadedImage( writeScaledFile(future.result()) );
        emit imageLoaded(); // hide the loader
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

QImage ImageLoader::writeScaledFile(QImage img)
{
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
        //imgwritter.setCompression(0);
        //imgwritter.setQuality(100);
        imgwritter.setFormat("PNG");
        imgwritter.write(img);
    }
    QImage scaledImage(getScaledPath());
    return scaledImage;
}

QString ImageLoader::getLocalOriginalPath()
{
   // return originalLocation+QDir::separator()+getUUID()+"."+getOriginalExetension();
   //we saving all local originals in PNG formats to prevent non transparent bg for jpg src imgs cropped to circle or ellipse shaped png's
    return originalLocation+QDir::separator()+getUUID()+".png";
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
