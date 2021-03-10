#ifndef SAVEDIALOG_H
#define SAVEDIALOG_H

#include <QWidget>
#include "imageloader.h"
#include <QImageWriter>
#include <QSettings>
#include "widgets/waitingSpinner/waitingspinnerwidget.h"

namespace Ui {
class SaveDialog;
}

class SaveDialog : public QWidget
{
    Q_OBJECT

signals:
    //void saveImage();

public:
    explicit SaveDialog(QWidget *parent = nullptr, QImage img = QImage(), QString defaultPath = "",
                        QString defaultFileBaseName = "",
                        ImageLoader *imageLoader = nullptr);
    ~SaveDialog();

private slots:

    void on_changeLocation_clicked();
    void on_defaultPath_textChanged(const QString &arg1);


    void on_defaultFileBaseName_textChanged(const QString &arg1);

    void init_Formats();
    void on_formats_comboBox_currentTextChanged(const QString &arg1);

    void presentFormatOption(QString format);
    void on_png_compression_slider_valueChanged(int value);

    void on_jpeg_quality_slider_valueChanged(int value);

    void on_save_button_clicked();

private:
    Ui::SaveDialog *ui;
    QString defaultPath,defaultFileBaseName,defaultExetension;
    ImageLoader * imageLoader = nullptr;
    QImageWriter writer;
    QStringList supportedFormats;
    QImage targetImage;
    WaitingSpinnerWidget *_loader = nullptr;
    QSettings settings;
};

//The image writer class for asyncImageWrite
class ImageWriter : public QObject {
public:
    QString comment;
    int jpeg_qual,png_qual;
    bool tiff_comp;

    QFuture<bool> write(const QString &fileName,const QImage &img ,const QString &ext,const QString &comment)
    {
        auto writeImageWorker = [this](const QString &fileName,const QImage &img ,const QString &ext,const QString &comment) {
            QImageWriter imgwriter;
            QStringList formats = {"JPG","JPEG","PNG","TIFF"};
            int ext_pos = -1;
            if(formats.contains(ext,Qt::CaseInsensitive)){
                ext_pos = formats.indexOf(QString(ext).toUpper());
            }
            switch (ext_pos) {
            case 0: // jpg
            {
                if(comment.isEmpty() == false && imgwriter.supportsOption(QImageIOHandler::Description))
                    imgwriter.setText("Description",comment);
                imgwriter.setQuality(jpeg_qual);
                break;
            }
            case 1: // jpeg
            {
                if(comment.isEmpty() == false && imgwriter.supportsOption(QImageIOHandler::Description))
                    imgwriter.setText("Description",comment);
                imgwriter.setQuality(jpeg_qual);
                break;
            }
            case 2: // png
            {
                imgwriter.setQuality(png_qual);
                qDebug()<<"PNG"<<imgwriter.quality()<<imgwriter.compression();
                break;
            }

            case 3: // tiff
            {
                imgwriter.setCompression(tiff_comp);
                break;
            }
            default: // other

                break;
            }
            imgwriter.setFormat(ext.toUpper().toUtf8());
            imgwriter.setFileName(fileName);
            return imgwriter.write(img);
        };
        return QtConcurrent::run(writeImageWorker, fileName,img, ext,comment);
    }
};

#endif // SAVEDIALOG_H
