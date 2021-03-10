#include "savedialog.h"
#include "ui_savedialog.h"
#include <QFileDialog>
#include <QTimer>

SaveDialog::SaveDialog(QWidget *parent, QImage img,  QString defaultPath, QString defaultFileBaseName, ImageLoader *imageLoader) :
    QWidget(parent),
    ui(new Ui::SaveDialog)
{
    ui->setupUi(this);
    this->imageLoader = imageLoader;

    this->defaultPath           = defaultPath;
    this->defaultFileBaseName   = defaultFileBaseName;
    this->defaultExetension     = imageLoader->getOriginalExetension();

    ui->defaultFileBaseName->setText(this->defaultFileBaseName);
    ui->defaultPath->setText(this->defaultPath);

    this->targetImage = img;

    _loader = new WaitingSpinnerWidget(this,true,true);
    _loader->setRoundness(70.0);
    _loader->setMinimumTrailOpacity(15.0);
    _loader->setTrailFadePercentage(70.0);
    _loader->setNumberOfLines(10);
    _loader->setLineLength(8);
    _loader->setLineWidth(2);
    _loader->setInnerRadius(2);
    _loader->setRevolutionsPerSecond(3);
    _loader->setColor(QColor("#1e90ff"));


    init_Formats();
    this->resize(this->sizeHint().width(),this->minimumHeight());
}

SaveDialog::~SaveDialog()
{
    _loader->deleteLater();
    delete ui;
}

void SaveDialog::on_changeLocation_clicked()
{
    QString location = QFileDialog::getExistingDirectory(this,
         QObject::tr("Destination directory"), defaultPath);

    if(location.isEmpty() == false){
        ui->defaultPath->setText(location);
    }
}

void SaveDialog::init_Formats()
{
    QList<QByteArray> formats       = writer.supportedImageFormats();

    //ignored formats
    QStringList blackListedFormats  = {"cur", "icns", "ico", "pbm", "pgm",
                                      "ppm", "tif", "wbmp", "xbm", "xpm"};
    supportedFormats.clear();
    ui->formats_comboBox->clear();

    for (QString val : formats) {
        if(!blackListedFormats.contains(val))
            supportedFormats.append(val);
    }

    ui->formats_comboBox->addItems(supportedFormats);

    //default to original image extension
    if(supportedFormats.contains(defaultExetension))
    {
        ui->formats_comboBox->setCurrentText(defaultExetension);
    }

    //JPEG
    ui->jpeg_quality_slider->setRange(0,100);
    ui->jpeg_quality_slider->setValue(100);

    //PNG
    ui->png_compression_slider->setRange(0,100);
    ui->png_compression_slider->setValue(10);

    //TIFF
    ui->tiff_no_compression_rb->setChecked(true);
}

void SaveDialog::presentFormatOption(QString format)
{
    auto formatsOptions = ui->formatsWidget->findChildren<QGroupBox*>();
    QGroupBox *jpeg = nullptr; // for JPG
    foreach (QObject *obj, formatsOptions) {
        QGroupBox *formatBox = qobject_cast<QGroupBox*>(obj);
        formatBox->hide();
        if(formatBox->objectName() == format){
            formatBox->show();
        }
        if(formatBox->objectName() == "jpeg"){
            jpeg = formatBox;
            jpeg->setTitle("JPEG");
        }
    }
    //show JPEG option for JPG format
    if(format == "jpg"){
        jpeg->show();
        jpeg->setTitle("JPG");
    }
    //update geometry after above loop is finished
    QTimer::singleShot(300,this,[=]{
        this->resize(this->width(),this->minimumHeight());
    });
}

void SaveDialog::on_defaultPath_textChanged(const QString &arg1)
{
    bool empty = arg1.trimmed().isEmpty();
    ui->save_button->setEnabled(!empty);
}

void SaveDialog::on_defaultFileBaseName_textChanged(const QString &arg1)
{
    bool empty = arg1.trimmed().isEmpty();
    ui->save_button->setEnabled(!empty);
}

void SaveDialog::on_formats_comboBox_currentTextChanged(const QString &arg1)
{
    ui->image_extension_label->setText("."+arg1);
    presentFormatOption(arg1);
}

void SaveDialog::on_png_compression_slider_valueChanged(int value)
{
    ui->png_compression_label->setText(QString::number(value));
}

void SaveDialog::on_jpeg_quality_slider_valueChanged(int value)
{
    ui->jpeg_quality_label->setText(QString::number(value));
}

void SaveDialog::on_save_button_clicked()
{
    qDebug()<<"write started";
    _loader->start();
    ImageWriter writer;
    QString ext = ui->image_extension_label->text();
    QString targetFileName = ui->defaultPath->text()+QDir::separator()+ui->defaultFileBaseName->text()+ext;
    QString comment = ui->jpeg_comment_textEdit->toPlainText().trimmed();
    const int jpeg_qual   = ui->jpeg_quality_slider->value();
    const int png_qual    = ui->png_compression_slider->value();
    const bool tiff_comp  = ui->tiff_lzw_compression_rb->isChecked() ? 1 : 0;
    writer.jpeg_qual = jpeg_qual;
    writer.png_qual  = png_qual;
    writer.tiff_comp = tiff_comp;

    QFuture<bool> future = writer.write(targetFileName,targetImage,QString(ext).remove("."),comment);
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>();
    connect(watcher, &QFutureWatcher<bool>::finished,
            [future,this,watcher]() {
        qDebug()<<"write ended";
        _loader->stop();
        //qDebug()<<future.result();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}
