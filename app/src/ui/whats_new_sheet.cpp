#include "ui/whats_new_sheet.h"

#include "core/changelog.h"
#include "ui/style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

WhatsNewSheet::WhatsNewSheet(const QString& markdown, const QString& version, bool showAfterUpdates,
                             QWidget* parent)
    : QDialog(parent)
    , m_markdown(markdown)
    , m_versions(new QComboBox(this))
    , m_text(new QTextBrowser(this))
    , m_more(new QLabel(this))
    , m_show(new QCheckBox(tr("Show this after each update"), this))
{
    setWindowTitle(tr("What's new"));
    auto* title = new QLabel(tr("What's new"), this);
    title->setObjectName(u"sheetTitle"_s);
    m_versions->setToolTip(tr("Version"));
    m_versions->setAccessibleName(tr("Version"));
    m_versions->setFixedWidth(150);
    for (const core::ChangelogRelease& r : core::changelogReleases(markdown)) {
        m_versions->addItem(r.version);
    }
    m_versions->setVisible(m_versions->count() > 1); // review: hidden while it holds one entry
    connect(m_versions, &QComboBox::currentTextChanged, this, &WhatsNewSheet::showVersion);
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    head->addWidget(title);
    head->addWidget(m_versions);
    head->addStretch();
    head->addWidget(sheetCloseButton(this));

    m_text->setObjectName(u"whatsNewText"_s);
    m_text->setOpenExternalLinks(true);
    m_text->setFrameShape(QFrame::NoFrame);
    m_text->document()->setIndentWidth(18); // the mock's list indent, not Qt's 40 px
    m_more->setText(u"<a href=\"#all\" style=\"color:%1\">%2</a>"_s.arg(tokens().accent.name(),
                                                                        tr("The full list of changes")));
    m_more->setObjectName(u"credit"_s);
    connect(m_more, &QLabel::linkActivated, this, &WhatsNewSheet::showAll);

    m_show->setChecked(showAfterUpdates);
    auto* close = new QPushButton(tr("Close"), this);
    close->setObjectName(u"primary"_s);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto* foot = new QHBoxLayout;
    foot->addWidget(m_show);
    foot->addStretch();
    foot->addWidget(close);

    QVBoxLayout* layout = sheetLayout(this, foot);
    layout->setSpacing(10);
    layout->addLayout(head);
    layout->addWidget(m_text, 1);
    layout->addWidget(m_more);
    setFixedWidth(600);
    resize(600, 520);
    showVersion(version);
}

WhatsNewSheet::~WhatsNewSheet() = default;

QString WhatsNewSheet::bundledChangelog()
{
    QFile file(u":/CHANGELOG.md"_s);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

void WhatsNewSheet::showVersion(const QString& version)
{
    m_version = version;
    m_all = false;
    if (m_versions->currentText() != version) {
        const QSignalBlocker block(m_versions);
        m_versions->setCurrentText(version);
    }
    render();
}

void WhatsNewSheet::showAll()
{
    m_all = true;
    render();
}

bool WhatsNewSheet::showAfterUpdates() const
{
    return m_show->isChecked();
}

QString WhatsNewSheet::html() const
{
    return m_text->toHtml();
}

void WhatsNewSheet::render()
{
    const core::ChangelogNotes notes = core::changelogNotes(core::changelogSection(m_markdown, m_version));
    const Tokens& t = tokens();
    QString html = u"<div style=\"color:%1\">"_s.arg(t.text.name());
    for (qsizetype i = 0; i < notes.lead.size(); ++i) {
        const QString text = core::changelogInlineHtml(notes.lead.at(i));
        html += i == 0 ? u"<p style=\"font-size:15px; font-weight:600; margin:0 0 6px 0\">%1</p>"_s.arg(text)
                       : u"<p style=\"color:%1; margin:0 0 6px 0\">%2</p>"_s.arg(t.muted.name(), text);
    }
    int shown = 0;
    int total = 0;
    for (const core::ChangelogGroup& g : notes.groups) {
        total += static_cast<int>(g.items.size());
        if (!m_all && shown >= kHighlights) {
            continue;
        }
        html += u"<p style=\"color:%1; font-size:11px; font-weight:700; margin:10px 0 4px 0\">%2</p><ul "
                "style=\"margin:0\">"_s.arg(t.muted.name(), g.title.toUpper().toHtmlEscaped());
        for (const QString& item : g.items) {
            if (!m_all && shown >= kHighlights) {
                break;
            }
            html += u"<li style=\"margin-bottom:4px\">%1</li>"_s.arg(core::changelogInlineHtml(item));
            ++shown;
        }
        html += u"</ul>"_s;
    }
    if (notes.lead.isEmpty() && notes.groups.isEmpty()) {
        html += u"<p style=\"color:%1\">%2</p>"_s.arg(t.muted.name(),
                                                      tr("No notes for this version.").toHtmlEscaped());
    }
    html += u"</div>"_s;
    m_text->setHtml(html);
    m_more->setVisible(!m_all && total > shown);
}

} // namespace pixelfx::ui
