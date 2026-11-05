#include "services/thumbnailer.h"

#include "core/looks.h"
#include "core/sidecar_store.h"
#include "services/qt_image_bridge.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/io.h>
#include <pfx/pipeline.h>

namespace pixelfx::services {

Thumbnailer::Thumbnailer(QObject* parent)
    : QObject(parent)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(2); // light work beside the editor's own renders
}

Thumbnailer::~Thumbnailer()
{
    m_pool->clear();
    m_pool->waitForDone();
}

Thumbnail Thumbnailer::make(const QString& path, int longEdge, const QString& sidecarFolder)
{
    Thumbnail thumb{path, {}, {}};
    const QString absolute = QFileInfo(path).absoluteFilePath();
    pfx::Settings settings;
    if (!sidecarFolder.isEmpty()) {
        const core::SidecarStore store(sidecarFolder);
        const QString fingerprint = core::fileFingerprint(absolute);
        if (auto stored = store.read(fingerprint)) {
            settings = std::move(*stored);
            thumb.edited = QFileInfo(store.pathFor(fingerprint)).lastModified();
        }
    }
    // An unedited RAW shows the JPEG inside it, fast; an edited one is developed as it was edited.
    pfx::LoadOptions options;
    options.maxLongEdge = longEdge;
    options.embeddedPreview = settings.isEmpty();
    options.rawTone = settings.isEmpty() || settings.text("color.profile", "standard") == "camera";
    auto loaded = pfx::load(absolute.toStdString(), options);
    if (!loaded) {
        return thumb;
    }
    const pfx::LensFacts lens = pfx::lensFacts(pfx::readExif(loaded.value().metadata.exif));
    pfx::RenderOptions render;
    render.lens = &lens;
    const auto rendered = settings.isEmpty() ? pfx::Result<pfx::Image>(std::move(loaded).value().image)
                                             : core::renderPhoto(loaded.value().image, settings, render);
    if (!rendered) {
        return thumb;
    }
    if (const auto display = pfx::toSrgb8(rendered.value())) {
        thumb.image = toQImage(display.value());
    }
    return thumb;
}

void Thumbnailer::requestRender(int key, std::shared_ptr<const pfx::Image> source,
                                const pfx::Settings& settings)
{
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, key] {
        Q_EMIT rendered(key, watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(m_pool, [source = std::move(source), settings]() -> QImage {
        if (!source) {
            return {};
        }
        const auto out = core::renderPhoto(*source, settings, {1, nullptr}); // tiny: one thread each
        if (!out) {
            return {};
        }
        const auto display = pfx::toSrgb8(out.value());
        return display ? toQImage(display.value()) : QImage();
    }));
}

void Thumbnailer::request(const QString& path, int longEdge, const QString& sidecarFolder)
{
    auto* watcher = new QFutureWatcher<Thumbnail>(this);
    connect(watcher, &QFutureWatcher<Thumbnail>::finished, this, [this, watcher] {
        Q_EMIT ready(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(m_pool, &Thumbnailer::make, path, longEdge, sidecarFolder));
}

} // namespace pixelfx::services
