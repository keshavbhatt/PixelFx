// PixelFx entry point: identity and command line (Application), then the main window.
//
// Debug hooks for headless checks (documented, not hidden; rewrite kit R-ADR-010):
//   PIXELFX_DEBUG_OPEN=<file>          open this photo at start
//   PIXELFX_DEBUG_WINDOW_SIZE=<w>x<h>  window size
//   PIXELFX_DEBUG_PRESET=<file>        apply this preset or recipe once the photo is open
//   PIXELFX_DEBUG_COMPARE=1            turn on before and after once the photo is open
//   PIXELFX_DEBUG_CROP=<ratio>[,<deg>] enter Crop mode with a ratio chip ("Free", "3:2") and
//                                      a straightening angle once the photo is open
//   PIXELFX_DEBUG_SHEET=export|settings|preset with GRAB: grab that sheet instead of the window
//   PIXELFX_DEBUG_ZOOM=<scale>         zoom to this scale (1 is 100%) once the photo is open; with
//                                      GRAB, grab once the full-size render is shown
//   PIXELFX_DEBUG_GRAB=<file.png>      grab the window once the photo is shown (or after 2 s
//                                      with nothing to show) and quit
#include "app/application.h"
#include "core/log_sink.h"
#include "core/settings/settings.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/export_sheet.h"
#include "ui/main_window.h"
#include "ui/preset_save_sheet.h"
#include "ui/settings_sheet.h"
#include "ui/style.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTimer>

#include <pfx/document.h>

using namespace Qt::StringLiterals;

namespace {

void applyCropHook(pixelfx::ui::MainWindow& window)
{
    if (const QString crop = qEnvironmentVariable("PIXELFX_DEBUG_CROP"); !crop.isEmpty()) {
        QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window, [&window, crop] {
            window.enterCrop();
            window.cropPanel()->chooseRatio(crop.section(u',', 0, 0));
            if (crop.contains(u',')) {
                window.cropPanel()->setAngle(crop.section(u',', 1, 1).toDouble());
            }
        });
    }
}

void applyGrabHook(pixelfx::ui::MainWindow& window, const QStringList& files)
{
    const QString grab = qEnvironmentVariable("PIXELFX_DEBUG_GRAB");
    if (grab.isEmpty()) {
        return;
    }
    const bool exportSheet = qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"export"_s;
    const bool settingsSheet = qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"settings"_s;
    const bool presetSheet = qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"preset"_s;
    const auto save = [&window, grab, exportSheet, settingsSheet, presetSheet] {
        if (presetSheet && window.document().isOpen()) {
            auto* sheet = new pixelfx::ui::PresetSaveSheet(window.document().settings(), {}, &window);
            sheet->setName(QStringLiteral("Warm evening"));
            sheet->show();
            QTimer::singleShot(300, sheet, [sheet, grab] {
                sheet->grab().save(grab);
                QCoreApplication::quit();
            });
            return;
        }
        if (settingsSheet) {
            auto* sheet = new pixelfx::ui::SettingsSheet(window.settings(), &window);
            sheet->show();
            QTimer::singleShot(300, sheet, [sheet, grab] {
                sheet->grab().save(grab);
                QCoreApplication::quit();
            });
            return;
        }
        if (exportSheet && window.document().isOpen()) {
            auto* sheet =
                new pixelfx::ui::ExportSheet(window.document().info().path, window.document().outputSize(),
                                             pixelfx::core::ExportOptions{}, &window);
            sheet->show();
            QTimer::singleShot(300, sheet, [sheet, grab] {
                sheet->grab().save(grab);
                QCoreApplication::quit();
            });
            return;
        }
        window.grab().save(grab);
        QCoreApplication::quit();
    };
    if (qEnvironmentVariableIsSet("PIXELFX_DEBUG_ZOOM")) { // the sharp full-size picture is what counts
        QObject::connect(&window.document(), &pixelfx::services::Document::fullReady, &window,
                         [save, &window] { QTimer::singleShot(300, &window, save); });
        return;
    }
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

void applyDebugHooks(pixelfx::ui::MainWindow& window, QStringList& files)
{
    if (const QString zoom = qEnvironmentVariable("PIXELFX_DEBUG_ZOOM"); !zoom.isEmpty()) {
        QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window, [&window, zoom] {
            QTimer::singleShot(100, &window, [&window, zoom] { window.canvas()->setZoom(zoom.toDouble()); });
        });
    }
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
    if (qEnvironmentVariableIsSet("PIXELFX_DEBUG_COMPARE")) {
        QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window,
                         [&window] { window.setCompare(true); });
    }
    applyCropHook(window);
    applyGrabHook(window, files);
}

} // namespace

int main(int argc, char** argv)
{
    pixelfx::core::LogSink::install();
    pixelfx::app::Application app(argc, argv);
    if (app.shouldExit()) {
        return app.exitCode();
    }
    // The theme is chosen before any window exists: icons are tinted when they are made.
    const auto theme = app.settings().theme();
    pixelfx::ui::useLightTheme(theme == pixelfx::core::Settings::Theme::Light ||
                               (theme == pixelfx::core::Settings::Theme::System &&
                                QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light));
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
