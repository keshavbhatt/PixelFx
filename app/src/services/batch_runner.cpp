#include "services/batch_runner.h"

#include "core/presets.h"
#include "core/sidecar_store.h"
#include "services/exporter.h"

#include <QFileInfo>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <pfx/auto.h>

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
    r.output.sizePreset = QString::fromStdString(body.sizePreset);
    r.output.fitInside = body.fitInside;
    for (const auto& [name, ext] : kFormats) {
        if (body.format == name) {
            r.output.extension = QString::fromLatin1(ext);
        }
    }
    r.output.quality = body.quality;
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
    m_pool->setMaxThreadCount(kAtOnce);
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
    ++m_generation;
    m_files = files;
    m_recipe = recipe;
    m_states.assign(static_cast<std::size_t>(files.size()), State::Waiting);
    m_reasons.assign(static_cast<std::size_t>(files.size()), QString());
    m_targets.assign(static_cast<std::size_t>(files.size()), QString());
    m_queue.clear();
    for (int i = 0; i < files.size(); ++i) {
        m_queue.push_back(i);
    }
    m_paused = false;
    m_running = 0;
    m_clock.start();
    for (int i = 0; i < kAtOnce; ++i) {
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
    for (int i = 0; i < kAtOnce; ++i) {
        startNext();
    }
}

void BatchRunner::setPaused(bool paused)
{
    m_paused = paused;
    for (int i = 0; !paused && i < kAtOnce; ++i) {
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

void BatchRunner::startNext()
{
    if (m_paused || m_queue.empty() || m_running >= kAtOnce) {
        return;
    }
    const int index = m_queue.front();
    m_queue.erase(m_queue.begin());
    m_states[static_cast<std::size_t>(index)] = State::Working;
    ++m_running;
    Q_EMIT photoChanged(index);

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
        Q_EMIT photoChanged(index);
        const auto done = static_cast<int>(std::ranges::count_if(
            m_states, [](State s) { return s != State::Waiting && s != State::Working; }));
        Q_EMIT progress(done, total());
        startNext();
        if (m_running == 0 && m_queue.empty()) {
            Q_EMIT finished(saved(), failed(), m_clock.elapsed());
        }
    });
    watcher->setFuture(QtConcurrent::run(m_pool, [source, recipe, options] {
        pfx::Settings own;
        if (recipe.keepEdits && !recipe.sidecarFolder.isEmpty()) {
            own = core::SidecarStore(recipe.sidecarFolder)
                      .read(core::fileFingerprint(source))
                      .value_or(pfx::Settings{});
        }
        return exportFile(
            source, [&](const pfx::Image& photo) { return settingsFor(recipe, own, photo); }, options);
    }));
}

} // namespace pixelfx::services
