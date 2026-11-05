#include "ui/main_window.h"

#include "core/crop_geometry.h"
#include "core/looks.h"
#include "core/settings/settings.h"
#include "core/settings_clipboard.h"
#include "ui/about_sheet.h"
#include "ui/adjust_panel.h"
#include "ui/batch_page.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/export_sheet.h"
#include "ui/filmstrip.h"
#include "ui/heal_panel.h"
#include "ui/heal_tool.h"
#include "ui/icons.h"
#include "ui/look_editor.h"
#include "ui/looks_panel.h"
#include "ui/paste_sheet.h"
#include "ui/preset_save_sheet.h"
#include "ui/presets_panel.h"
#include "ui/settings_sheet.h"
#include "ui/shortcuts_sheet.h"
#include "ui/straighten_dial.h"
#include "ui/style.h"
#include "ui/welcome_page.h"
#include "ui/whats_new_sheet.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QConicalGradient>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <pfx/auto.h>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// Preset pictures come back keyed by generation and row, so late ones from an older list are
// dropped.
constexpr int kThumbnailKeySpan = 10000;
constexpr int kThumbnailGenerations = 20000;

/// What the image library in this build reads (HEIC, JPEG XL, RAW only where it has them).
const QStringList& supportedSuffixes()
{
    static const QStringList suffixes = [] {
        QStringList out;
        for (const std::string& ext : pfx::readableExtensions()) {
            out << QString::fromStdString(ext).mid(1);
        }
        return out;
    }();
    return suffixes;
}

/// The colour-wheel mark of the header (mock: .brand .mark).
class BrandMark : public QWidget
{
public:
    explicit BrandMark(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(22, 22);
        setAccessibleName(QStringLiteral("PixelFx"));
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QConicalGradient wheel(rect().center(), 90);
        const QList<QColor> stops = {QColor(0xFF5A4E), QColor(0xE04FA0), QColor(0x8B5CF6), QColor(0x3B82F6),
                                     QColor(0x3FC1C9), QColor(0x6CCB5F), QColor(0xF7E35A), QColor(0xFFB84D)};
        for (qsizetype i = 0; i < stops.size(); ++i) {
            wheel.setColorAt(static_cast<double>(i) / static_cast<double>(stops.size()), stops.at(i));
        }
        wheel.setColorAt(1.0, stops.first());
        p.setPen(Qt::NoPen);
        p.setBrush(wheel);
        p.drawEllipse(rect().adjusted(1, 1, -1, -1));
    }
};

QToolButton* headerButton(QWidget* parent, const QString& glyph, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setObjectName(u"headerButton"_s);
    b->setIcon(icons::themed(glyph, tokens().text, tokens().faint));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    b->setAutoRaise(true);
    return b;
}

QString describe(const services::DocumentInfo& info)
{
    QString format = info.format.endsWith(u"load"_s) ? info.format.chopped(4).toUpper() : info.format;
    const QString suffix = QFileInfo(info.path).suffix().toUpper();
    if (format == u"HEIF"_s && !suffix.isEmpty()) {
        format = suffix; // one loader for HEIC, HEIF and AVIF: name the file's own kind
    } else if (format == u"DCRAW"_s) {
        format = u"RAW (%1)"_s.arg(suffix); // camera raw: say which maker's format
    }
    const QString size =
        QLocale().formattedDataSize(QFileInfo(info.path).size(), 1, QLocale::DataSizeSIFormat);
    return u"%1 x %2 · %3 · %4"_s.arg(info.size.width()).arg(info.size.height()).arg(format, size);
}

} // namespace

MainWindow::MainWindow(core::Settings& settings, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
{
    setupUi();
    setupActions();
    setAcceptDrops(true);
    setMinimumSize(1024, 640);
    if (!restoreGeometry(m_settings.windowGeometry())) {
        resize(1280, 820);
    }
    restoreState(m_settings.windowState());

    applyGeneralSettings();
    connectPresets();
    connectLooks();
    connectFilmstrip();
    connectDocument();
    connectCrop();
    connectZoom();
    connectClipboard();
    connectWhiteBalance();
    buildMainMenu(); // last: it lists the actions made above
    updateHeader();
}

void MainWindow::connectDocument()
{
    connect(&m_document, &services::Document::openStarted, this, [this](const QString& path) {
        leaveTools();
        if (isBatching()) {
            leaveBatch();
        }
        m_stack->setCurrentWidget(m_canvasPage);
        m_canvas->setStatus(tr("Opening %1").arg(QFileInfo(path).fileName()));
    });
    connect(&m_document, &services::Document::opened, this, &MainWindow::showOpened);
    connect(&m_document, &services::Document::opened, this,
            [this] { m_canvas->setImage(m_document.preview(), m_document.displaySize()); });
    connect(&m_document, &services::Document::previewReady, this, [this](const QImage& image) {
        if (isHealing()) {
            showHealImage(image);
        } else if (!m_showingOriginal) {
            m_canvas->setImage(image, m_document.displaySize());
        }
    });
    connect(&m_document, &services::Document::originalReady, this, [this](const QImage& image) {
        m_canvas->setOriginal(image);
        if (m_showingOriginal) {
            m_canvas->showImage(image);
        }
    });
    connect(&m_document, &services::Document::failed, this, &MainWindow::showFailed);
    connect(&m_document, &services::Document::settingsReplaced, m_adjust, &AdjustPanel::setSettings);
    connect(&m_document, &services::Document::historyChanged, this, &MainWindow::updateHistoryButtons);
    connect(&m_document, &services::Document::histogramReady, m_adjust, &AdjustPanel::setHistogram);
    connect(m_adjust, &AdjustPanel::settingsChanged, &m_document, &services::Document::setSettings);
    connect(m_adjust, &AdjustPanel::editFinished, &m_document, &services::Document::commit);
    connect(m_adjust, &AdjustPanel::autoRequested, &m_document, &services::Document::autoAdjust);
    connect(&m_exporter, &services::Exporter::finished, this, [this](const QString& target) {
        ++m_exportSaved;
        if (m_exportTotal <= 1) {
            showToast(tr("Saved %1").arg(QFileInfo(target).fileName()));
        }
        exportNext();
    });
    connect(&m_exporter, &services::Exporter::skipped, this, [this] {
        if (m_exportTotal <= 1) {
            showToast(tr("Not saved: a file with that name exists, and Skip is chosen."));
        }
        exportNext();
    });
    connect(&m_exporter, &services::Exporter::failed, this,
            [this](const QString& source, const QString& message) {
                if (m_copying) {
                    m_copying = false;
                    showToast(tr("Not copied. %1").arg(message));
                    return;
                }
                showToast(tr("%1 not saved. %2").arg(QFileInfo(source).fileName(), message));
                exportNext();
            });
    connect(&m_exporter, &services::Exporter::copied, this, [this](const QImage& image) {
        m_copying = false;
        QGuiApplication::clipboard()->setImage(image);
        showToast(tr("Copied to the clipboard, %1 x %2").arg(image.width()).arg(image.height()));
    });
}

void MainWindow::exportPhoto()
{
    if (!m_document.isOpen() || m_exporter.isBusy()) {
        return;
    }
    leaveTools();
    // With a selection, every selected photo goes out with its own settings (the current one
    // from the editor, the others from their remembered edits).
    QList<ExportJob> jobs;
    const QList<int> selected = m_strip->selection();
    const core::SidecarStore store(core::SidecarStore::defaultFolder());
    for (const int index : selected) {
        const QString path = m_strip->files().at(index);
        if (path == m_document.info().path) {
            jobs.append({path, m_document.settings()});
        } else {
            jobs.append({path, m_settings.rememberEdits()
                                   ? store.read(core::fileFingerprint(path)).value_or(pfx::Settings{})
                                   : pfx::Settings{}});
        }
    }
    if (jobs.isEmpty()) {
        jobs.append({m_document.info().path, m_document.settings()});
    }
    ExportSheet sheet(m_document.info().path, m_document.outputSize(), m_settings.exportOptions(), this);
    sheet.setPhotoCount(static_cast<int>(jobs.size()));
    sheet.setPreview(m_document.preview());
    const int result = sheet.exec();
    if (result == ExportSheet::kCopyToClipboard) {
        m_settings.setExportOptions(sheet.options());
        m_copying = true;
        m_exporter.startCopy(m_document.info().path, m_document.settings(), sheet.options());
        return;
    }
    if (result != QDialog::Accepted) {
        return;
    }
    m_exportOptions = sheet.options();
    m_settings.setExportOptions(m_exportOptions);
    m_exportQueue = jobs;
    m_exportTotal = static_cast<int>(jobs.size());
    m_exportSaved = 0;
    exportNext();
}

void MainWindow::exportNext()
{
    if (m_exportQueue.isEmpty()) {
        if (m_exportTotal > 1) {
            showToast(tr("Saved %1 of %2 photos").arg(m_exportSaved).arg(m_exportTotal));
        }
        return;
    }
    const ExportJob job = m_exportQueue.takeFirst();
    const int number = m_exportTotal - static_cast<int>(m_exportQueue.size());
    showToast(m_exportTotal > 1 ? tr("Exporting %1 of %2").arg(number).arg(m_exportTotal)
                                : tr("Exporting %1").arg(QFileInfo(job.path).fileName()));
    m_exporter.start(job.path, job.settings, m_exportOptions);
}

void MainWindow::showToast(const QString& text)
{
    m_toast->setText(text);
    m_toast->adjustSize();
    m_toast->move((m_canvas->width() - m_toast->width()) / 2, 12);
    m_toast->show();
    m_toast->raise();
    QTimer::singleShot(4000, m_toast, [toast = m_toast, text] {
        if (toast->text() == text) {
            toast->hide();
        }
    });
}

void MainWindow::updateHistoryButtons()
{
    const core::History& h = m_document.history();
    m_history->setEnabled(m_document.isOpen());
    m_undo->setEnabled(h.canUndo());
    m_redo->setEnabled(h.canRedo());
    m_undo->setToolTip(h.canUndo() ? tr("Undo %1 (Ctrl+Z)").arg(h.undoLabel()) : tr("Undo (Ctrl+Z)"));
    m_redo->setToolTip(h.canRedo() ? tr("Redo %1 (Ctrl+Shift+Z)").arg(h.redoLabel())
                                   : tr("Redo (Ctrl+Shift+Z)"));
}

void MainWindow::setCompare(bool on)
{
    if (m_document.isOpen()) {
        m_compare->setChecked(on);
    }
}

void MainWindow::showHistory()
{
    const core::History& h = m_document.history();
    QMenu menu(this);
    menu.addSection(tr("History"));
    const QStringList labels = h.labels();
    for (int i = static_cast<int>(labels.size()) - 1; i >= 0; --i) { // newest first, as in history.html
        QAction* step = menu.addAction(labels.at(i));
        step->setCheckable(true);
        step->setChecked(i == h.currentIndex());
        connect(step, &QAction::triggered, this, [this, i] { m_document.goToStep(i); });
    }
    // Snapshots: named states kept with the edit (history.html).
    const auto& snapshots = m_document.snapshots();
    if (!snapshots.empty()) {
        menu.addSection(tr("Snapshots"));
        for (std::size_t i = 0; i < snapshots.size(); ++i) {
            connect(menu.addAction(QString::fromStdString(snapshots[i].name)), &QAction::triggered, this,
                    [this, i] { m_document.applySnapshot(i); });
        }
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Save snapshot...")), &QAction::triggered, this, [this] {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Save snapshot"), tr("Name"), QLineEdit::Normal,
                                  tr("Snapshot %1").arg(m_document.snapshots().size() + 1), &ok);
        if (ok) {
            m_document.saveSnapshot(name);
            showToast(tr("Saved snapshot %1").arg(name.trimmed()));
        }
    });
    menu.exec(m_history->mapToGlobal(QPoint(0, m_history->height())));
}

void MainWindow::showOriginal(bool on)
{
    if (!m_document.isOpen() || on == m_showingOriginal || (on && isInTool())) {
        return;
    }
    m_showingOriginal = on;
    m_canvas->showImage(on ? m_document.original() : m_document.preview());
    if (on) {
        m_canvas->setDetail(QImage()); // the sharp picture is of the edit
    } else {
        m_canvas->setDetail(m_document.fullImage());
    }
    m_original->setDown(on);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildEditButtons()
{
    m_undo = headerButton(m_header, u"undo"_s, tr("Undo (Ctrl+Z)"));
    m_redo = headerButton(m_header, u"redo"_s, tr("Redo (Ctrl+Shift+Z)"));
    m_history = headerButton(m_header, u"history"_s, tr("History (H)"));
    m_history->setEnabled(false);
    connect(m_history, &QToolButton::clicked, this, &MainWindow::showHistory);
    m_compare = headerButton(m_header, u"compare"_s, tr("Before and after (\\)"));
    m_compare->setCheckable(true);
    m_compare->setEnabled(false);
    m_original = headerButton(m_header, u"original"_s, tr("Show the original while held (Space)"));
    connect(m_undo, &QToolButton::clicked, &m_document, &services::Document::undo);
    connect(m_redo, &QToolButton::clicked, &m_document, &services::Document::redo);
    connect(m_original, &QToolButton::pressed, this, [this] { showOriginal(true); });
    connect(m_original, &QToolButton::released, this, [this] { showOriginal(false); });
    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
    m_original->setEnabled(false);
    m_export = new QPushButton(icons::themed(u"export"_s, tokens().text), tr("Export"), m_header);
    m_export->setToolTip(tr("Save a copy with these settings (Ctrl+Shift+E)"));
    m_export->setEnabled(false);
    connect(m_export, &QPushButton::clicked, this, &MainWindow::exportPhoto);
}

void MainWindow::buildHeader()
{
    m_header = new QWidget(this);
    m_header->setObjectName(u"header"_s);
    m_header->setAttribute(Qt::WA_StyledBackground);
    m_header->setFixedHeight(50);
    auto* open = headerButton(m_header, u"open"_s, tr("Open images (Ctrl+O)"));
    connect(open, &QToolButton::clicked, this, &MainWindow::chooseFiles);
    m_docName = new QLabel(m_header);
    m_docName->setObjectName(u"docName"_s);
    m_docMeta = new QLabel(m_header);
    m_docMeta->setObjectName(u"docMeta"_s);
    auto* doc = new QVBoxLayout;
    doc->setSpacing(0);
    doc->addWidget(m_docName);
    doc->addWidget(m_docMeta);
    buildEditButtons();
    m_menuButton = headerButton(m_header, u"menu"_s, tr("Main menu (F10)"));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);

    // Thin separators between the groups (mock .header .sep).
    const auto separator = [this] {
        auto* line = new QFrame(m_header);
        line->setObjectName(u"headerSep"_s);
        line->setFixedSize(1, 24);
        return line;
    };
    QWidget* sepHistory = separator();
    QWidget* sepView = separator();
    // Batch's own: Add photos and Add folder (mock batch.html).
    auto* addPhotos = new QPushButton(icons::themed(u"plus"_s, tokens().text), tr("Add photos"), m_header);
    auto* addFolder = new QPushButton(icons::themed(u"folder"_s, tokens().text), tr("Add folder"), m_header);
    addFolder->setToolTip(tr("Every photo in a folder, subfolders included"));
    connect(addPhotos, &QPushButton::clicked, this, [this] { m_batch->chooseFiles(); });
    connect(addFolder, &QPushButton::clicked, this, [this] { m_batch->chooseFolder(); });
    m_batchHeader = {addPhotos, addFolder};
    auto* header = new QHBoxLayout(m_header);
    header->setContentsMargins(12, 0, 10, 0);
    header->setSpacing(6);
    header->addWidget(new BrandMark(m_header));
    m_wordmark = new QLabel(u"PixelFx"_s, m_header); // the start page's header (mock welcome.html)
    m_wordmark->setObjectName(u"wordmark"_s);
    header->addWidget(m_wordmark);
    header->addSpacing(4);
    header->addWidget(open);
    header->addSpacing(4);
    header->addLayout(doc);
    header->addStretch();
    header->addWidget(m_undo);
    header->addWidget(m_redo);
    header->addWidget(m_history);
    header->addWidget(sepHistory);
    header->addWidget(m_compare);
    header->addWidget(m_original);
    header->addWidget(sepView);
    header->addWidget(m_export);
    header->addWidget(addPhotos);
    header->addWidget(addFolder);
    header->addWidget(m_menuButton);
    m_editHeader = {open, m_undo, m_redo, m_history, sepHistory, m_compare, m_original, sepView, m_export};
    for (QWidget* w : std::as_const(m_batchHeader)) {
        w->hide();
    }
}

void MainWindow::buildBatch()
{
    BatchPage::setPhotoTest(&MainWindow::isSupported);
    m_batch = new BatchPage(m_settings.exportOptions(), this);
    connect(m_batch, &BatchPage::outputChanged, this,
            [this](const core::ExportOptions& output) { m_settings.setExportOptions(output); });
    connect(m_batch, &BatchPage::looksAdded, this, &MainWindow::refreshLooks);
    connect(m_batch, &BatchPage::summaryChanged, this, [this](const QString& summary) {
        if (isBatching()) {
            m_docMeta->setText(summary);
        }
    });
    // The window title shows a running batch, so it can be followed from the task bar.
    connect(&m_batch->runner(), &services::BatchRunner::progress, this,
            [this](int done, int total) { setWindowTitle(tr("Batch %1 of %2").arg(done).arg(total)); });
    connect(&m_batch->runner(), &services::BatchRunner::finished, this, [this](int saved, int failed) {
        setWindowTitle(m_document.isOpen() ? QFileInfo(m_document.info().path).fileName() : QString());
        if (!isBatching()) {
            showToast(failed > 0 ? tr("Batch done: %1 saved, %2 not saved").arg(saved).arg(failed)
                                 : tr("Batch done: %n saved", nullptr, saved));
        }
    });
    m_stack->addWidget(m_batch);
}

void MainWindow::buildWelcome()
{
    m_welcome = new WelcomePage(this);
    connect(m_welcome, &WelcomePage::openRequested, this, &MainWindow::chooseFiles);
    connect(m_welcome, &WelcomePage::batchRequested, this, [this] { enterBatch(); });
    connect(m_welcome, &WelcomePage::openFolderRequested, this, [this] {
        const QString folder =
            QFileDialog::getExistingDirectory(this, tr("Open folder"), m_settings.lastOpenFolder());
        if (!folder.isEmpty()) {
            m_settings.setLastOpenFolder(folder);
            enterBatch();
            m_batch->addPaths({folder});
        }
    });
    connect(m_welcome, &WelcomePage::recentOpened, this, [this](const QString& path) { openFiles({path}); });
    connect(m_welcome, &WelcomePage::clearRecentRequested, this, [this] { m_settings.clearRecentFiles(); });
    connect(&m_settings, &core::Settings::recentFilesChanged, this, [this](const QStringList& files) {
        if (m_stack->currentWidget() == m_welcome) { // nothing decoded behind the editor
            m_welcome->setRecentFiles(files);
        }
    });
    m_welcome->setSidecarFolder(core::SidecarStore::defaultFolder());
    m_welcome->setRecentFiles(m_settings.recentFiles());
}

void MainWindow::setupUi()
{
    buildHeader();

    buildWelcome();
    m_canvasPage = buildCanvasPage();
    connect(m_compare, &QToolButton::toggled, m_canvas, &Canvas::setCompare);
    m_toast = new QLabel(m_canvas);
    m_toast->setObjectName(u"toast"_s);
    m_toast->setAttribute(Qt::WA_StyledBackground);
    m_toast->hide();
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_canvasPage);
    buildBatch();
    connect(m_stack, &QStackedWidget::currentChanged, this, &MainWindow::updateHeader);

    m_adjust = new AdjustPanel(this);
    m_cropPanel = new CropPanel(this);
    m_panels = new QStackedWidget(this);
    m_panels->addWidget(m_adjust);
    m_panels->addWidget(m_cropPanel);
    m_panels->setFixedWidth(m_adjust->width());
    m_panels->hide();
    buildRail();
    m_presets = new PresetsPanel(this);
    m_presets->hide();
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    // Docks around the panels, so Tab can hide them without touching what each mode shows.
    const auto dock = [this](QWidget* inside) {
        auto* d = new QWidget(this);
        auto* l = new QHBoxLayout(d);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(inside);
        return d;
    };
    m_leftDock = dock(m_presets);
    m_rightDock = dock(m_panels);
    body->addWidget(m_rail);
    body->addWidget(m_leftDock);
    body->addWidget(m_stack, 1);
    body->addWidget(m_rightDock);

    auto* central = new QWidget(this);
    auto* column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_header);
    column->addLayout(body, 1);
    setCentralWidget(central);
    setWindowTitle(QString());
}

void MainWindow::setupActions()
{
    auto* openAction = new QAction(tr("Open images"), this);
    openAction->setObjectName(u"open"_s);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::chooseFiles);
    auto* quitAction = new QAction(tr("Quit"), this);
    quitAction->setObjectName(u"quit"_s);
    quitAction->setShortcut(QKeySequence(u"Ctrl+Q"_s));
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
    // F11 explicitly: under the KDE platform theme QKeySequence::FullScreen is Ctrl+Shift+F
    // (rewrite kit, rules learned).
    auto* fullScreen = new QAction(tr("Full screen"), this);
    fullScreen->setObjectName(u"fullScreen"_s);
    fullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullScreen, &QAction::triggered, this,
            [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    auto* exportAction = new QAction(tr("Export"), this);
    exportAction->setObjectName(u"export"_s);
    exportAction->setShortcut(QKeySequence(u"Ctrl+Shift+E"_s));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportPhoto);
    auto* shortcuts = new QAction(tr("Keyboard shortcuts"), this);
    shortcuts->setObjectName(u"shortcuts"_s);
    shortcuts->setShortcuts({QKeySequence(Qt::Key_F1), QKeySequence(u"Ctrl+/"_s)});
    connect(shortcuts, &QAction::triggered, this, [this] {
        ShortcutsSheet sheet(this);
        sheet.exec();
    });
    auto* whatsNew = new QAction(tr("What's new"), this);
    whatsNew->setObjectName(u"whatsNew"_s);
    connect(whatsNew, &QAction::triggered, this, &MainWindow::showWhatsNew);
    auto* about = new QAction(tr("About PixelFx"), this);
    about->setObjectName(u"about"_s);
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
    addActions({whatsNew, about});
    auto* openFolder = new QAction(tr("Open folder"), this);
    openFolder->setObjectName(u"openFolder"_s);
    openFolder->setShortcut(QKeySequence(u"Ctrl+Shift+O"_s));
    connect(openFolder, &QAction::triggered, m_welcome, &WelcomePage::openFolderRequested);
    addActions({shortcuts, openFolder});
    auto* settingsAction = new QAction(tr("Settings"), this);
    settingsAction->setObjectName(u"settings"_s);
    settingsAction->setShortcut(QKeySequence(u"Ctrl+,"_s));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
    auto* clipping = new QAction(tr("Clipping warnings"), this);
    clipping->setObjectName(u"clipping"_s);
    clipping->setShortcut(QKeySequence(Qt::Key_J));
    connect(clipping, &QAction::triggered, this,
            [this] { m_settings.setClippingWarnings(!m_settings.clippingWarnings()); });
    addActions({openAction, quitAction, fullScreen, exportAction, settingsAction, clipping});
    setupEditActions();
    setupCropActions();
    setupHeal();
    setupZoomActions();
}

void MainWindow::setupEditActions()
{
    auto* undo = new QAction(tr("Undo"), this);
    undo->setObjectName(u"undo"_s);
    undo->setShortcut(QKeySequence::Undo);
    connect(undo, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.undo();
        }
    });
    auto* redo = new QAction(tr("Redo"), this);
    redo->setObjectName(u"redo"_s);
    redo->setShortcuts({QKeySequence(u"Ctrl+Shift+Z"_s), QKeySequence(u"Ctrl+Y"_s)});
    connect(redo, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.redo();
        }
    });
    auto* resetAll = new QAction(tr("Reset all"), this);
    resetAll->setObjectName(u"resetAll"_s);
    resetAll->setShortcut(QKeySequence(u"Ctrl+R"_s));
    connect(resetAll, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            m_adjust->resetAll();
        }
    });
    auto* history = new QAction(tr("History"), this);
    history->setObjectName(u"history"_s);
    history->setShortcut(QKeySequence(Qt::Key_H));
    connect(history, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            showHistory();
        }
    });
    auto* autoAction = new QAction(tr("Auto"), this);
    autoAction->setObjectName(u"auto"_s);
    autoAction->setShortcut(QKeySequence(u"Ctrl+Shift+A"_s));
    connect(autoAction, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.autoAdjust();
        }
    });
    addAction(autoAction);
    auto* compare = new QAction(tr("Before and after"), this);
    compare->setObjectName(u"compare"_s);
    compare->setShortcut(QKeySequence(Qt::Key_Backslash));
    connect(compare, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            m_compare->toggle();
        }
    });
    addActions({undo, redo, resetAll, history, compare});
}

void MainWindow::setupZoomActions()
{
    // Shortcuts as in the shortcuts sheet: Ctrl+0 fit, Ctrl+1 100%, Ctrl+Plus and Ctrl+Minus.
    const auto add = [this](const QString& id, const QString& name, const QList<QKeySequence>& keys,
                            auto slot) {
        auto* a = new QAction(name, this);
        a->setObjectName(id);
        a->setShortcuts(keys);
        connect(a, &QAction::triggered, this, [this, slot] {
            if (m_document.isOpen() && !isCropping()) {
                slot();
            }
        });
        addAction(a);
    };
    add(u"fit"_s, tr("Fit"), {QKeySequence(u"Ctrl+0"_s)}, [this] { m_canvas->zoomToFit(); });
    add(u"actualSize"_s, tr("Actual size"), {QKeySequence(u"Ctrl+1"_s)}, [this] { m_canvas->setZoom(1.0); });
    add(u"zoomIn"_s, tr("Zoom in"), {QKeySequence::ZoomIn, QKeySequence(u"Ctrl+="_s)},
        [this] { m_canvas->zoomIn(); });
    add(u"zoomOut"_s, tr("Zoom out"), {QKeySequence::ZoomOut}, [this] { m_canvas->zoomOut(); });
}

void MainWindow::setupCropActions()
{
    // Mode keys of the shortcuts sheet: E and C.
    auto* edit = new QAction(tr("Edit"), this);
    edit->setObjectName(u"edit"_s);
    edit->setShortcut(QKeySequence(Qt::Key_E));
    connect(edit, &QAction::triggered, this, [this] {
        leaveTools();
        leaveBatch();
    });
    auto* batch = new QAction(tr("Batch"), this);
    batch->setObjectName(u"batch"_s);
    batch->setShortcut(QKeySequence(Qt::Key_B));
    connect(batch, &QAction::triggered, this, &MainWindow::enterBatch);
    addAction(batch);
    auto* openRecipe = new QAction(tr("Open batch recipe..."), this);
    openRecipe->setObjectName(u"openBatchRecipe"_s);
    connect(openRecipe, &QAction::triggered, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Open batch recipe"), QString(),
                                                          tr("Batch recipes (*.pfxbatch)"));
        if (!file.isEmpty()) {
            enterBatch();
            if (const QString error = m_batch->loadRecipe(file); !error.isEmpty()) {
                showToast(error);
            }
        }
    });
    addAction(openRecipe);
    addAction(edit);
    auto* crop = new QAction(tr("Crop"), this);
    crop->setObjectName(u"crop"_s);
    crop->setShortcut(QKeySequence(Qt::Key_C));
    connect(crop, &QAction::triggered, this, [this] { isCropping() ? leaveCrop(true) : enterCrop(); });
    auto* cropDone = new QAction(tr("Apply the crop"), this);
    cropDone->setShortcuts({QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    connect(cropDone, &QAction::triggered, this, [this] { leaveCrop(true); });
    auto* cropCancel = new QAction(tr("Cancel the crop"), this);
    cropCancel->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(cropCancel, &QAction::triggered, this, [this] { leaveCrop(false); });
    auto* cropSwap = new QAction(tr("Swap landscape and portrait"), this);
    cropSwap->setShortcut(QKeySequence(Qt::Key_X));
    connect(cropSwap, &QAction::triggered, m_cropPanel, &CropPanel::swapOrientation);
    m_cropActions = {cropDone, cropCancel, cropSwap};
    for (QAction* a : std::as_const(m_cropActions)) {
        a->setEnabled(false);
    }
    addActions({crop, cropDone, cropCancel, cropSwap});
}

void MainWindow::buildRail()
{
    m_rail = new QWidget(this);
    m_rail->setObjectName(u"rail"_s);
    m_rail->setAttribute(Qt::WA_StyledBackground);
    m_rail->setFixedWidth(56);
    // Shown from the start: Batch works without a photo open (mock welcome.html has the rail).
    const auto railButton = [this](const QString& glyph, const QString& text, const QString& tip) {
        auto* b = new QToolButton(m_rail);
        b->setObjectName(u"railButton"_s);
        QIcon icon;
        for (const qreal dpr : {1.0, 2.0}) {
            icon.addPixmap(icons::pixmap(glyph, tokens().muted, 20, dpr), QIcon::Normal, QIcon::Off);
            icon.addPixmap(icons::pixmap(glyph, tokens().accent, 20, dpr), QIcon::Normal, QIcon::On);
            icon.addPixmap(icons::pixmap(glyph, tokens().faint, 20, dpr), QIcon::Disabled, QIcon::Off);
        }
        b->setIcon(icon);
        b->setIconSize(QSize(20, 20));
        b->setText(text);
        b->setToolTip(tip);
        b->setAccessibleName(text);
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setFixedSize(44, 48);
        return b;
    };
    m_editMode = railButton(u"sliders"_s, tr("Edit"), tr("Edit (E)"));
    m_editMode->setChecked(true);
    m_cropMode = railButton(u"crop"_s, tr("Crop"), tr("Crop and rotate (C)"));
    m_cropMode->setEnabled(false);
    m_healMode = railButton(u"bandage"_s, tr("Heal"), tr("Heal blemishes, dust and small objects (R)"));
    m_healMode->setEnabled(false);
    m_batchMode = railButton(u"stack"_s, tr("Batch"), tr("Batch: one recipe for many photos (B)"));
    connect(m_editMode, &QToolButton::clicked, this, [this] {
        leaveTools();
        leaveBatch();
    });
    connect(m_batchMode, &QToolButton::clicked, this, &MainWindow::enterBatch);
    connect(m_cropMode, &QToolButton::clicked, this, &MainWindow::enterCrop);
    auto* column = new QVBoxLayout(m_rail);
    column->setContentsMargins(6, 8, 6, 8);
    column->setSpacing(4);
    column->addWidget(m_editMode);
    column->addWidget(m_cropMode);
    column->addWidget(m_healMode);
    column->addWidget(m_batchMode);
    column->addStretch();
    // Settings at the foot of the rail (mock: the gear), as Ctrl+, and the main menu.
    auto* gear = new QToolButton(m_rail);
    gear->setObjectName(u"railButton"_s);
    gear->setIcon(icons::themed(u"settings"_s, tokens().muted));
    gear->setIconSize(QSize(20, 20));
    gear->setFixedSize(44, 40);
    gear->setToolTip(tr("Settings (Ctrl+,)"));
    gear->setAccessibleName(tr("Settings"));
    connect(gear, &QToolButton::clicked, this, &MainWindow::openSettings);
    column->addWidget(gear, 0, Qt::AlignHCenter);
}

QWidget* MainWindow::buildCanvasPage()
{
    auto* page = new QWidget(this);
    m_canvas = new Canvas(page);
    m_canvas->setFocusPolicy(Qt::ClickFocus); // a click on the photo lets Tab hide the panels
    m_cropStrip = new QWidget(page);
    m_cropStrip->setObjectName(u"strip"_s);
    m_cropStrip->setAttribute(Qt::WA_StyledBackground);
    m_cropStrip->setFixedHeight(92); // mock --strip-h
    m_cropSize = new QLabel(m_cropStrip);
    m_cropSize->setObjectName(u"mono"_s);
    auto* hint = new QLabel(
        tr("Enter applies, Escape cancels, X swaps orientation, Shift keeps the ratio while dragging"),
        m_cropStrip);
    hint->setObjectName(u"muted"_s);
    auto* row = new QHBoxLayout(m_cropStrip);
    row->setContentsMargins(10, 0, 10, 0);
    row->setSpacing(16);
    row->addStretch();
    row->addWidget(m_cropSize);
    row->addWidget(hint);
    row->addStretch();
    m_cropStrip->hide();
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_canvas, 1);
    column->addWidget(m_cropStrip);
    m_stripDock = new QWidget(page);
    auto* stripRow = new QVBoxLayout(m_stripDock);
    stripRow->setContentsMargins(0, 0, 0, 0);
    m_strip = new Filmstrip(m_stripDock);
    m_strip->hide();
    stripRow->addWidget(m_strip);
    column->addWidget(m_stripDock);
    return page;
}

void MainWindow::copySettings()
{
    if (m_document.isOpen() && !isInTool()) {
        QGuiApplication::clipboard()->setText(core::settingsToClipboard(m_document.settings()));
        showToast(tr("Settings copied"));
    }
}

void MainWindow::pasteSettings()
{
    if (!m_document.isOpen() || isInTool()) {
        return;
    }
    const auto copied = core::settingsFromClipboard(QGuiApplication::clipboard()->text());
    if (!copied) {
        showToast(tr("Nothing to paste: copy a photo's settings first."));
        return;
    }
    pfx::Settings settings = *copied;
    const auto photos = static_cast<int>(m_strip->selection().size());
    if (photos > 1) {
        // Several photos: ask which parts (apply-to-selected.html), as a preset of those parts.
        PasteSheet sheet(settings, photos, this);
        if (sheet.exec() != QDialog::Accepted || sheet.units().isEmpty()) {
            return;
        }
        settings = core::presetFrom(settings, sheet.units());
        applyToSelection([settings](const pfx::Settings& s) { return core::applyPreset(s, settings); },
                         tr("Paste settings"));
        return;
    }
    applyToSelection([settings](const pfx::Settings& s) { return core::pasted(s, settings); },
                     tr("Paste settings"));
}

void MainWindow::pasteImage()
{
    // A picture on the clipboard (a screenshot, an image copied in a browser) opens like a file:
    // saved once as PNG in the app's cache, so edits are remembered and Export works as usual.
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/pasted"_s;
    QDir().mkpath(folder);
    const QString path =
        folder + u"/pasted-%1.png"_s.arg(QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s));
    if (!image.save(path, "PNG")) {
        showToast(tr("Could not keep the pasted picture: the cache folder is not writable."));
        return;
    }
    openFiles({path});
}

void MainWindow::connectClipboard()
{
    const auto refresh = [this] {
        m_adjust->setPasteEnabled(
            core::settingsFromClipboard(QGuiApplication::clipboard()->text()).has_value());
    };
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, refresh);
    refresh();
    connect(m_adjust, &AdjustPanel::copyRequested, this, &MainWindow::copySettings);
    connect(m_adjust, &AdjustPanel::pasteRequested, this, &MainWindow::pasteSettings);
    auto* copy = new QAction(tr("Copy settings"), this);
    copy->setObjectName(u"copySettings"_s);
    copy->setShortcut(QKeySequence(u"Ctrl+Shift+C"_s));
    connect(copy, &QAction::triggered, this, &MainWindow::copySettings);
    auto* paste = new QAction(tr("Paste settings"), this);
    paste->setObjectName(u"pasteSettings"_s);
    paste->setShortcut(QKeySequence(u"Ctrl+Shift+V"_s));
    connect(paste, &QAction::triggered, this, &MainWindow::pasteSettings);
    auto* pasteImageAction = new QAction(tr("Paste image"), this);
    pasteImageAction->setObjectName(u"pasteImage"_s);
    pasteImageAction->setShortcut(QKeySequence::Paste);
    connect(pasteImageAction, &QAction::triggered, this, &MainWindow::pasteImage);
    addActions({copy, paste, pasteImageAction});
}

void MainWindow::applyGeneralSettings()
{
    using S = core::Settings;
    const auto background = [](S::CanvasBackground b) {
        switch (b) {
        case S::CanvasBackground::MiddleGray:
            return QColor(0x80, 0x80, 0x80);
        case S::CanvasBackground::Black:
            return QColor(Qt::black);
        case S::CanvasBackground::White:
            return QColor(Qt::white);
        case S::CanvasBackground::DarkGray:
            break;
        }
        return QColor(); // the theme's canvas colour
    };
    const auto sidecars = [this](bool on) {
        m_document.setSidecarFolder(on ? core::SidecarStore::defaultFolder() : QString());
    };
    m_canvas->setBackground(background(m_settings.canvasBackground()));
    m_canvas->setClippingWarnings(m_settings.clippingWarnings());
    m_document.setOpenAsLeft(m_settings.openAsLeft());
    sidecars(m_settings.rememberEdits());
    connect(&m_settings, &S::canvasBackgroundChanged, this,
            [this, background](S::CanvasBackground b) { m_canvas->setBackground(background(b)); });
    connect(&m_settings, &S::clippingWarningsChanged, m_canvas, &Canvas::setClippingWarnings);
    connect(&m_settings, &S::openAsLeftChanged, &m_document, &services::Document::setOpenAsLeft);
    connect(&m_settings, &S::rememberEditsChanged, this, sidecars);
}

void MainWindow::connectPresets()
{
    m_presetThumbnails = new services::Thumbnailer(this);
    m_thumbnailTimer = new QTimer(this);
    m_thumbnailTimer->setSingleShot(true);
    m_thumbnailTimer->setInterval(500); // pictures follow the edit once it settles
    connect(m_thumbnailTimer, &QTimer::timeout, this, &MainWindow::requestPresetThumbnails);
    connect(m_presetThumbnails, &services::Thumbnailer::rendered, this, [this](int key, const QImage& image) {
        if (key / kThumbnailKeySpan == m_thumbnailGeneration % kThumbnailGenerations) {
            m_presets->setThumbnail(static_cast<std::size_t>(key % kThumbnailKeySpan), image);
        }
    });
    connect(&m_document, &services::Document::opened, this, [this] { m_thumbnailTimer->start(0); });
    connect(&m_document, &services::Document::historyChanged, this, [this] { m_thumbnailTimer->start(); });
    connect(m_presets, &PresetsPanel::applyRequested, this, &MainWindow::applyPreset);
    connect(m_presets, &PresetsPanel::previewRequested, this, [this](const core::PresetEntry* entry) {
        if (entry == nullptr || entry->autoEnhance || !m_document.isOpen() || isInTool()) {
            m_document.setHoverSettings(std::nullopt);
            return;
        }
        m_document.setHoverSettings(core::applyPreset(m_document.settings(), entry->settings));
    });
    connect(m_presets, &PresetsPanel::saveRequested, this, &MainWindow::savePreset);
    connectPresetActions();
    refreshPresets();
}

void MainWindow::importPresets()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Import presets"), m_settings.lastOpenFolder(),
        tr("Presets (*.pfxpreset *.pfxrecipe *.xmp);;PixelFx presets "
           "(*.pfxpreset *.pfxrecipe);;Lightroom and Camera Raw presets (*.xmp)"));
    if (files.isEmpty()) {
        return;
    }
    int imported = 0;
    QString lastError;
    QStringList skipped;
    for (const QString& file : files) {
        const auto result = m_library.importFile(file);
        if (result.path.isEmpty()) {
            lastError = result.error;
        } else {
            ++imported;
            skipped << result.skipped;
        }
    }
    refreshPresets();
    skipped.removeDuplicates();
    QString text = lastError.isEmpty()
                       ? tr("Imported %n preset(s).", nullptr, imported)
                       : tr("Imported %1 of %2. %3").arg(imported).arg(files.size()).arg(lastError);
    if (!skipped.isEmpty()) {
        // Lightroom settings with no equivalent are named, so nothing goes missing silently.
        text += u' ' + tr("Left out: %1.").arg(skipped.mid(0, 6).join(u", "_s));
    }
    showToast(text);
}

void MainWindow::connectPresetActions()
{
    connect(m_presets, &PresetsPanel::importRequested, this, &MainWindow::importPresets);
    connect(m_presets, &PresetsPanel::renameRequested, this, [this](const core::PresetEntry& e) {
        bool ok = false;
        const QString name =
            QInputDialog::getText(this, tr("Rename preset"), tr("Name"), QLineEdit::Normal, e.name, &ok);
        if (ok && !name.trimmed().isEmpty() && core::PresetLibrary::rename(e.path, name)) {
            refreshPresets();
        }
    });
    connect(m_presets, &PresetsPanel::updateRequested, this, [this](const core::PresetEntry& e) {
        const QStringList units = core::unitsIn(e.settings);
        const auto answer = QMessageBox::question(this, tr("Update preset"),
                                                  tr("Replace what %1 holds (%2) with this photo's settings?")
                                                      .arg(e.name, core::presetSummary(e.settings)));
        if (answer == QMessageBox::Yes &&
            core::PresetLibrary::update(e.path, core::presetFrom(m_document.settings(), units))) {
            refreshPresets();
            showToast(tr("Updated %1").arg(e.name));
        }
    });
    connect(m_presets, &PresetsPanel::exportRequested, this, [this](const core::PresetEntry& e) {
        const QString file = QFileDialog::getSaveFileName(this, tr("Export preset"), e.name + u".pfxpreset"_s,
                                                          tr("PixelFx presets (*.pfxpreset)"));
        if (!file.isEmpty()) {
            showToast(core::PresetLibrary::exportTo(e, file)
                          ? tr("Exported %1").arg(QFileInfo(file).fileName())
                          : tr("Not exported: the folder is not writable."));
        }
    });
    connect(m_presets, &PresetsPanel::showFileRequested, this, [](const core::PresetEntry& e) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(e.path).absolutePath()));
    });
    connect(m_presets, &PresetsPanel::deleteRequested, this, [this](const core::PresetEntry& e) {
        // Deleting a preset file is the one thing here undo cannot bring back: ask (DESIGN 5).
        const auto answer = QMessageBox::question(this, tr("Delete preset"),
                                                  tr("Delete %1? This cannot be undone.").arg(e.name));
        if (answer == QMessageBox::Yes && m_library.remove(e.path)) {
            refreshPresets();
        }
    });
}

void MainWindow::connectLooks()
{
    LooksPanel* looks = m_presets->looksPanel();
    m_lookThumbnails = new services::Thumbnailer(this);
    connect(m_lookThumbnails, &services::Thumbnailer::rendered, this,
            [this, looks](int key, const QImage& image) {
                if (key / kThumbnailKeySpan == m_lookGeneration % kThumbnailGenerations) {
                    looks->setThumbnail((key % kThumbnailKeySpan) - 1, image); // 0 is Original
                }
            });
    connect(m_thumbnailTimer, &QTimer::timeout, this, &MainWindow::requestLookThumbnails);
    connect(looks, &LooksPanel::lookChosen, this, [this](const QString& id, int strength) {
        if (!m_document.isOpen() || isInTool()) {
            return;
        }
        QString name = tr("No look");
        for (const core::LookEntry& e : m_presets->looksPanel()->entries()) {
            if (e.id == id) {
                name = tr("Look %1").arg(e.name);
            }
        }
        applyToSelection([id, strength](const pfx::Settings& s) { return core::withLook(s, id, strength); },
                         name);
    });
    connect(looks, &LooksPanel::strengthMoved, this, [this, looks](int strength) {
        m_document.setSettings(core::withLook(m_document.settings(), looks->current(), strength));
    });
    connect(looks, &LooksPanel::strengthReleased, this, [this, looks](int strength) {
        if (looks->current().isEmpty()) {
            return;
        }
        m_document.setSettings(core::withLook(m_document.settings(), looks->current(), strength));
        m_document.commit(tr("Look strength %1").arg(strength));
    });
    connect(looks, &LooksPanel::importRequested, this, &MainWindow::importLooks);
    connect(looks, &LooksPanel::newLookRequested, this, [this] { editLook({}); });
    connect(looks, &LooksPanel::editLookRequested, this, &MainWindow::editLook);
    connect(looks, &LooksPanel::exportLookRequested, this, &MainWindow::exportLook);
    connect(looks, &LooksPanel::deleteLookRequested, this, &MainWindow::deleteLook);
    // The panel follows the photo's settings: opening, undo, a preset, a pasted edit.
    const auto follow = [this, looks] {
        const pfx::Settings& s = m_document.settings();
        looks->setCurrent(QString::fromStdString(s.text("look.id", "")), s.number("look.strength", 100.0));
    };
    connect(&m_document, &services::Document::opened, this, follow);
    connect(&m_document, &services::Document::settingsReplaced, this, follow);
    connect(&m_document, &services::Document::historyChanged, this, follow);
    looks->setEntries(core::LookLibrary(core::LookLibrary::defaultFolder()).entries());
    m_batch->setLooks(looks->entries());
}

void MainWindow::exportLook(const QString& id)
{
    for (const core::LookEntry& e : m_presets->looksPanel()->entries()) {
        if (e.id != id) {
            continue;
        }
        const QString target = QFileDialog::getSaveFileName(this, tr("Export look"), e.name + u".pfxlook"_s,
                                                            tr("PixelFx looks (*.pfxlook)"));
        if (!target.isEmpty()) {
            showToast(core::LookLibrary::exportTo(e, target) ? tr("Exported %1").arg(e.name)
                                                             : tr("%1 could not be exported.").arg(e.name));
        }
    }
}

void MainWindow::deleteLook(const QString& id)
{
    const core::LookLibrary library(core::LookLibrary::defaultFolder());
    for (const core::LookEntry& e : m_presets->looksPanel()->entries()) {
        if (e.id == id && e.isMine() && library.remove(e.path)) {
            showToast(tr("Deleted %1").arg(e.name));
        }
    }
    refreshLooks();
}

void MainWindow::refreshLooks()
{
    const core::LookLibrary library(core::LookLibrary::defaultFolder());
    m_presets->looksPanel()->setEntries(library.entries());
    m_batch->setLooks(m_presets->looksPanel()->entries());
    requestLookThumbnails();
    m_document.refresh(); // the photo's own look may have changed
}

void MainWindow::editLook(const QString& id)
{
    if (!m_document.isOpen()) {
        return;
    }
    core::LookEntry entry;
    for (const core::LookEntry& e : m_presets->looksPanel()->entries()) {
        if (e.id == id && e.isMine()) {
            entry = e;
        }
    }
    LookEditor editor(m_document.previewSource(), m_document.settings(), entry,
                      core::LookLibrary::defaultFolder(), this);
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    refreshLooks();
    showToast(tr("Saved the look %1").arg(core::lookName(editor.savedId())));
}

void MainWindow::importLooks()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Import LUTs"), m_settings.lastOpenFolder(),
        tr("LUTs (*.cube *.png *.tif *.tiff);;Cube LUTs (*.cube);;HaldCLUT pictures "
           "(*.png *.tif *.tiff)"));
    if (files.isEmpty()) {
        return;
    }
    const core::LookLibrary library(core::LookLibrary::defaultFolder());
    int imported = 0;
    QString lastError;
    for (const QString& file : files) {
        QString error;
        if (library.importLut(file, &error).isEmpty()) {
            lastError = u"%1: %2"_s.arg(QFileInfo(file).fileName(), error);
        } else {
            ++imported;
        }
    }
    m_presets->looksPanel()->setEntries(library.entries());
    m_batch->setLooks(m_presets->looksPanel()->entries());
    requestLookThumbnails();
    showToast(lastError.isEmpty()
                  ? tr("Imported %n look(s).", nullptr, imported)
                  : tr("Imported %1 of %2. %3").arg(imported).arg(files.size()).arg(lastError));
}

void MainWindow::requestLookThumbnails()
{
    const auto source = m_document.thumbnailSource();
    if (!source || !m_document.isOpen()) {
        return;
    }
    ++m_lookGeneration;
    const int base = (m_lookGeneration % kThumbnailGenerations) * kThumbnailKeySpan;
    // The photo's own edit under each look, as the tile shows what a click gives.
    const pfx::Settings photo = core::withLook(m_document.settings(), {}, 0);
    m_lookThumbnails->requestRender(base, source, photo);
    const auto& entries = m_presets->looksPanel()->entries();
    for (std::size_t i = 0; i < entries.size() && i + 1 < kThumbnailKeySpan; ++i) {
        m_lookThumbnails->requestRender(
            base + static_cast<int>(i) + 1, source,
            core::withLook(photo, entries[i].id, entries[i].look->defaultStrength));
    }
}

void MainWindow::refreshPresets()
{
    m_presets->setEntries(m_library.entries());
    requestPresetThumbnails();
}

void MainWindow::requestPresetThumbnails()
{
    const auto source = m_document.thumbnailSource();
    if (!source || !m_document.isOpen()) {
        return;
    }
    ++m_thumbnailGeneration;
    const auto& entries = m_presets->entries();
    for (std::size_t i = 0; i < entries.size() && i < kThumbnailKeySpan; ++i) {
        const core::PresetEntry& e = entries[i];
        const pfx::Settings s = e.autoEnhance ? m_document.settings().overlaidWith(pfx::autoAdjust(*source))
                                              : core::applyPreset(m_document.settings(), e.settings);
        m_presetThumbnails->requestRender(
            ((m_thumbnailGeneration % kThumbnailGenerations) * kThumbnailKeySpan) + static_cast<int>(i),
            source, s);
    }
}

void MainWindow::applyPreset(const core::PresetEntry& entry)
{
    if (!m_document.isOpen() || isInTool()) {
        return;
    }
    m_document.setHoverSettings(std::nullopt);
    if (entry.autoEnhance) {
        m_document.autoAdjust();
        return;
    }
    const pfx::Settings preset = entry.settings;
    applyToSelection([preset](const pfx::Settings& s) { return core::applyPreset(s, preset); },
                     tr("Preset %1").arg(entry.name));
}

void MainWindow::savePreset()
{
    if (!m_document.isOpen()) {
        return;
    }
    QStringList groups;
    for (const core::PresetEntry& e : m_presets->entries()) {
        if (!e.isBuiltIn() && !groups.contains(e.group)) {
            groups << e.group;
        }
    }
    PresetSaveSheet sheet(m_document.settings(), groups, this);
    if (sheet.exec() != QDialog::Accepted) {
        return;
    }
    const pfx::Settings preset = core::presetFrom(m_document.settings(), sheet.units());
    const QString path = m_library.save(sheet.name(), sheet.group(), preset);
    if (path.isEmpty()) {
        showToast(tr("Not saved: the preset folder is not writable."));
        return;
    }
    if (!sheet.fileCopy().isEmpty()) {
        core::PresetEntry entry;
        entry.name = sheet.name();
        entry.settings = preset;
        (void)core::PresetLibrary::exportTo(entry, sheet.fileCopy());
    }
    refreshPresets();
    showToast(tr("Saved preset %1").arg(sheet.name()));
}

void MainWindow::openSettings()
{
    SettingsSheet sheet(m_settings, this);
    sheet.exec();
}

void MainWindow::connectWhiteBalance()
{
    connect(m_adjust, &AdjustPanel::whiteBalanceAutoRequested, this, [this] {
        m_document.autoWhiteBalance();
        m_adjust->markWhiteBalanceAuto();
    });
    connect(m_adjust, &AdjustPanel::whiteBalancePickRequested, this, [this](bool on) {
        on&& m_document.isOpen() && !isInTool() ? m_canvas->startPick() : m_canvas->cancelPick();
    });
    connect(m_canvas, &Canvas::pointPicked, this, [this](QPointF fraction) {
        m_adjust->setPicking(false);
        m_document.pickWhiteBalance(fraction);
    });
    auto* pick = new QAction(tr("Pick a neutral point"), this);
    pick->setShortcut(QKeySequence(Qt::Key_W));
    connect(pick, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isInTool()) {
            m_adjust->setPicking(true);
            m_canvas->startPick();
        }
    });
    addAction(pick);
}

void MainWindow::connectZoom()
{
    // Zoomed in past the preview: a full-size render, made a moment after edits settle.
    connect(m_canvas, &Canvas::zoomChanged, this, [this] {
        const bool want = m_canvas->needsDetail() && !m_showingOriginal;
        if (want != m_document.isFullResolution()) {
            m_document.setFullResolution(want);
            if (!want) {
                m_canvas->setDetail(QImage());
            }
        }
    });
    connect(&m_document, &services::Document::fullReady, m_canvas, &Canvas::setDetail);
    connect(&m_document, &services::Document::fullStale, this, [this] { m_canvas->setDetail(QImage()); });
    connect(m_canvas, &Canvas::fullScreenRequested, this,
            [this] { isFullScreen() ? showNormal() : showFullScreen(); });
}

void MainWindow::connectCrop()
{
    connect(m_cropPanel, &CropPanel::settingsChanged, this, [this](const pfx::Settings& settings) {
        m_document.setSettings(settings);
        m_canvas->setCrop(core::cropOf(settings));
        updateCropStrip();
    });
    connect(m_cropPanel, &CropPanel::ratioChanged, m_canvas, &Canvas::setCropRatio);
    connect(m_cropPanel, &CropPanel::overlayChanged, m_canvas, &Canvas::setCropOverlay);
    connect(m_cropPanel, &CropPanel::finished, this, &MainWindow::leaveCrop);
    connect(m_canvas, &Canvas::cropEdited, m_cropPanel, &CropPanel::setCrop);
    connect(m_canvas->dial(), &StraightenDial::angleChanged, m_cropPanel, &CropPanel::setAngle);
    connect(m_cropPanel, &CropPanel::settingsChanged, this, [this](const pfx::Settings& settings) {
        m_canvas->dial()->setAngle(settings.number("geometry.angle", 0.0));
    });
    connect(m_cropPanel, &CropPanel::levelLineRequested, m_canvas, &Canvas::startLevelLine);
    connect(m_canvas, &Canvas::levelLineDrawn, m_cropPanel, &CropPanel::levelTo);
}

bool MainWindow::isCropping() const
{
    return m_canvas->isCropping();
}

void MainWindow::enterBatch()
{
    leaveTools();
    m_batch->setPresets(m_library.entries());
    m_batch->setSidecarFolder(m_settings.rememberEdits() ? core::SidecarStore::defaultFolder() : QString());
    if (m_batch->files().isEmpty() && m_strip->files().size() > 1) {
        m_batch->addPaths(m_strip->files()); // the photos open in the editor, as a start
    }
    // Settings from a photo: the one open in the editor.
    m_batch->setPhotoSettings(m_document.isOpen() ? QFileInfo(m_document.info().path).fileName() : QString(),
                              m_document.settings());
    m_stack->setCurrentWidget(m_batch);
    m_presets->hide();
    m_panels->hide();
    m_batchMode->setChecked(true);
}

void MainWindow::updateHeader()
{
    // The header follows the page (mocks welcome.html, editor.html, batch.html): the wordmark
    // and the menu on the start page, the photo and its tools in the editor, Batch's own.
    const bool welcome = m_stack->currentWidget() == m_welcome;
    const bool batch = m_stack->currentWidget() == m_batch;
    for (QWidget* w : std::as_const(m_editHeader)) {
        w->setVisible(!welcome && !batch);
    }
    for (QWidget* w : std::as_const(m_batchHeader)) {
        w->setVisible(batch);
    }
    m_wordmark->setVisible(welcome);
    m_docName->setVisible(!welcome);
    m_docMeta->setVisible(!welcome);
    if (batch) {
        m_docName->setText(tr("Batch"));
        m_docMeta->setText(m_batch->summary());
        setWindowTitle(tr("Batch"));
    } else if (m_document.isOpen()) {
        const QString name = QFileInfo(m_document.info().path).fileName();
        m_docName->setText(name);
        m_docMeta->setText(describe(m_document.info()));
        setWindowTitle(name);
    } else {
        setWindowTitle(QString());
    }
}

void MainWindow::leaveBatch()
{
    if (m_stack->currentWidget() != m_batch) {
        return;
    }
    m_stack->setCurrentWidget(m_document.isOpen() ? m_canvasPage : static_cast<QWidget*>(m_welcome));
    m_presets->setVisible(m_document.isOpen());
    m_panels->setVisible(m_document.isOpen());
    m_editMode->setChecked(true);
}

bool MainWindow::isBatching() const
{
    return m_stack->currentWidget() == m_batch;
}

void MainWindow::enterCrop()
{
    leaveBatch();
    leaveHeal();
    if (!m_document.isOpen() || isCropping()) {
        m_cropMode->setChecked(isCropping());
        m_editMode->setChecked(!isCropping());
        return;
    }
    showOriginal(false);
    m_canvas->cancelPick();
    m_adjust->setPicking(false);
    m_compare->setChecked(false);
    m_compare->setEnabled(false);
    m_undo->setEnabled(false);
    m_redo->setEnabled(false);
    m_history->setEnabled(false);
    m_cropStart = m_document.settings();
    m_document.setCropEditing(true);
    m_cropPanel->begin(m_cropStart, m_document.info().size);
    m_canvas->setCrop(core::cropOf(m_cropStart));
    m_canvas->setCropRatio(m_cropPanel->ratio());
    m_canvas->setCropOverlay(m_cropPanel->overlay());
    m_canvas->setCropMode(true);
    m_canvas->dial()->setAngle(m_cropStart.number("geometry.angle", 0.0));
    m_canvas->setImage(m_document.preview(), m_document.displaySize());
    m_panels->setCurrentWidget(m_cropPanel);
    m_presets->hide(); // mock crop.html: no left panel while cropping
    m_cropStrip->show();
    m_strip->hide();
    updateCropStrip();
    for (QAction* a : std::as_const(m_cropActions)) {
        a->setEnabled(true);
    }
    m_cropMode->setChecked(true);
}

void MainWindow::leaveCrop(bool apply)
{
    if (!isCropping()) {
        return;
    }
    for (QAction* a : std::as_const(m_cropActions)) {
        a->setEnabled(false);
    }
    m_canvas->setCropMode(false);
    if (!apply) {
        m_document.setSettings(m_cropStart);
    }
    m_document.setCropEditing(false);
    if (apply) {
        m_document.commit(tr("Crop and rotate"));
    }
    m_adjust->setSettings(m_document.settings());
    m_panels->setCurrentWidget(m_adjust);
    m_presets->show();
    m_cropStrip->hide();
    m_strip->show();
    m_compare->setEnabled(true);
    updateHistoryButtons();
    m_editMode->setChecked(true);
}

void MainWindow::updateCropStrip()
{
    const QSize size = m_document.outputSize();
    m_cropSize->setText(tr("%1 x %2 px").arg(size.width()).arg(size.height()));
}

bool MainWindow::isSupported(const QString& path)
{
    return supportedSuffixes().contains(QFileInfo(path).suffix().toLower());
}

void MainWindow::openFiles(const QStringList& files)
{
    QStringList photos;
    for (const QString& file : files) {
        if (isSupported(file)) {
            photos << QFileInfo(file).absoluteFilePath();
        }
    }
    if (photos.isEmpty()) {
        if (!files.isEmpty()) {
            showFailed(files.first(), tr("PixelFx does not open this kind of file yet."));
        }
        return;
    }
    // Several photos replace the strip; one joins it (or is switched to when already there).
    QStringList strip = m_strip->files();
    int target = 0;
    if (photos.size() > 1) {
        strip = photos;
    } else if (const auto at = strip.indexOf(photos.first()); at >= 0) {
        target = static_cast<int>(at);
    } else {
        strip << photos.first();
        target = static_cast<int>(strip.size()) - 1;
    }
    if (strip != m_strip->files()) {
        m_strip->setFiles(strip);
        for (const QString& path : strip) {
            m_stripThumbnails->request(path, 2 * FilmTile::kWidth,
                                       m_settings.rememberEdits() ? core::SidecarStore::defaultFolder()
                                                                  : QString());
        }
    }
    showPhoto(target);
}

void MainWindow::showPhoto(int index)
{
    if (index < 0 || index >= m_strip->files().size() || isInTool()) {
        return;
    }
    m_strip->setCurrent(index);
    if (m_document.info().path != m_strip->files().at(index)) {
        m_document.open(m_strip->files().at(index), previewLongEdge());
    }
}

void MainWindow::connectFilmstrip()
{
    m_stripThumbnails = new services::Thumbnailer(this);
    connect(m_stripThumbnails, &services::Thumbnailer::ready, this, [this](const services::Thumbnail& t) {
        m_strip->setThumbnail(t.path, t.image, t.edited.isValid());
    });
    connect(m_strip, &Filmstrip::currentRequested, this, &MainWindow::showPhoto);
    connect(m_strip, &Filmstrip::addRequested, this, &MainWindow::chooseFiles);
    connect(m_strip, &Filmstrip::selectionChanged, m_presets, &PresetsPanel::setTargetCount);
    connect(m_strip, &Filmstrip::selectionChanged, this, [this](int count) {
        m_export->setText(count > 1 ? tr("Export %n photos", nullptr, count) : tr("Export"));
    });
    // The current photo's tile follows its edits, from the preview (no decoding).
    auto* tileTimer = new QTimer(this);
    tileTimer->setSingleShot(true);
    tileTimer->setInterval(400);
    connect(tileTimer, &QTimer::timeout, this, [this] {
        if (m_document.isOpen() && !m_document.isCropEditing()) {
            m_strip->setThumbnail(m_document.info().path,
                                  m_document.preview().scaled(4 * FilmTile::kWidth, 4 * FilmTile::kHeight,
                                                              Qt::KeepAspectRatio, Qt::SmoothTransformation),
                                  !m_document.settings().isEmpty());
        }
    });
    connect(&m_document, &services::Document::previewReady, tileTimer, qOverload<>(&QTimer::start));
    const auto key = [this](const QKeySequence& seq, auto slot) {
        auto* a = new QAction(this);
        a->setShortcut(seq);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
    };
    // Left and Right step through the strip in keyPressEvent, so a focused slider keeps its arrows.
    key(QKeySequence::SelectAll, [this] { m_strip->selectAll(); });
}

void MainWindow::applyToSelection(const std::function<pfx::Settings(const pfx::Settings&)>& change,
                                  const QString& label)
{
    // The current photo through its Document (one history step); the others through their
    // remembered edits, which is what they open with (apply-to-selected.html: nothing is written
    // to the photos themselves).
    m_document.apply(change(m_document.settings()), label);
    const QList<int> selected = m_strip->selection();
    if (selected.size() < 2) {
        return;
    }
    if (!m_settings.rememberEdits()) {
        showToast(tr("Applied to this photo only: turn on Remember edits in Settings to edit several."));
        return;
    }
    const core::SidecarStore store(core::SidecarStore::defaultFolder());
    int done = 0;
    for (const int index : selected) {
        const QString path = m_strip->files().at(index);
        if (path == m_document.info().path) {
            continue;
        }
        const QString fingerprint = core::fileFingerprint(path);
        if (fingerprint.isEmpty()) {
            continue;
        }
        const pfx::Settings before = store.read(fingerprint).value_or(pfx::Settings{});
        if (store.write(path, fingerprint, change(before))) {
            ++done;
            m_stripThumbnails->request(path, 2 * FilmTile::kWidth, store.folder());
        }
    }
    showToast(tr("%1 applied to %n photo(s)", nullptr, done + 1).arg(label));
}

void MainWindow::chooseFiles()
{
    QStringList patterns;
    for (const QString& suffix : supportedSuffixes()) {
        // Portal dialogs match case-sensitively, and cameras write IMG_0001.JPG.
        patterns << u"*."_s + suffix << u"*."_s + suffix.toUpper();
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open images"), m_settings.lastOpenFolder(), tr("Images (%1)").arg(patterns.join(u' ')));
    if (files.isEmpty()) {
        return;
    }
    m_settings.setLastOpenFolder(QFileInfo(files.first()).absolutePath());
    openFiles(files);
}

void MainWindow::showOpened(const services::DocumentInfo& info)
{
    const QString name = QFileInfo(info.path).fileName();
    m_docName->setText(name);
    m_docMeta->setText(describe(info));
    setWindowTitle(name);
    m_settings.addRecentFile(info.path);
    m_adjust->setPhotoDetails(info.exposure, QStringList{info.camera, info.lens}.join(u'\n').trimmed());
    m_panels->show();
    m_rail->show();
    m_presets->setVisible(!isInTool());
    m_strip->setVisible(!isInTool());
    m_original->setEnabled(true);
    m_compare->setEnabled(true);
    m_export->setEnabled(true);
    m_canvas->setOriginal(m_document.original());
    m_showingOriginal = false;
    m_cropMode->setEnabled(true);
    m_healMode->setEnabled(true);
}

void MainWindow::showFailed(const QString& path, const QString& message)
{
    m_stack->setCurrentWidget(m_canvasPage);
    m_canvas->setError(tr("Could not open %1. %2").arg(QFileInfo(path).fileName(), message));
}

int MainWindow::previewLongEdge() const
{
    // At least the screen's long edge in device pixels, so the fit view is sharp; M3 renders the
    // visible region at full resolution when zoomed in.
    constexpr int kMinimum = 2048;
    const QScreen* s = screen();
    if (s == nullptr) {
        return kMinimum;
    }
    const QSize pixels = s->size() * s->devicePixelRatio();
    return std::max({kMinimum, pixels.width(), pixels.height()});
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Closing during a batch asks once (batch-running.html); the photos written so far stay.
    if (m_batch->runner().isRunning()) {
        const auto answer = QMessageBox::question(
            this, tr("Batch still running"),
            tr("Stop the batch and quit? Photos already saved are kept; the rest are not processed."));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_batch->runner().stop();
    }
    m_document.flush();
    m_settings.setWindowGeometry(saveGeometry());
    m_settings.setWindowState(saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    const bool any = std::ranges::any_of(
        urls, [](const QUrl& url) { return url.isLocalFile() && isSupported(url.toLocalFile()); });
    if (any) {
        event->acceptProposedAction();
        m_welcome->setDragActive(true);
    }
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_welcome->setDragActive(false);
    QMainWindow::dragLeaveEvent(event);
}

QAction* MainWindow::namedAction(const QString& name) const
{
    auto* a = findChild<QAction*>(name, Qt::FindDirectChildrenOnly);
    Q_ASSERT_X(a, "namedAction", qPrintable(name));
    return a;
}

void MainWindow::setPanelsHidden(bool hidden)
{
    m_leftDock->setVisible(!hidden);
    m_rightDock->setVisible(!hidden);
    m_stripDock->setVisible(!hidden);
    m_hidePanels->setChecked(hidden);
}

bool MainWindow::panelsHidden() const
{
    return m_hidePanels->isChecked();
}

void MainWindow::setupPanelActions()
{
    m_hidePanels = new QAction(tr("Hide panels"), this);
    m_hidePanels->setObjectName(u"hidePanels"_s);
    m_hidePanels->setCheckable(true);
    connect(m_hidePanels, &QAction::triggered, this, [this](bool on) { setPanelsHidden(on); });
    // Tab hides the panels only while the photo or nothing has the focus, so Tab still walks
    // through sliders and fields for keyboard users.
    auto* tab = new QAction(this);
    tab->setShortcut(QKeySequence(Qt::Key_Tab));
    connect(tab, &QAction::triggered, this, [this] {
        if (m_document.isOpen()) {
            setPanelsHidden(!panelsHidden());
        }
    });
    const auto followFocus = [this, tab](QWidget*, QWidget* now) {
        tab->setEnabled(now == nullptr || now == this || now == m_canvas || m_canvas->isAncestorOf(now));
    };
    connect(qApp, &QApplication::focusChanged, this, followFocus);
    followFocus(nullptr, QApplication::focusWidget());
    // F10 opens the main menu, as its tooltip says.
    auto* menu = new QAction(this);
    menu->setShortcut(QKeySequence(Qt::Key_F10));
    connect(menu, &QAction::triggered, m_menuButton, &QToolButton::showMenu);
    addActions({m_hidePanels, tab, menu});
}

void MainWindow::buildRecentMenu(QMenu* recent)
{
    connect(recent, &QMenu::aboutToShow, this, [this, recent] {
        recent->clear();
        const QStringList files = m_settings.recentFiles();
        for (const QString& file : files) {
            QAction* a = recent->addAction(QFileInfo(file).fileName());
            a->setToolTip(QDir::toNativeSeparators(file));
            connect(a, &QAction::triggered, this, [this, file] { openFiles({file}); });
        }
        if (files.isEmpty()) {
            recent->addAction(tr("No recent photos"))->setEnabled(false);
        } else {
            recent->addSeparator();
            connect(recent->addAction(tr("Clear recent")), &QAction::triggered, this,
                    [this] { m_settings.clearRecentFiles(); });
        }
    });
}

void MainWindow::buildMainMenu()
{
    // The entries of menu.html that exist; each one is the window's own action, so the
    // shortcut shown is the one that works.
    setupPanelActions();
    auto* menu = new QMenu(m_menuButton);
    QAction* open = namedAction(u"open"_s);
    open->setIcon(icons::themed(u"open"_s, tokens().muted));
    menu->addAction(open);
    menu->addAction(namedAction(u"openFolder"_s));
    buildRecentMenu(menu->addMenu(tr("Open recent")));
    menu->addAction(namedAction(u"pasteImage"_s));
    menu->addSeparator();

    QMenu* edit = menu->addMenu(tr("Edit"));
    edit->addActions({namedAction(u"undo"_s), namedAction(u"redo"_s), namedAction(u"history"_s)});
    edit->addSeparator();
    edit->addActions({namedAction(u"copySettings"_s), namedAction(u"pasteSettings"_s)});
    edit->addSeparator();
    edit->addAction(namedAction(u"resetAll"_s));
    QMenu* view = menu->addMenu(tr("View"));
    view->addAction(namedAction(u"compare"_s));
    QAction* clipping = namedAction(u"clipping"_s);
    clipping->setCheckable(true);
    clipping->setChecked(m_settings.clippingWarnings());
    connect(&m_settings, &core::Settings::clippingWarningsChanged, clipping, &QAction::setChecked);
    view->addAction(clipping);
    view->addSeparator();
    view->addActions({namedAction(u"fit"_s), namedAction(u"zoomIn"_s), namedAction(u"zoomOut"_s),
                      namedAction(u"actualSize"_s)});
    view->addSeparator();
    view->addAction(m_hidePanels);
    view->addAction(namedAction(u"fullScreen"_s));
    QMenu* photo = menu->addMenu(tr("Photo"));
    photo->addAction(namedAction(u"auto"_s));
    photo->addAction(namedAction(u"crop"_s));
    photo->addAction(namedAction(u"heal"_s));
    photo->addSeparator();
    photo->addActions({namedAction(u"edit"_s), namedAction(u"batch"_s)});
    menu->addSeparator();
    menu->addAction(namedAction(u"export"_s));
    menu->addAction(namedAction(u"openBatchRecipe"_s));
    menu->addSeparator();
    connect(menu->addAction(tr("Import presets...")), &QAction::triggered, this, &MainWindow::importPresets);
    connect(menu->addAction(tr("Show presets folder")), &QAction::triggered, this, [this] {
        QDir().mkpath(m_library.folder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_library.folder()));
    });
    menu->addSeparator();
    menu->addAction(namedAction(u"settings"_s));
    menu->addAction(namedAction(u"shortcuts"_s));
    menu->addAction(namedAction(u"whatsNew"_s));
    connect(menu->addAction(tr("Report a bug")), &QAction::triggered, this,
            [] { QDesktopServices::openUrl(QUrl(u"https://github.com/keshavbhatt/PixelFx/issues"_s)); });
    menu->addAction(namedAction(u"about"_s));
    menu->addSeparator();
    QAction* quit = namedAction(u"quit"_s);
    quit->setIcon(icons::themed(u"quit"_s, tokens().muted));
    menu->addAction(quit);
    m_menuButton->setMenu(menu);
}

void MainWindow::showWhatsNew()
{
    WhatsNewSheet sheet(WhatsNewSheet::bundledChangelog(), QApplication::applicationVersion(),
                        m_settings.showWhatsNew(), this);
    sheet.exec();
    m_settings.setShowWhatsNew(sheet.showAfterUpdates());
    m_settings.setWhatsNewSeen(QApplication::applicationVersion());
}

void MainWindow::showAbout()
{
    AboutSheet sheet(this);
    connect(&sheet, &AboutSheet::whatsNewRequested, this, [this, &sheet] {
        sheet.accept();
        QTimer::singleShot(0, this, &MainWindow::showWhatsNew);
    });
    sheet.exec();
}

bool MainWindow::showWhatsNewIfUpdated()
{
    const QString version = QApplication::applicationVersion();
    const QString seen = m_settings.whatsNewSeen();
    if (seen == version) {
        return false;
    }
    m_settings.setWhatsNewSeen(version);
    const bool upgraded = !seen.isEmpty() || QFile::exists(core::Settings::legacyFilePath());
    if (!upgraded || !m_settings.showWhatsNew()) {
        return false;
    }
    showWhatsNew();
    return true;
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (isHealing() && (event->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier)) == Qt::NoModifier) {
        const double step = event->modifiers().testFlag(Qt::ShiftModifier) ? 0.01 : 0.002;
        const QHash<int, QPointF> moves = {{Qt::Key_Left, {-step, 0}},
                                           {Qt::Key_Right, {step, 0}},
                                           {Qt::Key_Up, {0, -step}},
                                           {Qt::Key_Down, {0, step}}};
        if (moves.contains(event->key())) {
            m_healTool->nudge(moves.value(event->key()));
            m_canvas->update();
            return;
        }
    }
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && !isInTool()) {
        showOriginal(true);
        return;
    }
    if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) &&
        event->modifiers() == Qt::NoModifier && m_document.isOpen() && !isInTool()) {
        showPhoto(m_strip->current() + (event->key() == Qt::Key_Right ? 1 : -1));
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        showOriginal(false);
        return;
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::dropEvent(QDropEvent* event)
{
    m_welcome->setDragActive(false);
    QStringList files;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            files << url.toLocalFile();
        }
    }
    openFiles(files);
    event->acceptProposedAction();
}

} // namespace pixelfx::ui
