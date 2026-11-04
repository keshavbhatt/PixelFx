// PixelFx entry point: identity and command line (Application), then the main window.
//
// Debug hooks for headless checks (documented, not hidden; rewrite kit R-ADR-010):
//   PIXELFX_DEBUG_OPEN=<file>          open this photo at start
//   PIXELFX_DEBUG_WINDOW_SIZE=<w>x<h>  window size
//   PIXELFX_DEBUG_PRESET=<file>        apply this preset or recipe once the photo is open
//   PIXELFX_DEBUG_COMPARE=1            turn on before and after once the photo is open
//   PIXELFX_DEBUG_CROP=<ratio>[,<deg>] enter Crop mode with a ratio chip ("Free", "3:2") and
//                                      a straightening angle once the photo is open
//   PIXELFX_DEBUG_SHEET=export|settings|preset|shortcuts|watermark|look with GRAB: grab that sheet instead of
//   the window PIXELFX_DEBUG_ZOOM=<scale>         zoom to this scale (1 is 100%) once the photo is open; with
//                                      GRAB, grab once the full-size render is shown
//   PIXELFX_DEBUG_BATCH=<folder>       open Batch with this folder and process it; with GRAB, grab
//                                      when the batch is done
//   PIXELFX_DEBUG_GRAB=<file.png>      grab the window once the photo is shown (or after 2 s
//                                      with nothing to show) and quit
#include "app/application.h"
#include "core/log_sink.h"
#include "core/looks.h"
#include "core/settings/settings.h"
#include "core/sidecar_store.h"
#include "services/batch_runner.h"
#include "ui/batch_page.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/export_sheet.h"
#include "ui/look_editor.h"
#include "ui/main_window.h"
#include "ui/preset_save_sheet.h"
#include "ui/settings_sheet.h"
#include "ui/shortcuts_sheet.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"

#include <QDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <QTextStream>
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

// A sheet instead of the window (PIXELFX_DEBUG_SHEET); false when none is asked for.
bool grabSheet(pixelfx::ui::MainWindow& window, const QString& grab, bool exportSheet, bool settingsSheet,
               bool presetSheet)
{
    QDialog* sheet = nullptr;
    if (presetSheet && window.document().isOpen()) {
        auto* preset = new pixelfx::ui::PresetSaveSheet(window.document().settings(), {}, &window);
        preset->setName(QStringLiteral("Warm evening"));
        sheet = preset;
    } else if (settingsSheet) {
        sheet = new pixelfx::ui::SettingsSheet(window.settings(), &window);
    } else if (qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"shortcuts"_s) {
        sheet = new pixelfx::ui::ShortcutsSheet(&window);
    } else if (qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"watermark"_s && window.document().isOpen()) {
        // A sample text watermark, unsaved, in the scratch profile's folder.
        auto* mark =
            new pixelfx::ui::WatermarkSheet(window.document().preview(), QString(),
                                            pixelfx::core::WatermarkLibrary::defaultFolder(), &window);
        sheet = mark;
    } else if (qEnvironmentVariable("PIXELFX_DEBUG_SHEET") == u"look"_s && window.document().isOpen()) {
        // The look editor on a built-in look's steps, unsaved.
        pixelfx::core::LookEntry entry = pixelfx::core::builtInLooks().at(3); // Film: curve, grading, grain
        entry.path.clear();
        auto* editor =
            new pixelfx::ui::LookEditor(window.document().previewSource(), window.document().settings(),
                                        entry, pixelfx::core::LookLibrary::defaultFolder(), &window);
        sheet = editor;
    } else if (exportSheet && window.document().isOpen()) {
        sheet = new pixelfx::ui::ExportSheet(window.document().info().path, window.document().outputSize(),
                                             window.settings().exportOptions(), &window);
    }
    if (sheet == nullptr) {
        return false;
    }
    sheet->show();
    QTimer::singleShot(300, sheet, [sheet, grab] {
        sheet->grab().save(grab);
        QCoreApplication::quit();
    });
    return true;
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
        if (grabSheet(window, grab, exportSheet, settingsSheet, presetSheet)) {
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

void applyBatchHook(pixelfx::ui::MainWindow& window, const QString& grab)
{
    const QString folder = qEnvironmentVariable("PIXELFX_DEBUG_BATCH");
    if (folder.isEmpty()) {
        return;
    }
    QTimer::singleShot(200, &window, [&window, folder] {
        window.enterBatch();
        window.batchPage()->addPaths({folder});
        window.batchPage()->process();
    });
    if (!grab.isEmpty()) {
        QObject::connect(&window.batchPage()->runner(), &pixelfx::services::BatchRunner::finished, &window,
                         [&window, grab] {
                             QTimer::singleShot(300, &window, [&window, grab] {
                                 window.grab().save(grab);
                                 QCoreApplication::quit();
                             });
                         });
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
    if (qEnvironmentVariableIsSet("PIXELFX_DEBUG_BATCH")) {
        applyBatchHook(window, qEnvironmentVariable("PIXELFX_DEBUG_GRAB"));
        return;
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

// pixelfx --batch recipe.pfxbatch files...: the recipe on every file, no window; a line per
// photo, and exit code 1 when one was not saved (Batch page notes, FEATURES M6).
int runBatch(pixelfx::app::Application& app)
{
    QTextStream out(stdout);
    QTextStream err(stderr);
    const auto doc = pfx::loadDocument(app.cliOptions().batch.toStdString());
    if (!doc || doc.value().kind != pfx::DocumentKind::Batch) {
        err << "pixelfx: "
            << (doc ? u"this is not a batch recipe"_s : QString::fromStdString(doc.error().message)) << '\n';
        return 2;
    }
    QStringList files;
    for (const QString& file : app.cliOptions().files) {
        if (pixelfx::ui::MainWindow::isSupported(file)) {
            files << file;
        } else {
            err << "pixelfx: skipped " << file << ": not a photo PixelFx reads\n";
        }
    }
    if (files.isEmpty()) {
        err << "pixelfx: no photos to process\n";
        return 2;
    }
    pixelfx::services::BatchRecipe recipe = pixelfx::services::recipeFromBody(
        doc.value().batch,
        app.settings().rememberEdits() ? pixelfx::core::SidecarStore::defaultFolder() : QString());
    if (recipe.output.folder.isEmpty()) {
        recipe.output.folder = app.settings().exportOptions().folder;
    }
    pixelfx::services::BatchRunner runner;
    QObject::connect(&runner, &pixelfx::services::BatchRunner::photoChanged, &runner, [&](int i) {
        using S = pixelfx::services::BatchRunner::State;
        if (runner.state(i) == S::Saved) {
            out << files.at(i) << " -> " << runner.target(i) << '\n';
        } else if (runner.state(i) == S::Failed || runner.state(i) == S::Skipped) {
            out << files.at(i) << ": not saved. " << runner.reason(i) << '\n';
        }
        out.flush();
    });
    int code = 0;
    QObject::connect(&runner, &pixelfx::services::BatchRunner::finished, &runner, [&](int saved, int failed) {
        out << saved << " saved, " << failed << " not saved\n";
        code = failed > 0 ? 1 : 0;
        QCoreApplication::quit();
    });
    runner.start(files, recipe);
    QApplication::exec();
    return code;
}

} // namespace

int main(int argc, char** argv)
{
    pixelfx::core::LogSink::install();
    pixelfx::app::Application app(argc, argv);
    if (app.shouldExit()) {
        return app.exitCode();
    }
    if (!app.cliOptions().batch.isEmpty()) {
        try {
            return runBatch(app);
        } catch (const std::exception& e) {
            QTextStream(stderr) << "pixelfx: the batch stopped: " << e.what() << '\n';
            return 2;
        }
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
