#include "services/render_scheduler.h"

#include "core/looks.h"
#include "services/qt_image_bridge.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/image.h>
#include <pfx/io.h>
#include <pfx/pipeline.h>

namespace pixelfx::services {

namespace {

/// Live steps closer together than this are a drag.
constexpr int kDragGapMs = 150;
/// How long after the last request of a drag the full-size render starts.
constexpr int kSettleMs = 120;

struct Job
{
    quint64 generation = 0;
    pfx::Settings settings;
    pfx::LensFacts lens;
    bool draft = false;
    /// The half-size source: given when known, made by the job otherwise, and handed back.
    std::shared_ptr<const pfx::Image> draftSource;
    QImage image;
    core::Histogram histogram;
    QString error;
};

Job renderJob(const std::shared_ptr<const pfx::Image>& source,
              Job job) // NOLINT(performance-unnecessary-value-param)
{
    pfx::RenderOptions options;
    options.lens = &job.lens;
    if (job.draft && !job.draftSource) {
        job.draftSource = std::make_shared<const pfx::Image>(pfx::downscale(*source, 2));
    }
    const pfx::Image& input = job.draft ? *job.draftSource : *source;
    const auto rendered = core::renderPhoto(input, job.settings, options);
    if (!rendered) {
        job.error = QString::fromStdString(rendered.error().message);
        return job;
    }
    const auto display = pfx::toSrgb8(rendered.value());
    if (!display) {
        job.error = QString::fromStdString(display.error().message);
        return job;
    }
    job.image = toQImage(display.value());
    job.histogram = core::histogramOf(job.image);
    return job;
}

} // namespace

RenderScheduler::RenderScheduler(QObject* parent)
    : QObject(parent)
    , m_settle(new QTimer(this))
{
    m_settle->setSingleShot(true);
    m_settle->setInterval(kSettleMs);
    connect(m_settle, &QTimer::timeout, this, [this] {
        if (!m_draftShown && !m_draftRunning && !(m_pending && m_pendingDraft)) {
            return; // the newest picture is full size already
        }
        m_pending = m_latest;
        m_pendingDraft = false;
        if (!m_running) {
            startNext();
        }
    });
}

RenderScheduler::~RenderScheduler() = default;

void RenderScheduler::setSource(std::shared_ptr<const pfx::Image> source)
{
    m_source = std::move(source);
    m_draftSource.reset();
    ++m_sourceGeneration;
    m_pending.reset();
    m_settle->stop();
    m_draftShown = false;
}

void RenderScheduler::request(const pfx::Settings& settings, bool live)
{
    if (!m_source) {
        return;
    }
    const bool dragging = live && m_sinceLive.isValid() && m_sinceLive.elapsed() < kDragGapMs;
    if (live) {
        m_sinceLive.restart();
    } else {
        m_sinceLive.invalidate();
        m_settle->stop();
        m_draftShown = false; // this full-size render replaces any half-size picture
    }
    m_pending = settings;
    m_pendingDraft = dragging;
    m_latest = settings;
    if (dragging) {
        m_settle->start();
    }
    if (!m_running) {
        startNext();
    }
}

void RenderScheduler::startNext()
{
    if (!m_pending || !m_source) {
        return;
    }
    Job job;
    job.generation = m_sourceGeneration;
    job.settings = std::move(*m_pending);
    job.lens = m_lens;
    job.draft = m_pendingDraft;
    job.draftSource = m_draftSource;
    m_draftRunning = job.draft;
    m_pending.reset();
    m_running = true;
    auto* watcher = new QFutureWatcher<Job>(this);
    connect(watcher, &QFutureWatcher<Job>::finished, this, [this, watcher] {
        const Job done = watcher->result();
        watcher->deleteLater();
        m_running = false;
        m_draftRunning = false;
        if (done.generation == m_sourceGeneration) {
            if (done.draft) {
                m_draftSource = done.draftSource;
            }
            m_draftShown = done.draft;
            if (done.error.isEmpty()) {
                Q_EMIT rendered(done.image, done.settings, done.histogram);
            } else {
                Q_EMIT failed(done.error);
            }
        }
        startNext();
    });
    watcher->setFuture(QtConcurrent::run(renderJob, m_source, std::move(job)));
}

} // namespace pixelfx::services
