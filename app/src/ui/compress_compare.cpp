#include "ui/compress_compare.h"

#include "ui/status_pill.h"
#include "ui/style.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kPictureHeight = 120;

QLabel* picture(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(u"comparePicture"_s);
    label->setFixedHeight(kPictureHeight);
    label->setMinimumWidth(1);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

// The middle of `image` that fills `size` in device pixels, one image pixel to one device pixel.
QPixmap atFullSize(const QImage& image, QSize size, double ratio)
{
    if (image.isNull()) {
        return {};
    }
    const QSize device = (QSizeF(size) * ratio).toSize().boundedTo(image.size());
    const QRect middle(QPoint((image.width() - device.width()) / 2, (image.height() - device.height()) / 2),
                       device);
    QPixmap pixmap = QPixmap::fromImage(image.copy(middle));
    pixmap.setDevicePixelRatio(ratio);
    return pixmap;
}

} // namespace

CompressCompare::CompressCompare(QWidget* parent)
    : QWidget(parent)
    , m_beforeText(new QLabel(this))
    , m_afterText(new QLabel(this))
    , m_beforePicture(picture(this))
    , m_afterPicture(picture(this))
    , m_zoom(new StatusPill(this))
{
    setObjectName(u"batchBar"_s);
    setAttribute(Qt::WA_StyledBackground);
    m_beforeText->setObjectName(u"muted"_s);
    m_afterText->setObjectName(u"muted"_s);
    m_zoom->setStatus(StatusPill::Kind::Plain, tr("100% zoom, same spot"));
    const auto heading = [this](const QString& title, QLabel* text, QWidget* trailing) {
        auto* row = new QHBoxLayout;
        row->setSpacing(kGapControls);
        auto* bold = new QLabel(title, this);
        bold->setObjectName(u"itemTitle"_s);
        row->addWidget(bold);
        row->addWidget(text, 1);
        if (trailing != nullptr) {
            row->addWidget(trailing);
        }
        return row;
    };
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(kPanelPadding, 10, kPanelPadding, 10);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    grid->addLayout(heading(tr("Before"), m_beforeText, nullptr), 0, 0);
    grid->addLayout(heading(tr("After"), m_afterText, m_zoom), 0, 1);
    grid->addWidget(m_beforePicture, 1, 0);
    grid->addWidget(m_afterPicture, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    hide();
}

CompressCompare::~CompressCompare() = default;

void CompressCompare::setPair(const QImage& before, const QString& beforeText, const QImage& after,
                              const QString& afterText)
{
    m_before = before;
    m_after = after;
    m_beforeText->setText(beforeText);
    m_afterText->setText(afterText);
    m_beforePicture->setAccessibleName(tr("Before, at 100%"));
    m_afterPicture->setAccessibleName(tr("After, at 100%"));
    place();
}

void CompressCompare::setWaiting(const QString& beforeText)
{
    m_beforeText->setText(beforeText);
    m_afterText->clear();
}

void CompressCompare::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    place();
}

void CompressCompare::place()
{
    const double ratio = devicePixelRatioF();
    m_beforePicture->setPixmap(atFullSize(m_before, m_beforePicture->size(), ratio));
    m_afterPicture->setPixmap(atFullSize(m_after, m_afterPicture->size(), ratio));
}

} // namespace pixelfx::ui
