#include "gift.h"
#include <QMainWindow>

Gift::Gift(graphicsView *view)
{
    this->view = view;
    imageLoader = new ImageLoader(this);
    connect(imageLoader,&ImageLoader::imageLoadStarted,[=](){
       this->view->showLoader();
    });
    connect(imageLoader,&ImageLoader::imageLoaded,[=](){
       this->view->hideLoader();
    });
    connect(imageLoader,SIGNAL(loadedImage(QImage)),this,SLOT(loadImage(QImage)));

    //giftProcess
    giftProcess = new QProcess(this);
    giftProcess->setWorkingDirectory(qApp->applicationDirPath());
    connect(giftProcess, static_cast<void (QProcess::*)
                (int,QProcess::ExitStatus)>(&QProcess::finished),
                [this](int exitCode,QProcess::ExitStatus exitStatus)
        {
            qDebug()<<"PIXEL PROCESS FINISHED"<<exitCode<<exitStatus;
            if(exitCode == 0){
                updateTempImage(giftProcess->readAll());
            }
            setProcessing(false);
    });

    //giftCropProcess
    giftCropProcess = new QProcess(this);
    giftCropProcess->setWorkingDirectory(qApp->applicationDirPath());
    connect(giftCropProcess, static_cast<void (QProcess::*)
                (int,QProcess::ExitStatus)>(&QProcess::finished),
                [this](int exitCode,QProcess::ExitStatus exitStatus)
        {
            qDebug()<<"PIXEL PROCESS FINISHED"<<exitCode<<exitStatus;
            if(exitCode == 0){
                auto byte = giftCropProcess->readAll();
                QPixmap pix;
                pix.loadFromData(byte);
                modifiedImage = pix.toImage();
                this->view->updateImageView(modifiedImage);
            }
            setProcessing(false);
    });
}

void Gift::loadImage(QImage image)
{
    this->reset();
    if(view->loadImage(image)){
        scaledImage = image;
    }else{
        QMessageBox::information(nullptr,tr("Image load Error"),tr("Image %1\n load failed").arg(imageLoader->getOriginalPath()));
    }
    //we have scaled file written in imageLoader writeScaled function
    //scaledImage.save(imageLoader->getScaledPath());
}

void Gift::openFile()
{
    QObject *parentMainWindow = view->parent();
    while (!parentMainWindow -> objectName().contains("MainWindow")){
        parentMainWindow = parentMainWindow -> parent();
    }
    QWidget *mainWindow = qobject_cast<QMainWindow*>(parentMainWindow);
    imageLoader->openFile(mainWindow);
}

void Gift::imageInfo()
{
    imageLoader->imageInfo();
}

void Gift::saveFile(const QImage &img)
{
    QString defaultPath = settings.value("last_image_dir_path").isValid() ?
                settings.value("last_image_dir_path").toString() :
                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    QFileInfo fileinfo(imageLoader->getOriginalPath());
    QString defaultFileBaseName = fileinfo.baseName();
    saveDialog = new SaveDialog(nullptr,img,defaultPath ,defaultFileBaseName, this->imageLoader);
    saveDialog->setWindowTitle(QApplication::applicationName()+" | "+tr("Save Image"));
    saveDialog->setAttribute(Qt::WA_DeleteOnClose);
    saveDialog->setWindowFlags(Qt::Dialog | saveDialog->windowFlags());
    saveDialog->setWindowModality(Qt::ApplicationModal);
    saveDialog->show();
}

void Gift::prepareImageForSaving()
{
    QProcess *prepareFinalImage = new QProcess(this);
    prepareFinalImage->setWorkingDirectory(qApp->applicationDirPath());
    connect(prepareFinalImage, static_cast<void (QProcess::*)
                (int,QProcess::ExitStatus)>(&QProcess::finished),
                [this,prepareFinalImage](int exitCode,QProcess::ExitStatus exitStatus)
        {
            qDebug()<<"PIXEL PROCESS FINISHED"<<exitCode<<exitStatus;
            if(exitCode == 0){
                auto byte = prepareFinalImage->readAll();
                QPixmap pix;
                pix.loadFromData(byte);
                // open save dialog
                saveFile(pix.toImage());
            }
            setProcessing(false);
        prepareFinalImage->deleteLater();
        this->view->hideLoader();
    });
    setProcessing(true);
    QStringList args;
    args<<"-c"<<"./pixel "+currentFilterChain+" \""+
          imageLoader->getLocalOriginalPath()+"\"";
    prepareFinalImage->start("bash",args);
    this->view->showLoader();
}

void Gift::compare(bool compare)
{
    if(compare){
        if(!scaledImage.isNull())
            this->view->updateImageView(scaledImage);
    }else{
        if(!modifiedImage.isNull())
            this->view->updateImageView(modifiedImage);
    }
}


void Gift::reset()
{
    modifiedImage.detach();

    currentFilterChain = "";

    // null the modified image so that compare won't load previously loaded image
    modifiedImage =  QImage();

    giftCropProcess->close();
    giftProcess->close();
    setProcessing(false);
    emit resetCoreFilters();
}

bool Gift::isProcessing()
{
    return processing;
}

void Gift::applyFilter(QString filterChain)
{    
    setProcessing(true);
    QStringList args;
    args<<"-c"<<"./pixel "+filterChain+" \""+
          imageLoader->getScaledPath()+"\"";
    giftProcess->start("bash",args);
    currentFilterChain = filterChain;
    #ifdef QT_DEBUG
        qDebug()<<giftProcess->arguments();
    #endif
}

void Gift::cropAndApplyFilter()
{
    setProcessing(true);
    QStringList args;
    args<<"-c"<<"./pixel "+currentFilterChain+" \""+
          imageLoader->getScaledPath()+"\"";
    giftCropProcess->start("bash",args);
    #ifdef QT_DEBUG
        qDebug()<<giftCropProcess->arguments();
    #endif
}

void Gift::crop()
{
    if(this->view->isSceneEmpty())
        return;    
    this->view->showLoader();
    ImageReader reader;
    QFuture<QImage> future = reader.read(this->imageLoader->getLocalOriginalPath());
    QFutureWatcher<QImage> *watcher = new QFutureWatcher<QImage>();
    connect(watcher, &QFutureWatcher<QImage>::finished,
            [future,this,watcher]() {
        show_cropDialog(QPixmap::fromImage(future.result()));
        this->view->hideLoader();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void Gift::show_cropDialog( QPixmap localOriginlImage)
{
    CropWidget* cropDialog = new CropWidget(nullptr,localOriginlImage);
    cropDialog->setWindowFlags(Qt::Dialog | cropDialog->windowFlags());
    cropDialog->setWindowModality(Qt::WindowModal);
    cropDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(cropDialog,&CropWidget::croppedImage,[=](QPixmap pixmap){
       QByteArray bArray;
       QBuffer buffer(&bArray);
       buffer.open(QIODevice::WriteOnly);
       pixmap.save(&buffer,"PNG");
       updateTempImage(bArray,true);
       this->view->centerItem(pixmap.rect());
       buffer.close();
       buffer.deleteLater();
       cropDialog->close();
    });
    cropDialog->show();
}

void Gift::updateTempImage(const QByteArray &byte, bool saveFiltered)
{

    if(saveFiltered) //bytearray is passed from crop dialog so we need to change the localOriginal
    {
        QPixmap pix;
        pix.loadFromData(byte);
        pix.save(imageLoader->getLocalOriginalPath());
        // write the original cropped image
        modifiedImage = imageLoader->writeScaledFile(pix.toImage());
        //apply filter and update view here
        cropAndApplyFilter();
    }else{
        modifiedImage.loadFromData(byte);
        view->updateImageView(modifiedImage);
    }
}

void Gift::setProcessing(bool processing)
{
    this->processing = processing;
    emit stateChanged(processing);
}

Gift::~Gift()
{
    imageLoader->deleteLater();
}
