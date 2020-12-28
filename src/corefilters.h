#ifndef COREFILTERS_H
#define COREFILTERS_H

#include <QWidget>
#include <QDebug>
#include <QSlider>
#include <QStack>
#include <QGroupBox>
#include <QPropertyAnimation>



#include "gift.h"
#include "graphicsviewcontrols.h"
#include "QtColorWidgets/hue_slider.hpp"
#include "QtColorWidgets/gradient_slider.hpp"

using namespace color_widgets;

namespace Ui {
class coreFilters;
}

class coreFilters : public QWidget
{
    Q_OBJECT


public:
    explicit coreFilters(QWidget *parent = nullptr, Gift *gift = nullptr, graphicsViewControls *viewControl = nullptr);
    ~coreFilters();

public slots:
    void reset();
    void updateScope(QImage image);
    void apply();

private slots:
    void updateFilterChain();
    QString valueStr(QSlider *slider);
    QString getRgb();
    QString getGamma();
    QString getHue();
    QString getSharpness();
    int  getRgbSliderLabel(QSlider *slider);
    void on_resetButton_clicked();
    void connectSliders();
    void init_rgbSliders();
    void init_HueSliders();
    void init_basicSliders();
    void init_sharpenSliders();

    bool isRGBSlider(QSlider *slider);
    bool isSepiaSlider(QSlider *slider);
    QString getSepia();
    bool isThreshholdSlider(QSlider *slider);
    QString getThreshold();

    void animateWidget(QFrame *containerFrame, bool checked);
    QString getBlur();
    void init_noiseSliders();
    QString getNoiseFilter();
    QString getBrightness();
    QString getContrast();
    QString getSigmoidalContrast();
    QString getSaturation();
    QString getPixelate();
    QString getHueShift();
protected slots:
    bool eventFilter(QObject *watched, QEvent *event);

private:
    Ui::coreFilters *ui;
    Gift *gift = nullptr;
    graphicsViewControls *viewControls = nullptr;
    QString filterChain;
    QStack<QString> filterStack;
};

#endif // COREFILTERS_H
