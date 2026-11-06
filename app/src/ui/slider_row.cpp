#include "ui/slider_row.h"

#include "ui/panel_slider.h"
#include "ui/style.h"

#include <QEvent>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

SliderRow::SliderRow(const QString& label, QWidget* parent)
    : QWidget(parent)
    , m_label(label)
    , m_format([](int ticks) { return QString::number(ticks); })
    , m_slider(new PanelSlider(Qt::Horizontal, this))
    , m_name(new QLabel(label, this))
    , m_value(new QLabel(this))
    , m_field(new QLineEdit(this))
{
    m_slider->setAccessibleName(label);
    m_slider->setToolTip(tr("%1. Double-click to reset.").arg(label));
    m_slider->installEventFilter(this);
    m_name->installEventFilter(this); // a double-click on the name resets too
    m_name->setObjectName(u"adjustName"_s);
    m_name->setBuddy(m_slider);
    m_value->setObjectName(u"adjustValue"_s);
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_value->setMinimumWidth(44);
    m_value->setCursor(Qt::IBeamCursor);
    m_value->setToolTip(tr("Click to type a value"));
    m_value->installEventFilter(this);
    m_field->setObjectName(u"adjustValueField"_s);
    m_field->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_field->setFixedWidth(56);
    m_field->setAccessibleName(label);
    m_field->setToolTip(tr("Enter keeps the value, Escape goes back, Up and Down nudge it"));
    m_field->installEventFilter(this);
    m_field->hide();
    QFont figures = m_value->font();
    figures.setFeature(QFont::Tag("tnum"), 1); // digits of one width, so the number holds still
    m_value->setFont(figures);
    m_field->setFont(figures);
    auto* top = new QHBoxLayout;
    top->setSpacing(kGapControls);
    top->setContentsMargins(0, 0, 0, 0);
    top->addWidget(m_name);
    top->addStretch();
    top->addWidget(m_value);
    top->addWidget(m_field);
    auto* column = new QVBoxLayout(this);
    // The track 24 px under the name's top; the value's 1 px hover border fills the top pixel.
    column->setContentsMargins(0, 6, 0, 8);
    column->setSpacing(1);
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
    connect(m_field, &QLineEdit::editingFinished, this, [this] { endTyping(true); });
    refresh();
}

SliderRow::~SliderRow() = default;

void SliderRow::setFullName(const QString& name)
{
    m_label = name;
    m_slider->setAccessibleName(name);
    m_slider->setToolTip(tr("%1. Double-click to reset.").arg(name));
    m_field->setAccessibleName(name);
}

void SliderRow::setHelp(const QString& text)
{
    m_slider->setToolTip(tr("%1. Double-click to reset.").arg(text));
}

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

void SliderRow::setTicksPerUnit(double ticks)
{
    m_ticksPerUnit = ticks;
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

std::optional<int> SliderRow::parse(const QString& typed) const
{
    // The unit after the number ("4%", "12\u00B0") and a leading plus are allowed.
    QString number = typed.trimmed();
    while (!number.isEmpty() && !number.back().isDigit() && number.back() != u'.' && number.back() != u',') {
        number.chop(1);
    }
    if (number.startsWith(u'+')) {
        number.remove(0, 1);
    }
    bool ok = false;
    double value = locale().toDouble(number, &ok);
    if (!ok) {
        value = QLocale::c().toDouble(number, &ok);
    }
    if (m_typedUnit && ok) {
        value = m_typedUnit(value);
    }
    if (!ok || !std::isfinite(value)) {
        return std::nullopt;
    }
    const double ticks = std::round(value * m_ticksPerUnit);
    return static_cast<int>(std::clamp(ticks, static_cast<double>(m_slider->minimum()),
                                       static_cast<double>(m_slider->maximum())));
}

void SliderRow::beginTyping()
{
    if (!m_field->isHidden()) {
        return;
    }
    m_field->setText(text());
    m_field->setFixedHeight(m_value->height()); // the row keeps its height
    m_value->hide();
    m_field->show();
    m_field->setFocus(Qt::OtherFocusReason);
    m_field->selectAll();
}

void SliderRow::endTyping(bool keep)
{
    if (m_field->isHidden()) {
        return;
    }
    if (keep) {
        if (const std::optional<int> typed = parse(m_field->text())) {
            m_slider->setValue(*typed); // emits moved and released when it changes
        }
    }
    const QSignalBlocker block(m_field); // hiding takes the focus away, which would finish again
    const bool hadFocus = m_field->hasFocus();
    m_field->hide();
    m_value->show();
    if (hadFocus) {
        m_slider->setFocus(Qt::OtherFocusReason);
    }
}

bool SliderRow::fieldEvent(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool nudge = key->key() == Qt::Key_Up || key->key() == Qt::Key_Down;
        if (key->key() != Qt::Key_Escape && !nudge) {
            return false;
        }
        event->accept(); // the field takes these keys before any shortcut does
        if (event->type() == QEvent::KeyPress) {
            if (nudge) {
                const int steps = (key->key() == Qt::Key_Up ? 1 : -1) *
                                  (key->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1);
                const int base = parse(m_field->text()).value_or(m_slider->value());
                m_slider->setValue(base + (steps * m_slider->singleStep()));
                m_field->setText(text());
                m_field->selectAll();
            } else {
                endTyping(false);
            }
        }
        return true;
    }
    return false;
}

void SliderRow::setFlush()
{
    // The name level with the field's top; under the track as much room as a field's own box.
    layout()->setContentsMargins(0, 0, 0, kGapRelated);
}

void SliderRow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange) {
        constexpr double kOffOpacity = 0.45;
        QGraphicsOpacityEffect* dim = nullptr;
        if (!isEnabled()) {
            dim = new QGraphicsOpacityEffect(this);
            dim->setOpacity(kOffOpacity);
        }
        setGraphicsEffect(dim); // deletes the one before
    }
    QWidget::changeEvent(event);
}

bool SliderRow::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_slider || watched == m_name) && event->type() == QEvent::MouseButtonDblClick) {
        m_slider->setValue(m_default); // emits moved and released
        return true;
    }
    if (watched == m_value && event->type() == QEvent::MouseButtonPress &&
        static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        beginTyping();
        return true;
    }
    if (watched == m_slider && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            beginTyping();
            return true;
        }
    }
    if (watched == m_field && fieldEvent(event)) {
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace pixelfx::ui
