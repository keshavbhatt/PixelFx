#pragma once

#include <QDate>
#include <QSize>
#include <QString>

namespace pixelfx::core {

/// What Export writes (mock export.html; FEATURES X1, X2, X5, X6). Remembered between exports.
struct ExportOptions
{
    enum class IfExists
    {
        AddNumber,
        Overwrite,
        Skip,
    };
    QString extension = QStringLiteral("jpg"); ///< without the dot
    int quality = 90;
    int bitsPerSample = 8;
    int longEdge = 0; ///< 0: original size
    bool keepMetadata = true;
    QString folder;
    QString nameTemplate = QStringLiteral("{name}-edit");
    IfExists ifExists = IfExists::AddNumber;

    bool operator==(const ExportOptions&) const = default;
};

/// The file name for `source` under `options`, before the if-exists policy: tokens {name}
/// (the source's base name), {date} (yyyy-MM-dd), {w} and {h} (output size). Characters that
/// cannot be in a file name become "-"; an empty result falls back to {name}.
[[nodiscard]] QString exportFileName(const QString& source, const ExportOptions& options,
                                     const QSize& outputSize, const QDate& date);

/// The path to write, applying the if-exists policy, or an empty string to skip. Never the
/// source itself unless the policy is Overwrite and the template makes it so.
[[nodiscard]] QString exportTarget(const QString& source, const ExportOptions& options,
                                   const QSize& outputSize, const QDate& date);

[[nodiscard]] QString ifExistsKey(ExportOptions::IfExists value);
[[nodiscard]] ExportOptions::IfExists ifExistsFromKey(const QString& key);

} // namespace pixelfx::core
