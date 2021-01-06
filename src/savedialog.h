#ifndef SAVEDIALOG_H
#define SAVEDIALOG_H

#include <QWidget>
#include "imageloader.h"
#include <QImageWriter>

namespace Ui {
class SaveDialog;
}

class SaveDialog : public QWidget
{
    Q_OBJECT

signals:
    void saveImage();

public:
    explicit SaveDialog(QWidget *parent = nullptr, QString defaultPath = "",
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
};

#endif // SAVEDIALOG_H
