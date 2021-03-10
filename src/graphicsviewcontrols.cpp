#include "graphicsviewcontrols.h"
#include "ui_graphicsviewcontrols.h"

graphicsViewControls::graphicsViewControls(QWidget *parent,
    graphicsView *graphicsViewWidget) :
    QWidget(parent),
ui(new Ui::graphicsViewControls)
{
    ui->setupUi(this);
    this->graphicsViewWidget = graphicsViewWidget;
    connect(this->graphicsViewWidget,&graphicsView::itemAdded,[=](){
        this->rotationAngle = 0;
        if(this->graphicsViewWidget->fitToViewChecked){
            ui->fit->blockSignals(true);
            ui->fit->setChecked(true);
            ui->fit->blockSignals(false);
        }else{
            ui->fit->setChecked(false);
        }
    });
}


graphicsViewControls::~graphicsViewControls()
{
    delete ui;
}

QString graphicsViewControls::getFilter()
{
    return filter;
}

void graphicsViewControls::setFilter(QString filterStr)
{
    Q_UNUSED(filterStr);
    this->filter = " " + flipState() + " " +rotateState();
    emit filterChanged(filter);
}

QString graphicsViewControls::rotateState()
{
    return rotationAngle == 0 ? "" : QString("-rotate %1").arg(rotationAngle);
    //+QString::number(this->graphicsViewWidget->rotation(nullptr));
}


QString graphicsViewControls::flipState()
{
    bool fliph = ui->flipHorizontly->isChecked();
    bool flipv = ui->flipVertically->isChecked();
    return QString(fliph ? " -fliph":"") + QString(flipv ? " -flipv":"");
}

void graphicsViewControls::on_flipHorizontly_toggled(bool checked)
{
    Q_UNUSED(checked);
    setFilter();
}

void graphicsViewControls::on_flipVertically_toggled(bool checked)
{
    Q_UNUSED(checked);
    setFilter();
}

void graphicsViewControls::on_rotateClockwise_clicked()
{
//  this->graphicsViewWidget->rotateItem(nullptr,90);
    if(rotationAngle >= 0 && rotationAngle <= 360 )
        rotationAngle = ( 360 - rotationAngle )- 90;
    else
        rotationAngle = 0;
    setFilter();
}



void graphicsViewControls::uncheckFitToView()
{
    ui->fit->setChecked(false);
}

void graphicsViewControls::on_fit_toggled(bool checked)
{
    this->graphicsViewWidget->setFitToView(checked);
    if(checked){
        this->graphicsViewWidget->fitToView();
    }
}
