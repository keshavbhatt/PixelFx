#include "app/application.h"

#include "app/logging.h"
#include "app/single_instance.h"
#include "app/version.h"
#include "core/log_sink.h"
#include "core/settings/settings.h"

#include <QDir>
#include <QIcon>
#include <QJsonArray>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace pixelfx::app {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    // Identity first: QSettings, QStandardPaths and the log location derive from it.
    setApplicationName(QString::fromLatin1(version::kApplicationName));
    setApplicationDisplayName(QString::fromLatin1(version::kDisplayName));
    setApplicationVersion(QString::fromLatin1(version::kVersion));
    setProperty("revision", QString::fromLatin1(version::kGitRevision)); // for About and Diagnostics
    setOrganizationName(QString::fromLatin1(version::kOrganizationName));
    setOrganizationDomain(QString::fromLatin1(version::kOrganizationDomain));
    setDesktopFileName(QString::fromLatin1(version::kDesktopId));
    setWindowIcon(QIcon(u":/icons/pixelfx.png"_s));

    const CliParseResult parsed = parseCliOptions(arguments(), QDir::currentPath());
    if (!parsed.ok()) {
        QTextStream(stderr) << parsed.errorText << '\n';
        m_exitCode = 2;
        return;
    }
    if (parsed.helpRequested) {
        QTextStream(stdout) << parsed.helpText;
        m_exitCode = 0;
        return;
    }
    if (parsed.versionRequested) {
        QTextStream(stdout) << applicationName() << ' ' << applicationVersion() << " ("
                            << version::kGitRevision << ")\n";
        m_exitCode = 0;
        return;
    }
    m_cli = parsed.options;
    // A named profile gets its own settings, data and cache by changing the application name.
    if (!m_cli.profile.isEmpty()) {
        setApplicationName(u"%1-%2"_s.arg(QString::fromLatin1(version::kApplicationName), m_cli.profile));
    }
    setupLogging();

    if (!m_cli.batch.isEmpty()) {
        // A command line batch runs on its own, next to any open PixelFx (main runs it).
        m_settings = std::make_unique<core::Settings>();
        return;
    }
    m_instance = std::make_unique<SingleInstance>(instanceKeyFor(m_cli.profile), this);
    if (!m_instance->isPrimary()) {
        m_exitCode = forwardToPrimary() ? 0 : 1;
        return;
    }
    if (m_cli.quit) {
        m_exitCode = 0; // nothing running to quit
        return;
    }
    connect(m_instance.get(), &SingleInstance::commandReceived, this, &Application::dispatchCommand);

    m_settings = std::make_unique<core::Settings>();
    if (m_cli.profile.isEmpty()) {
        m_settings->migrateFromVersion1(core::Settings::legacyFilePath());
    }
    qCInfo(lcApp).noquote() << u"pixelfx %1 (%2) profile=%3"_s.arg(
        QString::fromLatin1(version::kVersion), QString::fromLatin1(version::kGitRevision),
        m_cli.profile.isEmpty() ? u"default"_s : m_cli.profile);
    qCInfo(lcApp) << "settings:" << m_settings->fileName() << "log:" << core::LogSink::logFilePath();
}

Application::~Application()
{
    if (m_settings) {
        m_settings->sync();
    }
}

core::Settings& Application::settings()
{
    return *m_settings;
}

void Application::setupLogging() const
{
    core::LogSink::install();
    if (m_cli.noLogFile) {
        return;
    }
    core::LogSink::setLogFile(m_cli.logFile.value_or(core::LogSink::defaultLogFilePath()));
}

bool Application::forwardToPrimary()
{
    bool delivered = true;
    for (const QJsonObject& command : commandsFor(m_cli)) {
        delivered = m_instance->sendToPrimary(command) && delivered;
    }
    if (!delivered) {
        QTextStream(stderr) << "PixelFx is running but did not answer.\n";
    }
    return delivered;
}

void Application::dispatchCommand(const QJsonObject& command)
{
    const QString cmd = command.value(u"cmd"_s).toString();
    if (cmd == u"open"_s) {
        QStringList files;
        const QJsonArray list = command.value(u"files"_s).toArray();
        for (const auto& v : list) {
            files << v.toString();
        }
        Q_EMIT openRequested(files);
        Q_EMIT raiseRequested();
    } else if (cmd == u"quit"_s) {
        Q_EMIT quitRequested();
    } else if (cmd == u"raise"_s) {
        Q_EMIT raiseRequested();
    } else {
        qCWarning(lcApp) << "unknown command" << command;
    }
}

} // namespace pixelfx::app
