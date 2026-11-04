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

ExportOutcome exportFile(const QString& source,
                         const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                         const core::ExportOptions& options)
{
    ExportOutcome out{source, {}, {}, false};
    const auto fail = [&out](const std::string& message) {
        out.error = QString::fromStdString(message);
        return out;
    };
    auto loaded = pfx::load(QFileInfo(source).absoluteFilePath().toStdString());
    if (!loaded) {
        return fail(loaded.error().message);
    }
    const pfx::Settings settings = settingsFor(loaded.value().image);
    auto rendered = pfx::render(std::move(loaded.value().image), settings);
    if (!rendered) {
        return fail(rendered.error().message);
    }
    pfx::Image image = std::move(rendered).value();
    // A size preset decides the size: an exact frame (filled or fitted) or a long edge.
    int longEdge = options.longEdge;
    if (const core::SizePreset* preset = core::sizePreset(options.sizePreset)) {
        longEdge = preset->longEdge;
        const QSize exact = core::exactSize(*preset, QSize(image.width(), image.height()));
        if (exact.isValid()) {
            auto framed = options.fitInside ? pfx::fitInside(image, exact.width(), exact.height(), 1.0F)
                                            : pfx::fillSize(image, exact.width(), exact.height());
            if (!framed) {
                return fail(framed.error().message);
            }
            image = std::move(framed).value();
        }
    }
    if (longEdge > 0 && image.longEdge() > longEdge) { // never enlarges
        auto resized = pfx::fitLongEdge(image, longEdge);
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
    save.keepLocation = options.keepLocation;
    const auto saved = pfx::save(image, out.target.toStdString(), save, &loaded.value().metadata);
    if (!saved) {
        return fail(saved.error().message);
    }
    return out;
}

Exporter::Exporter(QObject* parent)
    : QObject(parent)
{}

Exporter::~Exporter() = default;

void Exporter::start(const QString& source, const pfx::Settings& settings, const core::ExportOptions& options)
{
    m_busy = true;
    auto* watcher = new QFutureWatcher<ExportOutcome>(this);
    connect(watcher, &QFutureWatcher<ExportOutcome>::finished, this, [this, watcher] {
        const ExportOutcome o = watcher->result();
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
    watcher->setFuture(QtConcurrent::run([source, settings, options] {
        return exportFile(source, [&settings](const pfx::Image&) { return settings; }, options);
    }));
}

} // namespace pixelfx::services
