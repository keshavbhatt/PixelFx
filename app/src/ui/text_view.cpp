#include "ui/text_view.h"

#include <QEvent>
#include <QTextDocument>

#include <algorithm>
#include <cmath>
#include <memory>

namespace pixelfx::ui {

TextView::TextView(int maximumTextHeight, QWidget* parent)
    : QTextBrowser(parent)
    , m_maximum(maximumTextHeight)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // The layout keeps the height it last measured until told otherwise: new text is a new height.
    connect(document(), &QTextDocument::contentsChanged, this, [this] { updateGeometry(); });
}

TextView::~TextView() = default;

bool TextView::hasHeightForWidth() const
{
    return true;
}

int TextView::heightForWidth(int width) const
{
    const QMargins frame = contentsMargins();
    const std::unique_ptr<QTextDocument> laidOut(document()->clone());
    laidOut->setTextWidth(width - frame.left() - frame.right());
    const int text = static_cast<int>(std::ceil(laidOut->size().height()));
    return std::min(text, m_maximum) + frame.top() + frame.bottom();
}

void TextView::changeEvent(QEvent* event)
{
    // A style sheet or font applied after the text was measured changes its height too.
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
    }
    QTextBrowser::changeEvent(event);
}

QSize TextView::sizeHint() const
{
    const int width = QTextBrowser::sizeHint().width();
    return {width, heightForWidth(width)};
}

} // namespace pixelfx::ui
