#pragma once

#include "core/export_options.h"

#include <QObject>
#include <QString>

#include <functional>
#include <pfx/image.h>
#include <pfx/settings.h>

namespace pixelfx::services {

/// What exporting one photo did: the file written, or why not.
struct ExportOutcome
{
    QString source;
    QString target;
    QString error;
    bool skipped = false;
};

/// Decodes `source`, renders the settings `settingsFor` works out from the decoded photo (Auto
/// enhance needs it), resizes and writes it under `options`. Blocking: call on a worker.
[[nodiscard]] ExportOutcome exportFile(const QString& source,
                                       const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                                       const core::ExportOptions& options);

/// Writes a photo with its settings at full resolution on a worker thread (FEATURES X1, X9):
/// decode, render, resize, encode. Export is the only way PixelFx writes an image.
class Exporter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Exporter)

public:
    explicit Exporter(QObject* parent = nullptr);
    ~Exporter() override;

    void start(const QString& source, const pfx::Settings& settings, const core::ExportOptions& options);
    [[nodiscard]] bool isBusy() const { return m_busy; }

Q_SIGNALS:
    void finished(const QString& target);
    /// The file already existed and the policy is Skip.
    void skipped(const QString& source);
    /// `message` is user-facing and says why.
    void failed(const QString& source, const QString& message);

private:
    bool m_busy = false;
};

} // namespace pixelfx::services
