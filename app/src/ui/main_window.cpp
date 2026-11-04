#include "ui/main_window.h"

#include "core/settings/settings.h"
#include "ui/canvas.h"
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
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QStackedWidget>
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

    connect(&m_document, &services::Document::openStarted, this, [this](const QString& path) {
        m_stack->setCurrentWidget(m_canvas);
        m_canvas->setStatus(tr("Opening %1").arg(QFileInfo(path).fileName()));
    });
    connect(&m_document, &services::Document::opened, this, &MainWindow::showOpened);
    connect(&m_document, &services::Document::previewReady, this,
            [this](const QImage& image) { m_canvas->setImage(image, m_document.info().size); });
    connect(&m_document, &services::Document::failed, this, &MainWindow::showFailed);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
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
    header->addWidget(menuButton);

    m_welcome = new WelcomePage(this);
    connect(m_welcome, &WelcomePage::openRequested, this, &MainWindow::chooseFiles);
    m_canvas = new Canvas(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_canvas);

    auto* central = new QWidget(this);
    auto* column = new QVBoxLayout(central);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_header);
    column->addWidget(m_stack, 1);
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
    addActions({openAction, quitAction, fullScreen});
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
