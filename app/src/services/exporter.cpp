#include "services/exporter.h"

#include "core/watermark.h"
#include "services/logging.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/composite.h>
#include <pfx/io.h>
#include <pfx/pipeline.h>
#include <pfx/resample.h>

namespace pixelfx::services {

namespace {

// Lays the saved watermark at `file` on the image; an error when the file cannot be read, so a
// missing watermark never goes unnoticed.
QString applyWatermark(pfx::Image& image, const QString& file, const pfx::Metadata& metadata)
{
    if (file.isEmpty()) {
        return {};
    }
    const auto mark = core::WatermarkLibrary::load(file);
    if (!mark) {
        return QObject::tr("The watermark %1 could not be read.").arg(QFileInfo(file).completeBaseName());
    }
    const core::WatermarkTokens tokens = core::tokensFrom(pfx::readExif(metadata.exif), QDate::currentDate());
    const core::WatermarkOverlay overlay =
        core::renderWatermark(*mark, QSize(image.width(), image.height()), tokens, [&image](const QRect& r) {
            return pfx::meanLuminance(image, r.x(), r.y(), r.width(), r.height());
        });
    if (!overlay.image.isNull()) {
        pfx::blendOverlay(image, {overlay.image.constBits(), overlay.image.width(), overlay.image.height(),
                                  overlay.image.bytesPerLine(), overlay.origin.x(), overlay.origin.y()});
    }
    return {};
}

// The size the options ask for: a size preset's exact frame (filled or fitted) or long edge, or
// the long edge; never enlarging to a long edge. A reason on failure.
std::string resize(pfx::Image& image, const core::ExportOptions& options)
{
    int longEdge = options.longEdge;
    if (const core::SizePreset* preset = core::sizePreset(options.sizePreset)) {
        longEdge = preset->longEdge;
        const QSize exact = core::exactSize(*preset, QSize(image.width(), image.height()));
        if (exact.isValid()) {
            auto framed = options.fitInside ? pfx::fitInside(image, exact.width(), exact.height(), 1.0F)
                                            : pfx::fillSize(image, exact.width(), exact.height());
            if (!framed) {
                return framed.error().message;
            }
            image = std::move(framed).value();
        }
    }
    if (longEdge > 0 && image.longEdge() > longEdge) {
        auto resized = pfx::fitLongEdge(image, longEdge);
        if (!resized) {
            return resized.error().message;
        }
        image = std::move(resized).value();
    }
    return {};
}

} // namespace

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
    if (const std::string error = resize(image, options); !error.empty()) {
        return fail(error);
    }
    // The watermark goes on last, on the final pixels, so its size is right for them.
    if (const QString error = applyWatermark(image, options.watermark, loaded.value().metadata);
        !error.isEmpty()) {
        out.error = error;
        return out;
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
