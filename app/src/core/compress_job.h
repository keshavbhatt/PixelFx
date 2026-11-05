#pragma once

#include <QDate>
#include <QSet>
#include <QString>

#include <pfx/compress.h>

namespace pixelfx::core {

/// Batch's Compress only (batch-compress.html): how files are made smaller and where they go.
struct CompressJob
{
    pfx::CompressOptions codec;
    QString folder; ///< where the files go when they do not replace the originals
    QString nameTemplate = QStringLiteral("{name}");
    bool replaceOriginals = false;
    /// When the result is not smaller, the original's bytes are what is saved.
    bool keepIfNotSmaller = true;

    bool operator==(const CompressJob& other) const;
};

/// Pictures/PixelFx/Compressed.
[[nodiscard]] QString defaultCompressFolder();

/// The file to write for `source`: the name template ({name}, {date}) with the source's own
/// extension, in the job's folder, or the source itself when replacing the originals. A name
/// already taken gets a number, unless it is in `ours` (written by the last run of the same
/// list), which is replaced; the source is never written unless replacing.
[[nodiscard]] QString compressTarget(const QString& source, const CompressJob& job, const QSet<QString>& ours,
                                     const QDate& date);

} // namespace pixelfx::core
