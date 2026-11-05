#include "ui/slider_row.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

SliderRow::SliderRow(const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_label(label)
    , m_format([](int ticks) { return QString::number(ticks); })
    , m_slider(new QSlider(Qt::Horizontal, this))
    , m_name(new QLabel(label, this))
    , m_value(new QLabel(this))
{
    m_slider->setAccessibleName(label);
    m_slider->setToolTip(tr("%1. Double-click to reset.").arg(label));
    m_slider->installEventFilter(this);
    m_name->setObjectName(u"adjustName"_s);
    m_name->setBuddy(m_slider);
    m_value->setObjectName(u"adjustValue"_s);
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_value->setMinimumWidth(44);
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_name);
    top->addStretch();
    top->addWidget(m_value);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 7, 0, 7);
    column->setSpacing(4);
    column->addLayout(top);
    column->addWidget(m_slider);
    connect(m_slider, &QSlider::valueChanged, this, [this](int ticks) {
        refresh();
        Q_EMIT ticksMoved(ticks);
        if (!m_slider->isSliderDown()) { // keyboard and wheel changes are complete at once
            Q_EMIT ticksReleased(ticks);
        }
    });
    connect(m_slider, &QSlider::sliderReleased, this, [this] { Q_EMIT ticksReleased(m_slider->value()); });
    refresh();
}

SliderRow::~SliderRow() = default;

void SliderRow::setRange(int minimum, int maximum, int defaultTicks)
{
    const QSignalBlocker block(m_slider);
    m_default = defaultTicks;
    m_slider->setRange(minimum, maximum);
    refresh();
}

void SliderRow::setFormat(std::function<QString(int)> format)
{
    m_format = std::move(format);
    refresh();
}

int SliderRow::ticks() const
{
    return m_slider->value();
}

void SliderRow::setTicks(int ticks)
{
    const QSignalBlocker block(m_slider);
    m_slider->setValue(ticks);
    refresh();
}

bool SliderRow::isChanged() const
{
    return m_slider->value() != m_default;
}

QString SliderRow::text() const
{
    return m_format(m_slider->value());
}

void SliderRow::refresh()
{
    m_value->setText(text());
    const bool changed = isChanged();
    if (m_value->property("changed").toBool() != changed) {
        for (QLabel* label : {m_name, m_value}) {
            label->setProperty("changed", changed);
            label->style()->unpolish(label);
            label->style()->polish(label);
        }
    }
}

bool SliderRow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_slider && event->type() == QEvent::MouseButtonDblClick) {
        m_slider->setValue(m_default); // emits moved and released
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace pixelfx::ui
