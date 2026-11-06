// PixelFx entry point: identity and command line (Application), then the main window.
//
// Debug hooks for headless checks (documented, not hidden; rewrite kit R-ADR-010):
//   PIXELFX_DEBUG_OPEN=<file>          open this photo at start
//   PIXELFX_DEBUG_WINDOW_SIZE=<w>x<h>  window size
//   PIXELFX_DEBUG_PRESET=<file>        apply this preset or recipe once the photo is open
//   PIXELFX_DEBUG_COMPARE=1|side       turn on before and after once the photo is open (side: side
//                                      by side)
//   PIXELFX_DEBUG_CROP=<ratio>[,<deg>] enter Crop mode with a ratio chip ("Free", "3:2") and
//                                      a straightening angle once the photo is open
//   PIXELFX_DEBUG_SHEET=<name>         with GRAB, grab the window with that sheet open: export,
//                                      settings, preset, shortcuts, watermark, look, newlook, preview,
//                                      lookpreview (the large preview opened from the look editor),
//                                      about, notices, diagnostics, whatsnew, paste
//   PIXELFX_DEBUG_TAB=presets          show the Presets tab of the left panel
//   PIXELFX_DEBUG_MENU=<submenu>       with GRAB, grab the main menu open ("-" for the menu
//                                      itself, a submenu's title such as View, or History)
//   PIXELFX_DEBUG_ZOOM=<scale>         zoom to this scale (1 is 100%) once the photo is open; with
//                                      GRAB, grab once the full-size render is shown
//   PIXELFX_DEBUG_BUSY=1               with ZOOM and GRAB, grab while the busy chip shows instead
//   PIXELFX_DEBUG_BATCH=<folder>       open Batch with this folder and process it; with GRAB, grab
//                                      when the batch is done (PIXELFX_DEBUG_BATCH_STATE=ready:
//                                      before processing, running: after the first photo)
//   PIXELFX_DEBUG_EXPAND=<group>       open this adjustment group ("Color grading") and close the
//                                      others
//   PIXELFX_DEBUG_HEAL=<x,y;x,y...>    enter Heal once the photo is open and heal a spot at each
//                                      point (fractions of the photo); "-" enters only
//   PIXELFX_DEBUG_TARGET=<x,y[,steps]> pick a colour on the photo for the Color mixer at x,y
//                                      (fractions) and, with steps, drag that far up
//   PIXELFX_DEBUG_HOVER=<name>[;<name>] preview the first preset or look as after resting on its tile;
//                                      the second's tile shows the wait half done
//   PIXELFX_DEBUG_THEME=light|dark    switch the theme once the window is up (the live switch)
//   PIXELFX_DEBUG_CLOSE=1              close the photo on show once it is open (Ctrl+W); with
//                                      GRAB, grab the next photo or the welcome page
//   PIXELFX_DEBUG_GRAB=<file.png>      grab the window once the photo is shown (or after 2 s
//                                      with nothing to show) and quit
#include "app/application.h"
#include "core/batch_resources.h"
#include "core/log_sink.h"
#include "core/looks.h"
#include "core/settings/settings.h"
#include "core/sidecar_store.h"
#include "core/watermark.h"
#include "services/batch_runner.h"
#include "ui/about_sheet.h"
#include "ui/batch_page.h"
#include "ui/canvas.h"
#include "ui/color_target_tool.h"
#include "ui/crop_panel.h"
#include "ui/export_sheet.h"
#include "ui/heal_tool.h"
#include "ui/look_editor.h"
#include "ui/looks_panel.h"
#include "ui/main_window.h"
#include "ui/paste_sheet.h"
#include "ui/photo_tile.h"
#include "ui/preset_save_sheet.h"
#include "ui/presets_panel.h"
#include "ui/preview_viewer.h"
#include "ui/settings_sheet.h"
#include "ui/shortcuts_sheet.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"
#include "ui/whats_new_sheet.h"

#include <QDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>

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

void applyHealHook(pixelfx::ui::MainWindow& window)
{
    const QString heal = qEnvironmentVariable("PIXELFX_DEBUG_HEAL");
    if (heal.isEmpty()) {
        return;
    }
    QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window, [&window, heal] {
        window.enterHeal();
        for (const QString& point : heal.split(u';', Qt::SkipEmptyParts)) {
            if (point.contains(u',')) {
                window.healTool()->addSpotAt(
                    {point.section(u',', 0, 0).toDouble(), point.section(u',', 1, 1).toDouble()});
            }
        }
    });
}

void applyTargetHook(pixelfx::ui::MainWindow& window)
{
    const QStringList target = qEnvironmentVariable("PIXELFX_DEBUG_TARGET").split(u',');
    if (target.size() < 2) {
        return;
    }
    QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window, [&window, target] {
        window.startColorTarget();
        const QPointF at(target[0].toDouble(), target[1].toDouble());
        if (target.size() > 2) {
            window.colorTargetTool()->dragAt(at, target[2].toDouble());
        }
        window.colorTargetTool()->hoverAt(at);
    });
}

void applyHoverHook(pixelfx::ui::MainWindow& window)
{
    const QStringList names = qEnvironmentVariable("PIXELFX_DEBUG_HOVER").split(u';', Qt::SkipEmptyParts);
    if (names.isEmpty()) {
        return;
    }
    QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window, [&window, names] {
        pixelfx::ui::PresetsPanel* panel = window.presetsPanel();
        pixelfx::ui::LooksPanel* looks = panel->looksPanel();
        bool isLook = false;
        for (const pixelfx::core::LookEntry& e : looks->entries()) {
            if (e.name == names.front()) {
                isLook = true;
                Q_EMIT looks->previewRequested(e.id, e.look->defaultStrength);
            }
        }
        panel->showTab(isLook ? 0 : 1);
        for (const pixelfx::core::PresetEntry& e : panel->entries()) {
            if (!isLook && e.name == names.front()) {
                Q_EMIT panel->previewRequested(&e);
            }
        }
        const std::vector<pixelfx::ui::PhotoTile*> tiles = isLook ? looks->visibleTiles() : panel->tiles();
        for (pixelfx::ui::PhotoTile* tile : tiles) {
            if (names.size() > 1 && tile->name() == names[1]) {
                tile->setWaiting(0.5);
            }
        }
    });
}

void applyExpandHook(const pixelfx::ui::MainWindow& window)
{
    const QString name = qEnvironmentVariable("PIXELFX_DEBUG_EXPAND");
    if (name.isEmpty()) {
        return;
    }
    for (auto* toggle : window.findChildren<QToolButton*>(u"groupToggle"_s)) {
        toggle->setChecked(toggle->text() == name);
    }
}

// The main menu or one of its submenus instead of the window (PIXELFX_DEBUG_MENU).
bool grabMenu(pixelfx::ui::MainWindow& window, const QString& grab)
{
    const QString name = qEnvironmentVariable("PIXELFX_DEBUG_MENU");
    if (name.isEmpty()) {
        return false;
    }
    QMenu* menu = name == u"History"_s ? window.historyMenu() : window.mainMenu();
    if (name != u"-"_s && name != u"History"_s) {
        for (const QAction* a : menu->actions()) {
            if (a->menu() != nullptr && a->text() == name) {
                menu = a->menu();
            }
        }
    }
    menu->popup(window.mapToGlobal(QPoint(window.width() - 280, 50)));
    QTimer::singleShot(300, menu, [menu, grab] {
        menu->grab().save(grab);
        QCoreApplication::quit();
    });
    return true;
}

// A sheet of the app (PIXELFX_DEBUG_SHEET), or nullptr.
QDialog* appSheet(pixelfx::ui::MainWindow& window, const QString& name)
{
    if (name == u"settings"_s) {
        return new pixelfx::ui::SettingsSheet(window.settings(), &window);
    }
    if (name == u"shortcuts"_s) {
        return new pixelfx::ui::ShortcutsSheet(&window);
    }
    if (name == u"about"_s || name == u"diagnostics"_s || name == u"notices"_s) {
        auto* about = new pixelfx::ui::AboutSheet(&window);
        auto page = pixelfx::ui::AboutSheet::DiagnosticsPage;
        if (name == u"about"_s) {
            page = pixelfx::ui::AboutSheet::AboutPage;
        } else if (name == u"notices"_s) {
            page = pixelfx::ui::AboutSheet::NoticesPage;
        }
        about->showPage(page);
        return about;
    }
    if (name == u"whatsnew"_s) {
        return new pixelfx::ui::WhatsNewSheet(pixelfx::ui::WhatsNewSheet::bundledChangelog(),
                                              QApplication::applicationVersion(), true, &window);
    }
    return nullptr;
}

// A sheet about the open photo (PIXELFX_DEBUG_SHEET), or nullptr.
QDialog* photoSheet(pixelfx::ui::MainWindow& window, const QString& name)
{
    const pixelfx::services::Document& doc = window.document();
    if (!doc.isOpen()) {
        return nullptr;
    }
    if (name == u"preset"_s) {
        auto* preset = new pixelfx::ui::PresetSaveSheet(doc.settings(), {}, &window);
        preset->setName(QStringLiteral("Warm evening"));
        preset->setPhoto(doc.preview());
        return preset;
    }
    if (name == u"paste"_s) {
        auto* paste = new pixelfx::ui::PasteSheet(doc.settings(), 6, &window);
        paste->setSource(QFileInfo(doc.info().path).fileName());
        return paste;
    }
    if (name == u"watermark"_s) { // a sample text watermark, unsaved, in the scratch profile's folder
        return new pixelfx::ui::WatermarkSheet(doc.preview(), QString(),
                                               pixelfx::core::WatermarkLibrary::defaultFolder(), &window);
    }
    if (name == u"look"_s || name == u"newlook"_s) { // a built-in look's steps, unsaved, or a new look
        pixelfx::core::LookEntry entry;
        if (name == u"look"_s) {
            entry = pixelfx::core::builtInLooks().at(3); // Film: curve, grading, grain
            entry.path.clear();
        }
        return new pixelfx::ui::LookEditor(doc.previewSource(), doc.settings(), entry,
                                           pixelfx::core::LookLibrary::defaultFolder(), &window);
    }
    if (name == u"preview"_s || name == u"lookpreview"_s) {
        // The large preview on the open photo: as three photos of a batch, or as the look editor's.
        const QImage before = doc.original();
        const QImage after = doc.preview();
        const QSize full = doc.outputSize();
        const bool look = name == u"lookpreview"_s;
        QWidget* from = &window;
        if (look) {
            from = new pixelfx::ui::LookEditor(doc.previewSource(), doc.settings(), {},
                                               pixelfx::core::LookLibrary::defaultFolder(), &window);
        }
        return new pixelfx::ui::PreviewViewer(
            look ? 1 : 3,
            [name = look ? u"My look"_s : QFileInfo(doc.info().path).fileName()](int) { return name; },
            look ? u"Look preview"_s : u"Batch preview \u00B7 Golden hour"_s,
            [before, after, full](int, int) {
                return pixelfx::ui::PreviewViewer::Photo{before, after, full};
            },
            from);
    }
    if (name == u"export"_s) {
        return window.newExportSheet();
    }
    return nullptr;
}

// A sheet instead of the window (PIXELFX_DEBUG_SHEET); false when none is asked for.
bool grabSheet(pixelfx::ui::MainWindow& window, const QString& grab)
{
    const QString name = qEnvironmentVariable("PIXELFX_DEBUG_SHEET");
    QDialog* sheet = appSheet(window, name);
    if (sheet == nullptr) {
        sheet = photoSheet(window, name);
    }
    if (sheet == nullptr) {
        return false;
    }
    sheet->show();
    // The window as seen: dimmed, with the sheet in the middle, once work the sheet starts on
    // opening (Export's size estimate) has had time to finish.
    constexpr int kSettleMs = 1500;
    QTimer::singleShot(kSettleMs, sheet, [&window, sheet, grab] {
        QPixmap shot = window.grab();
        const QPixmap front = sheet->grab();
        QPainter painter(&shot);
        painter.drawPixmap((shot.width() - front.width()) / 2, (shot.height() - front.height()) / 2, front);
        painter.end();
        shot.save(grab);
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
    const auto save = [&window, grab] {
        if (grabSheet(window, grab) || grabMenu(window, grab)) {
            return;
        }
        window.grab().save(grab);
        QCoreApplication::quit();
    };
    if (qEnvironmentVariableIsSet("PIXELFX_DEBUG_BUSY")) { // the wait for it, beside the zoom bar
        QObject::connect(window.canvas(), &pixelfx::ui::Canvas::busyChanged, &window, [save](bool shown) {
            if (shown) {
                save();
            }
        });
        return;
    }
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
    QObject::connect(&window.document(), &pixelfx::services::Document::closed, &window,
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
    // PIXELFX_DEBUG_BATCH_STATE: "ready" grabs the page before processing, "running" once the
    // first photo is done; by default the grab waits for the end.
    const QString state = qEnvironmentVariable("PIXELFX_DEBUG_BATCH_STATE");
    const auto save = [&window, grab] {
        window.grab().save(grab);
        QCoreApplication::quit();
    };
    QTimer::singleShot(200, &window, [&window, folder, state, grab, save] {
        window.enterBatch();
        window.batchPage()->addPaths({folder});
        if (state == u"ready"_s && !grab.isEmpty()) {
            QTimer::singleShot(2000, &window, save);
            return;
        }
        window.batchPage()->process();
    });
    if (state == u"running"_s && !grab.isEmpty()) {
        QObject::connect(&window.batchPage()->runner(), &pixelfx::services::BatchRunner::progress, &window,
                         [&window, save](int done) {
                             if (done == 1) {
                                 QTimer::singleShot(300, &window, save);
                             }
                         });
        return;
    }
    if (!grab.isEmpty() && state != u"ready"_s) {
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
    if (const QString theme = qEnvironmentVariable("PIXELFX_DEBUG_THEME"); !theme.isEmpty()) {
        QTimer::singleShot(0, &window, [&window, theme] {
            window.settings().setTheme(theme == u"light"_s ? pixelfx::core::Settings::Theme::Light
                                                           : pixelfx::core::Settings::Theme::Dark);
        });
    }
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
    if (const QString compare = qEnvironmentVariable("PIXELFX_DEBUG_COMPARE"); !compare.isEmpty()) {
        QObject::connect(&window.document(), &pixelfx::services::Document::opened, &window,
                         [&window, compare] {
                             window.setCompare(true);
                             window.namedAction(u"sideBySide"_s)->setChecked(compare == u"side"_s);
                         });
    }
    if (qEnvironmentVariableIsSet("PIXELFX_DEBUG_CLOSE")) {
        QObject::connect(
            &window.document(), &pixelfx::services::Document::opened, &window,
            [&window] {
                QTimer::singleShot(0, &window, [&window] { window.namedAction(u"closePhoto"_s)->trigger(); });
            },
            Qt::SingleShotConnection); // the first photo only
    }
    applyCropHook(window);
    applyExpandHook(window);
    if (qEnvironmentVariable("PIXELFX_DEBUG_TAB") == u"presets"_s) {
        window.presetsPanel()->showTab(1);
    }
    applyHealHook(window);
    applyTargetHook(window);
    applyHoverHook(window);
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
    // The user's looks and the watermark the recipe carries (presets.md section 4, version 2).
    const pixelfx::core::AdoptedResources adopted = pixelfx::core::adoptBatchResources(
        doc.value().batch, pixelfx::core::lookFolder(), pixelfx::core::WatermarkLibrary::defaultFolder());
    for (const QString& problem : adopted.problems) {
        err << "pixelfx: " << problem << '\n';
    }
    recipe.output.watermark = adopted.watermarkPath;
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
    // The theme: chosen before the window exists, then followed as the setting or, for System,
    // the desktop changes.
    const auto light = [&app] {
        const auto theme = app.settings().theme();
        return theme == pixelfx::core::Settings::Theme::Light ||
               (theme == pixelfx::core::Settings::Theme::System &&
                QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Light);
    };
    pixelfx::ui::useLightTheme(light());
    pixelfx::ui::applyStyle(app);
    QObject::connect(&app.settings(), &pixelfx::core::Settings::themeChanged, &app,
                     [&app, light] { pixelfx::ui::switchTheme(app, light()); });
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app, light] { pixelfx::ui::switchTheme(app, light()); });

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
    if (!qEnvironmentVariableIsSet("PIXELFX_DEBUG_GRAB")) {
        // Once the window has painted, never during --batch (that returned above).
        QTimer::singleShot(500, &window, [&window] { window.showWhatsNewIfUpdated(); });
    }
    return QApplication::exec();
}
