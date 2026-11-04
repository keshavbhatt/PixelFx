#pragma once

#include "core/export_options.h"

#include <QObject>
#include <QString>

#include <pfx/settings.h>

namespace pixelfx::services {

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
