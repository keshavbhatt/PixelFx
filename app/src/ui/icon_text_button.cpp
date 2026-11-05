#include "ui/icon_text_button.h"

#include <QStyleOptionButton>
#include <QStylePainter>

namespace pixelfx::ui {

namespace {
constexpr int kQtGap = 4; // what Qt leaves between a push button's icon and text
} // namespace

IconTextButton::IconTextButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QPushButton(icon, text, parent)
{}

IconTextButton::~IconTextButton() = default;

bool IconTextButton::hasIconAndText() const
{
    return !icon().isNull() && !text().isEmpty();
}

int IconTextButton::extraWidth() const
{
    return hasIconAndText() ? kGap - kQtGap : 0;
}

QSize IconTextButton::sizeHint() const
{
    return QPushButton::sizeHint() + QSize(extraWidth(), 0);
}

QSize IconTextButton::minimumSizeHint() const
{
    return QPushButton::minimumSizeHint() + QSize(extraWidth(), 0);
}

void IconTextButton::paintEvent(QPaintEvent* event)
{
    if (!hasIconAndText()) {
        QPushButton::paintEvent(event);
        return;
    }
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);
    QStyleOptionButton frame = option;
    frame.text.clear();
    frame.icon = QIcon();
    painter.drawControl(QStyle::CE_PushButton, frame);

    // The style draws the text, centred, in the colours of the button's state; it moves right by
    // half the icon and gap so that icon, gap and text together sit in the middle.
    const QRect area = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
    const QSize iconSize = option.iconSize;
    const int shift = (iconSize.width() + kGap) / 2;
    const bool rtl = layoutDirection() == Qt::RightToLeft;
    QStyleOptionButton label = option;
    label.icon = QIcon();
    painter.save();
    painter.translate(rtl ? -shift : shift, 0);
    painter.drawControl(QStyle::CE_PushButtonLabel, label);
    painter.restore();

    const int textWidth = fontMetrics().horizontalAdvance(option.text);
    const int left = area.center().x() + 1 - ((iconSize.width() + kGap + textWidth) / 2);
    const QRect iconRect(left, area.y() + ((area.height() - iconSize.height()) / 2), iconSize.width(),
                         iconSize.height());
    QIcon::Mode mode = QIcon::Normal;
    if (!isEnabled()) {
        mode = QIcon::Disabled;
    } else if (hasFocus()) {
        mode = QIcon::Active;
    }
    icon().paint(&painter, QStyle::visualRect(layoutDirection(), area, iconRect), Qt::AlignCenter, mode,
                 isChecked() ? QIcon::On : QIcon::Off);
}

} // namespace pixelfx::ui
