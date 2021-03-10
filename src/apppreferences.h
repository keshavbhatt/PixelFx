#ifndef APPPREFERENCES_H
#define APPPREFERENCES_H

#include <QWidget>
#include <QFrame>
#include <QSettings>

namespace Ui {
class AppPreferences;
}

class AppPreferences : public QWidget
{
    Q_OBJECT

public:
    explicit AppPreferences(QWidget *parent = nullptr);
    ~AppPreferences();

private slots:
    void animateWidget(QFrame *containerFrame, bool checked);
    void on_checkBox_toggled(bool checked);

private:
    Ui::AppPreferences *ui;
    QSettings settings;
};

#endif // APPPREFERENCES_H
