#pragma once

#include "core/export_options.h"

#include <QImage>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <pfx/image.h>
#include <pfx/io.h>
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

/// A photo rendered for export, not yet written: what Export writes and Copy to clipboard copies.
struct PreparedExport
{
    pfx::Image image;
    pfx::Metadata metadata;
    std::vector<std::uint8_t> profile; ///< the photo's own ICC profile, for Same as original
    QString error;                     ///< user-facing; empty on success
};

/// Decodes `source`, renders the settings `settingsFor` works out from the decoded photo, then
/// resizes, sharpens for output and lays the watermark under `options`. `load` can decode a smaller copy, for
/// a preview of what would be written. Blocking: call on a worker.
[[nodiscard]] PreparedExport prepareExport(const QString& source,
                                           const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                                           const core::ExportOptions& options,
                                           const pfx::LoadOptions& load = {});

/// About how many bytes Export writes under `options` for a photo `photoSize` big, from
/// `preview` (the photo as edited, any size): the preview encoded at the output size, or, for a
/// larger output, at two sizes and extended to it. -1 when it cannot tell. Blocking: call on a
/// worker.
[[nodiscard]] qint64 estimateExportSize(const QImage& preview, const QSize& photoSize,
                                        const core::ExportOptions& options);

/// Decodes `source`, renders the settings `settingsFor` works out from the decoded photo (Auto
/// enhance needs it), resizes and writes it under `options`. Blocking: call on a worker.
[[nodiscard]] ExportOutcome exportFile(const QString& source,
                                       const std::function<pfx::Settings(const pfx::Image&)>& settingsFor,
                                       const core::ExportOptions& options, core::NameFacts facts = {},
                                       core::NameClaims* claims = nullptr);

/// Writes a photo with its settings at full resolution on a worker thread (FEATURES X1, X9):
/// decode, render, resize, encode. Export is the only way PixelFx writes an image.
class Exporter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Exporter)

public:
    explicit Exporter(QObject* parent = nullptr);
    ~Exporter() override;

    /// Exports on a worker. `claims` is shared by the photos of one export of several, so two of
    /// one name never write one file.
    void start(const QString& source, const pfx::Settings& settings, const core::ExportOptions& options,
               std::shared_ptr<core::NameClaims> claims = {});
    /// The same pixels as an export, onto `copied` instead of a file (Copy to clipboard).
    void startCopy(const QString& source, const pfx::Settings& settings, const core::ExportOptions& options);
    [[nodiscard]] bool isBusy() const { return m_busy; }

Q_SIGNALS:
    void finished(const QString& target);
    /// Copy to clipboard: the exported pixels, 8-bit sRGB.
    void copied(const QImage& image);
    /// The file already existed and the policy is Skip.
    void skipped(const QString& source);
    /// `message` is user-facing and says why.
    void failed(const QString& source, const QString& message);

private:
    bool m_busy = false;
};

} // namespace pixelfx::services
