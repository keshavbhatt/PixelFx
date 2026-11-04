#pragma once

#include "app/cli_options.h"

#include <QApplication>

#include <memory>
#include <optional>

namespace pixelfx::core {
class Settings;
} // namespace pixelfx::core

namespace pixelfx::app {

class SingleInstance;

/// Sets identity (names, version, profile suffix), parses the command line, owns Settings and
/// the single-instance guard, and hands them out by reference: the only sanctioned global
/// (CODING_STANDARDS section 4).
class Application : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)

public:
    Application(int& argc, char** argv);
    ~Application() override;

    [[nodiscard]] bool shouldExit() const { return m_exitCode.has_value(); }
    [[nodiscard]] int exitCode() const { return m_exitCode.value_or(0); }
    [[nodiscard]] const CliOptions& cliOptions() const { return m_cli; }
    [[nodiscard]] core::Settings& settings();

    /// Runs one single-instance command (from a second launch, or from our own command line).
    void dispatchCommand(const QJsonObject& command);

Q_SIGNALS:
    void raiseRequested();
    void openRequested(const QStringList& files);
    void quitRequested();

private:
    void setupLogging();
    [[nodiscard]] bool forwardToPrimary();

    CliOptions m_cli;
    std::optional<int> m_exitCode;
    std::unique_ptr<SingleInstance> m_instance;
    std::unique_ptr<core::Settings> m_settings;
};

} // namespace pixelfx::app
