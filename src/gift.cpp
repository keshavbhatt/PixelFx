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
    QString fileName = QFileDialog::getSaveFileName(view,
         QObject::tr("Save Image"), defaultPath,
         QObject::tr("Supported formats (*.png *.jpg *.jpeg);;PNG(*.png);;JPEG(*.jpeg)"));
    if(fileName.isEmpty() == false){
        applyFilterExportImage(imageLoader->getOriginalPath(),fileName);
    }
}

void Gift::applyFilterExportImage(const QString originalFilePath,const QString newFilePath )
{
    qDebug()<<originalFilePath<<newFilePath;
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
        //qDebug()<<giftProcess->arguments();
    #endif
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
