#include "hspacer.h"
#include "ui_hspacer.h"

HSpacer::HSpacer(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HSpacer)
{
    ui->setupUi(this);
}

HSpacer::~HSpacer()
{
    delete ui;
}
