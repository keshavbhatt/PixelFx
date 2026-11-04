#pragma once

#include "core/histogram.h"
#include "core/history.h"
#include "core/sidecar_store.h"

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QString>

#include <memory>
#include <optional>

class QThreadPool;
class QTimer;
#include <pfx/image.h>
#include <pfx/settings.h>

namespace pixelfx::services {

class RenderScheduler;

/// What is known about the open photo's file.
struct DocumentInfo
{
    QString path;
    QSize size;     ///< full size, upright
    QString format; ///< loader name, for example "jpegload"
    int bitsPerSample = 8;
};

/// One open photo (PLAN section 6): its decoded preview-size source, its settings and their
/// history. Opening decodes on a worker; every settings change re-renders on a worker through
/// the RenderScheduler, newest request first (ADR-006).
class Document : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Document)

public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    /// Remembers each photo's edits in `folder` (empty: remember nothing). Set before opening.
    void setSidecarFolder(const QString& folder);
    /// Whether a photo opens with its remembered edits (true) or as the original.
    void setOpenAsLeft(bool on) { m_openAsLeft = on; }
    /// Writes a pending edit file now (before quitting).
    void flush();

    /// Starts opening `path`, decoding a source whose long edge is at most `previewLongEdge`.
    void open(const QString& path, int previewLongEdge);

    [[nodiscard]] bool isOpen() const { return !m_info.path.isEmpty(); }
    [[nodiscard]] const DocumentInfo& info() const { return m_info; }
    /// The newest rendered preview, and the photo with no settings (for before and after).
    [[nodiscard]] const QImage& preview() const { return m_preview; }
    [[nodiscard]] const QImage& original() const { return m_original; }
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] const core::Histogram& histogram() const { return m_histogram; }

    [[nodiscard]] const pfx::Settings& settings() const { return m_settings; }
    /// Changes the settings and re-renders, without a history step (live slider moves).
    void setSettings(const pfx::Settings& settings);
    /// Records the current settings as a history step named `label` (slider released, reset).
    void commit(const QString& label);
    /// Replaces the settings as one history step (a preset, pasted settings).
    void apply(const pfx::Settings& settings, const QString& label);
    void undo();
    void redo();
    /// Jumps to a history step (the History popup).
    void goToStep(int index);
    /// Light and Color from the photo's statistics (the Auto button), as one step. Curves and
    /// Detail stay as they are.
    void autoAdjust();
    /// White balance only, from the photo's statistics (the Auto choice of the menu).
    void autoWhiteBalance();
    /// White balance that makes the photo grey at `fraction` (of the photo as shown, geometry
    /// applied), sampled from the original around it; one history step.
    void pickWhiteBalance(QPointF fraction);
    [[nodiscard]] const core::History& history() const { return m_history; }

    /// While cropping, the preview shows the whole straightened photo so the crop box can move
    /// over it; the crop stays in the settings.
    void setCropEditing(bool on);
    [[nodiscard]] bool isCropEditing() const { return m_cropEditing; }
    /// The size of the photo these settings export: the upright size after turn, straighten
    /// and crop (the uncropped size while cropping).
    [[nodiscard]] QSize displaySize() const;
    [[nodiscard]] QSize outputSize() const;

    /// While on (zoomed in past the preview), keeps a full-size render of the settings, made a
    /// moment after they stop changing; the full photo is decoded on first use and dropped when
    /// this goes off.
    void setFullResolution(bool on);
    [[nodiscard]] bool isFullResolution() const { return m_fullOn; }
    /// The newest full-size render matching the settings; null while one is being made.
    [[nodiscard]] const QImage& fullImage() const { return m_full; }

Q_SIGNALS:
    void openStarted(const QString& path);
    void opened(const pixelfx::services::DocumentInfo& info);
    void previewReady(const QImage& image);
    void histogramReady(const pixelfx::core::Histogram& histogram);
    /// The photo with only its geometry, for before and after (it lines up with the edit).
    void originalReady(const QImage& image);
    /// The full-size render for zoomed-in views, and the moment the one shown stops matching.
    void fullReady(const QImage& image);
    void fullStale();
    /// The settings changed from outside the editing controls (open, undo, redo).
    void settingsReplaced(const pfx::Settings& settings);
    void historyChanged();
    /// `message` is user-facing and says why, in plain words.
    void failed(const QString& path, const QString& message);

private:
    struct Result;
    /// Runs on a worker thread: probe, decode at preview size, render the original for display.
    static Result openOnWorker(Result result, int previewLongEdge);
    void deliver(const Result& result);
    void replaceSettings(const pfx::Settings& settings, bool render = true);
    void scheduleSave();
    /// What the preview renders: the settings, without the crop while cropping.
    [[nodiscard]] pfx::Settings renderSettings() const;
    void requestRender();
    /// Renders the original again when the geometry it needs changed.
    void refreshOriginal();
    void scheduleFull();
    void saveNow(bool wait);

    DocumentInfo m_info;
    QImage m_preview;
    QImage m_original;
    core::Histogram m_histogram;
    pfx::Settings m_settings;
    core::History m_history;
    RenderScheduler* m_scheduler = nullptr;
    RenderScheduler* m_originalScheduler = nullptr;
    QImage m_plainOriginal; ///< decoded with no settings at all
    pfx::Settings m_originalGeometry;
    bool m_cropEditing = false;
    bool m_openAsLeft = true;
    bool m_fullOn = false;
    QImage m_full;
    RenderScheduler* m_fullScheduler = nullptr;
    QTimer* m_fullTimer = nullptr;
    bool m_fullLoaded = false;
    quint64 m_fullGeneration = 0;
    std::shared_ptr<const pfx::Image> m_source;
    std::optional<core::SidecarStore> m_sidecars;
    QString m_fingerprint;
    QTimer* m_saveTimer = nullptr;
    QThreadPool* m_savePool = nullptr;
    quint64 m_generation = 0;
    int m_pending = 0;
};

} // namespace pixelfx::services
