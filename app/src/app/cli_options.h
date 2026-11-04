#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace pixelfx::app {

/// What the command line asked for (FEATURES S4, S3).
struct CliOptions
{
    QStringList files; ///< absolute paths
    QString profile;   ///< empty: the default profile
    bool quit = false; ///< ask a running PixelFx to quit
    bool noLogFile = false;
    std::optional<QString> logFile;
};

struct CliParseResult
{
    CliOptions options;
    bool helpRequested = false;
    bool versionRequested = false;
    QString helpText;
    QString errorText;
    [[nodiscard]] bool ok() const { return errorText.isEmpty(); }
};

/// Pure parser over `arguments` (argv including the program name). Relative file paths are made
/// absolute against `workingDirectory`, so a second instance can forward them.
[[nodiscard]] CliParseResult parseCliOptions(const QStringList& arguments, const QString& workingDirectory);

/// The single-instance commands for these options, in order: {"cmd":"open","files":[...]},
/// {"cmd":"quit"}, or {"cmd":"raise"} when there is nothing else to do.
[[nodiscard]] QList<QJsonObject> commandsFor(const CliOptions& options);

} // namespace pixelfx::app
