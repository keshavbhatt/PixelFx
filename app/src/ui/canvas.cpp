#include "ui/canvas.h"

#include "ui/style.h"

#include <QPainter>

namespace pixelfx::ui {

namespace {
constexpr int kMargin = 28;
} // namespace

Canvas::Canvas(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAccessibleName(tr("Photo"));
}

void Canvas::setImage(const QImage& image, const QSize& fullSize)
{
    m_image = image;
    m_fullSize = fullSize.isValid() ? fullSize : image.size();
    m_status.clear();
    m_error.clear();
    update();
}

void Canvas::setStatus(const QString& text)
{
    m_status = text;
    m_error.clear();
    update();
}

void Canvas::setError(const QString& text)
{
    m_error = text;
    m_status.clear();
    update();
}

QRect Canvas::photoRect() const
{
    if (m_image.isNull()) {
        return {};
    }
    const QRect area = rect().adjusted(kMargin, kMargin, -kMargin, -kMargin);
    QSize size = m_image.size();
    size.scale(area.size(), Qt::KeepAspectRatio);
    if (size.width() > m_fullSize.width()) { // never enlarge past 100% of the photo at fit
        size = m_image.size().scaled(m_fullSize, Qt::KeepAspectRatio);
    }
    QRect target(QPoint(0, 0), size);
    target.moveCenter(area.center());
    return target;
}

void Canvas::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter painter(this);
    painter.fillRect(rect(), t.canvas);
    if (!m_image.isNull()) {
        const QRect target = photoRect();
        painter.fillRect(target.translated(0, 6).adjusted(-2, -2, 2, 2), QColor(0, 0, 0, 90));
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(target, m_image);
    }
    const QString& line = m_error.isEmpty() ? m_status : m_error;
    if (!line.isEmpty()) {
        painter.setPen(m_error.isEmpty() ? t.muted : t.danger);
        painter.drawText(rect().adjusted(kMargin, kMargin, -kMargin, -kMargin),
                         Qt::AlignCenter | Qt::TextWordWrap, line);
    }
}

} // namespace pixelfx::ui
