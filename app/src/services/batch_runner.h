#pragma once

#include "core/export_options.h"

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

#include <pfx/document.h>
#include <pfx/image.h>
#include <pfx/settings.h>
#include <vector>

class QThreadPool;

namespace pixelfx::services {

/// One recipe for many photos (mock batch.html): presets stacked in order, optionally on top of
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

    void start(const QStringList& files, const BatchRecipe& recipe);
    /// Runs the failed photos of the last batch again.
    void retryFailed();
    void setPaused(bool paused);
    void stop();
    [[nodiscard]] bool isRunning() const { return m_running > 0 || (!m_queue.empty() && !m_paused); }
    [[nodiscard]] bool isPaused() const { return m_paused; }
    [[nodiscard]] int total() const { return static_cast<int>(m_states.size()); }
    [[nodiscard]] int saved() const;
    [[nodiscard]] int failed() const;
    [[nodiscard]] State state(int index) const { return m_states.at(static_cast<std::size_t>(index)); }
    [[nodiscard]] const QString& reason(int index) const
    {
        return m_reasons.at(static_cast<std::size_t>(index));
    }
    [[nodiscard]] const QString& target(int index) const
    {
        return m_targets.at(static_cast<std::size_t>(index));
    }
    /// The settings `recipe` gives a photo whose own edits are `own` (pure, for tests).
    [[nodiscard]] static pfx::Settings settingsFor(const BatchRecipe& recipe, const pfx::Settings& own,
                                                   const pfx::Image& photo);

    static constexpr int kAtOnce = 2; // a 24 MP photo needs about 1 GB while it is processed

Q_SIGNALS:
    void photoChanged(int index);
    /// After each photo: how many are done (saved, skipped or failed) of all.
    void progress(int done, int total);
    void finished(int saved, int failed, qint64 milliseconds);

private:
    void startNext();
    QStringList m_files;
    BatchRecipe m_recipe;
    std::vector<State> m_states;
    std::vector<QString> m_reasons;
    std::vector<QString> m_targets;
    std::vector<int> m_queue;
    int m_running = 0;
    bool m_paused = false;
    quint64 m_generation = 0;
    QElapsedTimer m_clock;
    QThreadPool* m_pool = nullptr;
};

} // namespace pixelfx::services
