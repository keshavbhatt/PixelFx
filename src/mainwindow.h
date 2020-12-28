#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStyleFactory>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QDesktopServices>
#include <QLabel>
#include <QVBoxLayout>
#include <QDialog>
#include <QtNetwork>
#include <QAction>

#include "gift.h"
#include "corefilters.h"
#include "graphicsviewcontrols.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected slots:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *watched, QEvent *event);
private:
    Ui::MainWindow *ui;
    QSettings settings;
    Gift *gift = nullptr;
    coreFilters *coreFiltersWidget = nullptr;
    graphicsViewControls *graphicsViewControlsWidget = nullptr;
    QAction *compareAction;
    QAction *cropAction;
    QAction *saveAction;
    QAction *undoAction, *redoAction;

private slots:
    void setStyle(QString fname);

    void aboutApp();
    void init_toolbar();
    void on_dockWidget_dockLocationChanged(const Qt::DockWidgetArea &area);
    void updateDockWidgetStyle(const Qt::DockWidgetArea &area);
    void updateToolBar(bool hasImage);

};

#endif // MAINWINDOW_H
