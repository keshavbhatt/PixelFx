#pragma once

#include "core/histogram.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>

#include <memory>
#include <optional>
#include <pfx/image.h>
#include <pfx/lens.h>
#include <pfx/settings.h>

namespace pixelfx::services {

/// Renders a photo's preview with given settings on a worker thread (ADR-006). One render runs
/// at a time; while it runs, only the newest request waits, so dragging a slider never builds a
/// queue. A new source drops whatever was waiting and any result still in flight.
class RenderScheduler : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RenderScheduler)

public:
    explicit RenderScheduler(QObject* parent = nullptr);
    ~RenderScheduler() override;

    void setSource(std::shared_ptr<const pfx::Image> source);
    /// The photo's camera and lens, for its lens profile; kept until the next photo's.
    void setLens(const pfx::LensFacts& lens) { m_lens = lens; }
    /// `live`: a step of a change still being made, as while a slider or a curve point is dragged.
    /// Live steps in quick succession render from the source at half size, a quarter of the
    /// work, and the newest settings render at full size once the steps stop.
    void request(const pfx::Settings& settings, bool live = false);
    /// A render is running or waiting, or the newest picture is a half-size one still to be
    /// drawn at full size.
    [[nodiscard]] bool isBusy() const { return m_running || m_pending.has_value() || m_draftShown; }

Q_SIGNALS:
    /// The display image for `settings`, in sRGB.
    void rendered(const QImage& image, const pfx::Settings& settings,
                  const pixelfx::core::Histogram& histogram);
    void failed(const QString& message);

private:
    void startNext();

    std::shared_ptr<const pfx::Image> m_source;
    /// m_source at half size, made on the first draft render.
    std::shared_ptr<const pfx::Image> m_draftSource;
    pfx::LensFacts m_lens;
    std::optional<pfx::Settings> m_pending;
    bool m_pendingDraft = false;
    /// The newest settings, drawn at full size once the requests stop.
    pfx::Settings m_latest;
    bool m_draftShown = false;
    bool m_draftRunning = false;
    /// Since the last live step; invalid after any other request.
    QElapsedTimer m_sinceLive;
    class QTimer* m_settle;
    quint64 m_sourceGeneration = 0;
    bool m_running = false;
};

} // namespace pixelfx::services
