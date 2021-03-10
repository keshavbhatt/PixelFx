#include "apppreferences.h"
#include "ui_apppreferences.h"

#include <QPropertyAnimation>

AppPreferences::AppPreferences(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AppPreferences)
{
    ui->setupUi(this);

    //style
    foreach (QWidget *widget, this->findChildren<QWidget*>()) {
        if(widget->objectName()!="headerWidget")
            widget->setStyleSheet("background-color:transparent");
        if( widget->inherits("QGroupBox")){
            widget->setStyleSheet("border:none;background-color:"
                                  " qlineargradient(spread:reflect,"
                                  " x1:0, y1:1, x2:0, y2:0, stop:0 "
                                  "rgba(17, 18, 20, 240), stop:0.5"
                                  " rgba(17, 18, 20, 96), stop:1 "
                                  "rgba(17, 18, 20, 240));");
            QGroupBox *gb = qobject_cast<QGroupBox *>(widget);
            connect(gb,&QGroupBox::toggled,[=](bool checked){
                QFrame *containerFrame = gb->findChild<QFrame*>();
                animateWidget(containerFrame,checked);
                if( !checked ) gb->setMaximumHeight( 30 );
                else gb->setMaximumHeight( 16777215 );
            });
        }
    }

    //load settings
    load_settings();
}

void AppPreferences::load_settings()
{
    //use native fileDialog
    ui->useNativeFileDialog->setChecked(settings.value("useNativeFileDialog",true).toBool());
}

void AppPreferences::animateWidget(QFrame *containerFrame,bool checked)
{
    int contentHeight = containerFrame->sizeHint().height();

    QPropertyAnimation * contentAnimation = containerFrame->findChild<QPropertyAnimation*>();
    if(contentAnimation == nullptr)
    {
        contentAnimation = new QPropertyAnimation(containerFrame, "minimumHeight",containerFrame);
        contentAnimation->setDuration(100);
        contentAnimation->disconnect();
    }
    connect(contentAnimation,&QPropertyAnimation::stateChanged,[containerFrame,checked](QAbstractAnimation::State newState, QAbstractAnimation::State oldState){
        Q_UNUSED(oldState);
        if(newState == QAbstractAnimation::Running){
            containerFrame->setVisible(true);
        }
        else if(newState == QAbstractAnimation::Stopped){
            containerFrame->setVisible( checked );
        }
    });
    contentAnimation->setStartValue(0);
    contentAnimation->setEndValue(contentHeight);
    contentAnimation->setDirection(checked ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    contentAnimation->start(QAbstractAnimation::KeepWhenStopped);
}

AppPreferences::~AppPreferences()
{
    delete ui;
}

void AppPreferences::on_useNativeFileDialog_toggled(bool checked)
{
    settings.setValue("useNativeFileDialog",checked);
}
