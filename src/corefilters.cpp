#include "corefilters.h"
#include "ui_corefilters.h"

//ui->sepia->setRange(-1,100);
//ui->sepia->setValue(-1);
//+" -sepia "+valueStr(ui->sepia)


coreFilters::coreFilters(QWidget *parent, Gift *gift, graphicsViewControls *viewControl) :
    QWidget(parent),
    ui(new Ui::coreFilters)
{
    ui->setupUi(this);
    this->gift = gift;
    this->viewControls = viewControl;

    init_basicSliders();
    init_HueSliders();
    init_rgbSliders();
    init_sharpenSliders();
    init_noiseSliders();
    connectSliders();

    //style
    foreach (QWidget *widget, this->findChildren<QWidget*>()) {
        widget->setStyleSheet("background-color:transparent");
        if( widget->inherits("QGroupBox") && widget->objectName().contains("coreGroupBox")){
            widget->setStyleSheet("border:none;background-color: qlineargradient(spread:reflect, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(17, 18, 20, 240), stop:0.5 rgba(17, 18, 20, 96), stop:1 rgba(17, 18, 20, 240));");
            QGroupBox *gb = qobject_cast<QGroupBox *>(widget);
            connect(gb,&QGroupBox::toggled,[=](bool checked){
                QFrame *containerFrame = gb->findChild<QFrame*>();
                animateWidget(containerFrame,checked);
                if( !checked ) gb->setMaximumHeight( 30 );
                else gb->setMaximumHeight( 16777215 );
            });
        }
    }
    ui->coreGroupBoxHistogram->setStyleSheet("QGroupBox{padding-top: 1em; margin-top: -1em; border:none;background-color: qlineargradient(spread:reflect, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(17, 18, 20, 240), stop:0.5 rgba(17, 18, 20, 96), stop:1 rgba(17, 18, 20, 240)) }");
    ui->resetButtonWidget->setStyleSheet("QWidget{border:none;background-color: qlineargradient(spread:reflect, x1:0, y1:1, x2:0, y2:0, stop:0 rgba(17, 18, 20, 235), stop:0.205212 rgba(24, 26, 28, 221), stop:0.371336 rgba(32, 35, 38, 205), stop:0.563518 rgba(40, 44, 48, 176), stop:0.729642 rgba(45, 49, 54, 160), stop:0.863192 rgba(49, 54, 59, 146), stop:1 rgba(191, 64, 64, 0));}");
}

void coreFilters::animateWidget(QFrame *containerFrame,bool checked)
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

// connect all sliders to ther slots
void coreFilters::connectSliders()
{
    connect(gift,&Gift::stateChanged,[=](bool processing){
       this->setEnabled(!processing);
    });

    foreach (coreSlider *slider, this->findChildren<coreSlider*>())
    {
        slider->installEventFilter(this);
        connect(slider,&QSlider::sliderReleased,[=](){
            apply();
        });

        //update label
        connect(slider,&coreSlider::valueChanged,[=](const int val){
            QLabel *val_label = this->findChild<QLabel*>(slider->objectName().left(3)+"_v_lb");
            if(isRGBSlider(slider)){
                val_label->setText(QString::number(getRgbSliderLabel(slider)));
            }else{
                val_label->setText(QString::number(val));
            }
        });

        //connect reset buttons
        QPushButton *val_reset_btn = this->findChild<QPushButton*>(slider->objectName().left(3)+"_pb");
        connect(val_reset_btn,&QPushButton::clicked,[=](){
            if(slider->objectName() == ui->hueslider->objectName()){
                ui->hueslider->setValue(180);
                ui->intensity->reset();
                ui->lightness->reset();
            }else if(isRGBSlider(slider)){
                slider->setValue(50);
            }else if(isSepiaSlider(slider)){
                slider->setValue(0);
            }else if(isThreshholdSlider(slider)){
                slider->setValue(0);
            }else{
                slider->reset();
            }
            apply();
        });

        //init labels
        slider->setValue(slider->value()+1);
        slider->setValue(slider->value()-1);
    }
}

// reimplimented eventfilter
bool coreFilters::eventFilter(QObject *watched, QEvent *event)
{
    if(watched->inherits("QSlider")){
        if(event->type() == QEvent::Wheel){
            return true;
        }
    }
    return QWidget::eventFilter(watched,event);
}

// initialize basic slider widget
void coreFilters::init_basicSliders()
{
    ui->brightness->setRange(-100,100);
    ui->contrast->setRange(-100,100);
    ui->sigmoidal->setRange(-100,100);
    ui->saturation->setRange(-100,100);


    ui->brightness->setDefaultValue(0);
    ui->contrast->setDefaultValue(0);
    ui->sigmoidal->setDefaultValue(0);
    ui->saturation->setDefaultValue(0);

    ui->gamma->setRange(-100,100);
    ui->gamma->setDefaultValue(0);


}

// initialize hue slider widget
void coreFilters::init_HueSliders()
{
    //colorize slider

    ui->lightness->setRange(0,100);
    ui->intensity->setRange(0,100);

    ui->hueslider->setValue(180);
    ui->intensity->setDefaultValue(50);
    ui->lightness->setDefaultValue(0);

    //hue shift slider
    ui->hslider->setRange(-180,180);
    ui->hslider->setDefaultValue(0);
}

// initialize RGB slider widget
void coreFilters::init_rgbSliders()
{
    GradientSlider * gdr = new GradientSlider(Qt::Horizontal,this);
    gdr->setObjectName("red");
    QVector<QColor>col;
    col.append(QColor::fromRgb(0,128,128/2));
    col.append(QColor::fromRgb(128,0,0));
    gdr->setColors(col);
    gdr->setValue(50);
    ui->redVerticalLayout->addWidget(gdr);

    GradientSlider * gdg = new GradientSlider(Qt::Horizontal,this);
    gdg->setObjectName("green");
    QVector<QColor>colg;
    colg.append(QColor::fromRgb(128,0,128/2));
    colg.append(QColor::fromRgb(0,128,0));
    gdg->setColors(colg);
    gdg->setValue(50);
    ui->greenVerticalLayout->addWidget(gdg);

    GradientSlider * gdb = new GradientSlider(Qt::Horizontal,this);
    gdb->setObjectName("blue");
    QVector<QColor>colb;
    colb.append(QColor::fromRgb(148,148,0));
    colb.append(QColor::fromRgb(0,0,128));
    gdb->setColors(colb);
    gdb->setValue(50);
    ui->blueVerticalLayout->addWidget(gdb);

    GradientSlider * gds = new GradientSlider(Qt::Horizontal,this);
    gds->setObjectName("sepia");
    QVector<QColor>cols;
    cols.append(QColor::fromRgba(05704214));
    cols.append(QColor::fromRgb(112,66,20));
    gds->setColors(cols);
    gds->setValue(0);
    ui->sepiaVerticalLayout->addWidget(gds);

    GradientSlider * gdt = new GradientSlider(Qt::Horizontal,this);
    gdt->setObjectName("threshold");
    QVector<QColor>colt;
    colt.append(QColor::fromRgb(255,255,255));
    colt.append(QColor::fromRgb(0,0,0));
    gdt->setColors(colt);
    gdt->setValue(0);
    ui->thresholdVerticalLayout->addWidget(gdt);
}

//initialize noise reduction filters
void coreFilters::init_noiseSliders()
{
    ui->mean->setRange(0,10);
    ui->median->setRange(0,10);

    ui->mean->setDefaultValue(0);
    ui->median->setDefaultValue(0);
}

// initialize details/sharpen slider widget
void coreFilters::init_sharpenSliders()
{
    //shapen sliders
    ui->sharpen->setRange(0,50);
    ui->sharpen->setDefaultValue(0);

    //pixelate sliders
    ui->pixelate->setRange(0,50);
    ui->pixelate->setDefaultValue(0);

    //blur sliders
    ui->gaussianblur->setRange(0,50);
    ui->gaussianblur->setDefaultValue(0);

}

// reset sliders to their default values and clears filterstack and filterchain
void coreFilters::reset()
{
    foreach (coreSlider *slider, this->findChildren<coreSlider*>())
    {
        if(slider->objectName() == ui->hueslider->objectName()){
            slider->setValue(180);
        }else if(isRGBSlider(slider)){
            slider->setValue(50);
        }else if(isSepiaSlider(slider)){
            slider->setValue(0);
        }else if(isThreshholdSlider(slider)){
            slider->setValue(0);
        }else{
            slider->reset();
        }
    }
    filterChain.clear();
    filterStack.clear();

    //put first item in filterStack for init
    updateFilterChain();
    filterStack.push(filterChain);
}

// apply filterchain if it's not the last applied one
void coreFilters::apply()
{
    updateFilterChain();
    if(!filterStack.isEmpty() && filterStack.top() != filterChain)
    {
        filterStack.push(filterChain);
        gift->applyFilter(filterChain);
    }
}

// updates the main filterchain
void coreFilters::updateFilterChain()
{
    filterChain = QString(getBrightness()
                         +getContrast()
                         +getSigmoidalContrast()
                         +getSaturation()
                         +getGamma()
                         +getRgb()
                         +getSepia()
                         +getThreshold()
                         +getHue()
                         +getSharpness()
                         +getPixelate()
                         +getBlur()
                         +getHueShift()
                         +getNoiseFilter()
                         +viewControls->getFilter()
                         );
}

QString coreFilters::getBrightness()
{
    int val = ui->brightness->value();
    return val == 0 ?  "" : QString(" -brightness " +valueStr(ui->brightness));
}

QString coreFilters::getContrast()
{
    int val = ui->contrast->value();
    return val == 0 ?  "" : QString(" -contrast " +valueStr(ui->contrast));
}

QString coreFilters::getSigmoidalContrast()
{
    int val = ui->sigmoidal->value();
    return val == 0 ?  "" : QString(" -sigmoid 0.5,"   +QString::number(ui->sigmoidal->value()/10));
}


QString coreFilters::getSaturation()
{
    int val = ui->saturation->value();
    return val == 0 ? "" : QString(" -saturation "+valueStr(ui->saturation));
}

QString coreFilters::getPixelate()
{
    int val = ui->pixelate->value();
    return val == 0 ? "" : QString(" -pixelate "+valueStr(ui->pixelate));
}

QString coreFilters::getHueShift()
{
    int val = ui->hslider->value();
    return val == 0 ? "" : QString(" -hue "+valueStr(ui->hslider));
}

//get noise reduction filters
QString coreFilters::getNoiseFilter()
{
    QString mean, median;
    mean    = ui->mean->value()     == 0 ? "": " -mean "+valueStr(ui->mean);
    median  = ui->median->value()   == 0 ? "": " -median "+valueStr(ui->median);

    return mean + median;
}

//get gaussianBlur
QString coreFilters::getBlur()
{
    double value = double(ui->gaussianblur->value())/(10.0/2.0);
    //prepare final mask
    QString blurStr;
    blurStr.append(QString::number(value,'f',1));

    return value > 0.0 ? " -blur "+blurStr : "";
}


// get unsharpen/sharpen filter
QString coreFilters::getSharpness()
{

    // return blank if values make no sense
    if(ui->sharpen->value() == 0){
        return "";
    }

    double sigma = double(ui->sharpen->value())/(10.0/2.0);
    //prepare final mask
    QString unsharpenVal;
    unsharpenVal.append(QString::number(sigma,'f',1));
    unsharpenVal.append(",");
    unsharpenVal.append("1.0");
    unsharpenVal.append(",");
    unsharpenVal.append("0.05");

    return unsharpenVal.isEmpty() ? "":" -unsharp "+unsharpenVal;
}

// get hue filter
QString coreFilters::getHue()
{
    QString hue;
    hue.append(valueStr(ui->hueslider));
    hue.append(",");
    hue.append(valueStr(ui->intensity));
    hue.append(",");
    hue.append(valueStr(ui->lightness));
    return  ui->lightness->value() == 0 ? "" : " -colorize "+hue;
}

// get gamma filter
QString coreFilters::getGamma()
{
    QString gammaVal;
    const int  int_value = ui->gamma->value();
    const double dpi = 100.0;
    if(int_value > 9 ){
        double value = double(int_value) * 10;
        double gamma = double( value / dpi)+1;
        gammaVal = QString::number(gamma,'f',1);
    }else if(int_value >= 0){
        gammaVal ="1."+QString::number(int_value,'f',0);
    }else if(int_value < 0){
        double value = double(int_value);
        double gamma = 1.0 - double( value / -dpi);
        double gamma_plus = -gamma > 0 ? -gamma : gamma;
        if(int_value == -100){
            gamma_plus = 0.0;
        }
        gammaVal = QString::number(gamma_plus,'f',1);
    }
    return gammaVal.isEmpty() ? "":" -gamma "+gammaVal;
}

// return true is slider type is one from RGB sliders
bool coreFilters::isRGBSlider(QSlider *slider)
{
    return (slider->objectName() == "red"  ||
            slider->objectName() == "green"||
            slider->objectName() == "blue");
}

// return true is slider is sepia
bool coreFilters::isSepiaSlider(QSlider *slider)
{
    return slider->objectName() == "sepia";
}

// return true is slider is threshold
bool coreFilters::isThreshholdSlider(QSlider *slider)
{
    return slider->objectName() == "threshold";
}

// get rgb sliders label values
int coreFilters::getRgbSliderLabel(QSlider *slider)
{
    int value = double(slider->value());
    if(value == 50.00){
        value = 0.00;
    }else if (value < 50.00) {
        value = (value - 100.00) + 50;
    }else{
        value = value - 50.00;
    }
    return value;
}

// get sepia filter
QString coreFilters::getSepia()
{
    auto sepiaSlider = this->findChild<GradientSlider*>("sepia");
    return sepiaSlider->value() == 0 ?"" : " -sepia "+valueStr(sepiaSlider);
}

// get threshold filter
QString coreFilters::getThreshold()
{
    auto thresholdSlider = this->findChild<GradientSlider*>("threshold");
    return thresholdSlider->value() == 0 ?"" : " -threshold "+valueStr(thresholdSlider);
}

// get RGB filter
QString coreFilters::getRgb()
{
    auto redSlider = this->findChild<GradientSlider*>("red");
    auto greenSlider = this->findChild<GradientSlider*>("green");
    auto blueSlider = this->findChild<GradientSlider*>("blue");

    // return blank if values make no sense
    if(redSlider->value() == 50 &&
       greenSlider->value() == 50 &&
            blueSlider->value() == 50){
        return "";
    }

    double red = double(redSlider->value());
    double green = double(greenSlider->value());
    double blue = double(blueSlider->value());


    if(red == 50.00){
        red = 0.00;
    }else if (red < 50.00) {
        red = red - 100.00;
    }else{
        red = red - 50.00;
    }

    if(green == 50.00){
        green = 0.00;
    }else if (green < 50.00) {
        green = green - 100.00;
    }else{
        green = green - 50.00;
    }

    if(blue == 50.00){
        blue = 0.00;
    }else if (blue < 50.00) {
        blue = blue - 100.00;
    }else{
        blue = blue - 50.00;
    }

    QString rgbStr;
    rgbStr.append(QString::number(red,'f',1));
    rgbStr.append(",");
    rgbStr.append(QString::number(green,'f',1));
    rgbStr.append(",");
    rgbStr.append(QString::number(blue,'f',1));

    return " -colorbalance "+rgbStr;
}


// helper func. to return number type to QString
QString coreFilters::valueStr(QSlider *slider)
{
    return QString::number(slider->value());
}

// reset sliders and reset everything
void coreFilters::on_resetButton_clicked()
{
    reset();
    apply();
    gift->applyFilter("");
}

// updated histogram
void coreFilters::updateScope(QImage image)
{
    QImage *img = new QImage(image);
    ui->labelScope->renderHistogram(img);
    delete img; // free resources
}

coreFilters::~coreFilters()
{
    delete ui;
}

