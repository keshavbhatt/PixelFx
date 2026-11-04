#include "ui/main_window.h"

#include "core/settings/settings.h"
#include "ui/adjust_panel.h"
#include "ui/canvas.h"
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
}

void MainWindow::connectDocument()
{
    connect(&m_document, &services::Document::openStarted, this, [this](const QString& path) {
        m_stack->setCurrentWidget(m_canvas);
        m_canvas->setStatus(tr("Opening %1").arg(QFileInfo(path).fileName()));
    });
    connect(&m_document, &services::Document::opened, this, &MainWindow::showOpened);
    connect(&m_document, &services::Document::opened, this,
            [this] { m_canvas->setImage(m_document.preview(), m_document.info().size); });
    connect(&m_document, &services::Document::previewReady, this, [this](const QImage& image) {
        if (!m_showingOriginal) {
            m_canvas->showImage(image);
        }
    });
    connect(&m_document, &services::Document::failed, this, &MainWindow::showFailed);
    connect(&m_document, &services::Document::settingsReplaced, m_adjust, &AdjustPanel::setSettings);
    connect(&m_document, &services::Document::historyChanged, this, &MainWindow::updateHistoryButtons);
    connect(&m_document, &services::Document::histogramReady, m_adjust, &AdjustPanel::setHistogram);
    connect(m_adjust, &AdjustPanel::settingsChanged, &m_document, &services::Document::setSettings);
    connect(m_adjust, &AdjustPanel::editFinished, &m_document, &services::Document::commit);
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
    ExportSheet sheet(m_document.info().path, m_document.info().size, m_settings.exportOptions(), this);
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
    m_undo->setEnabled(h.canUndo());
    m_redo->setEnabled(h.canRedo());
    m_undo->setToolTip(h.canUndo() ? tr("Undo %1 (Ctrl+Z)").arg(h.undoLabel()) : tr("Undo (Ctrl+Z)"));
    m_redo->setToolTip(h.canRedo() ? tr("Redo %1 (Ctrl+Shift+Z)").arg(h.redoLabel())
                                   : tr("Redo (Ctrl+Shift+Z)"));
}

void MainWindow::showOriginal(bool on)
{
    if (!m_document.isOpen() || on == m_showingOriginal) {
        return;
    }
    m_showingOriginal = on;
    m_canvas->showImage(on ? m_document.original() : m_document.preview());
    m_original->setDown(on);
}

MainWindow::~MainWindow() = default;

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
    m_undo = headerButton(m_header, u"undo"_s, tr("Undo (Ctrl+Z)"));
    m_redo = headerButton(m_header, u"redo"_s, tr("Redo (Ctrl+Shift+Z)"));
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
    header->addSpacing(8);
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
    m_canvas = new Canvas(this);
    m_toast = new QLabel(m_canvas);
    m_toast->setObjectName(u"toast"_s);
    m_toast->setAttribute(Qt::WA_StyledBackground);
    m_toast->hide();
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_canvas);

    m_adjust = new AdjustPanel(this);
    m_adjust->hide();
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(m_stack, 1);
    body->addWidget(m_adjust);

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
    auto* undo = new QAction(tr("Undo"), this);
    undo->setShortcut(QKeySequence::Undo);
    connect(undo, &QAction::triggered, &m_document, &services::Document::undo);
    auto* redo = new QAction(tr("Redo"), this);
    redo->setShortcuts({QKeySequence(u"Ctrl+Shift+Z"_s), QKeySequence(u"Ctrl+Y"_s)});
    connect(redo, &QAction::triggered, &m_document, &services::Document::redo);
    auto* resetAll = new QAction(tr("Reset all"), this);
    resetAll->setShortcut(QKeySequence(u"Ctrl+R"_s));
    connect(resetAll, &QAction::triggered, this, [this] {
        if (m_document.isOpen()) {
            m_adjust->resetAll();
        }
    });
    auto* exportAction = new QAction(tr("Export"), this);
    exportAction->setShortcut(QKeySequence(u"Ctrl+Shift+E"_s));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportPhoto);
    addActions({openAction, quitAction, fullScreen, undo, redo, resetAll, exportAction});
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
    m_adjust->show();
    m_original->setEnabled(true);
    m_export->setEnabled(true);
    m_showingOriginal = false;
}

void MainWindow::showFailed(const QString& path, const QString& message)
{
    m_stack->setCurrentWidget(m_canvas);
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
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
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
