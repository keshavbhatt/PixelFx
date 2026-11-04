#pragma once

#include "core/history.h"

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

#include <memory>
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

    /// Starts opening `path`, decoding a source whose long edge is at most `previewLongEdge`.
    void open(const QString& path, int previewLongEdge);

    [[nodiscard]] bool isOpen() const { return !m_info.path.isEmpty(); }
    [[nodiscard]] const DocumentInfo& info() const { return m_info; }
    /// The newest rendered preview, and the photo with no settings (for before and after).
    [[nodiscard]] const QImage& preview() const { return m_preview; }
    [[nodiscard]] const QImage& original() const { return m_original; }
    [[nodiscard]] bool isBusy() const;

    [[nodiscard]] const pfx::Settings& settings() const { return m_settings; }
    /// Changes the settings and re-renders, without a history step (live slider moves).
    void setSettings(const pfx::Settings& settings);
    /// Records the current settings as a history step named `label` (slider released, reset).
    void commit(const QString& label);
    /// Replaces the settings as one history step (a preset, pasted settings).
    void apply(const pfx::Settings& settings, const QString& label);
    void undo();
    void redo();
    [[nodiscard]] const core::History& history() const { return m_history; }

Q_SIGNALS:
    void openStarted(const QString& path);
    void opened(const pixelfx::services::DocumentInfo& info);
    void previewReady(const QImage& image);
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

    DocumentInfo m_info;
    QImage m_preview;
    QImage m_original;
    pfx::Settings m_settings;
    core::History m_history;
    RenderScheduler* m_scheduler = nullptr;
    quint64 m_generation = 0;
    int m_pending = 0;
};

} // namespace pixelfx::services
