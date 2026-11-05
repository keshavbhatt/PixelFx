#include "ui/photo_tile.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QContextMenuEvent>
#include <QGridLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr double kRadius = 8.0;
constexpr int kStarChip = 22;

} // namespace

PhotoTile::PhotoTile(QString id, QString name, Badge badge, QWidget* parent)
    : QWidget(parent)
    , m_id(std::move(id))
    , m_name(std::move(name))
    , m_badge(badge)
{
    setFixedHeight(kHeight);
    setMinimumWidth(90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    refreshText();
}

PhotoTile::~PhotoTile() = default;

void PhotoTile::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void PhotoTile::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void PhotoTile::setBadge(Badge badge)
{
    m_badge = badge;
    refreshText();
    update();
}

void PhotoTile::setTwoLineName(bool on)
{
    m_twoLines = on;
    setFixedHeight(on ? kTwoLineHeight : kHeight);
    update();
}

void PhotoTile::setDescription(const QString& description)
{
    m_description = description;
    refreshText();
}

void PhotoTile::refreshText()
{
    //: A preset tile's name for screen readers, "Portrait film 400, in Favorites".
    setAccessibleName(m_badge == Badge::Favorite ? tr("%1, in Favorites").arg(m_name) : m_name);
    setAccessibleDescription(m_description);
    setToolTip(m_description.isEmpty() ? m_name : u"%1\n%2"_s.arg(m_name, m_description));
}

void PhotoTile::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath clip;
    clip.addRoundedRect(box, kRadius, kRadius);
    p.fillPath(clip, t.hover);
    p.save();
    p.setClipPath(clip);
    const QRectF imageRect(box.left(), box.top(), box.width(), kImageHeight);
    if (!m_image.isNull()) {
        // object-fit: cover
        const QSizeF scaled = QSizeF(m_image.size()).scaled(imageRect.size(), Qt::KeepAspectRatioByExpanding);
        const QRectF source((scaled.width() - imageRect.width()) / 2 * m_image.width() / scaled.width(),
                            (scaled.height() - imageRect.height()) / 2 * m_image.height() / scaled.height(),
                            imageRect.width() * m_image.width() / scaled.width(),
                            imageRect.height() * m_image.height() / scaled.height());
        p.drawImage(imageRect, m_image, source);
    }
    p.restore();
    QFont small = font();
    small.setPixelSize(12);
    p.setFont(small);
    p.setPen(t.text);
    const QFontMetrics metrics(small);
    const QRectF text(box.left() + 7, imageRect.bottom(), box.width() - 14,
                      box.bottom() - imageRect.bottom());
    if (m_twoLines) {
        // Wrapped onto two lines; a name longer still is cut at the end of the second.
        const QString shown = metrics.elidedText(m_name, Qt::ElideRight, qRound((2 * text.width()) - 24));
        p.drawText(text.adjusted(0, 4, 0, -4), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, shown);
    } else {
        p.drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                   metrics.elidedText(m_name, Qt::ElideRight, width() - 16));
    }
    paintBadge(p, box);
    if (m_selected || showsFocusRing(this)) {
        p.setPen(QPen(m_selected ? t.accent : t.muted, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, kRadius, kRadius);
    }
}

void PhotoTile::paintBadge(QPainter& p, const QRectF& box) const
{
    const Tokens& t = tokens();
    if (m_badge == Badge::Mine) {
        // The "Mine" pill, top right.
        QFont pill = font();
        pill.setPixelSize(11);
        pill.setWeight(QFont::DemiBold);
        const QString text = tr("Mine");
        const QFontMetrics fm(pill);
        const QRectF r(box.right() - fm.horizontalAdvance(text) - 17, box.top() + 5,
                       fm.horizontalAdvance(text) + 12, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(t.accentSoft);
        p.drawRoundedRect(r, 9, 9);
        p.setFont(pill);
        p.setPen(t.accent);
        p.drawText(r, Qt::AlignCenter, text);
    } else if (m_badge == Badge::Favorite) {
        // A yellow star on a dark chip: at least 3:1 over any photo (editor-presets-catalog review).
        const QRectF chip(box.right() - kStarChip - 5, box.top() + 5, kStarChip, kStarChip);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 140));
        p.drawRoundedRect(chip, 6, 6);
        const QPixmap star =
            icons::pixmap(QStringLiteral("star-on"), QColor(0xFF, 0xD4, 0x3B), 14, devicePixelRatioF());
        p.drawPixmap(QPointF(chip.center().x() - 7, chip.center().y() - 7), star);
    }
}

void PhotoTile::contextMenuEvent(QContextMenuEvent* event)
{
    Q_EMIT menuRequested(event->globalPos());
}

void PhotoTile::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked();
    }
}

void PhotoTile::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        Q_EMIT clicked();
        return;
    case Qt::Key_F:
        Q_EMIT favoriteRequested();
        return;
    case Qt::Key_Menu:
        Q_EMIT menuRequested(mapToGlobal(rect().center()));
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

void PhotoTile::enterEvent(QEnterEvent* /*event*/)
{
    Q_EMIT hovered(true);
}

void PhotoTile::leaveEvent(QEvent* /*event*/)
{
    Q_EMIT hovered(false);
}

QGridLayout* tileGrid(const std::vector<PhotoTile*>& tiles, QWidget* parent)
{
    auto* grid = new QGridLayout;
    grid->setContentsMargins(kToolRowPadding, 2, kToolRowPadding, 6);
    grid->setHorizontalSpacing(kGapControls);
    grid->setVerticalSpacing(kGapControls);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    int at = 0;
    for (PhotoTile* tile : tiles) {
        grid->addWidget(tile, at / 2, at % 2);
        ++at;
    }
    if (at % 2 == 1) {
        grid->addWidget(new QWidget(parent), at / 2, 1);
    }
    return grid;
}

} // namespace pixelfx::ui
