#include "ui/main_window.h"

#include "core/crop_geometry.h"
#include "core/settings/settings.h"
#include "ui/adjust_panel.h"
#include "ui/canvas.h"
#include "ui/crop_panel.h"
#include "ui/export_sheet.h"
#include "ui/icons.h"
#include "ui/style.h"
#include "ui/welcome_page.h"

#include <QAction>
#include <QCloseEvent>
#include <QConicalGradient>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

const QStringList& supportedSuffixes()
{
    static const QStringList suffixes = {u"jpg"_s,  u"jpeg"_s, u"png"_s, u"tif"_s,
                                         u"tiff"_s, u"webp"_s, u"gif"_s, u"bmp"_s};
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
    const QString format = info.format.endsWith(u"load"_s) ? info.format.chopped(4).toUpper() : info.format;
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

    m_document.setSidecarFolder(core::SidecarStore::defaultFolder());
    connectDocument();
    connectCrop();
}

void MainWindow::connectDocument()
{
    connect(&m_document, &services::Document::openStarted, this, [this](const QString& path) {
        if (isCropping()) {
            leaveCrop(true);
        }
        m_stack->setCurrentWidget(m_canvasPage);
        m_canvas->setStatus(tr("Opening %1").arg(QFileInfo(path).fileName()));
    });
    connect(&m_document, &services::Document::opened, this, &MainWindow::showOpened);
    connect(&m_document, &services::Document::opened, this,
            [this] { m_canvas->setImage(m_document.preview(), m_document.displaySize()); });
    connect(&m_document, &services::Document::previewReady, this, [this](const QImage& image) {
        if (!m_showingOriginal) {
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
    connect(&m_exporter, &services::Exporter::finished, this,
            [this](const QString& target) { showToast(tr("Saved %1").arg(QFileInfo(target).fileName())); });
    connect(&m_exporter, &services::Exporter::skipped, this,
            [this] { showToast(tr("Not saved: a file with that name exists, and Skip is chosen.")); });
    connect(&m_exporter, &services::Exporter::failed, this,
            [this](const QString&, const QString& message) { showToast(tr("Not saved. %1").arg(message)); });
}

void MainWindow::exportPhoto()
{
    if (!m_document.isOpen() || m_exporter.isBusy()) {
        return;
    }
    if (isCropping()) {
        leaveCrop(true);
    }
    ExportSheet sheet(m_document.info().path, m_document.outputSize(), m_settings.exportOptions(), this);
    if (sheet.exec() != QDialog::Accepted) {
        return;
    }
    const core::ExportOptions options = sheet.options();
    m_settings.setExportOptions(options);
    showToast(tr("Exporting %1").arg(QFileInfo(m_document.info().path).fileName()));
    m_exporter.start(m_document.info().path, m_document.settings(), options);
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
    menu.exec(m_history->mapToGlobal(QPoint(0, m_history->height())));
}

void MainWindow::showOriginal(bool on)
{
    if (!m_document.isOpen() || on == m_showingOriginal || (on && isCropping())) {
        return;
    }
    m_showingOriginal = on;
    m_canvas->showImage(on ? m_document.original() : m_document.preview());
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
    auto* menuButton = headerButton(m_header, u"menu"_s, tr("Main menu (F10)"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(menuButton);
    menu->addAction(icons::themed(u"open"_s, tokens().muted), tr("Open images..."), QKeySequence::Open, this,
                    &MainWindow::chooseFiles);
    menu->addSeparator();
    menu->addAction(icons::themed(u"quit"_s, tokens().muted), tr("Quit"), QKeySequence(u"Ctrl+Q"_s), this,
                    &MainWindow::close);
    menuButton->setMenu(menu);

    auto* header = new QHBoxLayout(m_header);
    header->setContentsMargins(12, 0, 10, 0);
    header->setSpacing(6);
    header->addWidget(new BrandMark(m_header));
    header->addSpacing(4);
    header->addWidget(open);
    header->addSpacing(4);
    header->addLayout(doc);
    header->addStretch();
    header->addWidget(m_undo);
    header->addWidget(m_redo);
    header->addWidget(m_history);
    header->addSpacing(8);
    header->addWidget(m_compare);
    header->addWidget(m_original);
    header->addSpacing(8);
    header->addWidget(m_export);
    header->addWidget(menuButton);
}

void MainWindow::setupUi()
{
    buildHeader();

    m_welcome = new WelcomePage(this);
    connect(m_welcome, &WelcomePage::openRequested, this, &MainWindow::chooseFiles);
    m_canvasPage = buildCanvasPage();
    connect(m_compare, &QToolButton::toggled, m_canvas, &Canvas::setCompare);
    m_toast = new QLabel(m_canvas);
    m_toast->setObjectName(u"toast"_s);
    m_toast->setAttribute(Qt::WA_StyledBackground);
    m_toast->hide();
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_canvasPage);

    m_adjust = new AdjustPanel(this);
    m_cropPanel = new CropPanel(this);
    m_panels = new QStackedWidget(this);
    m_panels->addWidget(m_adjust);
    m_panels->addWidget(m_cropPanel);
    m_panels->setFixedWidth(m_adjust->width());
    m_panels->hide();
    buildRail();
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(m_rail);
    body->addWidget(m_stack, 1);
    body->addWidget(m_panels);

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
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::chooseFiles);
    auto* quitAction = new QAction(tr("Quit"), this);
    quitAction->setShortcut(QKeySequence(u"Ctrl+Q"_s));
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
    // F11 explicitly: under the KDE platform theme QKeySequence::FullScreen is Ctrl+Shift+F
    // (rewrite kit, rules learned).
    auto* fullScreen = new QAction(tr("Full screen"), this);
    fullScreen->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullScreen, &QAction::triggered, this,
            [this] { isFullScreen() ? showNormal() : showFullScreen(); });
    auto* exportAction = new QAction(tr("Export"), this);
    exportAction->setShortcut(QKeySequence(u"Ctrl+Shift+E"_s));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportPhoto);
    addActions({openAction, quitAction, fullScreen, exportAction});
    setupEditActions();
    setupCropActions();
}

void MainWindow::setupEditActions()
{
    auto* undo = new QAction(tr("Undo"), this);
    undo->setShortcut(QKeySequence::Undo);
    connect(undo, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.undo();
        }
    });
    auto* redo = new QAction(tr("Redo"), this);
    redo->setShortcuts({QKeySequence(u"Ctrl+Shift+Z"_s), QKeySequence(u"Ctrl+Y"_s)});
    connect(redo, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.redo();
        }
    });
    auto* resetAll = new QAction(tr("Reset all"), this);
    resetAll->setShortcut(QKeySequence(u"Ctrl+R"_s));
    connect(resetAll, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            m_adjust->resetAll();
        }
    });
    auto* history = new QAction(tr("History"), this);
    history->setShortcut(QKeySequence(Qt::Key_H));
    connect(history, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            showHistory();
        }
    });
    auto* autoAction = new QAction(tr("Auto"), this);
    autoAction->setShortcut(QKeySequence(u"Ctrl+Shift+A"_s));
    connect(autoAction, &QAction::triggered, this, [this] {
        if (!isCropping()) {
            m_document.autoAdjust();
        }
    });
    addAction(autoAction);
    auto* compare = new QAction(tr("Before and after"), this);
    compare->setShortcut(QKeySequence(Qt::Key_Backslash));
    connect(compare, &QAction::triggered, this, [this] {
        if (m_document.isOpen() && !isCropping()) {
            m_compare->toggle();
        }
    });
    addActions({undo, redo, resetAll, history, compare});
}

void MainWindow::setupCropActions()
{
    auto* crop = new QAction(tr("Crop"), this);
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
    m_rail->hide();
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
    m_editMode = railButton(u"sliders"_s, tr("Edit"), tr("Edit"));
    m_editMode->setChecked(true);
    m_cropMode = railButton(u"crop"_s, tr("Crop"), tr("Crop and rotate (C)"));
    m_cropMode->setEnabled(false);
    connect(m_editMode, &QToolButton::clicked, this, [this] {
        if (isCropping()) {
            leaveCrop(true);
        }
    });
    connect(m_cropMode, &QToolButton::clicked, this, &MainWindow::enterCrop);
    auto* column = new QVBoxLayout(m_rail);
    column->setContentsMargins(6, 8, 6, 8);
    column->setSpacing(4);
    column->addWidget(m_editMode);
    column->addWidget(m_cropMode);
    column->addStretch();
}

QWidget* MainWindow::buildCanvasPage()
{
    auto* page = new QWidget(this);
    m_canvas = new Canvas(page);
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
    return page;
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
}

bool MainWindow::isCropping() const
{
    return m_canvas->isCropping();
}

void MainWindow::enterCrop()
{
    if (!m_document.isOpen() || isCropping()) {
        m_cropMode->setChecked(isCropping());
        m_editMode->setChecked(!isCropping());
        return;
    }
    showOriginal(false);
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
    m_canvas->setImage(m_document.preview(), m_document.displaySize());
    m_panels->setCurrentWidget(m_cropPanel);
    m_cropStrip->show();
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
    m_cropStrip->hide();
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
    for (const QString& file : files) {
        if (isSupported(file)) {
            m_document.open(file, previewLongEdge());
            return;
        }
    }
    if (!files.isEmpty()) {
        showFailed(files.first(), tr("PixelFx does not open this kind of file yet."));
    }
}

void MainWindow::chooseFiles()
{
    QStringList patterns;
    for (const QString& suffix : supportedSuffixes()) {
        patterns << u"*."_s + suffix;
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
    m_panels->show();
    m_rail->show();
    m_original->setEnabled(true);
    m_compare->setEnabled(true);
    m_export->setEnabled(true);
    m_canvas->setOriginal(m_document.original());
    m_showingOriginal = false;
    m_cropMode->setEnabled(true);
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

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && !isCropping()) {
        showOriginal(true);
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
