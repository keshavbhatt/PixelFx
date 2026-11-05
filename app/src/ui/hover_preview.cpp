#include "ui/hover_preview.h"

#include "ui/photo_tile.h"

#include <QTimer>

namespace pixelfx::ui {

HoverPreview::HoverPreview(QObject* parent)
    : QObject(parent)
    , m_tick(new QTimer(this))
    , m_leave(new QTimer(this))
{
    m_tick->setInterval(16);
    connect(m_tick, &QTimer::timeout, this, &HoverPreview::tick);
    m_leave->setSingleShot(true);
    m_leave->setInterval(kNext);
    connect(m_leave, &QTimer::timeout, this, [this] {
        m_previewing = false;
        Q_EMIT ended();
    });
}

HoverPreview::~HoverPreview() = default;

void HoverPreview::hover(PhotoTile* tile, int index, bool inside)
{
    if (m_tile != nullptr) {
        m_tile->setWaiting(-1.0);
    }
    m_tick->stop();
    if (!inside) {
        m_tile = nullptr;
        if (m_previewing) {
            m_leave->start();
        }
        return;
    }
    m_leave->stop();
    m_tile = tile;
    m_index = index;
    m_delay = m_previewing ? kNext : kWait;
    m_clock.start();
    tile->setWaiting(0.0);
    m_tick->start();
}

void HoverPreview::tick()
{
    if (m_tile == nullptr) {
        m_tick->stop();
        return;
    }
    const double progress = static_cast<double>(m_clock.elapsed()) / m_delay;
    if (progress < 1.0) {
        m_tile->setWaiting(progress);
        return;
    }
    m_tick->stop();
    m_tile->setWaiting(-1.0);
    m_previewing = true;
    Q_EMIT started(m_index);
}

void HoverPreview::end()
{
    m_tick->stop();
    m_leave->stop();
    if (m_tile != nullptr) {
        m_tile->setWaiting(-1.0);
    }
    m_tile = nullptr;
    if (m_previewing) {
        m_previewing = false;
        Q_EMIT ended();
    }
}

} // namespace pixelfx::ui
