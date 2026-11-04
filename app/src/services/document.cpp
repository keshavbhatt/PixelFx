#include "services/document.h"

#include "services/logging.h"
#include "services/qt_image_bridge.h"
#include "services/render_scheduler.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::services {

struct Document::Result
{
    quint64 generation = 0;
    DocumentInfo info;
    std::shared_ptr<const pfx::Image> source;
    QImage original;
    core::Histogram histogram;
    QString error;
};

namespace {

QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
    , m_scheduler(new RenderScheduler(this))
{
    connect(m_scheduler, &RenderScheduler::rendered, this,
            [this](const QImage& image, const pfx::Settings&, const core::Histogram& histogram) {
                m_preview = image;
                m_histogram = histogram;
                Q_EMIT previewReady(m_preview);
                Q_EMIT histogramReady(m_histogram);
            });
    connect(m_scheduler, &RenderScheduler::failed, this,
            [this](const QString& message) { Q_EMIT failed(m_info.path, message); });
}

Document::~Document() = default;

bool Document::isBusy() const
{
    return m_pending > 0 || m_scheduler->isBusy();
}

Document::Result Document::openOnWorker(Result result,
                                        int previewLongEdge) // NOLINT(performance-unnecessary-value-param)
{
    const std::string path = QFileInfo(result.info.path).absoluteFilePath().toStdString();
    const auto probed = pfx::probe(path);
    if (!probed) {
        result.error = fromStd(probed.error().message);
        return result;
    }
    result.info.size = QSize(probed.value().width, probed.value().height);
    result.info.format = fromStd(probed.value().format);
    result.info.bitsPerSample = probed.value().bitsPerSample;
    auto loaded = pfx::load(path, {previewLongEdge});
    if (!loaded) {
        result.error = fromStd(loaded.error().message);
        return result;
    }
    const auto display = pfx::toSrgb8(loaded.value().image);
    if (!display) {
        result.error = fromStd(display.error().message);
        return result;
    }
    result.original = toQImage(display.value());
    result.histogram = core::histogramOf(result.original);
    result.source = std::make_shared<const pfx::Image>(std::move(loaded).value().image);
    return result;
}

void Document::open(const QString& path, int previewLongEdge)
{
    Result request;
    request.generation = ++m_generation;
    request.info.path = path;
    ++m_pending;
    Q_EMIT openStarted(path);
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        --m_pending;
        deliver(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&Document::openOnWorker, request, previewLongEdge));
}

void Document::deliver(const Result& result)
{
    if (result.generation != m_generation) {
        qCDebug(lcDocument) << "dropped a stale open of" << result.info.path;
        return;
    }
    if (!result.error.isEmpty()) {
        qCWarning(lcDocument) << "could not open" << result.info.path << result.error;
        Q_EMIT failed(result.info.path, result.error);
        return;
    }
    m_info = result.info;
    m_original = result.original;
    m_preview = result.original;
    m_histogram = result.histogram;
    m_scheduler->setSource(result.source);
    m_history.reset({}, tr("Opened"));
    replaceSettings({}, false); // the original is already the render of no settings
    qCInfo(lcDocument) << "opened" << m_info.path << m_info.size << m_info.format;
    Q_EMIT opened(m_info);
    Q_EMIT previewReady(m_preview);
    Q_EMIT histogramReady(m_histogram);
    Q_EMIT historyChanged();
}

void Document::setSettings(const pfx::Settings& settings)
{
    if (!isOpen() || settings == m_settings) {
        return;
    }
    m_settings = settings;
    m_scheduler->request(m_settings);
}

void Document::commit(const QString& label)
{
    if (m_history.push(label, m_settings)) {
        Q_EMIT historyChanged();
    }
}

void Document::apply(const pfx::Settings& settings, const QString& label)
{
    if (!isOpen()) {
        return;
    }
    replaceSettings(settings);
    commit(label);
}

void Document::undo()
{
    if (m_history.canUndo()) {
        replaceSettings(m_history.undo());
        Q_EMIT historyChanged();
    }
}

void Document::redo()
{
    if (m_history.canRedo()) {
        replaceSettings(m_history.redo());
        Q_EMIT historyChanged();
    }
}

void Document::replaceSettings(const pfx::Settings& settings, bool render)
{
    m_settings = settings;
    if (render) {
        m_scheduler->request(m_settings);
    }
    Q_EMIT settingsReplaced(m_settings);
}

} // namespace pixelfx::services
