#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include "rungaurd.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QString appname = APPNAMESTR;
    QApplication::setApplicationName(appname);
    QApplication::setOrganizationName("org.keshavnrj.ubuntu");
    QApplication::setApplicationVersion(VERSIONSTR);

    //allow multiple instances in debug builds
#ifndef QT_DEBUG
    RunGuard guard("org.keshavnrj.ubuntu."+appname);
    if ( !guard.tryToRun() ){
        QMessageBox::critical(0, appname,"An instance of "+appname+" is already running.");
        return 0;
    }
#endif

    MainWindow w;
    w.show();

    return a.exec();
}
