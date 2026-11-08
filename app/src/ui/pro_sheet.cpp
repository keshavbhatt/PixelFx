#include "ui/pro_sheet.h"

#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;
using Tier = pixelfx::services::LicenseService::Tier;
using Source = pixelfx::core::licensing::Source;

namespace pixelfx::ui {

namespace {

constexpr int kGlyph = 16;
constexpr int kPlanColumn = 64;

QLabel* glyphLabel(const QString& glyph, const QColor& color, int size, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setPixmap(icons::pixmap(glyph, color, size, parent->devicePixelRatioF()));
    label->setFixedSize(size, size);
    return label;
}

// "2 minutes ago", "3 hours ago", "on 6 November": when the server last answered.
QString checkedWhen(const QDateTime& at)
{
    const qint64 secs = at.secsTo(QDateTime::currentDateTime());
    if (secs < 60) {
        return ProSheet::tr("just now");
    }
    if (secs < 3600) {
        return ProSheet::tr("%n minutes ago", nullptr, static_cast<int>(secs / 60));
    }
    if (secs < 24 * 3600) {
        return ProSheet::tr("%n hours ago", nullptr, static_cast<int>(secs / 3600));
    }
    return ProSheet::tr("on %1").arg(QLocale().toString(at.date(), u"d MMMM"_s));
}

QString link(const QString& href, const QString& text)
{
    return u"<a href=\"%1\" style=\"color:%2; text-decoration:none\">%3</a>"_s.arg(href, tokens().link.name(),
                                                                                   text.toHtmlEscaped());
}

// One row of the table: the feature with an optional muted line, then Free and Pro.
struct PlanRow
{
    QString feature;
    QString detail;
    QString free; ///< a number, or empty for a tick, or "-" for none
    QString pro;
};

QLabel* planCell(const QString& value, bool pro, QWidget* parent)
{
    auto* cell = new QLabel(parent);
    cell->setAlignment(Qt::AlignCenter);
    cell->setFixedWidth(kPlanColumn);
    if (value.isEmpty()) {
        cell->setPixmap(icons::pixmap(u"check"_s, tokens().success, 14, parent->devicePixelRatioF()));
        cell->setToolTip(ProSheet::tr("Included"));
        cell->setAccessibleName(ProSheet::tr("Included"));
    } else if (value == u"-"_s) {
        cell->setText(u"–"_s);
        cell->setObjectName(u"planNo"_s);
        cell->setToolTip(ProSheet::tr("Not included"));
        cell->setAccessibleName(ProSheet::tr("Not included"));
    } else {
        cell->setText(value);
        cell->setObjectName(pro ? u"planYes"_s : u"planNo"_s);
    }
    return cell;
}

QWidget* planTable(QWidget* parent)
{
    const QList<PlanRow> rows = {
        {ProSheet::tr("Everything for one photo"),
         ProSheet::tr("Every adjustment, look and preset; crop, heal; export in every format"),
         {},
         {}},
        {ProSheet::tr("Make files smaller, any number of photos"), {}, {}, {}},
        {ProSheet::tr("Batch and paste settings"),
         ProSheet::tr("Free: %1 photos at a time").arg(core::licensing::kFreePhotos),
         QString::number(core::licensing::kFreePhotos), ProSheet::tr("Any")},
        {ProSheet::tr("Export camera RAW photos"),
         ProSheet::tr("Opening and editing a RAW is free"),
         u"-"_s,
         {}},
        {ProSheet::tr("Your Lightroom presets and LUTs in exports"),
         ProSheet::tr("Importing and trying them is free"),
         u"-"_s,
         {}},
        {ProSheet::tr("Watermarks"), {}, u"-"_s, {}},
        {ProSheet::tr("Share looks and presets as files, bake to .cube"), {}, u"-"_s, {}},
        {ProSheet::tr("Batch from the command line"), {}, u"-"_s, {}},
    };
    auto* table = new QWidget(parent);
    table->setAccessibleName(ProSheet::tr("What you get with Free and with Pro"));
    auto* grid = new QGridLayout(table);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(0);
    grid->setColumnStretch(0, 1);
    const QStringList heads = {ProSheet::tr("What you get"), ProSheet::tr("Free"), ProSheet::tr("Pro")};
    for (int c = 0; c < heads.size(); ++c) {
        QLabel* head = sectionLabel(heads.at(c), table);
        head->setAlignment(c == 0 ? Qt::AlignLeft : Qt::AlignHCenter);
        head->setContentsMargins(0, 0, 0, 6);
        grid->addWidget(head, 0, c);
    }
    int r = 1;
    for (const PlanRow& row : rows) {
        auto* rule = new QFrame(table);
        rule->setObjectName(u"divider"_s);
        rule->setFixedHeight(1);
        grid->addWidget(rule, r++, 0, 1, 3);
        auto* text = new QWidget(table);
        auto* lines = new QVBoxLayout(text);
        lines->setContentsMargins(0, 6, 0, 6);
        lines->setSpacing(0);
        lines->addWidget(new QLabel(row.feature, text));
        if (!row.detail.isEmpty()) {
            auto* detail = new QLabel(row.detail, text);
            detail->setObjectName(u"credit"_s);
            lines->addWidget(detail);
        }
        grid->addWidget(text, r, 0);
        grid->addWidget(planCell(row.free, false, table), r, 1);
        grid->addWidget(planCell(row.pro, true, table), r, 2);
        ++r;
    }
    return table;
}

} // namespace

QLabel* proMark(const QString& feature, QWidget* parent)
{
    auto* mark = new QLabel(ProSheet::tr("Pro"), parent);
    mark->setObjectName(u"proMark"_s);
    mark->setToolTip(ProSheet::tr("%1 are part of PixelFx Pro").arg(feature));
    mark->setAccessibleName(ProSheet::tr("Pro"));
    mark->setFixedHeight(16);
    return mark;
}

ProStatusBox::ProStatusBox(QWidget* parent)
    : QFrame(parent)
    , m_glyph(new QLabel(this))
    , m_title(new QLabel(this))
    , m_text(new QLabel(this))
    , m_extra(new QVBoxLayout)
{
    setObjectName(u"proStatus"_s);
    m_glyph->setFixedSize(kGlyph, kGlyph);
    m_title->setObjectName(u"proStatusTitle"_s);
    m_title->setWordWrap(true);
    m_text->setObjectName(u"credit"_s);
    m_text->setWordWrap(true);
    m_extra->setContentsMargins(0, 0, 0, 0);
    m_extra->setSpacing(0);
    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapTight);
    column->addWidget(m_title);
    column->addWidget(m_text);
    column->addLayout(m_extra);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(kGapGroups);
    row->addWidget(m_glyph, 0, Qt::AlignTop);
    row->addLayout(column, 1);
}

ProStatusBox::~ProStatusBox() = default;

void ProStatusBox::setGlyph(const QString& glyph, const QColor& color)
{
    m_glyph->setPixmap(icons::pixmap(glyph, color, kGlyph, devicePixelRatioF()));
}

void ProStatusBox::setTitle(const QString& title)
{
    m_title->setText(title);
}

void ProStatusBox::setText(const QString& text)
{
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());
}

ProSheet::ProSheet(services::LicenseService& license, QWidget* parent, Source source)
    : Sheet(parent)
    , m_license(license)
    , m_status(new ProStatusBox(this))
    , m_meter(new QProgressBar(this))
    , m_account(new QLineEdit(this))
    , m_accountNote(new QLabel(this))
{
    setWindowTitle(tr("PixelFx Pro"));
    m_meter->setObjectName(u"proMeter"_s);
    m_meter->setTextVisible(false);
    m_meter->setFixedHeight(4);
    m_meter->setRange(0, m_license.trialDays());
    m_status->extra()->addSpacing(kGapControls - kGapTight);
    m_status->extra()->addWidget(m_meter);

    m_plans = planTable(this);

    auto* accountHead = new QLabel(tr("Account ID"), this);
    accountHead->setObjectName(u"recentTitle"_s);
    m_account->setObjectName(u"accountId"_s);
    m_account->setReadOnly(true);
    m_account->setAccessibleName(tr("Account ID"));
    auto* copy = new QPushButton(icons::themed(u"copy"_s, tokens().text), tr("Copy"), this);
    copy->setObjectName(u"small"_s);
    copy->setToolTip(tr("Copy the Account ID"));
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_account->text()); });
    auto* accountRow = new QHBoxLayout;
    accountRow->setSpacing(kGapControls);
    accountRow->addWidget(m_account, 1);
    accountRow->addWidget(copy);
    m_accountNote->setObjectName(u"credit"_s);
    m_accountNote->setWordWrap(true);
    connect(m_accountNote, &QLabel::linkActivated, &m_license, &services::LicenseService::refresh);

    m_pass = new QFrame(this);
    m_pass->setObjectName(u"passLine"_s);
    auto* passText =
        new QLabel(tr("A <b>Ktechpit Pass</b> unlocks PixelFx and every other Ktechpit app."), m_pass);
    passText->setObjectName(u"small"_s);
    passText->setWordWrap(true);
    auto* passLink = new QLabel(link(u"#pass"_s, tr("Add PixelFx to it")), m_pass);
    passLink->setObjectName(u"small"_s);
    passLink->setToolTip(tr("Opens your Ktechpit Pass on ktechpit.com with PixelFx filled in"));
    connect(passLink, &QLabel::linkActivated, this,
            [this] { QDesktopServices::openUrl(m_license.passUrl()); });
    auto* passRow = new QHBoxLayout(m_pass);
    passRow->setContentsMargins(12, 10, 12, 10);
    passRow->setSpacing(kGapItems);
    passRow->addWidget(glyphLabel(u"layers"_s, tokens().muted, 14, m_pass), 0, Qt::AlignVCenter);
    passRow->addWidget(passText, 1);
    passRow->addWidget(passLink);

    auto* manage = new QPushButton(tr("Manage purchases"), this);
    manage->setObjectName(u"link"_s);
    manage->setToolTip(tr("Your purchases on ktechpit.com"));
    connect(manage, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(m_license.portalUrl()); });
    const FootButtons buttons = footButtons(this, tr("Choose a plan"), {manage});
    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons.row->insertWidget(buttons.row->indexOf(buttons.primary), close);
    m_choose = buttons.primary;
    m_choose->setIcon(icons::themed(u"external"_s, Qt::white));
    m_choose->setToolTip(tr("Opens the PixelFx checkout in your browser, with the prices in your currency"));
    connect(m_choose, &QPushButton::clicked, this,
            [this, source] { QDesktopServices::openUrl(m_license.checkoutUrl(source)); });

    auto* content = new QVBoxLayout;
    content->setSpacing(0);
    content->addWidget(m_status);
    content->addSpacing(14);
    content->addWidget(m_plans);
    content->addSpacing(16);
    content->addWidget(accountHead);
    content->addSpacing(6);
    content->addLayout(accountRow);
    content->addSpacing(6);
    content->addWidget(m_accountNote);
    content->addSpacing(14);
    content->addWidget(m_pass);
    QVBoxLayout* column = sheetLayout(this, buttons.row);
    addScrollingContent(column, sheetTitleRow(this, tr("PixelFx Pro")), content, this);

    connect(&m_license, &services::LicenseService::statusChanged, this, &ProSheet::refresh);
    connect(&m_license, &services::LicenseService::checkingChanged, this, &ProSheet::refresh);
    refresh();
    setSize(Size::Regular);
    // Focus starts on the way on, not in the Account ID.
    (m_choose->isHidden() ? close : static_cast<QWidget*>(m_choose))->setFocus();
}

ProSheet::~ProSheet() = default;

void ProSheet::refresh()
{
    const Tier tier = m_license.tier();
    const QLocale locale;
    switch (tier) {
    case Tier::Trial: {
        const int left = m_license.trialDaysLeft();
        m_status->setGlyph(u"star"_s, tokens().text);
        m_status->setTitle(tr("Pro trial: %n days left", nullptr, left));
        m_status->setText(
            tr("Everything in Pro is yours to use until %1. After that PixelFx stays free for one "
               "photo at a time.")
                .arg(locale.toString(m_license.trialEnd().date(), u"d MMMM"_s)));
        const int used = qBound(0, m_license.trialDays() - left, m_license.trialDays());
        m_meter->setValue(used);
        m_meter->setToolTip(tr("%1 of %2 days used").arg(used).arg(m_license.trialDays()));
        break;
    }
    case Tier::Free:
        m_status->setGlyph(u"image"_s, tokens().muted);
        m_status->setTitle(tr("Free"));
        m_status->setText(tr("Everything for one photo at a time. Pro adds the tools for many photos and for "
                             "the files you already own."));
        break;
    case Tier::Pro:
        m_status->setGlyph(u"check"_s, tokens().success);
        m_status->setTitle(
            m_license.isForever()
                ? tr("Pro, Forever")
                : tr("Pro, until %1").arg(locale.toString(m_license.paidUntil().date(), u"d MMMM yyyy"_s)));
        m_status->setText(tr("Thank you. Every Pro tool is yours, in every update."));
        break;
    }
    m_meter->setVisible(tier == Tier::Trial);
    m_plans->setVisible(tier != Tier::Pro);
    m_pass->setVisible(tier != Tier::Pro);
    m_choose->setVisible(tier != Tier::Pro);
    m_account->setText(m_license.accountId());

    QString note = tr("The checkout asks for it, and it is how a purchase finds this copy of PixelFx.");
    if (m_license.checking()) {
        note += u" "_s + tr("Checking with Ktechpit...");
    } else if (tier == Tier::Pro && m_license.lastChecked().isValid()) {
        note += u" "_s + tr("Checked with Ktechpit %1.").arg(checkedWhen(m_license.lastChecked()));
    } else if (tier != Tier::Pro) {
        note += u" "_s + tr("Bought already? %1.").arg(link(u"#check"_s, tr("Check again")));
    }
    m_accountNote->setText(note);
    fitHeight();
}

QString ProSheet::statusTitle() const
{
    return m_status->findChild<QLabel*>(u"proStatusTitle"_s)->text();
}

bool ProSheet::showsPlans() const
{
    return !m_plans->isHidden();
}

} // namespace pixelfx::ui
