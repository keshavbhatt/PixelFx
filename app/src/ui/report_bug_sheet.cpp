#include "ui/report_bug_sheet.h"

#include "core/about.h"
#include "core/log_sink.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kLogLines = 200;
constexpr int kLinesBeforeCrash = 40;
constexpr int kTextHeight = 92;
constexpr int kShownHeight = 140;

// The log file's lines, the rotated file's first.
QStringList logFileLines()
{
    QStringList lines;
    const QString path = core::LogSink::logFilePath();
    for (const QString& file : QStringList{path + u".1"_s, path}) {
        QFile f(file);
        if (!path.isEmpty() && f.open(QIODevice::ReadOnly)) {
            lines += QString::fromUtf8(f.readAll()).split(u'\n', Qt::SkipEmptyParts);
        }
    }
    return lines;
}

QStringList withoutNames(const QStringList& lines)
{
    QStringList out;
    out.reserve(lines.size());
    for (const QString& line : lines) {
        out << core::withoutNames(line);
    }
    return out;
}

} // namespace

ReportBugSheet::ReportBugSheet(const std::optional<core::crash::Report>& crash, QWidget* parent)
    : Sheet(parent)
{
    setWindowTitle(tr("Report a bug"));
    m_diagnosticsText = core::diagnosticsText(
        core::collectDiagnostics(QApplication::applicationVersion(), core::buildRevision()));
    const QStringList recent = core::LogSink::recentLines();
    m_logLines = withoutNames(recent.mid(std::max<qsizetype>(0, recent.size() - kLogLines)));
    if (crash) {
        m_crashText =
            core::crash::reportText(*crash, QApplication::applicationVersion(),
                                    withoutNames(core::previousRun(logFileLines(), kLinesBeforeCrash)));
    }

    auto* copy = new IconTextButton(icons::themed(u"copy"_s, tokens().text), tr("Copy report"), this);
    copy->setToolTip(tr("Copy the report as text"));
    connect(copy, &QPushButton::clicked, this, [this] {
        const core::BugReport r = report();
        QApplication::clipboard()->setText(core::reportTitle(r) + u"\n\n"_s + core::reportBody(r));
    });
    const FootButtons buttons = footButtons(this, tr("Open the issue tracker"), {copy}, true);
    buttons.primary->setToolTip(tr("Opens a new issue in your browser, filled in, to read and send"));
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, this, &ReportBugSheet::openTracker);

    QVBoxLayout* column = sheetLayout(this, buttons.row);
    column->addLayout(sheetTitleRow(this, tr("Report a bug")));
    if (crash) {
        auto* line = new QWidget(this);
        line->setObjectName(u"crashLine"_s);
        line->setAttribute(Qt::WA_StyledBackground);
        const QLocale locale;
        const QString when = crash->when.isValid()
                                 //: When PixelFx closed: "7 November at 14:02".
                                 ? tr("%1 at %2")
                                       .arg(locale.toString(crash->when.date(), u"d MMMM"_s),
                                            locale.toString(crash->when.time(), QLocale::ShortFormat))
                                 : QString();
        QLabel* text = warningLine(
            line, when.isEmpty()
                      ? tr("PixelFx closed unexpectedly. What it was doing is attached.")
                      : tr("PixelFx closed unexpectedly on %1. What it was doing is attached.").arg(when));
        text->setObjectName(u"crashText"_s); // the line's own colour: the glyph says it is a warning
        line->layout()->setContentsMargins(kGapControls + kGapSmall, kGapControls, kGapControls + kGapSmall,
                                           kGapControls);
        column->addWidget(line);
    }
    column->addLayout(buildWhatHappened());
    column->addWidget(sectionHeading(tr("Attached"), this));
    column->addLayout(buildAttached(crash.has_value()));
    auto* next = new QLabel(tr("The report opens in your browser on PixelFx's issue tracker, filled in, for "
                               "you to read and send. "
                               "PixelFx sends nothing itself."),
                            this);
    next->setObjectName(u"faint"_s);
    next->setWordWrap(true);
    column->addWidget(next);
    setSize(Size::Regular);
    m_text->setFocus();
}

ReportBugSheet::~ReportBugSheet() = default;

QLayout* ReportBugSheet::buildWhatHappened()
{
    auto* label = new QLabel(tr("What happened"), this);
    label->setObjectName(u"fieldLabel"_s);
    m_text = new QPlainTextEdit(this);
    m_text->setPlaceholderText(tr("What you did, what you expected, and what happened instead"));
    m_text->setAccessibleName(tr("What happened"));
    m_text->setFixedHeight(kTextHeight);
    m_text->setTabChangesFocus(true);
    label->setBuddy(m_text);
    auto* field = new QVBoxLayout;
    field->setSpacing(kGapSmall);
    field->addWidget(label);
    field->addWidget(m_text);
    return field;
}

QLayout* ReportBugSheet::buildAttached(bool crashed)
{
    auto* attached = new QVBoxLayout;
    attached->setSpacing(kGapRelated);
    if (crashed) {
        m_crash = addAttachment(attached, tr("What PixelFx was doing when it closed"),
                                tr("Where in PixelFx it stopped, and the lines the log wrote just before"),
                                m_crashText);
    }
    m_diagnostics = addAttachment(attached, tr("Diagnostics"),
                                  tr("Versions, system and the formats this build reads; nothing personal"),
                                  m_diagnosticsText);
    m_log = addAttachment(attached, tr("The last %1 lines of the log").arg(kLogLines),
                          tr("File and folder names are replaced by their kind: “a .jpg file”"),
                          m_logLines.join(u'\n'));
    return attached;
}

QCheckBox* ReportBugSheet::addAttachment(QVBoxLayout* column, const QString& name, const QString& note,
                                         const QString& text)
{
    auto* box = new QCheckBox(name, this);
    box->setChecked(true);
    auto* show = new QPushButton(tr("Show"), this);
    show->setObjectName(u"link"_s);
    show->setAccessibleName(tr("Show what is attached: %1").arg(name));
    auto* under = new QLabel(note, this);
    under->setObjectName(u"faint"_s);
    under->setWordWrap(true);
    under->setContentsMargins(box->sizeHint().height() + kGapSmall, 0, 0, 0);
    auto* view = new QPlainTextEdit(text, this);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFixedHeight(kShownHeight);
    view->setAccessibleName(name);
    view->hide();
    connect(show, &QPushButton::clicked, this, [this, show, view] {
        view->setVisible(!view->isVisible());
        show->setText(view->isVisible() ? tr("Hide") : tr("Show"));
        fitHeight();
    });
    auto* row = new QHBoxLayout;
    row->addWidget(box, 1);
    row->addWidget(show, 0, Qt::AlignTop);
    auto* item = new QVBoxLayout;
    item->setSpacing(0);
    item->addLayout(row);
    item->addWidget(under);
    item->addSpacing(kGapSmall);
    item->addWidget(view);
    column->addLayout(item);
    return box;
}

void ReportBugSheet::setWhatHappened(const QString& text)
{
    m_text->setPlainText(text);
}

core::BugReport ReportBugSheet::report() const
{
    core::BugReport r;
    r.whatHappened = m_text->toPlainText();
    if (m_crash != nullptr && m_crash->isChecked()) {
        r.crash = m_crashText;
    }
    if (m_diagnostics->isChecked()) {
        r.diagnostics = m_diagnosticsText;
    }
    if (m_log->isChecked()) {
        r.log = m_logLines;
    }
    return r;
}

QList<QCheckBox*> ReportBugSheet::attachments() const
{
    QList<QCheckBox*> out;
    for (QCheckBox* box : {m_crash, m_diagnostics, m_log}) {
        if (box != nullptr) {
            out << box;
        }
    }
    return out;
}

void ReportBugSheet::openTracker()
{
    const core::BugReport r = report();
    const QString title = core::reportTitle(r);
    const QString body = core::reportBody(r);
    QUrl url = core::issueUrl(title, body);
    if (url.isEmpty()) {
        QApplication::clipboard()->setText(body);
        url = core::fallbackUrl(title);
        Q_EMIT copiedForPasting();
    }
    QDesktopServices::openUrl(url);
    accept();
}

} // namespace pixelfx::ui
