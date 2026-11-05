#include "ui/about_sheet.h"

#include "core/about.h"
#include "core/changelog.h"
#include "core/log_sink.h"
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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

const auto kSourceUrl = u"https://github.com/keshavbhatt/PixelFx"_s;
const auto kIssuesUrl = u"https://github.com/keshavbhatt/PixelFx/issues"_s;

QString revision()
{
    const QString r = qApp->property("revision").toString();
    return r == u"unknown"_s ? QString() : r;
}

// "Released 5 November 2026" from the bundled changelog, or "Development build (66cb50c)".
QString buildLine()
{
    QFile file(u":/CHANGELOG.md"_s);
    if (file.open(QIODevice::ReadOnly)) {
        for (const core::ChangelogRelease& r : core::changelogReleases(QString::fromUtf8(file.readAll()))) {
            const QDate date = QDate::fromString(r.date, Qt::ISODate);
            if (r.version == QApplication::applicationVersion() && date.isValid()) {
                return AboutSheet::tr("Released %1").arg(QLocale().toString(date, QLocale::LongFormat));
            }
        }
    }
    return revision().isEmpty() ? AboutSheet::tr("Development build")
                                : AboutSheet::tr("Development build (%1)").arg(revision());
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
    auto* title = new QLabel(tr("About PixelFx"), this);
    title->setObjectName(u"sheetTitle"_s);
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

    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    foot->addWidget(close);

    QVBoxLayout* layout = sheetLayout(this, foot);
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(sheetCloseButton(this));
    layout->addLayout(titleRow);
    layout->addLayout(tabRow);
    layout->addWidget(m_pages, 1);
    setFixedSize(520, 480); // the same on every tab
    showPage(AboutPage);
}

AboutSheet::~AboutSheet() = default;

QWidget* AboutSheet::buildAboutPage()
{
    auto* page = new QWidget(this);
    auto* icon = new QLabel(page);
    icon->setPixmap(QIcon(u":/icons/pixelfx.png"_s).pixmap(56, 56));
    icon->setFixedSize(56, 56);
    auto* name = new QLabel(tr("PixelFx %1").arg(QApplication::applicationVersion()), page);
    name->setObjectName(u"aboutName"_s);
    auto* tagline = new QLabel(tr("A quick photo editor: sliders, looks, presets and batch."), page);
    tagline->setObjectName(u"muted"_s);
    auto* build = new QLabel(buildLine(), page);
    build->setObjectName(u"faint"_s);
    build->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(name);
    text->addWidget(tagline);
    text->addWidget(build);
    auto* head = new QHBoxLayout;
    head->setSpacing(14);
    head->addWidget(icon, 0, Qt::AlignVCenter);
    head->addLayout(text, 1);

    auto* whatsNew = smallButton(tr("What's new"), page);
    connect(whatsNew, &QPushButton::clicked, this, &AboutSheet::whatsNewRequested);
    auto* source = smallButton(tr("Source code"), page);
    connect(source, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(kSourceUrl)); });
    auto* bug = smallButton(tr("Report a bug"), page);
    bug->setToolTip(tr("Opens the issue tracker. Diagnostics has text to paste into the report."));
    connect(bug, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(kIssuesUrl)); });
    auto* links = new QHBoxLayout;
    links->setSpacing(6);
    links->addWidget(whatsNew);
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
    column->setSpacing(12);
    column->addLayout(head);
    column->addLayout(links);
    column->addWidget(divider);
    column->addWidget(credit);
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
    column->setSpacing(8);
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
    auto* copy = smallButton(tr("Copy"), page);
    connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(diagnostics()); });
    auto* logs = smallButton(tr("Open the log folder"), page);
    const QString logFile = core::LogSink::logFilePath();
    logs->setEnabled(!logFile.isEmpty());
    connect(logs, &QPushButton::clicked, this,
            [logFile] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(logFile).absolutePath())); });
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    buttons->addWidget(copy);
    buttons->addWidget(logs);
    buttons->addStretch();
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 4, 0, 0);
    column->setSpacing(8);
    column->addWidget(hint);
    column->addWidget(m_diagnostics, 1);
    column->addLayout(buttons);
    return page;
}

void AboutSheet::showPage(Page page)
{
    m_tabs->button(page)->setChecked(true);
    m_pages->setCurrentIndex(page);
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
