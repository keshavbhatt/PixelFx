#include "savedialog.h"
#include "ui_savedialog.h"
#include <QFileDialog>
#include <QTimer>

SaveDialog::SaveDialog(QWidget *parent, QString defaultPath, QString defaultFileBaseName, ImageLoader *imageLoader) :
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

    init_Formats();
    this->resize(this->sizeHint().width(),this->minimumHeight());
}

SaveDialog::~SaveDialog()
{
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

void SaveDialog::applyFilterExportImage(const QString originalFilePath,const QString newFilePath )
{
    qDebug()<<originalFilePath<<newFilePath;
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
    ui->png_compression_slider->setRange(0,10);
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
