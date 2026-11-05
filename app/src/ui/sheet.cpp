#include "ui/sheet.h"

#include "ui/style.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
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
        setMinimumSize(980, 640);
        resize(980, 640);
        m_resizable = true;
        return;
    }
    fitHeight();
}

void Sheet::fitHeight()
{
    // Measured at the sheet's own width: text that wraps is as tall as it is at this width.
    QLayout* content = layout();
    if (content == nullptr) {
        return;
    }
    content->activate();
    const int height = content->hasHeightForWidth() ? content->totalHeightForWidth(width())
                                                    : content->totalSizeHint().height();
    resize(width(), std::max(height, content->totalMinimumSize().height()));
}

void Sheet::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(tokens().border, 1.0));
    painter.setBrush(tokens().panel);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kRadius - 0.5, kRadius - 0.5);
}

void Sheet::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && windowHandle() != nullptr && windowHandle()->startSystemMove()) {
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
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

} // namespace pixelfx::ui
