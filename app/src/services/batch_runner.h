#pragma once

#include "core/compress_job.h"
#include "core/export_options.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <memory>
#include <optional>
#include <pfx/compress.h>
#include <pfx/document.h>
#include <pfx/image.h>
#include <pfx/settings.h>
#include <vector>

class QThreadPool;

namespace pixelfx::services {

/// One recipe for many photos: presets stacked in order, optionally on top of
/// an Auto enhance worked out per photo and of the edits already made in PixelFx, then the
/// output options of the Export sheet.
struct BatchRecipe
{
    std::vector<pfx::Settings> presets;
    bool autoFirst = false;
    bool keepEdits = true;
    core::ExportOptions output;
    /// Write into a "PixelFx" folder beside each original instead of output.folder.
    bool besideOriginals = false;
    /// Where remembered edits are (for keepEdits); empty: none.
    QString sidecarFolder;
    /// Compress only: each file made smaller in its own format, nothing else (the rest of the
    /// recipe is not used).
    std::optional<core::CompressJob> compress;
};

/// What Compress only did with one file.
struct CompressResult
{
    qint64 before = 0;
    qint64 after = 0; ///< the size saved: the original's when it was kept
    pfx::CompressMethod method = pfx::CompressMethod::Unchanged;
    pfx::CompressNote note = pfx::CompressNote::None;
    int quality = 100;         ///< a palette's, for PNG Fewer colours
    bool keptOriginal = false; ///< the result was not smaller, so the original's bytes were saved
    /// When the originals are replaced, the original's most detailed spot at 100% (as PNG), for
    /// the before and after; otherwise both are read from the files when wanted.
    QByteArray beforeSpot;
    pfx::Region spot;
};

/// A batch recipe file's body as a recipe to run, and back (DOCS/presets.md section 4). Step names
/// go into `names` when given.
[[nodiscard]] BatchRecipe recipeFromBody(const pfx::BatchBody& body, const QString& sidecarFolder);
[[nodiscard]] pfx::BatchBody bodyFromRecipe(const BatchRecipe& recipe, const QStringList& names = {});

/// Runs a BatchRecipe over photos in the background (ADR-006), a few at a time so memory stays
/// bounded, reporting each photo; Pause stops starting new ones, Stop drops the rest.
class BatchRunner : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BatchRunner)

public:
    enum class State
    {
        Waiting,
        Working,
        Saved,
        Skipped,
        Failed
    };

    explicit BatchRunner(QObject* parent = nullptr);
    ~BatchRunner() override;

    /// Runs `recipe` on `files`; with a `limit`, on the first that many, the rest left waiting
    /// for processRest.
    void start(const QStringList& files, const BatchRecipe& recipe, int limit = -1);
    /// Runs the failed photos of the last batch again.
    void retryFailed();
    /// Runs the photos a stopped (or limited) batch did not reach, or the next `limit` of them.
    void processRest(int limit = -1);
    void setPaused(bool paused);
    /// Photos worked on at once from the next run: 0 for automatic (kAtOnce, or up to
    /// kCompressAtOnce for Compress only), at most kMaxAtOnce.
    void setAtOnce(int count) { m_limit = std::clamp(count, 0, kMaxAtOnce); }
    void stop();
    [[nodiscard]] bool isRunning() const { return m_running > 0 || (!m_queue.empty() && !m_paused); }
    [[nodiscard]] bool isPaused() const { return m_paused; }
    [[nodiscard]] int total() const { return static_cast<int>(m_states.size()); }
    [[nodiscard]] int saved() const;
    [[nodiscard]] int failed() const;
    /// Photos not reached: still queued, or left when the batch was stopped.
    [[nodiscard]] int waiting() const;
    [[nodiscard]] State state(int index) const { return m_states.at(static_cast<std::size_t>(index)); }
    [[nodiscard]] const QString& reason(int index) const
    {
        return m_reasons.at(static_cast<std::size_t>(index));
    }
    [[nodiscard]] const QString& target(int index) const
    {
        return m_targets.at(static_cast<std::size_t>(index));
    }
    /// Compress only's result for the file at `index`.
    [[nodiscard]] const CompressResult& compressed(int index) const
    {
        return m_compressed.at(static_cast<std::size_t>(index));
    }
    [[nodiscard]] const QStringList& files() const { return m_files; }
    [[nodiscard]] bool isCompressing() const { return m_recipe.compress.has_value(); }
    /// The size of the spot kept for the before and after.
    static constexpr int kSpotWidth = 640;
    static constexpr int kSpotHeight = 240;
    /// The settings `recipe` gives a photo whose own edits are `own` (pure, for tests).
    [[nodiscard]] static pfx::Settings settingsFor(const BatchRecipe& recipe, const pfx::Settings& own,
                                                   const pfx::Image& photo);

    static constexpr int kAtOnce = 2; // a 24 MP photo needs about 1 GB while it is processed
    /// Compress only decodes without the float pipeline, so it runs more files at once.
    static constexpr int kCompressAtOnce = 4;
    static constexpr int kMaxAtOnce = 8;

Q_SIGNALS:
    void photoChanged(int index);
    /// After each photo: how many are done (saved, skipped or failed) of all.
    void progress(int done, int total);
    void finished(int saved, int failed, qint64 milliseconds);

private:
    void startNext();
    void startCompress(int index, const core::CompressJob& job);
    void finishOne(int index);
    QStringList m_files;
    BatchRecipe m_recipe;
    std::vector<State> m_states;
    std::vector<QString> m_reasons;
    std::vector<QString> m_targets;
    std::vector<CompressResult> m_compressed;
    // Files the last Compress only run of this list wrote: running it again replaces them.
    QSet<QString> m_ours;
    std::vector<int> m_queue;
    int m_running = 0;
    int m_atOnce = kAtOnce;
    int m_limit = 0;
    std::shared_ptr<core::NameClaims> m_claims; ///< the names this run's photos took, for its workers
    bool m_paused = false;
    quint64 m_generation = 0;
    QElapsedTimer m_clock;
    QThreadPool* m_pool = nullptr;
};

} // namespace pixelfx::services
