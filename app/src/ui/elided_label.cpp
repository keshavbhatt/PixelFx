#include "ui/elided_label.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

namespace pixelfx::ui {

ElidedLabel::ElidedLabel(QWidget* parent)
    : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
}

ElidedLabel::~ElidedLabel() = default;

QSize ElidedLabel::minimumSizeHint() const
{
    return {0, QLabel::minimumSizeHint().height()};
}

void ElidedLabel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
    const QRect area = contentsRect();
    const QString shown = fontMetrics().elidedText(text(), Qt::ElideRight, area.width());
    style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(), shown,
                          foregroundRole());
}

} // namespace pixelfx::ui
