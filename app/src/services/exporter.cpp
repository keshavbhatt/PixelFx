#include "services/exporter.h"

#include "core/looks.h"
#include "core/watermark.h"
#include "services/logging.h"
#include "services/qt_image_bridge.h"

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

PreparedExport prepareExport(const QString& source,
                             const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                             const core::ExportOptions& options)
{
    PreparedExport out;
    auto loaded = pfx::load(QFileInfo(source).absoluteFilePath().toStdString());
    if (!loaded) {
        out.error = QString::fromStdString(loaded.error().message);
        return out;
    }
    out.metadata = std::move(loaded.value().metadata);
    const pfx::Settings settings = settingsFor(loaded.value().image);
    auto rendered = core::renderPhoto(std::move(loaded.value().image), settings);
    if (!rendered) {
        out.error = QString::fromStdString(rendered.error().message);
        return out;
    }
    out.image = std::move(rendered).value();
    if (const std::string error = resize(out.image, options); !error.empty()) {
        out.error = QString::fromStdString(error);
        return out;
    }
    // The watermark goes on last, on the final pixels, so its size is right for them.
    out.error = applyWatermark(out.image, options.watermark, out.metadata);
    return out;
}

ExportOutcome exportFile(const QString& source,
                         const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                         const core::ExportOptions& options)
{
    ExportOutcome out{source, {}, {}, false};
    const PreparedExport prepared = prepareExport(source, settingsFor, options);
    if (!prepared.error.isEmpty()) {
        out.error = prepared.error;
        return out;
    }
    const pfx::Image& image = prepared.image;
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
    const auto saved = pfx::save(image, out.target.toStdString(), save, &prepared.metadata);
    if (!saved) {
        out.error = QString::fromStdString(saved.error().message);
    }
    return out;
}

Exporter::Exporter(QObject* parent)
    : QObject(parent)
{}

Exporter::~Exporter() = default;

void Exporter::startCopy(const QString& source, const pfx::Settings& settings,
                         const core::ExportOptions& options)
{
    m_busy = true;
    using Copy = std::pair<QImage, QString>;
    auto* watcher = new QFutureWatcher<Copy>(this);
    connect(watcher, &QFutureWatcher<Copy>::finished, this, [this, watcher, source] {
        const Copy c = watcher->result();
        watcher->deleteLater();
        m_busy = false;
        if (c.first.isNull()) {
            Q_EMIT failed(source, c.second);
        } else {
            Q_EMIT copied(c.first);
        }
    });
    watcher->setFuture(QtConcurrent::run([source, settings, options]() -> Copy {
        const PreparedExport prepared =
            prepareExport(source, [&settings](const pfx::Image&) { return settings; }, options);
        if (!prepared.error.isEmpty()) {
            return {QImage(), prepared.error};
        }
        const auto pixels = pfx::toSrgb8(prepared.image);
        if (!pixels) {
            return {QImage(), QString::fromStdString(pixels.error().message)};
        }
        return {toQImage(pixels.value()), QString()};
    }));
}

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
