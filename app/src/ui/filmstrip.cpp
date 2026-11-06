#include "ui/filmstrip.h"

#include "ui/icons.h"
#include "ui/style.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

Filmstrip::Filmstrip(QWidget* parent)
    : QWidget(parent)
    , m_scroll(new QScrollArea(this))
    , m_row(new QWidget(m_scroll))
    , m_tiles(new QHBoxLayout(m_row))
    , m_info(new QLabel(this))
    , m_selectionBar(new QWidget(this))
{
    setObjectName(u"strip"_s);
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(kHeight);
    m_row->setObjectName(u"stripRow"_s);
    m_tiles->setContentsMargins(0, 0, 0, 0);
    m_tiles->setSpacing(kGapControls);
    m_scroll->setWidget(m_row);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setFixedHeight(FilmTile::kHeight + 14);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_row->setAutoFillBackground(false);
    m_info->setObjectName(u"muted"_s);

    // "4 selected · Select all · Clear".
    const auto link = [this](const QString& text) {
        auto* b = new QPushButton(text, m_selectionBar);
        b->setObjectName(u"link"_s);
        b->setCursor(Qt::PointingHandCursor);
        return b;
    };
    auto* all = link(tr("Select all"));
    auto* clear = link(tr("Clear"));
    connect(all, &QPushButton::clicked, this, &Filmstrip::selectAll);
    connect(clear, &QPushButton::clicked, this, &Filmstrip::clearSelection);
    auto* bar = new QHBoxLayout(m_selectionBar);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(kGapTight);
    bar->addWidget(all);
    bar->addWidget(new QLabel(u"·"_s, m_selectionBar));
    bar->addWidget(clear);
    m_selectionBar->hide();

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kToolRowPadding, 0, kToolRowPadding, 0);
    layout->setSpacing(kGapControls);
    layout->addWidget(m_scroll, 1);
    layout->addWidget(m_info);
    layout->addWidget(m_selectionBar);
}

Filmstrip::~Filmstrip() = default;

QList<FilmTile*> Filmstrip::tiles() const
{
    return m_row->findChildren<FilmTile*>();
}

void Filmstrip::setFiles(const QStringList& files)
{
    m_files = files;
    for (auto it = m_thumbnails.begin(); it != m_thumbnails.end();) {
        it = files.contains(it.key()) ? std::next(it) : m_thumbnails.erase(it);
    }
    m_selected.clear();
    m_current = files.isEmpty() ? -1 : std::clamp(m_current, 0, static_cast<int>(files.size()) - 1);
    rebuild();
}

void Filmstrip::rebuild()
{
    while (const QLayoutItem* item = m_tiles->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (int i = 0; i < m_files.size(); ++i) {
        auto* tile = new FilmTile(i, m_files.at(i), m_row);
        connect(tile, &FilmTile::clicked, this, [this, i](Qt::KeyboardModifiers modifiers) {
            if (modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::ShiftModifier)) {
                toggle(i, modifiers.testFlag(Qt::ShiftModifier));
            } else {
                Q_EMIT currentRequested(i);
            }
        });
        connect(tile, &FilmTile::closeClicked, this, [this, i] { Q_EMIT closeRequested({i}); });
        connect(tile, &FilmTile::menuRequested, this, [this, i](const QPoint& at) {
            QMenu* menu = tileMenu(i);
            menu->exec(at);
            menu->deleteLater();
        });
        if (const auto it = m_thumbnails.constFind(m_files.at(i)); it != m_thumbnails.constEnd()) {
            tile->setThumbnail(it->image, it->edited);
        }
        m_tiles->addWidget(tile);
    }
    auto* add = new QToolButton(m_row);
    add->setObjectName(u"stripAdd"_s);
    add->setIcon(icons::themed(u"plus"_s, tokens().muted));
    add->setIconSize(QSize(20, 20));
    add->setFixedSize(64, 64);
    add->setToolTip(tr("Add images (Ctrl+O)"));
    add->setAccessibleName(tr("Add images"));
    connect(add, &QToolButton::clicked, this, &Filmstrip::addRequested);
    m_tiles->addWidget(add);
    m_tiles->addStretch();
    refresh();
}

void Filmstrip::setCurrent(int index)
{
    m_current = index;
    m_anchor = index;
    refresh();
    for (FilmTile* t : tiles()) {
        if (t->index() == index) {
            m_scroll->ensureWidgetVisible(t, 40, 0);
        }
    }
}

QMenu* Filmstrip::tileMenu(int index)
{
    // A tile of the selection closes the selection, any other just itself.
    const QList<int> selected = selection();
    const QList<int> these = selected.contains(index) ? selected : QList<int>{index};
    auto* menu = new QMenu(this);
    const QIcon close = icons::themed(u"close"_s, tokens().muted);
    QAction* one = menu->addAction(close, these.size() > 1
                                              ? tr("Close %n photos", nullptr, static_cast<int>(these.size()))
                                              : tr("Close photo"));
    if (these == QList<int>{m_current}) {
        one->setShortcut(QKeySequence(u"Ctrl+W"_s)); // shown; the window's own Close photo does the work
        one->setShortcutContext(Qt::WidgetShortcut);
    }
    connect(one, &QAction::triggered, this, [this, these] { Q_EMIT closeRequested(these); });
    const QAction* all = menu->addAction(icons::themed(u"images"_s, tokens().muted), tr("Close all photos"));
    connect(all, &QAction::triggered, this, [this] {
        QList<int> every;
        for (int i = 0; i < m_files.size(); ++i) {
            every << i;
        }
        Q_EMIT closeRequested(every);
    });
    return menu;
}

void Filmstrip::setThumbnail(const QString& path, const QImage& image, bool edited)
{
    if (m_files.contains(path)) {
        m_thumbnails.insert(path, {image, edited});
    }
    for (FilmTile* t : tiles()) {
        if (t->path() == path) {
            t->setThumbnail(image, edited);
        }
    }
}

QList<int> Filmstrip::selection() const
{
    if (m_selected.isEmpty()) {
        return {};
    }
    QSet<int> all = m_selected;
    if (m_current >= 0) {
        all.insert(m_current);
    }
    QList<int> out(all.begin(), all.end());
    std::ranges::sort(out);
    return out;
}

void Filmstrip::toggle(int index, bool range)
{
    if (range && m_anchor >= 0) {
        for (int i = std::min(m_anchor, index); i <= std::max(m_anchor, index); ++i) {
            m_selected.insert(i);
        }
    } else if (m_selected.contains(index) && index != m_current) {
        m_selected.remove(index);
    } else {
        m_selected.insert(index);
        m_anchor = index;
    }
    if (m_selected.size() == 1 && m_selected.contains(m_current)) {
        m_selected.clear(); // only the current photo: no selection
    }
    refresh();
    Q_EMIT selectionChanged(static_cast<int>(selection().size()));
}

void Filmstrip::selectAll()
{
    for (int i = 0; i < m_files.size(); ++i) {
        m_selected.insert(i);
    }
    if (m_files.size() < 2) {
        m_selected.clear();
    }
    refresh();
    Q_EMIT selectionChanged(static_cast<int>(selection().size()));
}

void Filmstrip::clearSelection()
{
    m_selected.clear();
    refresh();
    Q_EMIT selectionChanged(0);
}

void Filmstrip::refresh()
{
    const QList<int> selected = selection();
    const bool ticks = !selected.isEmpty();
    for (FilmTile* t : tiles()) {
        t->setState(t->index() == m_current, selected.contains(t->index()), ticks);
    }
    if (ticks) {
        m_info->setText(tr("%1 selected ·").arg(selected.size()));
    } else {
        m_info->setText(m_current >= 0 ? tr("%1 of %2").arg(m_current + 1).arg(m_files.size()) : QString());
    }
    m_selectionBar->setVisible(ticks);
}

// ---------------------------------------------------------------------------------------------

FilmTile::FilmTile(int index, const QString& path, QWidget* parent)
    : QWidget(parent)
    , m_index(index)
    , m_path(path)
    , m_close(new QToolButton(this))
{
    setFixedSize(kWidth, kHeight);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QFileInfo(path).fileName());
    setAccessibleName(QFileInfo(path).fileName());
    // A 20 px circle in a 24 px target, 2 px in from the top right corner.
    constexpr int kTarget = 24;
    m_close->setObjectName(u"tileClose"_s);
    m_close->setIcon(icons::themed(u"close"_s, Qt::white));
    m_close->setIconSize(QSize(12, 12));
    m_close->setFixedSize(kTarget, kTarget);
    m_close->move(kWidth - kTarget - 2, 2);
    m_close->setCursor(Qt::PointingHandCursor);
    //: The × on a filmstrip tile; "Close harbour.jpg".
    m_close->setAccessibleName(tr("Close %1").arg(QFileInfo(path).fileName()));
    m_close->setToolTip(tr("Close photo"));
    m_close->setFocusPolicy(Qt::NoFocus); // the keyboard closes with Ctrl+W
    m_close->hide();
    connect(m_close, &QToolButton::clicked, this, &FilmTile::closeClicked);
}

FilmTile::~FilmTile() = default;

void FilmTile::setThumbnail(const QImage& image, bool edited)
{
    m_image = image;
    m_edited = edited;
    update();
}

void FilmTile::setState(bool current, bool selected, bool ticks)
{
    m_close->setToolTip(current ? tr("Close photo (Ctrl+W)") : tr("Close photo"));
    m_current = current;
    m_selected = selected;
    m_ticks = ticks;
    update();
}

void FilmTile::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath clip;
    clip.addRoundedRect(box, 6, 6);
    p.save();
    p.setClipPath(clip);
    p.fillRect(box, t.input);
    if (!m_image.isNull()) {
        const QSizeF source = QSizeF(box.size()).scaled(m_image.size(), Qt::KeepAspectRatio);
        p.setOpacity(m_selected && !m_current ? 0.92 : 1.0);
        p.drawImage(box, m_image,
                    QRectF((m_image.width() - source.width()) / 2, (m_image.height() - source.height()) / 2,
                           source.width(), source.height()));
        p.setOpacity(1.0);
    }
    p.restore();
    if (m_current || m_selected) {
        p.setPen(QPen(t.accent, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 6, 6);
    }
    if (m_edited) {
        const QRectF mark(width() - 21, height() - 21, 16, 16);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 153));
        p.drawRoundedRect(mark, 4, 4);
        p.drawPixmap(mark.adjusted(2, 2, -2, -2).toRect(),
                     icons::pixmap(u"sliders"_s, Qt::white, 12, devicePixelRatioF()));
    }
    if (m_ticks) { // selection ticks, top left
        const QRectF tick(5, 5, 16, 16);
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(m_selected ? t.accentStrong : QColor(0, 0, 0, 120));
        p.drawRoundedRect(tick, 4, 4);
        if (m_selected) {
            p.setPen(QPen(Qt::white, 2));
            p.drawPolyline(QPolygonF({tick.topLeft() + QPointF(4, 8), tick.topLeft() + QPointF(7, 11),
                                      tick.topLeft() + QPointF(12, 5)}));
        }
    }
}

void FilmTile::enterEvent(QEnterEvent* event)
{
    m_close->show();
    QWidget::enterEvent(event);
}

void FilmTile::leaveEvent(QEvent* event)
{
    m_close->hide();
    QWidget::leaveEvent(event);
}

void FilmTile::contextMenuEvent(QContextMenuEvent* event)
{
    Q_EMIT menuRequested(event->globalPos());
}

void FilmTile::mousePressEvent(QMouseEvent* event)
{
    // The press is taken here, where the click is: passed on, a sheet would start moving its
    // window and the release would never come back (a click on a tile in a sheet did nothing).
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FilmTile::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked(event->modifiers());
    }
}

} // namespace pixelfx::ui
