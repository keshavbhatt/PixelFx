#include "ui/sheet.h"

#include "ui/style.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

namespace pixelfx::ui {

namespace {

constexpr int kScrimAlpha = 115; // 45% black

// Dims the window under a sheet, following its size. Over another sheet it keeps that sheet's
// rounded corners.
class Scrim : public QWidget
{
public:
    explicit Scrim(QWidget* window)
        : QWidget(window)
        , m_rounded(qobject_cast<Sheet*>(window) != nullptr)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setGeometry(window->rect());
        window->installEventFilter(this);
        raise();
        show();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parentWidget() && event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const QColor shade(0, 0, 0, kScrimAlpha);
        if (!m_rounded) {
            painter.fillRect(rect(), shade);
            return;
        }
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), Sheet::kRadius, Sheet::kRadius);
        painter.fillPath(path, shade);
    }

private:
    bool m_rounded;
};

} // namespace

Sheet::Sheet(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setContentsMargins(1, 1, 1, 1); // the border stays clear of the content
}

Sheet::~Sheet() = default;

void Sheet::setSize(Size size)
{
    switch (size) {
    case Size::Narrow:
        setFixedWidth(440);
        break;
    case Size::Regular:
        setFixedWidth(560);
        break;
    case Size::Wide:
        setFixedWidth(820);
        break;
    case Size::Editor:
    case Size::Picker: {
        const QSize least = size == Size::Editor ? QSize(980, 640) : QSize(900, 640);
        setMinimumSize(least);
        resize(least);
        m_resizable = true;
        return;
    }
    }
    m_followsContent = true;
    fitHeight();
}

int Sheet::heightLimit() const
{
    constexpr int kMargin = 24; // the window shows above and below the sheet
    const QWidget* under = parentWidget() != nullptr ? parentWidget()->window() : nullptr;
    const QScreen* display = under != nullptr ? under->screen() : screen();
    int limit = display != nullptr ? display->availableGeometry().height() : QWIDGETSIZE_MAX;
    if (under != nullptr) {
        limit = std::min(limit, under->height());
    }
    return std::max(0, limit - (2 * kMargin));
}

void Sheet::fitHeight()
{
    // Measured at the sheet's own width: text that wraps is as tall as it is at this width.
    QLayout* content = layout();
    if (content == nullptr || m_fitting) {
        return;
    }
    m_fitting = true;
    content->activate();
    const int natural = content->hasHeightForWidth() ? content->totalHeightForWidth(width())
                                                     : content->totalSizeHint().height();
    const int height = std::max(std::min(natural, heightLimit()), content->totalMinimumSize().height());
    if (height != this->height()) {
        resize(width(), height);
        // An open sheet stays in the middle of its window as it grows or shrinks.
        if (isVisible() && parentWidget() != nullptr) {
            const QRect under = parentWidget()->window()->geometry();
            move(x(), under.top() + ((under.height() - height) / 2));
        }
    }
    m_fitting = false;
}

bool Sheet::event(QEvent* event)
{
    const bool handled = QDialog::event(event);
    if (event->type() == QEvent::LayoutRequest && m_followsContent) {
        fitHeight(); // a row shown or hidden, text that changed
    }
    return handled;
}

void Sheet::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(tokens().border, 1.0));
    painter.setBrush(tokens().panel);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius - 0.5, kRadius - 0.5);
}

// The sheet moves once a press on its background drags past the drag distance. A press alone
// starts nothing, so a click on a link in a label (which passes the press on) still reaches it.
void Sheet::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_movePress = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void Sheet::mouseMoveEvent(QMouseEvent* event)
{
    if (m_movePress && event->buttons().testFlag(Qt::LeftButton) &&
        (event->globalPosition().toPoint() - *m_movePress).manhattanLength() >=
            QApplication::startDragDistance()) {
        m_movePress.reset();
        if (windowHandle() != nullptr && windowHandle()->startSystemMove()) {
            event->accept();
            return;
        }
    }
    QDialog::mouseMoveEvent(event);
}

void Sheet::mouseReleaseEvent(QMouseEvent* event)
{
    m_movePress.reset();
    QDialog::mouseReleaseEvent(event);
}

Qt::Edges Sheet::edgesAt(const QPoint& global) const
{
    constexpr int kReach = 6;
    const QPoint at = mapFromGlobal(global);
    if (!rect().contains(at)) {
        return {};
    }
    Qt::Edges edges;
    edges.setFlag(Qt::LeftEdge, at.x() < kReach);
    edges.setFlag(Qt::RightEdge, at.x() >= width() - kReach);
    edges.setFlag(Qt::TopEdge, at.y() < kReach);
    edges.setFlag(Qt::BottomEdge, at.y() >= height() - kReach);
    return edges;
}

void Sheet::showResizeCursor(Qt::Edges edges)
{
    if (edges == m_cursorEdges) {
        return;
    }
    if (m_cursorEdges != Qt::Edges()) {
        QGuiApplication::restoreOverrideCursor();
    }
    m_cursorEdges = edges;
    if (edges == Qt::Edges()) {
        return;
    }
    const bool horizontal = edges.testAnyFlags(Qt::LeftEdge | Qt::RightEdge);
    const bool vertical = edges.testAnyFlags(Qt::TopEdge | Qt::BottomEdge);
    Qt::CursorShape shape = horizontal ? Qt::SizeHorCursor : Qt::SizeVerCursor;
    if (horizontal && vertical) {
        const bool falling =
            edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge);
        shape = falling ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
    }
    QGuiApplication::setOverrideCursor(shape);
}

bool Sheet::eventFilter(QObject* watched, QEvent* event)
{
    // The sheet's edges are covered by its content, so the pointer is watched over all of it.
    const QEvent::Type type = event->type();
    if (m_resizable && (type == QEvent::MouseMove || type == QEvent::MouseButtonPress) &&
        watched->isWidgetType() && static_cast<QWidget*>(watched)->window() == this) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        const Qt::Edges edges = edgesAt(mouse->globalPosition().toPoint());
        if (type == QEvent::MouseMove && mouse->buttons() == Qt::NoButton) {
            showResizeCursor(edges);
        } else if (type == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton &&
                   edges != Qt::Edges() && windowHandle() != nullptr &&
                   windowHandle()->startSystemResize(edges)) {
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void Sheet::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_resizable) {
        qApp->installEventFilter(this);
    }
    if (m_scrim == nullptr && parentWidget() != nullptr) {
        m_scrim = new Scrim(parentWidget()->window());
    }
}

void Sheet::hideEvent(QHideEvent* event)
{
    if (m_resizable) {
        qApp->removeEventFilter(this);
        showResizeCursor({});
    }
    delete m_scrim;
    QDialog::hideEvent(event);
}

SheetScroll::SheetScroll(QWidget* content, int edge, QWidget* parent)
    : QScrollArea(parent)
{
    setObjectName(QStringLiteral("sheetScroll"));
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);
    setWidget(content);
    viewport()->setAutoFillBackground(false); // the sheet's own panel shows through
    content->setAutoFillBackground(false);
    content->installEventFilter(this);
    if (QLayout* layout = content->layout()) {
        keepScrollBarInPadding(this, layout, edge);
    }
}

SheetScroll::~SheetScroll() = default;

int SheetScroll::heightForWidth(int width) const
{
    const QWidget* content = widget();
    if (content == nullptr) {
        return 0;
    }
    return content->hasHeightForWidth() ? content->heightForWidth(width) : content->sizeHint().height();
}

QSize SheetScroll::sizeHint() const
{
    const int width = widget() != nullptr ? widget()->sizeHint().width() : 0;
    return {width, heightForWidth(this->width() > 0 ? this->width() : width)};
}

QSize SheetScroll::minimumSizeHint() const
{
    constexpr int kLeast = 120; // a few rows, when the window is very short
    return {0, std::min(kLeast, sizeHint().height())};
}

bool SheetScroll::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == widget() && event->type() == QEvent::LayoutRequest) {
        updateGeometry(); // the content changed: the sheet fits it again
    }
    return QScrollArea::eventFilter(watched, event);
}

void addScrollingContent(QVBoxLayout* column, QLayout* heading, QLayout* content, QWidget* sheet)
{
    const QMargins padding = column->contentsMargins();
    column->setContentsMargins(padding.left(), padding.top(), 0, padding.bottom());
    heading->setContentsMargins(0, 0, padding.right(), 0);
    column->addLayout(heading);
    auto* body = new QWidget(sheet);
    body->setLayout(content);
    content->setContentsMargins(0, 0, 0, 0);
    column->addWidget(new SheetScroll(body, padding.right(), sheet), 1);
}

} // namespace pixelfx::ui
