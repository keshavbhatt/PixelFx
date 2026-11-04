#include "app/cli_options.h"

#include <QCommandLineParser>
#include <QDir>
#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace pixelfx::app {

CliParseResult parseCliOptions(const QStringList& arguments, const QString& workingDirectory)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(u"Quick photo editor with looks, presets and batch editing."_s);
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();
    const QCommandLineOption profile(u"profile"_s, u"Use a separate profile (settings, data and cache)."_s,
                                     u"name"_s);
    const QCommandLineOption quit(u"quit"_s, u"Ask a running PixelFx to quit."_s);
    const QCommandLineOption logFile(u"log-file"_s, u"Write the log to this file."_s, u"path"_s);
    const QCommandLineOption noLogFile(u"no-log-file"_s, u"Do not write a log file."_s);
    const QCommandLineOption batch(
        u"batch"_s, u"Run a batch recipe (.pfxbatch) on the files, without a window."_s, u"recipe"_s);
    parser.addOptions({profile, quit, logFile, noLogFile, batch});
    parser.addPositionalArgument(u"files"_s, u"Photos to open."_s, u"[files...]"_s);

    CliParseResult result;
    result.helpText = parser.helpText();
    if (!parser.parse(arguments)) {
        result.errorText = parser.errorText();
        return result;
    }
    result.helpRequested = parser.isSet(help);
    result.versionRequested = parser.isSet(version);
    CliOptions& o = result.options;
    o.profile = parser.value(profile);
    if (!o.profile.isEmpty() && o.profile.contains(u'/')) {
        result.errorText = u"A profile name cannot contain a slash."_s;
        return result;
    }
    o.quit = parser.isSet(quit);
    if (parser.isSet(batch)) {
        o.batch = QDir::cleanPath(QDir(workingDirectory).absoluteFilePath(parser.value(batch)));
    }
    o.noLogFile = parser.isSet(noLogFile);
    if (parser.isSet(logFile)) {
        o.logFile = parser.value(logFile);
    }
    const QDir cwd(workingDirectory);
    for (const QString& file : parser.positionalArguments()) {
        o.files << QDir::cleanPath(cwd.absoluteFilePath(file));
    }
    return result;
}

QList<QJsonObject> commandsFor(const CliOptions& options)
{
    QList<QJsonObject> commands;
    if (!options.files.isEmpty()) {
        commands.append({{u"cmd"_s, u"open"_s}, {u"files"_s, QJsonArray::fromStringList(options.files)}});
    }
    if (options.quit) {
        commands.append({{u"cmd"_s, u"quit"_s}});
    }
    if (commands.isEmpty()) {
        commands.append({{u"cmd"_s, u"raise"_s}});
    }
    return commands;
}

} // namespace pixelfx::app
