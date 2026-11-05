#include "ui/sheet.h"

#include "ui/style.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWindow>

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

void Sheet::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_scrim == nullptr && parentWidget() != nullptr) {
        m_scrim = new Scrim(parentWidget()->window());
    }
}

void Sheet::hideEvent(QHideEvent* event)
{
    delete m_scrim;
    QDialog::hideEvent(event);
}

} // namespace pixelfx::ui
