#include "gift.h"

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

    //real giftProcess
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

    //instant giftProcess
    instantGiftProcess = new QProcess(this);
    instantGiftProcess->setWorkingDirectory(qApp->applicationDirPath());
    connect(instantGiftProcess, static_cast<void (QProcess::*)
                (int,QProcess::ExitStatus)>(&QProcess::finished),
                [this](int exitCode,QProcess::ExitStatus exitStatus)
        {
            qDebug()<<"PIXEL PROCESS FINISHED"<<exitCode<<exitStatus;
            if(exitCode == 0){
                auto byte = instantGiftProcess->readAll();
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
    scaledImage.save(imageLoader->getScaledPath());
}

void Gift::openFile()
{
    imageLoader->openFile(view);
}

void Gift::imageInfo()
{
    imageLoader->imageInfo();
}

void Gift::saveFile()
{
    QString defaultPath = settings.value("last_image_dir_path").isValid() ?
                settings.value("last_image_dir_path").toString() :
                QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

    QFileInfo fileinfo(imageLoader->getOriginalPath());
    QString defaultFileBaseName = fileinfo.baseName();
    saveDialog = new SaveDialog(nullptr,defaultPath ,defaultFileBaseName, this->imageLoader);
    saveDialog->setWindowTitle(QApplication::applicationName()+" | "+tr("Save Image"));
    saveDialog->setAttribute(Qt::WA_DeleteOnClose);
    saveDialog->setWindowFlags(Qt::Dialog | saveDialog->windowFlags());
    saveDialog->setWindowModality(Qt::WindowModal);
    saveDialog->show();
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

    instantGiftProcess->close();
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

void Gift::instantApplyFilter()
{
    setProcessing(true);
    QStringList args;
    args<<"-c"<<"./pixel "+currentFilterChain+" \""+
          imageLoader->getScaledPath()+"\"";
    instantGiftProcess->start("bash",args);
    #ifdef QT_DEBUG
        qDebug()<<instantGiftProcess->arguments();
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
            [future,this]() {
        show_cropDialog(QPixmap::fromImage(future.result()));
        this->view->hideLoader();
    });
    watcher->setFuture(future);
}

void Gift::show_cropDialog( QPixmap localOriginlImage)
{
    CropWidget* cropDialog = new CropWidget(nullptr,localOriginlImage);
    cropDialog->setWindowFlags(Qt::Dialog | cropDialog->windowFlags());
    cropDialog->setWindowModality(Qt::WindowModal);
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
        instantApplyFilter();
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
