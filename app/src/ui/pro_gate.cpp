#include "ui/pro_gate.h"

#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/pro_sheet.h"
#include "ui/style.h"

#include <QDesktopServices>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace licensing = core::licensing;

namespace {

QString checkAgainLink()
{
    return u"<a href=\"#check\" style=\"color:%1; text-decoration:none\">%2</a>"_s.arg(
        tokens().link.name(), ProPrompt::tr("Check again"));
}

} // namespace

QString describeUses(const QList<licensing::ProUse>& uses)
{
    QStringList lines;
    for (const licensing::ProUse& use : uses) {
        //: A Pro item and which one: "Lightroom preset · Clean Travel".
        lines << ProPrompt::tr("%1 · %2").arg(licensing::itemLabel(use.item), use.name);
    }
    return lines.join(u'\n');
}

ProPrompt::ProPrompt(services::LicenseService& license, licensing::Source source, const QString& title,
                     const QString& text, QWidget* parent)
    : Sheet(parent)
    , m_license(license)
    , m_source(source)
    , m_text(new QLabel(text, this))
    , m_note(new QLabel(this))
{
    setWindowTitle(title);
    m_text->setWordWrap(true);
    m_note->setObjectName(u"credit"_s);
    m_note->setWordWrap(true);
    connect(m_note, &QLabel::linkActivated, &m_license, &services::LicenseService::refresh);

    auto* whatPro = new QPushButton(tr("What Pro adds"), this);
    whatPro->setObjectName(u"link"_s);
    connect(whatPro, &QPushButton::clicked, this, [this] { done(WhatProAdds); });
    m_freeWay = new QPushButton(this);
    m_freeWay->hide();
    connect(m_freeWay, &QPushButton::clicked, this, [this] { done(FreeWay); });
    const FootButtons buttons = footButtons(this, tr("Choose a plan"), {whatPro});
    buttons.row->insertWidget(buttons.row->indexOf(buttons.primary), m_freeWay);
    buttons.primary->setIcon(icons::themed(u"external"_s, Qt::white));
    buttons.primary->setToolTip(
        tr("Opens the PixelFx checkout in your browser, with the prices in your currency"));
    connect(buttons.primary, &QPushButton::clicked, this, [this, source] {
        QDesktopServices::openUrl(m_license.checkoutUrl(source));
        done(ChosePlan);
    });

    QHBoxLayout* heading = sheetTitleRow(this, title);
    m_title = qobject_cast<QLabel*>(heading->itemAt(0)->widget());
    auto* content = new QVBoxLayout;
    content->setSpacing(kGapControls);
    content->addWidget(m_text);
    content->addWidget(m_note);
    QVBoxLayout* column = sheetLayout(this, buttons.row);
    column->addLayout(heading);
    column->addLayout(content);

    // Check again: the answer shows here; a purchase lets the job go on.
    connect(&m_license, &services::LicenseService::checkingChanged, this, &ProPrompt::refreshNote);
    connect(&m_license, &services::LicenseService::statusChanged, this, [this] {
        if (m_license.hasPro()) {
            done(NowPro);
        } else {
            refreshNote();
        }
    });
    refreshNote();
    setSize(Size::Narrow);
}

ProPrompt::~ProPrompt() = default;

void ProPrompt::setFreeWay(const QString& label)
{
    m_freeWay->setText(label);
    m_freeWay->setVisible(!label.isEmpty());
}

void ProPrompt::setNote(const QString& note)
{
    m_noteText = note;
    refreshNote();
}

QString ProPrompt::title() const
{
    return m_title->text();
}

QString ProPrompt::text() const
{
    return m_text->text();
}

void ProPrompt::refreshNote()
{
    const QString check = m_license.checking() ? tr("Checking with Ktechpit...")
                                               : tr("Bought already? %1.").arg(checkAgainLink());
    m_note->setText(m_noteText.isEmpty() ? check : m_noteText.toHtmlEscaped() + u" "_s + check);
    fitHeight();
}

ProGate::ProGate(services::LicenseService* license, QWidget* window)
    : QObject(window)
    , m_license(license)
    , m_window(window)
{}

ProGate::~ProGate() = default;

bool ProGate::hasPro() const
{
    return m_license == nullptr || m_license->hasPro();
}

int ProGate::admitPhotos(Count job, int count, const QString& what)
{
    if (hasPro() || count <= licensing::kFreePhotos) {
        return count;
    }
    const std::unique_ptr<ProPrompt> prompt(newCountPrompt(job, count, what));
    switch (run(*prompt)) {
    case ProPrompt::FreeWay:
        return licensing::kFreePhotos;
    case ProPrompt::NowPro:
        return count;
    default:
        return 0;
    }
}

ProPrompt* ProGate::newCountPrompt(Count job, int count, const QString& what) const
{
    const int free = licensing::kFreePhotos;
    QString title;
    QString text;
    QString freeWay;
    licensing::Source source = licensing::Source::Batch;
    switch (job) {
    case Count::Batch:
    case Count::BatchRest:
        title = tr("Batch %n photos with Pro", nullptr, count);
        text = tr("Free batches up to %1 photos at a time. With Pro, all %2 go in one run.")
                   .arg(free)
                   .arg(count);
        freeWay =
            job == Count::Batch ? tr("Process the first %1").arg(free) : tr("Process the next %1").arg(free);
        break;
    case Count::Export:
        title = tr("Export %n photos with Pro", nullptr, count);
        text =
            tr("Free exports up to %1 photos at a time. With Pro, all %2 go at once.").arg(free).arg(count);
        freeWay = tr("Export the first %1").arg(free);
        source = licensing::Source::Export;
        break;
    case Count::Paste:
        title = what.isEmpty() ? tr("Paste settings to %n photos with Pro", nullptr, count)
                               : tr("%1 on %n photos with Pro", nullptr, count).arg(what);
        text = tr("Free changes up to %1 photos at a time. With Pro, all %2 at once.").arg(free).arg(count);
        freeWay =
            what.isEmpty() ? tr("Paste to the first %1").arg(free) : tr("Apply to the first %1").arg(free);
        source = licensing::Source::Paste;
        break;
    }
    auto* prompt = new ProPrompt(*m_license, source, title, text, m_window);
    prompt->setFreeWay(freeWay);
    if (job == Count::Batch || job == Count::BatchRest) {
        prompt->setNote(tr("Making files smaller stays free for any number of photos."));
    }
    return prompt;
}

bool ProGate::admit(licensing::Source source, const QString& title, const QString& text)
{
    if (hasPro()) {
        return true;
    }
    ProPrompt prompt(*m_license, source, title, text, m_window);
    return run(prompt) == ProPrompt::NowPro;
}

bool ProGate::admitUses(licensing::Source source, const QString& title, const QList<licensing::ProUse>& uses)
{
    if (uses.isEmpty() || hasPro()) {
        return true;
    }
    return admit(source, title,
                 tr("Edit with them as long as you like; saving needs Pro.") + u"\n\n"_s +
                     describeUses(uses));
}

void ProGate::showProSheet(licensing::Source source)
{
    if (m_license == nullptr) {
        return;
    }
    ProSheet sheet(*m_license, m_window, source);
    sheet.exec();
}

void ProGate::openCheckout(licensing::Source source)
{
    if (m_license != nullptr) {
        QDesktopServices::openUrl(m_license->checkoutUrl(source));
    }
}

int ProGate::run(ProPrompt& prompt)
{
    Q_EMIT prompting(&prompt);
    const int outcome = prompt.exec();
    if (outcome == ProPrompt::WhatProAdds) {
        // After the prompt has gone, so the sheet does not stack on it.
        QTimer::singleShot(0, this, [this, source = prompt.source()] { showProSheet(source); });
    }
    return outcome;
}

} // namespace pixelfx::ui
