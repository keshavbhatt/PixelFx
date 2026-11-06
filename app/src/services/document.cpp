#include "services/document.h"

#include "core/crop_geometry.h"
#include "core/photo_details.h"
#include "services/logging.h"
#include "services/qt_image_bridge.h"
#include "services/render_scheduler.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QPointF>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include <array>
#include <pfx/auto.h>
#include <pfx/color.h>
#include <pfx/io.h>
#include <pfx/lens.h>
#include <pfx/pipeline.h>
#include <pfx/registry.h>
#include <pfx/resample.h>

using namespace Qt::StringLiterals;

namespace pixelfx::services {

struct Document::Result
{
    quint64 generation = 0;
    DocumentInfo info;
    std::shared_ptr<const pfx::Image> source;
    QImage original;
    core::Histogram histogram;
    QString sidecarFolder;
    QString fingerprint;
    pfx::Settings stored;
    std::vector<pfx::BatchStep> snapshots;
    QString error;
    bool openAsLeft = true;
    bool previewOnly = false; ///< the JPEG inside a RAW, not the RAW
    bool rawTone = true;
};

namespace {

QString fromStd(const std::string& text)
{
    return QString::fromStdString(text);
}

// Whether settings ask for a RAW's own tone: the Camera profile.
bool wantsRawTone(const pfx::Settings& s)
{
    return s.text("color.profile", "standard") == "camera";
}

} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
    , m_scheduler(new RenderScheduler(this))
    , m_originalScheduler(new RenderScheduler(this))
    , m_fullScheduler(new RenderScheduler(this))
    , m_fullTimer(new QTimer(this))
    , m_saveTimer(new QTimer(this))
    , m_savePool(new QThreadPool(this))
{
    m_savePool->setMaxThreadCount(1); // writes stay in order
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, [this] { saveNow(false); });
    connect(m_scheduler, &RenderScheduler::rendered, this,
            [this](const QImage& image, const pfx::Settings& settings, const core::Histogram& histogram) {
                if (m_previewExact && settings != renderSettings()) {
                    return;
                }
                m_previewExact = false;
                m_preview = image;
                m_histogram = histogram;
                Q_EMIT previewReady(m_preview);
                Q_EMIT histogramReady(m_histogram);
                if (m_hover) {
                    setHoverState(HoverState::Shown);
                }
            });
    connect(m_originalScheduler, &RenderScheduler::rendered, this,
            [this](const QImage& image, const pfx::Settings& settings, const core::Histogram&) {
                if (settings == m_originalGeometry) {
                    m_original = image;
                    Q_EMIT originalReady(m_original);
                }
            });
    m_fullTimer->setSingleShot(true);
    m_fullTimer->setInterval(250); // a moment after a slider stops: no full-size render per step
    connect(m_fullTimer, &QTimer::timeout, this, [this] {
        if (m_fullOn && m_fullLoaded) {
            m_fullScheduler->request(renderSettings());
        }
    });
    connect(m_fullScheduler, &RenderScheduler::rendered, this,
            [this](const QImage& image, const pfx::Settings& settings, const core::Histogram&) {
                if (m_fullOn && settings == renderSettings()) {
                    m_full = image;
                    if (!m_hover) {
                        m_fullOfEdit = image;
                        m_fullOfEditSettings = settings;
                    }
                    setFullState(FullState::Ready);
                    Q_EMIT fullReady(image);
                }
            });
    connect(m_scheduler, &RenderScheduler::failed, this,
            [this](const QString& message) { Q_EMIT failed(m_info.path, message); });
}

Document::~Document()
{
    flush();
}

void Document::setSidecarFolder(const QString& folder)
{
    if (folder.isEmpty()) {
        m_sidecars.reset();
    } else {
        m_sidecars.emplace(folder);
    }
}

void Document::flush()
{
    if (m_saveTimer->isActive()) {
        m_saveTimer->stop();
        saveNow(true);
    }
    m_savePool->waitForDone();
}

void Document::scheduleSave()
{
    if (m_sidecars && !m_fingerprint.isEmpty()) {
        m_saveTimer->start();
    }
}

void Document::saveNow(bool wait)
{
    if (!m_sidecars || m_fingerprint.isEmpty()) {
        return;
    }
    m_savePool->start([store = *m_sidecars, photo = m_info.path, fp = m_fingerprint, settings = m_settings,
                       snapshots = m_snapshots] {
        if (!store.write(photo, fp, settings, &snapshots)) {
            qCWarning(lcDocument) << "could not remember the edits of" << photo;
        }
    });
    if (wait) {
        m_savePool->waitForDone();
    }
}

bool Document::isBusy() const
{
    return m_pending > 0 || m_scheduler->isBusy();
}

Document::Result Document::openOnWorker(Result result,
                                        int previewLongEdge) // NOLINT(performance-unnecessary-value-param)
{
    const std::string path = QFileInfo(result.info.path).absoluteFilePath().toStdString();
    const auto probed = pfx::probe(path);
    if (!probed && !result.previewOnly) {
        result.error = fromStd(probed.error().message);
        return result;
    }
    if (!result.sidecarFolder.isEmpty()) {
        result.fingerprint = core::fileFingerprint(QFileInfo(result.info.path).absoluteFilePath());
        const core::SidecarStore store(result.sidecarFolder);
        if (auto stored = store.read(result.fingerprint)) {
            result.stored = std::move(*stored);
        }
        result.snapshots = store.readSnapshots(result.fingerprint);
    }
    // A RAW opens on its Camera profile unless its remembered edits say otherwise.
    const bool remembered = result.openAsLeft && !result.stored.isEmpty();
    result.rawTone = remembered ? wantsRawTone(result.stored) : true;
    pfx::LoadOptions options;
    options.maxLongEdge = previewLongEdge;
    options.rawTone = result.rawTone;
    auto loaded = result.previewOnly ? pfx::loadPreview(path, options) : pfx::load(path, options);
    if (!loaded) {
        result.error = fromStd(loaded.error().message);
        return result;
    }
    // The preview inside a RAW is its own, smaller picture.
    const pfx::ImageInfo& info = result.previewOnly ? loaded.value().info : probed.value();
    result.info.size = result.previewOnly ? QSize(loaded.value().image.width(), loaded.value().image.height())
                                          : QSize(info.width, info.height);
    result.info.format = fromStd(info.format);
    result.info.bitsPerSample = info.bitsPerSample;
    result.info.raw = loaded.value().info.raw;
    result.info.asShotKelvin = loaded.value().info.asShotKelvin;
    const pfx::ExifSummary exif = pfx::readExif(loaded.value().metadata.exif);
    result.info.camera = core::cameraName(exif);
    result.info.lens = QString::fromStdString(exif.lens).trimmed();
    result.info.exposure = core::exposureLine(exif);
    result.info.lensFacts = pfx::lensFacts(exif);
    // On the worker: the first search reads the whole database.
    result.info.lensProfiles = pfx::lensProfilesAvailable();
    result.info.lensProfile = QString::fromStdString(pfx::lensProfileName(result.info.lensFacts));
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

void Document::openPreview(const QString& path, int previewLongEdge)
{
    flush();
    Result request;
    request.generation = ++m_generation;
    request.info.path = path;
    request.previewOnly = true;
    ++m_pending;
    m_previewLongEdge = previewLongEdge;
    Q_EMIT openStarted(path);
    auto* watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher] {
        --m_pending;
        deliver(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(&Document::openOnWorker, request, previewLongEdge));
}

void Document::open(const QString& path, int previewLongEdge)
{
    flush(); // the photo being left keeps its last edit
    Result request;
    request.generation = ++m_generation;
    request.info.path = path;
    request.sidecarFolder = m_sidecars ? m_sidecars->folder() : QString();
    request.openAsLeft = m_openAsLeft;
    ++m_pending;
    m_previewLongEdge = previewLongEdge;
    Q_EMIT openStarted(path);
    if (pfx::isRawFile(QFileInfo(path).absoluteFilePath().toStdString())) {
        // The JPEG inside shows at once while the RAW itself is developed.
        setDeveloping(true);
        const quint64 generation = request.generation;
        using Early = std::pair<QImage, QSize>;
        auto* early = new QFutureWatcher<Early>(this);
        connect(early, &QFutureWatcher<Early>::finished, this, [this, early, generation] {
            const Early e = early->result();
            early->deleteLater();
            if (generation == m_generation && m_developing && !e.first.isNull()) {
                Q_EMIT earlyPreview(e.first, e.second);
            }
        });
        early->setFuture(QtConcurrent::run([path, previewLongEdge]() -> Early {
            const auto loaded =
                pfx::loadPreview(QFileInfo(path).absoluteFilePath().toStdString(), {previewLongEdge});
            if (!loaded) {
                return {};
            }
            const auto display = pfx::toSrgb8(loaded.value().image);
            return display ? Early{toQImage(display.value()),
                                   QSize(loaded.value().image.width(), loaded.value().image.height())}
                           : Early{};
        }));
    } else {
        setDeveloping(false);
    }
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
    setDeveloping(false);
    if (!result.error.isEmpty()) {
        qCWarning(lcDocument) << "could not open" << result.info.path << result.error;
        Q_EMIT failed(result.info.path, result.error);
        return;
    }
    m_info = result.info;
    m_rawTone = result.rawTone;
    m_plainOriginal = result.original;
    m_original = result.original;
    m_originalGeometry = {};
    m_cropEditing = false;
    m_healEditing = false;
    m_fullOn = false;
    m_fullLoaded = false;
    ++m_fullGeneration;
    m_fullTimer->stop();
    m_fullScheduler->setSource(nullptr);
    m_full = QImage();
    m_fullOfEdit = QImage();
    setFullState(FullState::Off);
    m_preview = result.original;
    m_histogram = result.histogram;
    m_fingerprint = result.fingerprint;
    m_snapshots = result.snapshots;
    m_source = result.source;
    m_beforeMixer = {}; // a new photo: colorBeforeMixer renders again
    m_scheduler->setSource(result.source);
    m_scheduler->setLens(result.info.lensFacts);
    m_fullScheduler->setLens(result.info.lensFacts);
    m_hover.reset();
    m_previewExact = false;
    setHoverState(HoverState::Off);
    if (auto small = pfx::fitLongEdge(*result.source, 120)) {
        m_thumbnailSource = std::make_shared<const pfx::Image>(std::move(small).value());
    }
    m_originalScheduler->setSource(result.source);
    // As it was left (FEATURES E17): the remembered edits are the starting point of the history.
    pfx::Settings start = m_openAsLeft ? result.stored : pfx::Settings{};
    if (m_info.raw && start.isEmpty()) {
        start.set("color.profile", std::string("camera")); // a RAW looks as its camera showed it
    }
    m_history.reset(start, tr("Opened"));
    replaceSettings(start, !start.isEmpty()); // no settings: the original is the render
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
    const pfx::Settings before = renderSettings();
    m_settings = settings;
    if (renderSettings() != before) { // moving the crop box while cropping needs no render
        requestRender();
    }
    followRawTone();
}

void Document::setDeveloping(bool on)
{
    if (on != m_developing) {
        m_developing = on;
        Q_EMIT developingChanged(on);
    }
}

void Document::followRawTone()
{
    if (!m_info.raw || wantsRawTone(m_settings) == m_rawTone) {
        return;
    }
    m_rawTone = wantsRawTone(m_settings);
    setDeveloping(true);
    const quint64 generation = ++m_developGeneration;
    const std::string path = QFileInfo(m_info.path).absoluteFilePath().toStdString();
    pfx::LoadOptions options;
    options.maxLongEdge = m_previewLongEdge;
    options.rawTone = m_rawTone;
    auto* watcher = new QFutureWatcher<std::shared_ptr<const pfx::Image>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<const pfx::Image>>::finished, this,
            [this, watcher, generation] {
                const auto image = watcher->result();
                watcher->deleteLater();
                if (generation != m_developGeneration || !image) {
                    return;
                }
                setDeveloping(false);
                m_source = image;
                m_scheduler->setSource(image);
                m_originalScheduler->setSource(image);
                m_beforeMixer = {};
                if (auto small = pfx::fitLongEdge(*image, 120)) {
                    m_thumbnailSource = std::make_shared<const pfx::Image>(std::move(small).value());
                }
                requestRender();
                refreshOriginal();
                if (m_fullOn) { // the sharp picture is developed again too
                    setFullResolution(false);
                    setFullResolution(true);
                }
            });
    watcher->setFuture(QtConcurrent::run([path, options]() -> std::shared_ptr<const pfx::Image> {
        auto loaded = pfx::load(path, options);
        return loaded ? std::make_shared<const pfx::Image>(std::move(loaded).value().image) : nullptr;
    }));
}

void Document::refresh()
{
    if (isOpen()) {
        requestRender();
    }
}

pfx::Settings Document::renderSettings() const
{
    if (m_hover) {
        return *m_hover;
    }
    if (m_healEditing) {
        return core::withoutGeometry(m_settings);
    }
    return m_cropEditing ? core::withoutCrop(m_settings) : m_settings;
}

void Document::setHoverSettings(const std::optional<pfx::Settings>& settings)
{
    if (!isOpen() || settings == m_hover) {
        return;
    }
    m_hover = settings;
    m_previewExact = true;
    setHoverState(m_hover ? HoverState::Preparing : HoverState::Off);
    // A preview comes after a deliberate wait on its tile, so zoomed in it is drawn at full size
    // too; when it ends, the edit's own full-size picture comes straight back if it still holds.
    if (!m_hover && m_fullOn && !m_fullOfEdit.isNull() && m_fullOfEditSettings == renderSettings()) {
        m_scheduler->request(renderSettings());
        m_fullTimer->stop();
        m_full = m_fullOfEdit;
        setFullState(FullState::Ready);
        Q_EMIT fullReady(m_full);
        return;
    }
    requestRender();
}

void Document::setHoverState(HoverState state)
{
    if (state != m_hoverState) {
        m_hoverState = state;
        Q_EMIT hoverStateChanged(state);
    }
}

void Document::setFullState(FullState state)
{
    if (state != m_fullState) {
        m_fullState = state;
        Q_EMIT fullStateChanged(state);
    }
}

void Document::requestRender()
{
    m_scheduler->request(renderSettings());
    scheduleFull();
}

void Document::scheduleFull()
{
    if (m_fullOn) {
        m_full = QImage();
        Q_EMIT fullStale();
        setFullState(m_fullLoaded ? FullState::Updating : FullState::Loading);
        m_fullTimer->start();
    }
}

void Document::setFullResolution(bool on)
{
    if (on == m_fullOn || !isOpen()) {
        return;
    }
    m_fullOn = on;
    if (!on) {
        m_fullTimer->stop();
        m_fullScheduler->setSource(nullptr); // frees the full-size photo
        m_fullLoaded = false;
        m_full = QImage();
        m_fullOfEdit = QImage();
        setFullState(FullState::Off);
        return;
    }
    setFullState(FullState::Loading);
    // Decode the full photo on a worker, then render it.
    const quint64 generation = ++m_fullGeneration;
    auto* watcher = new QFutureWatcher<std::shared_ptr<const pfx::Image>>(this);
    connect(watcher, &QFutureWatcher<std::shared_ptr<const pfx::Image>>::finished, this,
            [this, watcher, generation] {
                const auto image = watcher->result();
                watcher->deleteLater();
                if (!m_fullOn || generation != m_fullGeneration) {
                    return;
                }
                if (!image) {
                    setFullState(FullState::Off); // the enlarged preview stays: nothing to wait for
                    return;
                }
                m_fullScheduler->setSource(image);
                m_fullLoaded = true;
                m_fullScheduler->request(renderSettings());
            });
    const std::string path = QFileInfo(m_info.path).absoluteFilePath().toStdString();
    pfx::LoadOptions options;
    options.rawTone = m_rawTone;
    watcher->setFuture(QtConcurrent::run([path, options]() -> std::shared_ptr<const pfx::Image> {
        auto loaded = pfx::load(path, options);
        if (!loaded) {
            return nullptr;
        }
        return std::make_shared<const pfx::Image>(std::move(loaded).value().image);
    }));
}

void Document::refreshOriginal()
{
    const pfx::Settings geometry = core::geometryOnly(renderSettings());
    if (geometry == m_originalGeometry) {
        return;
    }
    m_originalGeometry = geometry;
    if (geometry.isEmpty()) {
        m_original = m_plainOriginal;
        Q_EMIT originalReady(m_original);
    } else {
        m_originalScheduler->request(geometry);
    }
}

void Document::setCropEditing(bool on)
{
    if (on == m_cropEditing) {
        return;
    }
    m_cropEditing = on;
    if (isOpen()) {
        requestRender();
        refreshOriginal();
    }
}

void Document::setHealEditing(bool on)
{
    if (on == m_healEditing) {
        return;
    }
    m_healEditing = on;
    if (isOpen()) {
        requestRender();
        refreshOriginal();
    }
}

QSize Document::displaySize() const
{
    return core::outputSize(renderSettings(), m_info.size);
}

QSize Document::outputSize() const
{
    return core::outputSize(m_settings, m_info.size);
}

void Document::commit(const QString& label)
{
    refreshOriginal();
    if (m_history.push(label, m_settings)) {
        scheduleSave();
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
        scheduleSave();
        Q_EMIT historyChanged();
    }
}

void Document::redo()
{
    if (m_history.canRedo()) {
        replaceSettings(m_history.redo());
        scheduleSave();
        Q_EMIT historyChanged();
    }
}

void Document::goToStep(int index)
{
    if (index != m_history.currentIndex()) {
        replaceSettings(m_history.goTo(index));
        scheduleSave();
        Q_EMIT historyChanged();
    }
}

void Document::autoAdjust()
{
    if (!isOpen() || !m_source) {
        return;
    }
    pfx::Settings next;
    for (const std::string& key : m_settings.keys()) {
        const std::string_view group = pfx::groupOf(key);
        if (group != "light" && group != "color") {
            next.set(key, *m_settings.find(key));
        }
    }
    apply(next.overlaidWith(pfx::autoAdjust(*m_source)), tr("Auto"));
}

namespace {

/// Replaces the white balance keys of `settings` with those of `wb` (absent keys: as shot).
pfx::Settings withWhiteBalance(const pfx::Settings& settings, const pfx::Settings& wb)
{
    pfx::Settings next = settings;
    for (const char* key : {"color.wb.temperature", "color.wb.tint"}) {
        if (const pfx::Value* v = wb.find(key)) {
            next.set(key, *v);
        } else {
            next.erase(key);
        }
    }
    return next;
}

} // namespace

void Document::saveSnapshot(const QString& name)
{
    if (!isOpen() || name.trimmed().isEmpty()) {
        return;
    }
    m_snapshots.push_back({name.trimmed().toStdString(), m_settings});
    scheduleSave();
    Q_EMIT historyChanged();
}

void Document::applySnapshot(std::size_t index)
{
    if (index < m_snapshots.size()) {
        apply(m_snapshots[index].settings,
              tr("Snapshot %1").arg(QString::fromStdString(m_snapshots[index].name)));
    }
}

void Document::autoWhiteBalance()
{
    if (!isOpen() || !m_source) {
        return;
    }
    apply(withWhiteBalance(m_settings, pfx::autoAdjust(*m_source)), tr("Auto white balance"));
}

void Document::pickWhiteBalance(QPointF fraction)
{
    if (!isOpen() || m_original.isNull()) {
        return;
    }
    const QImage img = m_original.convertToFormat(QImage::Format_RGB32);
    const int cx = std::clamp(static_cast<int>(fraction.x() * img.width()), 0, img.width() - 1);
    const int cy = std::clamp(static_cast<int>(fraction.y() * img.height()), 0, img.height() - 1);
    const int radius = std::max(1, static_cast<int>(0.003 * std::max(img.width(), img.height())));
    std::array<double, 3> sum{};
    int n = 0;
    for (int y = std::max(0, cy - radius); y <= std::min(img.height() - 1, cy + radius); ++y) {
        for (int x = std::max(0, cx - radius); x <= std::min(img.width() - 1, cx + radius); ++x) {
            const QRgb c = img.pixel(x, y);
            sum[0] += pfx::color::srgbDecode(qRed(c) / 255.0);
            sum[1] += pfx::color::srgbDecode(qGreen(c) / 255.0);
            sum[2] += pfx::color::srgbDecode(qBlue(c) / 255.0);
            ++n;
        }
    }
    const pfx::color::Vec3 rec2020 =
        pfx::color::transform(pfx::color::kSrgbToRec2020, {sum[0] / n, sum[1] / n, sum[2] / n});
    apply(withWhiteBalance(m_settings, pfx::neutralWhiteBalance(rec2020[0], rec2020[1], rec2020[2])),
          tr("White balance picker"));
}

std::optional<std::array<double, 3>> Document::colorBeforeMixer(QPointF fraction)
{
    if (!isOpen() || !m_source) {
        return std::nullopt;
    }
    // Small is enough to read a colour, and cheap to redo when an earlier setting changes;
    // spatial settings are shares of the photo, so the small render lines up with the shown one.
    constexpr int kLongEdge = 640;
    const pfx::Settings before = pfx::settingsBefore(m_settings, "mixer");
    if (m_beforeMixer.empty() || !(before == m_beforeMixerSettings)) {
        auto small = pfx::fitLongEdge(*m_source, kLongEdge);
        if (!small) {
            return std::nullopt;
        }
        pfx::RenderOptions options;
        options.lens = &m_info.lensFacts;
        auto rendered = pfx::render(std::move(small).value(), before, options);
        if (!rendered) {
            return std::nullopt;
        }
        m_beforeMixer = std::move(rendered).value();
        m_beforeMixerSettings = before;
    }
    const pfx::Image& img = m_beforeMixer;
    const int cx = std::clamp(static_cast<int>(fraction.x() * img.width()), 0, img.width() - 1);
    const int cy = std::clamp(static_cast<int>(fraction.y() * img.height()), 0, img.height() - 1);
    std::array<double, 3> sum{};
    int n = 0;
    for (int y = std::max(0, cy - 1); y <= std::min(img.height() - 1, cy + 1); ++y) {
        for (int x = std::max(0, cx - 1); x <= std::min(img.width() - 1, cx + 1); ++x) {
            const float* p = img.pixel(x, y);
            for (std::size_t c = 0; c < 3; ++c) {
                sum.at(c) += double{p[c]};
            }
            ++n;
        }
    }
    return std::array<double, 3>{sum[0] / n, sum[1] / n, sum[2] / n};
}

void Document::replaceSettings(const pfx::Settings& settings, bool render)
{
    m_settings = settings;
    if (render) {
        requestRender();
    }
    refreshOriginal();
    followRawTone();
    Q_EMIT settingsReplaced(m_settings);
}

} // namespace pixelfx::services
