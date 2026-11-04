#include "services/document.h"

#include "services/logging.h"
#include "services/qt_image_bridge.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/io.h>
#include <pfx/pipeline.h>

using namespace Qt::StringLiterals;

namespace pixelfx::services {

struct Document::Result
{
    quint64 generation = 0;
    DocumentInfo info;
    QImage preview;
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
{}

Document::~Document() = default;

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
    const auto rendered = pfx::render(loaded.value().image, pfx::Settings{});
    if (!rendered) {
        result.error = fromStd(rendered.error().message);
        return result;
    }
    const auto display = pfx::toSrgb8(rendered.value());
    if (!display) {
        result.error = fromStd(display.error().message);
        return result;
    }
    result.preview = toQImage(display.value());
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
    m_preview = result.preview;
    qCInfo(lcDocument) << "opened" << m_info.path << m_info.size << m_info.format;
    Q_EMIT opened(m_info);
    Q_EMIT previewReady(m_preview);
}

} // namespace pixelfx::services
