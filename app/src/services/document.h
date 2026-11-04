#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

namespace pixelfx::services {

/// What is known about the open photo's file.
struct DocumentInfo
{
    QString path;
    QSize size;     ///< full size, upright
    QString format; ///< loader name, for example "jpeg"
    int bitsPerSample = 8;
};

/// One open photo (PLAN section 6). Opening decodes and renders on a worker thread and
/// returns a display preview; only the newest request's result is delivered (ADR-006).
/// M2 renders the photo as it is; editing settings arrive with M3.
class Document : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Document)

public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    /// Starts opening `path`, rendering a preview whose long edge is at most `previewLongEdge`.
    void open(const QString& path, int previewLongEdge);

    [[nodiscard]] bool isOpen() const { return !m_info.path.isEmpty(); }
    [[nodiscard]] const DocumentInfo& info() const { return m_info; }
    [[nodiscard]] const QImage& preview() const { return m_preview; }
    [[nodiscard]] bool isBusy() const { return m_pending > 0; }

Q_SIGNALS:
    void openStarted(const QString& path);
    void opened(const pixelfx::services::DocumentInfo& info);
    void previewReady(const QImage& image);
    /// `message` is user-facing and says why, in plain words.
    void failed(const QString& path, const QString& message);

private:
    struct Result;
    /// Runs on a worker thread: probe, decode at preview size, render, convert for display.
    static Result openOnWorker(Result result, int previewLongEdge);
    void deliver(const Result& result);

    DocumentInfo m_info;
    QImage m_preview;
    quint64 m_generation = 0;
    int m_pending = 0;
};

} // namespace pixelfx::services
