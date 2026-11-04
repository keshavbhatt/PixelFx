// PixelFx entry point: identity and command line (Application), then the main window.
//
// Debug hooks for headless checks (documented, not hidden; rewrite kit R-ADR-010):
//   PIXELFX_DEBUG_OPEN=<file>          open this photo at start
//   PIXELFX_DEBUG_WINDOW_SIZE=<w>x<h>  window size
//   PIXELFX_DEBUG_PRESET=<file>        apply this preset or recipe once the photo is open
//   PIXELFX_DEBUG_GRAB=<file.png>      grab the window once the photo is shown (or after 2 s
//                                      with nothing to show) and quit
#include "app/application.h"
#include "core/log_sink.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QFileInfo>
#include <QTimer>

#include <pfx/document.h>

using namespace Qt::StringLiterals;

namespace {

void applyDebugHooks(pixelfx::ui::MainWindow& window, QStringList& files)
{
    if (const QString open = qEnvironmentVariable("PIXELFX_DEBUG_OPEN"); !open.isEmpty()) {
        files.prepend(QFileInfo(open).absoluteFilePath());
    }
    if (const QString size = qEnvironmentVariable("PIXELFX_DEBUG_WINDOW_SIZE"); size.contains(u'x')) {
        window.resize(size.section(u'x', 0, 0).toInt(), size.section(u'x', 1, 1).toInt());
    }
    if (const QString preset = qEnvironmentVariable("PIXELFX_DEBUG_PRESET"); !preset.isEmpty()) {
        QObject::connect(
            &window.document(), &pixelfx::services::Document::opened, &window, [&window, preset] {
                const auto doc = pfx::loadDocument(preset.toStdString());
                if (doc) {
                    window.document().apply(doc.value().settings, QFileInfo(preset).completeBaseName());
                }
            });
    }
    const QString grab = qEnvironmentVariable("PIXELFX_DEBUG_GRAB");
    if (grab.isEmpty()) {
        return;
    }
    const auto save = [&window, grab] {
        window.grab().save(grab);
        QCoreApplication::quit();
    };
    // Grab once rendering has settled (a preset re-renders after the first preview).
    QObject::connect(&window.document(), &pixelfx::services::Document::previewReady, &window,
                     [save, &window] {
                         QTimer::singleShot(400, &window, [save, &window] {
                             if (!window.document().isBusy()) {
                                 save();
                             }
                         });
                     });
    QObject::connect(&window.document(), &pixelfx::services::Document::failed, &window,
                     [save] { QTimer::singleShot(300, save); });
    if (files.isEmpty()) {
        QTimer::singleShot(2000, &window, save);
    }
}

} // namespace

int main(int argc, char** argv)
{
    pixelfx::core::LogSink::install();
    pixelfx::app::Application app(argc, argv);
    if (app.shouldExit()) {
        return app.exitCode();
    }
    pixelfx::ui::applyStyle(app);

    pixelfx::ui::MainWindow window(app.settings());
    QObject::connect(&app, &pixelfx::app::Application::openRequested, &window,
                     &pixelfx::ui::MainWindow::openFiles);
    QObject::connect(&app, &pixelfx::app::Application::quitRequested, &window,
                     &pixelfx::ui::MainWindow::close);
    QObject::connect(&app, &pixelfx::app::Application::raiseRequested, &window, [&window] {
        window.setWindowState((window.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        window.raise();
        window.activateWindow();
    });

    QStringList files = app.cliOptions().files;
    applyDebugHooks(window, files);
    window.show();
    if (!files.isEmpty()) {
        window.openFiles(files);
    }
    return QApplication::exec();
}
