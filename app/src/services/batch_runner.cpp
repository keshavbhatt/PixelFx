#include "services/batch_runner.h"

#include "core/presets.h"
#include "core/sidecar_store.h"
#include "services/exporter.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSaveFile>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <pfx/auto.h>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::services {

namespace {

// The file's format names and the export options' extensions.
constexpr std::array<std::pair<const char*, const char*>, 7> kFormats = {{{"jpeg", "jpg"},
                                                                          {"png", "png"},
                                                                          {"tiff", "tif"},
                                                                          {"webp", "webp"},
                                                                          {"avif", "avif"},
                                                                          {"heic", "heic"},
                                                                          {"jxl", "jxl"}}};

struct CompressOutcome
{
    QString target;
    QString error;
    CompressResult result;
};

// The original's most detailed spot at 100%, as PNG bytes.
QByteArray spotOf(const QString& source, const std::vector<std::uint8_t>& original, pfx::Region& spot)
{
    const auto region =
        pfx::detailRegion(source.toStdString(), BatchRunner::kSpotWidth, BatchRunner::kSpotHeight);
    if (!region) {
        return {};
    }
    spot = region.value();
    const auto pixels = pfx::loadRegion(original, spot);
    if (!pixels) {
        return {};
    }
    const pfx::Rgba8Image& p = pixels.value();
    const QImage image(p.pixels.data(), p.width, p.height, static_cast<qsizetype>(p.width) * 4,
                       QImage::Format_RGBA8888);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG", 0);
    return png;
}

// One file of Compress only, on a worker.
CompressOutcome compressOne(const QString& source, const core::CompressJob& job, const QSet<QString>& ours,
                            core::NameFacts facts, core::NameClaims* claims)
{
    CompressOutcome o;
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        o.error = BatchRunner::tr("The file could not be read.");
        return o;
    }
    // A file that is no photo is said so, as Edit and export does, not passed on unchanged.
    if (const auto info = pfx::probe(source.toStdString()); !info) {
        o.error = QString::fromStdString(info.error().message);
        return o;
    }
    const QByteArray raw = in.readAll();
    const std::vector<std::uint8_t> original(raw.begin(), raw.end());
    const auto c = pfx::compress(original, source.toStdString(), job.codec);
    if (!c) {
        o.error = QString::fromStdString(c.error().message);
        return o;
    }
    CompressResult& r = o.result;
    r.before = static_cast<qint64>(original.size());
    r.method = c.value().method;
    r.note = c.value().note;
    r.quality = c.value().quality;
    r.keptOriginal = job.keepIfNotSmaller && !c.value().smaller();
    const std::vector<std::uint8_t>& bytes = r.keptOriginal ? original : c.value().bytes;
    r.after = static_cast<qint64>(bytes.size());
    if (const auto metadata = pfx::readMetadata(source.toStdString())) {
        const core::NameFacts exif = core::nameFacts(metadata.value());
        facts.taken = exif.taken;
        facts.camera = exif.camera;
    }
    o.target = core::compressTarget(source, job, ours, QDate::currentDate(), facts, claims);
    if (o.target.isEmpty()) {
        o.error = BatchRunner::tr("No free file name was found in the folder.");
        return o;
    }
    if (job.replaceOriginals) {
        r.beforeSpot = spotOf(source, original, r.spot);
        if (r.keptOriginal) {
            return o; // nothing to replace
        }
    }
    QDir().mkpath(QFileInfo(o.target).absolutePath());
    if (QFileInfo::exists(o.target)) {
        pfx::closeFiles(); // read moments ago: Windows cannot replace a file still open
    }
    QSaveFile out(o.target);
    if (!out.open(QIODevice::WriteOnly) ||
        out.write(reinterpret_cast<const char*>(
                      bytes.data()), // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size()) ||
        !out.commit()) {
        o.error =
            BatchRunner::tr("The file could not be written to %1.").arg(QFileInfo(o.target).absolutePath());
        o.target.clear();
    }
    return o;
}

} // namespace

BatchRecipe recipeFromBody(const pfx::BatchBody& body, const QString& sidecarFolder)
{
    BatchRecipe r;
    for (const pfx::BatchStep& step : body.stack) {
        r.presets.push_back(step.settings);
    }
    r.autoFirst = body.autoFirst;
    r.keepEdits = body.keepExistingEdits;
    r.sidecarFolder = sidecarFolder;
    r.output.longEdge = body.longEdge;
    r.output.noEnlarge = body.noEnlarge;
    r.output.sizePreset = QString::fromStdString(body.sizePreset);
    r.output.fitInside = body.fitInside;
    for (const auto& [name, ext] : kFormats) {
        if (body.format == name) {
            r.output.extension = QString::fromLatin1(ext);
        }
    }
    r.output.quality = body.quality;
    r.output.colorSpace = core::colorSpaceFromKey(QString::fromStdString(body.colorSpace));
    r.output.sharpen = core::sharpenFromKey(QString::fromStdString(body.sharpen));
    r.output.keepMetadata = body.keepCamera;
    r.output.keepLocation = body.keepLocation;
    r.output.folder = QString::fromStdString(body.folder);
    r.output.nameTemplate = QString::fromStdString(body.name);
    r.output.ifExists = core::ExportOptions::IfExists::AddNumber;
    if (body.ifExists == "overwrite") {
        r.output.ifExists = core::ExportOptions::IfExists::Overwrite;
    } else if (body.ifExists == "skip") {
        r.output.ifExists = core::ExportOptions::IfExists::Skip;
    }
    r.besideOriginals = body.beside;
    return r;
}

pfx::BatchBody bodyFromRecipe(const BatchRecipe& recipe, const QStringList& names)
{
    pfx::BatchBody body;
    for (std::size_t i = 0; i < recipe.presets.size(); ++i) {
        const auto at = static_cast<qsizetype>(i);
        body.stack.push_back(
            {at < names.size() ? names.at(at).toStdString() : std::string(), recipe.presets[i]});
    }
    body.autoFirst = recipe.autoFirst;
    body.keepExistingEdits = recipe.keepEdits;
    body.longEdge = recipe.output.longEdge;
    body.noEnlarge = recipe.output.noEnlarge;
    if (core::sizePreset(recipe.output.sizePreset) != nullptr) {
        body.sizePreset = recipe.output.sizePreset.toStdString();
        body.fitInside = recipe.output.fitInside;
    }
    for (const auto& [name, ext] : kFormats) {
        if (recipe.output.extension == QLatin1StringView(ext)) {
            body.format = name;
        }
    }
    body.quality = recipe.output.quality;
    body.colorSpace = core::colorSpaceKey(recipe.output.colorSpace).toStdString();
    body.sharpen = core::sharpenKey(recipe.output.sharpen).toStdString();
    body.keepCamera = recipe.output.keepMetadata;
    body.keepLocation = recipe.output.keepLocation;
    body.folder = recipe.output.folder.toStdString();
    body.name = recipe.output.nameTemplate.toStdString();
    switch (recipe.output.ifExists) {
    case core::ExportOptions::IfExists::Overwrite:
        body.ifExists = "overwrite";
        break;
    case core::ExportOptions::IfExists::Skip:
        body.ifExists = "skip";
        break;
    case core::ExportOptions::IfExists::AddNumber:
        body.ifExists = "number";
        break;
    }
    body.beside = recipe.besideOriginals;
    return body;
}

BatchRunner::BatchRunner(QObject* parent)
    : QObject(parent)
    , m_pool(new QThreadPool(this))
{
    m_pool->setMaxThreadCount(kMaxAtOnce);
}

BatchRunner::~BatchRunner()
{
    m_queue.clear();
    m_pool->waitForDone();
}

pfx::Settings BatchRunner::settingsFor(const BatchRecipe& recipe, const pfx::Settings& own,
                                       const pfx::Image& photo)
{
    pfx::Settings s = recipe.keepEdits ? own : pfx::Settings{};
    if (recipe.autoFirst) {
        s = s.overlaidWith(pfx::autoAdjust(photo));
    }
    for (const pfx::Settings& preset : recipe.presets) {
        s = core::applyPreset(s, preset);
    }
    return s;
}

void BatchRunner::start(const QStringList& files, const BatchRecipe& recipe)
{
    m_ours.clear();
    if (recipe.compress && m_recipe.compress && files == m_files) {
        for (const QString& t : m_targets) {
            if (!t.isEmpty()) {
                m_ours.insert(QFileInfo(t).absoluteFilePath());
            }
        }
    }
    ++m_generation;
    m_claims = std::make_shared<core::NameClaims>(); // a new run: its photos' names so far
    m_files = files;
    m_recipe = recipe;
    m_states.assign(static_cast<std::size_t>(files.size()), State::Waiting);
    m_reasons.assign(static_cast<std::size_t>(files.size()), QString());
    m_targets.assign(static_cast<std::size_t>(files.size()), QString());
    m_compressed.assign(static_cast<std::size_t>(files.size()), CompressResult{});
    if (m_limit > 0) {
        m_atOnce = m_limit;
    } else {
        m_atOnce =
            recipe.compress ? std::clamp(QThread::idealThreadCount() / 2, 2, kCompressAtOnce) : kAtOnce;
    }
    m_queue.clear();
    for (int i = 0; i < files.size(); ++i) {
        m_queue.push_back(i);
    }
    m_paused = false;
    m_running = 0;
    m_clock.start();
    for (int i = 0; i < files.size(); ++i) {
        Q_EMIT photoChanged(i); // every row waits, nothing left of an earlier run
    }
    for (int i = 0; i < m_atOnce; ++i) {
        startNext();
    }
}

void BatchRunner::retryFailed()
{
    for (std::size_t i = 0; i < m_states.size(); ++i) {
        if (m_states[i] == State::Failed) {
            m_states[i] = State::Waiting;
            m_reasons[i].clear();
            m_queue.push_back(static_cast<int>(i));
            Q_EMIT photoChanged(static_cast<int>(i));
        }
    }
    m_paused = false;
    m_clock.start();
    for (int i = 0; i < m_atOnce; ++i) {
        startNext();
    }
}

void BatchRunner::processRest()
{
    for (std::size_t i = 0; i < m_states.size(); ++i) {
        if (m_states[i] == State::Waiting) {
            m_queue.push_back(static_cast<int>(i));
        }
    }
    m_paused = false;
    m_clock.start();
    for (int i = 0; i < m_atOnce; ++i) {
        startNext();
    }
}

void BatchRunner::setPaused(bool paused)
{
    m_paused = paused;
    for (int i = 0; !paused && i < m_atOnce; ++i) {
        startNext();
    }
}

void BatchRunner::stop()
{
    m_queue.clear();
    m_paused = false;
    if (m_running == 0) {
        Q_EMIT finished(saved(), failed(), m_clock.elapsed());
    }
}

int BatchRunner::saved() const
{
    return static_cast<int>(std::ranges::count(m_states, State::Saved));
}

int BatchRunner::failed() const
{
    return static_cast<int>(std::ranges::count(m_states, State::Failed));
}

int BatchRunner::waiting() const
{
    return static_cast<int>(std::ranges::count(m_states, State::Waiting));
}

void BatchRunner::startNext()
{
    if (m_paused || m_queue.empty() || m_running >= m_atOnce) {
        return;
    }
    const int index = m_queue.front();
    m_queue.erase(m_queue.begin());
    m_states[static_cast<std::size_t>(index)] = State::Working;
    ++m_running;
    Q_EMIT photoChanged(index);
    if (const auto& job = m_recipe.compress) {
        startCompress(index, *job);
        return;
    }

    core::ExportOptions options = m_recipe.output;
    const QString source = m_files.at(index);
    if (m_recipe.besideOriginals) {
        options.folder = QFileInfo(source).absolutePath() + u"/PixelFx"_s;
    }
    const BatchRecipe recipe = m_recipe;
    const quint64 generation = m_generation;
    auto* watcher = new QFutureWatcher<ExportOutcome>(this);
    connect(watcher, &QFutureWatcher<ExportOutcome>::finished, this, [this, watcher, index, generation] {
        const ExportOutcome o = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) {
            return; // a newer batch replaced this one
        }
        --m_running;
        const auto i = static_cast<std::size_t>(index);
        if (o.skipped) {
            m_states[i] = State::Skipped;
            m_reasons[i] = tr("A file with that name exists, and Skip is chosen.");
        } else if (!o.error.isEmpty()) {
            m_states[i] = State::Failed;
            m_reasons[i] = o.error;
        } else {
            m_states[i] = State::Saved;
            m_targets[i] = o.target;
        }
        finishOne(index);
    });
    core::NameFacts facts;
    facts.number = index + 1;
    facts.count = total();
    watcher->setFuture(QtConcurrent::run(m_pool, [source, recipe, options, facts, claims = m_claims] {
        pfx::Settings own;
        if (recipe.keepEdits && !recipe.sidecarFolder.isEmpty()) {
            own = core::SidecarStore(recipe.sidecarFolder)
                      .read(core::fileFingerprint(source))
                      .value_or(pfx::Settings{});
        }
        return exportFile(
            source, [&](const pfx::Image& photo) { return settingsFor(recipe, own, photo); }, options, facts,
            claims.get());
    }));
}

void BatchRunner::finishOne(int index)
{
    Q_EMIT photoChanged(index);
    const auto done = static_cast<int>(
        std::ranges::count_if(m_states, [](State s) { return s != State::Waiting && s != State::Working; }));
    Q_EMIT progress(done, total());
    startNext();
    if (m_running == 0 && m_queue.empty()) {
        Q_EMIT finished(saved(), failed(), m_clock.elapsed());
    }
}

void BatchRunner::startCompress(int index, const core::CompressJob& job)
{
    const QString source = m_files.at(index);
    const QSet<QString> ours = m_ours;
    const quint64 generation = m_generation;
    auto* watcher = new QFutureWatcher<CompressOutcome>(this);
    connect(watcher, &QFutureWatcher<CompressOutcome>::finished, this, [this, watcher, index, generation] {
        const CompressOutcome o = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) {
            return;
        }
        --m_running;
        const auto i = static_cast<std::size_t>(index);
        m_states[i] = o.error.isEmpty() ? State::Saved : State::Failed;
        m_reasons[i] = o.error;
        m_targets[i] = o.target;
        m_compressed[i] = o.result;
        finishOne(index);
    });
    core::NameFacts facts;
    facts.number = index + 1;
    facts.count = total();
    watcher->setFuture(QtConcurrent::run(m_pool, [source, job, ours, facts, claims = m_claims] {
        return compressOne(source, job, ours, facts, claims.get());
    }));
}

} // namespace pixelfx::services
