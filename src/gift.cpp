#include "gift.h"

Gift::Gift(graphicsView *view)
{
    this->view = view;
    imageLoader = new ImageLoader(this);
    connect(imageLoader,SIGNAL(loadedImage(QImage)),this,SLOT(loadImage(QImage)));

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
    // null the modified image so that compare won't load previously loaded image
    modifiedImage =  QImage();

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
    #ifdef QT_DEBUG
        qDebug()<<giftProcess->arguments();
    #endif
}


void Gift::crop()
{
    if(this->view->isSceneEmpty())
        return;

    // Refresh selection.
    CropWidget* cropDialog = new CropWidget(nullptr,this->view->getCurrentImage());
    cropDialog->setWindowFlags(Qt::Dialog | cropDialog->windowFlags());
    cropDialog->setWindowModality(Qt::WindowModal);
    connect(cropDialog,&CropWidget::croppedImage,[=](QPixmap pixmap){
       QByteArray bArray;
       QBuffer buffer(&bArray);
       buffer.open(QIODevice::WriteOnly);
       pixmap.save(&buffer,"PNG");
       updateTempImage(bArray);
       this->view->centerItem(pixmap.rect());
       buffer.close();
       buffer.deleteLater();
       cropDialog->close();
    });
    cropDialog->show();
}

void Gift::updateTempImage(const QByteArray &byte)
{
    modifiedImage.loadFromData(byte);
    view->updateImageView(modifiedImage);
}

void Gift::setProcessing(bool processing)
{
    this->processing = processing;
    emit stateChanged(processing);
}

Gift::~Gift()
{

}
