#pragma once

#include "core/histogram.h"

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
    void request(const pfx::Settings& settings);
    [[nodiscard]] bool isBusy() const { return m_running; }

Q_SIGNALS:
    /// The display image for `settings`, in sRGB.
    void rendered(const QImage& image, const pfx::Settings& settings,
                  const pixelfx::core::Histogram& histogram);
    void failed(const QString& message);

private:
    void startNext();

    std::shared_ptr<const pfx::Image> m_source;
    pfx::LensFacts m_lens;
    std::optional<pfx::Settings> m_pending;
    quint64 m_sourceGeneration = 0;
    bool m_running = false;
};

} // namespace pixelfx::services
