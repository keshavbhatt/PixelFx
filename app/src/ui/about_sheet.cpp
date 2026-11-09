#include "ui/about_sheet.h"

#include "core/about.h"
#include "core/changelog.h"
#include "core/links.h"
#include "core/log_sink.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

#include <AppStrip.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kNoticesHeight = 400;

// "5 November 2026", in the language of the app.
QString longDate(const QDate& date)
{
    return QLocale().toString(date, u"d MMMM yyyy"_s);
}

QString revision()
{
    return core::buildRevision();
}

// "Released 5 November 2026" from the bundled changelog, or "Development build, 5 November 2026"
// with the date of the revision it was built from.
QString buildLine()
{
    QFile file(u":/CHANGELOG.md"_s);
    if (file.open(QIODevice::ReadOnly)) {
        for (const core::ChangelogRelease& r : core::changelogReleases(QString::fromUtf8(file.readAll()))) {
            const QDate date = QDate::fromString(r.date, Qt::ISODate);
            if (r.version == QApplication::applicationVersion() && date.isValid()) {
                return AboutSheet::tr("Released %1").arg(longDate(date));
            }
        }
    }
    const QDate built = QDate::fromString(qApp->property("revisionDate").toString(), Qt::ISODate);
    return built.isValid() ? AboutSheet::tr("Development build, %1").arg(longDate(built))
                           : AboutSheet::tr("Development build");
}

QPushButton* smallButton(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(u"small"_s);
    return b;
}

} // namespace

AboutSheet::AboutSheet(QWidget* parent)
    : Sheet(parent)
    , m_tabs(new QButtonGroup(this))
    , m_pages(new QStackedWidget(this))
{
    setWindowTitle(tr("About PixelFx"));
    auto* tabRow = new QHBoxLayout;
    tabRow->setSpacing(0);
    const QStringList names = {tr("About"), tr("Open-source notices"), tr("Diagnostics")};
    for (int i = 0; i < names.size(); ++i) {
        auto* b = new QPushButton(names.at(i), this);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        m_tabs->addButton(b, i);
        tabRow->addWidget(b, 1);
    }
    m_pages->addWidget(buildAboutPage());
    m_pages->addWidget(buildNoticesPage());
    m_pages->addWidget(buildDiagnosticsPage());
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int id) { showPage(static_cast<Page>(id)); });

    const FootButtons buttons = footButtons(this, tr("Close"));
    connect(buttons.primary, &QPushButton::clicked, this, &QDialog::accept);

    QVBoxLayout* layout = sheetLayout(this, buttons.row);
    layout->addLayout(sheetTitleRow(this, tr("About PixelFx")));
    layout->addLayout(tabRow);
    layout->addWidget(m_pages, 1);
    setSize(Size::Regular);
    showPage(AboutPage);
}

AboutSheet::~AboutSheet() = default;

namespace {

// The app icon, the part it draws 60 px at its widest, with the clear canvas around that part cut
// away. The canvas margin is not even (the lens's side sits below it), so the cut follows what is
// drawn rather than taking the same from each edge.
QLabel* appIcon(QWidget* parent)
{
    constexpr int kShown = 60;
    constexpr int kProbe = 256;
    const QImage probe = icons::brand().pixmap(QSize(kProbe, kProbe), 1.0).toImage();
    QRect drawn;
    for (int y = 0; y < probe.height(); ++y) {
        for (int x = 0; x < probe.width(); ++x) {
            if (qAlpha(probe.pixel(x, y)) > 8) {
                drawn |= QRect(x, y, 1, 1);
            }
        }
    }
    if (drawn.isEmpty()) {
        drawn = probe.rect();
    }
    // The canvas size at which the drawn part is kShown px at its widest.
    const int canvasSize = qCeil(kShown * qreal(kProbe) / qMax(drawn.width(), drawn.height()));
    const QPixmap canvas = icons::brand().pixmap(canvasSize, canvasSize);
    const qreal ratio = canvas.devicePixelRatio();
    const qreal scale = canvas.width() / qreal(kProbe);
    const int side = qRound(kShown * ratio);
    const QPointF centre = QRectF(drawn).center() * scale;
    QPixmap shown = QPixmap::fromImage(
        canvas.toImage().copy(qRound(centre.x() - side / 2.0), qRound(centre.y() - side / 2.0), side, side));
    shown.setDevicePixelRatio(ratio);
    auto* icon = new QLabel(parent);
    icon->setPixmap(shown);
    icon->setFixedSize(kShown, kShown);
    return icon;
}

} // namespace

QWidget* AboutSheet::buildAboutPage()
{
    auto* page = new QWidget(this);
    QLabel* icon = appIcon(page);
    auto* name = new QLabel(tr("PixelFx %1").arg(QApplication::applicationVersion()), page);
    name->setObjectName(u"aboutName"_s);
    auto* tagline = new QLabel(tr("A quick photo editor: sliders, looks, presets and batch."), page);
    tagline->setObjectName(u"muted"_s);
    auto* build = new QLabel(buildLine(), page);
    build->setObjectName(u"faint"_s);
    build->setTextInteractionFlags(Qt::TextSelectableByMouse);
    build->setToolTip(revision());
    auto* text = new QVBoxLayout;
    text->setSpacing(kGapTight);
    text->addWidget(name);
    text->addWidget(tagline);
    text->addWidget(build);
    auto* head = new QHBoxLayout;
    head->setSpacing(kGapItems);
    head->addWidget(icon, 0, Qt::AlignVCenter);
    head->addLayout(text, 1);

    auto* whatsNew = smallButton(tr("What's new"), page);
    connect(whatsNew, &QPushButton::clicked, this, &AboutSheet::whatsNewRequested);
    auto* guide = smallButton(tr("Online guide"), page);
    connect(guide, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(core::guideUrl())); });
    auto* source = smallButton(tr("Source code"), page);
    connect(source, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(core::sourceUrl())); });
    auto* bug = smallButton(tr("Report a bug"), page);
    bug->setToolTip(tr("Put a report together, with Diagnostics, to send from your browser"));
    connect(bug, &QPushButton::clicked, this, &AboutSheet::reportBugRequested);
    auto* links = new QHBoxLayout;
    links->setSpacing(kGapRelated);
    links->addWidget(whatsNew);
    links->addWidget(guide);
    links->addWidget(source);
    links->addWidget(bug);
    links->addStretch();

    auto* divider = new QFrame(page);
    divider->setObjectName(u"divider"_s);
    divider->setFixedHeight(1);
    auto* credit = new QLabel(tr("Made by Keshav Bhatt and contributors. PixelFx is free software under the "
                                 "GPL, version 3 or later; its image library, libpixelfx, is Apache-2.0. It "
                                 "is built on Qt, libvips, Little CMS, libheif, libjxl, LibRaw and more: see "
                                 "Open-source notices."),
                              page);
    credit->setObjectName(u"credit"_s);
    credit->setWordWrap(true);

    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 4, 0, 0);
    column->setSpacing(kGapGroups);
    column->addLayout(head);
    column->addLayout(links);
    column->addWidget(divider);
    column->addWidget(credit);
    // The developer's other apps, two drawn afresh each time About opens.
    auto* moreDivider = new QFrame(page);
    moreDivider->setObjectName(u"divider"_s);
    moreDivider->setFixedHeight(1);
    column->addWidget(moreDivider);
    column->addWidget(
        new ktechpit::AppStrip(u"about"_s, 2, ktechpit::AppStrip::Orientation::Horizontal, page));
    column->addStretch();
    return page;
}

QWidget* AboutSheet::buildNoticesPage()
{
    auto* page = new QWidget(this);
    m_components = new QListWidget(page);
    m_components->setObjectName(u"noticeList"_s);
    m_components->setAccessibleName(tr("Components"));
    for (const core::Notice& n : core::notices()) {
        //: A row of the open-source notices: a component, its licence and what it does.
        auto* item = new QListWidgetItem(tr("%1  ·  %2").arg(n.name, n.licence), m_components);
        item->setToolTip(n.use);
    }
    m_components->setFixedHeight(150);
    m_notice = new QPlainTextEdit(page);
    m_notice->setObjectName(u"noticeText"_s);
    m_notice->setReadOnly(true);
    m_notice->setAccessibleName(tr("Licence text"));
    connect(m_components, &QListWidget::currentRowChanged, this, &AboutSheet::showNotice);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 4, 0, 0);
    column->setSpacing(kGapControls);
    column->addWidget(m_components);
    column->addWidget(m_notice, 1);
    m_components->setCurrentRow(0);
    return page;
}

QWidget* AboutSheet::buildDiagnosticsPage()
{
    auto* page = new QWidget(this);
    auto* hint =
        new QLabel(tr("Paste this into a bug report. It holds no photo names and no personal data."), page);
    hint->setObjectName(u"credit"_s);
    hint->setWordWrap(true);
    m_diagnostics = new QPlainTextEdit(page);
    m_diagnostics->setObjectName(u"diagnostics"_s);
    m_diagnostics->setReadOnly(true);
    m_diagnostics->setAccessibleName(tr("Diagnostics"));
    m_diagnostics->setPlainText(
        core::diagnosticsText(core::collectDiagnostics(QApplication::applicationVersion(), revision())));
    m_diagnostics->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diagnostics->ensurePolished();
    // Tall enough for every line, so nothing scrolls.
    const int lines = m_diagnostics->document()->blockCount();
    const QMargins margins = m_diagnostics->contentsMargins();
    m_diagnostics->setFixedHeight((lines * m_diagnostics->fontMetrics().lineSpacing()) +
                                  static_cast<int>(2 * m_diagnostics->document()->documentMargin()) +
                                  margins.top() + margins.bottom() + (2 * m_diagnostics->frameWidth()));
    auto* copy = smallButton(tr("Copy"), page);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(diagnostics()); });
    auto* logs = smallButton(tr("Open the log folder"), page);
    const QString logFile = core::LogSink::logFilePath();
    logs->setEnabled(!logFile.isEmpty());
    connect(logs, &QPushButton::clicked, this,
            [logFile] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(logFile).absolutePath())); });
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(kGapRelated);
    buttons->addWidget(copy);
    buttons->addWidget(logs);
    buttons->addStretch();
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 4, 0, 0);
    column->setSpacing(kGapControls);
    column->addWidget(hint);
    column->addWidget(m_diagnostics, 1);
    column->addLayout(buttons);
    return page;
}

void AboutSheet::showPage(Page page)
{
    m_tabs->button(page)->setChecked(true);
    m_pages->setCurrentIndex(page);
    // Each page as tall as it needs; the notices get room to read a licence.
    m_pages->setFixedHeight(page == NoticesPage ? kNoticesHeight
                                                : m_pages->currentWidget()->sizeHint().height());
    fitHeight();
}

AboutSheet::Page AboutSheet::page() const
{
    return static_cast<Page>(m_pages->currentIndex());
}

QString AboutSheet::diagnostics() const
{
    return m_diagnostics->toPlainText();
}

void AboutSheet::showNotice(int row)
{
    const auto& list = core::notices();
    if (row < 0 || row >= list.size()) {
        return;
    }
    if (m_components->currentRow() != row) {
        m_components->setCurrentRow(row); // comes back here through currentRowChanged
        return;
    }
    m_notice->setPlainText(core::noticeText(list.at(row)));
}

QString AboutSheet::noticeShown() const
{
    return m_notice->toPlainText();
}

} // namespace pixelfx::ui
