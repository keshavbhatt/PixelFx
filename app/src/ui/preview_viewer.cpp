#include "ui/preview_viewer.h"

#include "ui/canvas.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kMaxLongEdge = 4096; // enough for 1:1 on most screens, quick enough to wait for
constexpr int kNavSize = 40;
constexpr int kNavMargin = 72; // beside the photo, for the arrows

QToolButton* navButton(const QString& glyph, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(u"navButton"_s);
    b->setIcon(icons::themed(glyph, tokens().text));
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(kNavSize, kNavSize);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

} // namespace

PreviewViewer::PreviewViewer(int count, std::function<QString(int)> names, QString detail, Render render,
                             QWidget* parent)
    : QDialog(parent)
    , m_count(count)
    , m_names(std::move(names))
    , m_detail(std::move(detail))
    , m_render(std::move(render))
    , m_stage(new QWidget(this))
    , m_canvas(new Canvas(this))
{
    setWindowFlag(Qt::FramelessWindowHint);
    setWindowTitle(tr("Large preview"));
    setModal(true);
    connect(m_canvas, &Canvas::fullScreenRequested, this,
            [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    // With several photos, the arrows sit in margins beside the photo, never over it.
    m_stage->setObjectName(u"viewerStage"_s);
    m_stage->setAttribute(Qt::WA_StyledBackground);
    auto* stage = new QHBoxLayout(m_stage);
    const int side = count > 1 ? kNavMargin : 0;
    stage->setContentsMargins(side, 0, side, 0);
    stage->addWidget(m_canvas);
    auto* previous = navButton(u"chevron-left"_s, tr("Previous photo (Left)"), m_stage);
    auto* next = navButton(u"chevron-right"_s, tr("Next photo (Right)"), m_stage);
    connect(previous, &QToolButton::clicked, this, [this] { showPhoto(m_current - 1); });
    connect(next, &QToolButton::clicked, this, [this] { showPhoto(m_current + 1); });
    m_previous = previous;
    m_next = next;
    m_stage->installEventFilter(this);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(buildBar());
    column->addWidget(m_stage, 1);
    if (const QWidget* window = parent != nullptr ? parent->window() : nullptr) {
        resize(window->size()); // over the window it opens from
    }
}

PreviewViewer::~PreviewViewer() = default;

QWidget* PreviewViewer::buildBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(u"header"_s);
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(50);
    m_title = new QLabel(bar);
    m_title->setObjectName(u"docName"_s);
    m_subtitle = new QLabel(bar);
    m_subtitle->setObjectName(u"docMeta"_s);
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(m_title);
    text->addWidget(m_subtitle);
    m_views = new QButtonGroup(bar);
    QFrame* views =
        segmentBox({tr("Before"), tr("Split"), tr("After")}, static_cast<int>(View::After), m_views, bar);
    m_views->button(static_cast<int>(View::Before))->setToolTip(tr("The photo as it was (hold Space)"));
    m_views->button(static_cast<int>(View::Split))
        ->setToolTip(tr("Before and after side by side on a line you drag (\\)"));
    m_views->button(static_cast<int>(View::After))->setToolTip(tr("The photo as it will be"));
    connect(m_views, &QButtonGroup::idClicked, this, [this](int id) { setView(static_cast<View>(id)); });
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(kPanelPadding, 0, kToolRowPadding, 0);
    row->setSpacing(kGapControls);
    row->addLayout(text);
    row->addStretch();
    row->addWidget(views);
    row->addSpacing(kGapControls);
    row->addWidget(sheetCloseButton(this));
    return bar;
}

bool PreviewViewer::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_stage && event->type() == QEvent::Resize) {
        const int inset = (kNavMargin - kNavSize) / 2;
        const int y = (m_stage->height() - kNavSize) / 2;
        m_previous->move(inset, y);
        m_next->move(m_stage->width() - kNavSize - inset, y);
    }
    return QDialog::eventFilter(watched, event);
}

void PreviewViewer::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_current < 0) {
        showPhoto(0);
    }
}

void PreviewViewer::showPhoto(int index)
{
    if (index < 0 || index >= m_count) {
        return;
    }
    m_current = index;
    refreshTitle();
    m_previous->setVisible(m_count > 1);
    m_next->setVisible(m_count > 1);
    m_previous->setEnabled(index > 0);
    m_next->setEnabled(index < m_count - 1);
    m_canvas->setStatus(tr("Rendering..."));
    // Big enough for the screen at its pixel density, and for 1:1 on most photos.
    const QScreen* screen = this->screen() != nullptr ? this->screen() : QGuiApplication::primaryScreen();
    const QSize pixels = screen->size() * screen->devicePixelRatio();
    const int longEdge = std::min(kMaxLongEdge, std::max(pixels.width(), pixels.height()) * 2);
    const int generation = ++m_generation;
    auto* watcher = new QFutureWatcher<Photo>(this);
    connect(watcher, &QFutureWatcher<Photo>::finished, this, [this, watcher, generation, index] {
        const Photo photo = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) {
            return; // another photo was asked for meanwhile
        }
        m_photo = photo;
        m_canvas->setStatus(QString());
        if (photo.after.isNull()) {
            m_canvas->setError(tr("This photo could not be shown."));
            return;
        }
        m_canvas->setImage(photo.after, photo.fullSize);
        present();
        Q_EMIT photoShown(index);
    });
    watcher->setFuture(QtConcurrent::run(m_render, index, longEdge));
}

void PreviewViewer::refreshTitle()
{
    m_title->setText(m_names(m_current));
    QStringList parts{m_detail};
    if (m_count > 1) {
        parts << tr("%1 of %2").arg(m_current + 1).arg(m_count);
    }
    if (m_photo.fullSize.isValid()) {
        parts << tr("%1 x %2").arg(m_photo.fullSize.width()).arg(m_photo.fullSize.height());
    }
    parts.removeAll(QString());
    m_subtitle->setText(parts.join(u" \u00B7 "_s));
}

void PreviewViewer::setView(View view)
{
    m_view = view;
    m_views->button(static_cast<int>(view))->setChecked(true);
    present();
}

void PreviewViewer::present()
{
    refreshTitle();
    if (m_photo.after.isNull()) {
        return;
    }
    const View shown = m_holdingBefore ? View::Before : m_view;
    m_canvas->setOriginal(m_photo.before);
    m_canvas->setCompare(shown == View::Split);
    m_canvas->showImage(shown == View::Before ? m_photo.before : m_photo.after);
}

void PreviewViewer::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
        showPhoto(m_current - 1);
        return;
    case Qt::Key_Right:
        showPhoto(m_current + 1);
        return;
    case Qt::Key_Space:
        if (!event->isAutoRepeat()) {
            m_holdingBefore = true;
            present();
        }
        return;
    case Qt::Key_Backslash:
        setView(m_view == View::Split ? View::After : View::Split);
        return;
    case Qt::Key_F11:
        isFullScreen() ? showNormal() : showFullScreen();
        return;
    case Qt::Key_0:
    case Qt::Key_1:
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            event->key() == Qt::Key_0 ? m_canvas->zoomToFit() : m_canvas->setZoom(1.0);
            return;
        }
        break;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}

void PreviewViewer::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_holdingBefore = false;
        present();
        return;
    }
    QDialog::keyReleaseEvent(event);
}

} // namespace pixelfx::ui
