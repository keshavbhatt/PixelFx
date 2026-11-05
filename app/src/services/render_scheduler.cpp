#include "services/render_scheduler.h"

#include "core/looks.h"
#include "services/qt_image_bridge.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/io.h>
#include <pfx/pipeline.h>

namespace pixelfx::services {

namespace {

struct Job
{
    quint64 generation = 0;
    pfx::Settings settings;
    pfx::LensFacts lens;
    QImage image;
    core::Histogram histogram;
    QString error;
};

Job renderJob(const std::shared_ptr<const pfx::Image>& source,
              Job job) // NOLINT(performance-unnecessary-value-param)
{
    pfx::RenderOptions options;
    options.lens = &job.lens;
    const auto rendered = core::renderPhoto(*source, job.settings, options);
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
{}

RenderScheduler::~RenderScheduler() = default;

void RenderScheduler::setSource(std::shared_ptr<const pfx::Image> source)
{
    m_source = std::move(source);
    ++m_sourceGeneration;
    m_pending.reset();
}

void RenderScheduler::request(const pfx::Settings& settings)
{
    if (!m_source) {
        return;
    }
    m_pending = settings;
    if (!m_running) {
        startNext();
    }
}

void RenderScheduler::startNext()
{
    if (!m_pending || !m_source) {
        return;
    }
    Job job{m_sourceGeneration, std::move(*m_pending), m_lens, {}, {}, {}};
    m_pending.reset();
    m_running = true;
    auto* watcher = new QFutureWatcher<Job>(this);
    connect(watcher, &QFutureWatcher<Job>::finished, this, [this, watcher] {
        const Job done = watcher->result();
        watcher->deleteLater();
        m_running = false;
        if (done.generation == m_sourceGeneration) {
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
