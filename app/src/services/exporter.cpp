#include "services/exporter.h"

#include "services/logging.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/io.h>
#include <pfx/pipeline.h>
#include <pfx/resample.h>

namespace pixelfx::services {

namespace {

struct Outcome
{
    QString source;
    QString target;
    QString error;
    bool skipped = false;
};

Outcome exportOnWorker(const QString& source, const pfx::Settings& settings,
                       const core::ExportOptions& options)
{
    Outcome out{source, {}, {}, false};
    const auto fail = [&out](const std::string& message) {
        out.error = QString::fromStdString(message);
        return out;
    };
    auto loaded = pfx::load(QFileInfo(source).absoluteFilePath().toStdString());
    if (!loaded) {
        return fail(loaded.error().message);
    }
    auto rendered = pfx::render(loaded.value().image, settings);
    if (!rendered) {
        return fail(rendered.error().message);
    }
    pfx::Image image = std::move(rendered).value();
    if (options.longEdge > 0) {
        auto resized = pfx::fitLongEdge(image, options.longEdge);
        if (!resized) {
            return fail(resized.error().message);
        }
        image = std::move(resized).value();
    }
    out.target =
        core::exportTarget(source, options, QSize(image.width(), image.height()), QDate::currentDate());
    if (out.target.isEmpty()) {
        out.skipped = true;
        return out;
    }
    if (!QDir().mkpath(QFileInfo(out.target).absolutePath())) {
        out.error =
            QObject::tr("The folder %1 could not be created.").arg(QFileInfo(out.target).absolutePath());
        return out;
    }
    pfx::SaveOptions save;
    save.quality = options.quality;
    save.bitsPerSample = options.bitsPerSample;
    save.keepMetadata = options.keepMetadata;
    const auto saved = pfx::save(image, out.target.toStdString(), save, &loaded.value().metadata);
    if (!saved) {
        return fail(saved.error().message);
    }
    return out;
}

} // namespace

Exporter::Exporter(QObject* parent)
    : QObject(parent)
{}

Exporter::~Exporter() = default;

void Exporter::start(const QString& source, const pfx::Settings& settings, const core::ExportOptions& options)
{
    m_busy = true;
    auto* watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcher<Outcome>::finished, this, [this, watcher] {
        const Outcome o = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        if (o.skipped) {
            Q_EMIT skipped(o.source);
        } else if (!o.error.isEmpty()) {
            qCWarning(lcDocument) << "export failed" << o.source << o.error;
            Q_EMIT failed(o.source, o.error);
        } else {
            qCInfo(lcDocument) << "exported" << o.source << "to" << o.target;
            Q_EMIT finished(o.target);
        }
    });
    watcher->setFuture(QtConcurrent::run(exportOnWorker, source, settings, options));
}

} // namespace pixelfx::services
