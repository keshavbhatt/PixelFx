#include "services/exporter.h"

#include "core/looks.h"
#include "core/watermark.h"
#include "services/logging.h"
#include "services/qt_image_bridge.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
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
// The border of Fit inside: black for a dark photo, white for a light one.
float borderFor(const pfx::Image& image)
{
    double sum = 0.0;
    std::size_t n = 0;
    const int step = std::max(1, std::max(image.width(), image.height()) / 64);
    for (int y = 0; y < image.height(); y += step) {
        for (int x = 0; x < image.width(); x += step) {
            const float* p = image.pixel(x, y);
            sum += (0.2627 * double{p[0]}) + (0.6780 * double{p[1]}) +
                   (0.0593 * double{p[2]}); // Rec.2020 luminance
            ++n;
        }
    }
    constexpr double kMiddleGrey = 0.18;
    return (n > 0 && sum / static_cast<double>(n) < kMiddleGrey) ? 0.0F : 1.0F;
}

std::string resize(pfx::Image& image, const core::ExportOptions& options)
{
    const QSize photo(image.width(), image.height());
    if (const QSize frame = core::frameSize(options, photo); frame.isValid()) {
        auto framed =
            options.fitInside
                ? pfx::fitInside(image, frame.width(), frame.height(), borderFor(image))
                : pfx::fillSize(image, frame.width(), frame.height(), static_cast<float>(options.frameX),
                                static_cast<float>(options.frameY));
        if (!framed) {
            return framed.error().message;
        }
        image = std::move(framed).value();
        return {};
    }
    const QSize target = core::outputSize(options, photo);
    if (target == photo || !target.isValid()) {
        return {};
    }
    // The same shape: scaling, nothing cropped.
    auto resized = pfx::fillSize(image, target.width(), target.height());
    if (!resized) {
        return resized.error().message;
    }
    image = std::move(resized).value();
    return {};
}

} // namespace

namespace {

// The pixels an estimate encodes at most: the encoders take well under a second for them.
constexpr double kSamplePixels = 1e6;

// How bytes a pixel change with the picture's size: as pixels^exponent. A larger picture takes
// fewer a pixel, since more of each detail is smooth. Measured on photos from 6 to 48
// megapixels against encodes of their 1 megapixel copies: -0.22 to -0.35 for JPEG, -0.19 to
// -0.29 for PNG; within about 30 percent of the written file.
constexpr double kLossyExponent = -0.30;
constexpr double kLosslessExponent = -0.24;

// `image` cropped to the shape of `frame` and scaled to about `pixels` pixels.
pfx::Result<pfx::Image> sampleOf(const pfx::Image& image, const QSize& frame, double pixels)
{
    const double aspect = static_cast<double>(frame.width()) / frame.height();
    const int width = std::max(1, static_cast<int>(std::lround(std::sqrt(pixels * aspect))));
    const int height = std::max(1, static_cast<int>(std::lround(width / aspect)));
    return pfx::fillSize(image, width, height);
}

} // namespace

qint64 estimateExportSize(const QImage& preview, const QSize& photoSize, const core::ExportOptions& options)
{
    if (preview.isNull() || photoSize.isEmpty()) {
        return -1;
    }
    const auto source = fromDisplayImage(preview);
    if (!source) {
        return -1;
    }
    QSize target = core::frameSize(options, photoSize);
    if (!target.isValid()) {
        target = core::outputSize(options, photoSize);
    }
    if (!target.isValid() || target.isEmpty()) {
        target = photoSize;
    }
    const double wanted = static_cast<double>(target.width()) * target.height();
    const double previewPixels = static_cast<double>(preview.width()) * preview.height();
    auto sample = sampleOf(source.value(), target, std::min({wanted, kSamplePixels, previewPixels}));
    if (!sample) {
        return -1;
    }
    pfx::sharpenForOutput(sample.value(), options.sharpen);
    const auto bytes =
        pfx::encode(sample.value(), "." + options.extension.toStdString(), core::saveOptions(options));
    if (!bytes) {
        return -1;
    }
    const auto sampled = static_cast<double>(sample.value().pixelCount());
    const bool lossless = options.extension == u"png" || options.extension == u"tif";
    const double exponent = lossless ? kLosslessExponent : kLossyExponent;
    const double perPixel = static_cast<double>(bytes.value().size()) / sampled;
    return std::llround(perPixel * std::pow(std::max(1.0, wanted / sampled), exponent) * wanted);
}

PreparedExport prepareExport(const QString& source,
                             const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                             const core::ExportOptions& options, const pfx::LoadOptions& load)
{
    PreparedExport out;
    auto loaded = pfx::load(QFileInfo(source).absoluteFilePath().toStdString(), load);
    if (!loaded) {
        out.error = QString::fromStdString(loaded.error().message);
        return out;
    }
    out.metadata = std::move(loaded.value().metadata);
    out.profile = std::move(loaded.value().profile);
    const pfx::Settings settings = settingsFor(loaded.value().image);
    // A RAW is developed with its own tone only for the Camera profile.
    const bool tone = settings.text("color.profile", "standard") == "camera";
    if (loaded.value().info.raw && tone != load.rawTone) {
        pfx::LoadOptions again = load;
        again.rawTone = tone;
        if (auto redeveloped = pfx::load(QFileInfo(source).absoluteFilePath().toStdString(), again)) {
            loaded = std::move(redeveloped);
        }
    }
    const pfx::LensFacts lens = pfx::lensFacts(pfx::readExif(out.metadata.exif));
    pfx::RenderOptions render;
    render.lens = &lens;
    auto rendered = core::renderPhoto(std::move(loaded.value().image), settings, render);
    if (!rendered) {
        out.error = QString::fromStdString(rendered.error().message);
        return out;
    }
    out.image = std::move(rendered).value();
    if (const std::string error = resize(out.image, options); !error.empty()) {
        out.error = QString::fromStdString(error);
        return out;
    }
    pfx::sharpenForOutput(out.image, options.sharpen);
    // The watermark goes on last, on the final pixels, so its size is right for them.
    out.error = applyWatermark(out.image, options.watermark, out.metadata);
    return out;
}

ExportOutcome exportFile(const QString& source,
                         const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                         const core::ExportOptions& options, core::NameFacts facts, core::NameClaims* claims)
{
    ExportOutcome out{source, {}, {}, false};
    const PreparedExport prepared = prepareExport(source, settingsFor, options);
    if (!prepared.error.isEmpty()) {
        out.error = prepared.error;
        return out;
    }
    const pfx::Image& image = prepared.image;
    const core::NameFacts exif = core::nameFacts(prepared.metadata);
    facts.taken = exif.taken;
    facts.camera = exif.camera;
    out.target = core::exportTarget(source, options, QSize(image.width(), image.height()),
                                    QDate::currentDate(), facts, claims);
    if (out.target.isEmpty()) {
        out.skipped = true;
        return out;
    }
    if (!QDir().mkpath(QFileInfo(out.target).absolutePath())) {
        out.error =
            QObject::tr("The folder %1 could not be created.").arg(QFileInfo(out.target).absolutePath());
        return out;
    }
    const auto saved = pfx::save(image, out.target.toStdString(),
                                 core::saveOptions(options, prepared.profile), &prepared.metadata);
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
