#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle(APPNAMESTR);
    this->setWindowIcon(QIcon(":/icons/app/icon-128.png"));

    //theme settings mapped to values of indexes of themeCombo widget in SettingsWidget class
    int theme = settings.value("themeCombo",1).toInt();
    setStyle(":/qbreeze/"+QString(theme == 1 ? "dark" : "light") +".qss");

    //geometry and state load
    if(settings.value("mainwindow_geometry").isValid()){
        restoreGeometry(settings.value("mainwindow_geometry").toByteArray());
    }else{
        //this->resize(ui->mainToolBar->sizeHint().width(),500);
    }
    if(settings.value("mainwindow_windowState").isValid()){
        restoreState(settings.value("mainwindow_windowState").toByteArray());
    }else{
        QScreen* pScreen = QApplication::primaryScreen();
        QRect availableScreenSize = pScreen->availableGeometry();
        this->move(availableScreenSize.center()-this->rect().center());
    }


    //other UI
    gift = new Gift(ui->view);
    connect(gift,&Gift::resetCoreFilters,[=](){
        coreFiltersWidget->reset();
    });

//    connect(gift,&Gift::croppedImage,[=](){
//       coreFiltersWidget->apply();
//    });

    graphicsViewControlsWidget = new graphicsViewControls(this,ui->view);
    ui->bottomHLayout->addWidget(graphicsViewControlsWidget);

    coreFiltersWidget = new coreFilters(this,gift,graphicsViewControlsWidget);
    coreFiltersWidget->setMinimumWidth(250);

    ui->dockWidgetContents->layout()->addWidget(coreFiltersWidget);
    updateDockWidgetStyle(this->dockWidgetArea(ui->dockWidget));

    connect(graphicsViewControlsWidget,&graphicsViewControls::filterChanged,[=](QString filter){
        Q_UNUSED(filter); // we getting filter from the class itself
        coreFiltersWidget->apply();
    });

    //update histogram
    connect(ui->view,&graphicsView::pixUpdated,[=](QImage image){
       coreFiltersWidget->updateScope(image);
       updateToolBar(!image.isNull());
    });

    init_toolbar();
}


void MainWindow::updateDockWidgetStyle(const Qt::DockWidgetArea &area)
{
    this->setStyleSheet("QMainWindow::separator {width: 1px;border:none; border-"+QString(area == Qt::LeftDockWidgetArea ? "right" : "left")+": 1px solid rgba(159,160,164,59);}"
                        "QMainWindow::separator:hover {background:transparent}");
}

void MainWindow::on_dockWidget_dockLocationChanged(const Qt::DockWidgetArea &area)
{
    this->updateDockWidgetStyle(area);
}


void MainWindow::init_toolbar()
{
    ui->mainToolBar->installEventFilter(this);
    ui->mainToolBar->setMovable(false);
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ui->mainToolBar->setIconSize(QSize(32,32));

    ui->mainToolBar->addAction(QIcon(":/icons/folder-open-line.png"),tr("Open"),this->gift,SLOT(openFile()));
    ui->mainToolBar->actions().last()->setShortcut(QKeySequence::Open);

    ui->mainToolBar->addAction(QIcon(":/icons/save-line.png"),tr("Save"),this->gift,SLOT(saveFile()));
    saveAction = ui->mainToolBar->actions().last();
    ui->mainToolBar->actions().last()->setShortcut(QKeySequence::Save);

    ui->mainToolBar->addSeparator();

//    ui->mainToolBar->addAction(QIcon(":/icons/arrow-go-back-line.png"),tr("Undo"),this->coreFiltersWidget,SLOT(undo()));
//    undoAction = ui->mainToolBar->actions().last();
//    //    ui->mainToolBar->actions().last()->setVisible(false);

//    ui->mainToolBar->addAction(QIcon(":/icons/arrow-go-forward-line.png"),tr("Redo"),this->coreFiltersWidget,SLOT(redo()));
//    redoAction = ui->mainToolBar->actions().last();
//    //    ui->mainToolBar->actions().last()->setVisible(false);

    ui->mainToolBar->addAction(QIcon(":/icons/compare-image-line.png"),tr("Compare"));
    compareAction = ui->mainToolBar->actions().last();
    compareAction->setEnabled(!ui->view->isSceneEmpty());

    for(auto wPtr : compareAction->associatedWidgets()) {
            wPtr->installEventFilter(this);
    }

//    ui->mainToolBar->addSeparator();
//    ui->mainToolBar->addAction(QIcon(":/icons/fx-line.png"),tr("FX"),this,SLOT(aboutApp()));
//    ui->mainToolBar->addAction(QIcon(":/icons/grid-line.png"),tr("Texture"),this,SLOT(aboutApp()));
//    ui->mainToolBar->addAction(QIcon(":/icons/shape-line.png"),tr("Frame"),this,SLOT(aboutApp()));
    ui->mainToolBar->addAction(QIcon(":/icons/crop-line.png"),tr("Crop"),this->gift,SLOT(crop()));
    cropAction = ui->mainToolBar->actions().last();
    cropAction->setEnabled(!ui->view->isSceneEmpty());

    ui->mainToolBar->addAction(QIcon(":/icons/information-line.png"),tr("Info"),this->gift,SLOT(imageInfo()));
    QAction *info = ui->mainToolBar->actions().last();
    info->setObjectName("infoAction");
    info->setEnabled(!ui->view->isSceneEmpty());

    QWidget* hSpacer= new QWidget(this);
    hSpacer->setStyleSheet("background-color:transparent");
    hSpacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(hSpacer);

    ui->mainToolBar->addAction(QIcon(":/icons/setting-line.png"),tr("Preferences"),this,SLOT(aboutApp()));
    ui->mainToolBar->addAction(QIcon(":/icons/app/icon-64.png"),tr("About"),this,SLOT(aboutApp()));

    updateToolBar(!ui->view->isSceneEmpty());
}


void MainWindow::updateToolBar(bool hasImage)
{
    compareAction->setEnabled(hasImage);
    cropAction->setEnabled(hasImage);
    saveAction->setEnabled(hasImage);


    QAction *info = this->findChild<QAction*>("infoAction");
    info->setEnabled(hasImage);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    //ToolBar Movable on Hover
    if(watched == ui->mainToolBar && (event->type() == QEvent::HoverMove))
    {
        QPoint pos = static_cast<QMouseEvent*>(event)->pos();
        switch (this->toolBarArea(ui->mainToolBar)) {
        case Qt::NoToolBarArea:;
        case Qt::AllToolBarAreas:;
           case Qt::TopToolBarArea:{
            if(pos.x() < 10 )
                ui->mainToolBar->setMovable(true);
            break;
            }
           case Qt::BottomToolBarArea:{
            if(pos.x() < 10 )
                ui->mainToolBar->setMovable(true);
            break;
            }
           case Qt::LeftToolBarArea:{
            if(pos.y() < 10 )
                ui->mainToolBar->setMovable(true);
            break;
            }
           case Qt::RightToolBarArea:{
            if(pos.y() < 10 )
                ui->mainToolBar->setMovable(true);
            break;
            }
        }
    }
    if(watched == ui->mainToolBar && event->type() == QEvent::Leave){
        ui->mainToolBar->setMovable(false);
    }

    //Compare action button
    auto watchedWidget  = qobject_cast<QWidget*>(watched);
    if(compareAction != nullptr && watchedWidget != nullptr && watchedWidget != ui->mainToolBar
            && compareAction->associatedWidgets().contains(watchedWidget)
            && event->type() == QEvent::MouseButtonPress)
    {
        this->gift->compare(true);
    }
    if(compareAction != nullptr && watchedWidget != nullptr && watchedWidget != ui->mainToolBar
            && compareAction->associatedWidgets().contains(watchedWidget)
            && event->type() == QEvent::MouseButtonRelease)
    {
        this->gift->compare(false);
    }


    return QMainWindow::eventFilter(watched,event);
}

void MainWindow::setStyle(QString fname)
{
    QFile styleSheet(fname);
    if (!styleSheet.open(QIODevice::ReadOnly))
    {
        qWarning("Unable to open file");
        return;
    }
    qApp->setStyleSheet(styleSheet.readAll());
    styleSheet.close();
    qApp->setStyle(QStyleFactory::create("Fusion"));
    QPalette palette;
    palette.setColor(QPalette::Window,QColor(53,53,53));
    palette.setColor(QPalette::WindowText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::WindowText,QColor(127,127,127));
    palette.setColor(QPalette::Base,QColor(42,42,42));
    palette.setColor(QPalette::AlternateBase,QColor(66,66,66));
    palette.setColor(QPalette::ToolTipBase,Qt::white);
    palette.setColor(QPalette::ToolTipText,QColor(53,53,53));
    palette.setColor(QPalette::Text,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::Text,QColor(127,127,127));
    palette.setColor(QPalette::Dark,QColor(35,35,35));
    palette.setColor(QPalette::Shadow,QColor(20,20,20));
    palette.setColor(QPalette::Button,QColor(53,53,53));
    palette.setColor(QPalette::ButtonText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::ButtonText,QColor(127,127,127));
    palette.setColor(QPalette::BrightText,Qt::red);
    palette.setColor(QPalette::Link,QColor("#3DAEE9"));
    palette.setColor(QPalette::Highlight,QColor(49,106,150));
    palette.setColor(QPalette::Disabled,QPalette::Highlight,QColor(80,80,80));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::Disabled,QPalette::HighlightedText,QColor(127,127,127));
    qApp->setPalette(palette);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    settings.setValue("mainwindow_geometry",saveGeometry());
    settings.setValue("mainwindow_windowState", saveState());
    QWidget::closeEvent(event);
}

void MainWindow::aboutApp()
{
    QDialog *aboutDialog = new QDialog(this,Qt::Dialog);
    aboutDialog->setWindowModality(Qt::WindowModal);
    QVBoxLayout *layout = new QVBoxLayout;
    QLabel *message = new QLabel(aboutDialog);
    layout->addWidget(message);
    connect(message,&QLabel::linkActivated,[=](const QString linkStr){
        if(linkStr.contains("about_qt")){
            qApp->aboutQt();
        }else{
            QDesktopServices::openUrl(QUrl(linkStr));
        }
    });
    aboutDialog->setLayout(layout);
    aboutDialog->setAttribute(Qt::WA_DeleteOnClose,true);
    aboutDialog->show();

    QString mes =
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><img src=':/icons/app/icon-64.png' /></p>"
                 "<p align='center' style='-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><br /></p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'>Designed and Developed</p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'>by <span style=' font-weight:600;'>Keshav Bhatt</span> &lt;keshavnrj@gmail.com&gt;</p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'>Website: https://ktechpit.com</p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'>Runtime: <a href='http://about_qt'>Qt Toolkit</a></p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'>Version: "+QApplication::applicationVersion()+"</p>"
                 "<p align='center' style='-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><br /></p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><a href='https://snapcraft.io/search?q=keshavnrj'>More Apps</p>"
                 "<p align='center' style='-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><br /></p>"
                 "<p align='center' style=' margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><a href='https://github.com/keshavbhatt/SnapHerald'>Source Code</p>"
                 "<p align='center' style='-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;'><br /></p>";

    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    message->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(1000);
    a->setStartValue(0);
    a->setEndValue(1);
    a->setEasingCurve(QEasingCurve::InCurve);
    a->start(QPropertyAnimation::DeleteWhenStopped);
    message->setText(mes);
    message->show();
}

MainWindow::~MainWindow()
{
    delete this->gift;
    delete ui;
}

