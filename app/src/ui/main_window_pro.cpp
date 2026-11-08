#include "core/looks.h"
#include "core/presets.h"
#include "core/settings/settings.h"
#include "core/sidecar_store.h"
#include "services/licensing/license_service.h"
#include "ui/batch_page.h"
#include "ui/icons.h"
#include "ui/main_window.h"
#include "ui/presets_panel.h"
#include "ui/pro_gate.h"
#include "ui/pro_sheet.h"
#include "ui/style.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <pfx/io.h>

using namespace Qt::StringLiterals;
using Tier = pixelfx::services::LicenseService::Tier;
namespace licensing = pixelfx::core::licensing;

namespace pixelfx::ui {

namespace {

constexpr int kTrialNoteMs = 15000;

// The units an imported preset may set, for taking one off when it is no longer in the library:
// everything but the photo's geometry.
QStringList developUnits()
{
    QStringList units;
    for (const core::PresetUnit& unit : core::presetUnits()) {
        if (unit.id != u"crop"_s && unit.id != u"perspective"_s) {
            units << unit.id;
        }
    }
    return units;
}

} // namespace

void MainWindow::setLicense(services::LicenseService* license)
{
    m_license = license;
    delete m_pro;
    m_pro = new ProGate(license, this);
    m_batch->setProGate(m_pro);
    namedAction(u"pixelfxPro"_s)->setVisible(license != nullptr);
    if (license != nullptr) {
        connect(license, &services::LicenseService::statusChanged, this, [this] {
            refreshProAction();
            if (isVisible()) {
                showTrialEndIfDue();
            }
        });
    }
    refreshProAction();
}

void MainWindow::showPro()
{
    m_pro->showProSheet();
}

void MainWindow::refreshProAction()
{
    QAction* action = namedAction(u"pixelfxPro"_s);
    if (m_license == nullptr) {
        return;
    }
    m_presets->setProMarks(m_license->tier() != Tier::Pro);
    QString state;
    switch (m_license->tier()) {
    case Tier::Trial:
        state = tr("%n days left", nullptr, m_license->trialDaysLeft());
        break;
    case Tier::Free:
        state = tr("Free");
        break;
    case Tier::Pro:
        state = m_license->isForever()
                    ? tr("Forever")
                    : tr("Until %1").arg(QLocale().toString(m_license->paidUntil().date(), u"d MMM yyyy"_s));
        break;
    }
    // The menu shows what follows the tab at the right, where shortcuts go.
    action->setText(tr("PixelFx Pro") + u"\t"_s + state);
}

bool MainWindow::showTrialEndIfDue()
{
    if (m_license == nullptr || m_license->tier() != Tier::Free || m_settings.trialEndShown()) {
        return false;
    }
    m_settings.setTrialEndShown(true);
    // On the window, not the page stack, whose pages raise themselves when they change.
    auto* note = new QFrame(this);
    note->setObjectName(u"toast"_s);
    note->setAttribute(Qt::WA_StyledBackground);
    auto* glyph = new QLabel(note);
    glyph->setPixmap(icons::pixmap(u"info"_s, tokens().muted, 16, devicePixelRatioF()));
    auto* text =
        new QLabel(tr("Your Pro trial has ended. PixelFx stays free for one photo at a time."), note);
    auto* more = new QPushButton(tr("What Pro adds"), note);
    more->setObjectName(u"link"_s);
    auto* close = new QToolButton(note);
    close->setObjectName(u"flatIcon"_s);
    close->setIcon(icons::themed(u"close"_s, tokens().muted));
    close->setIconSize(QSize(14, 14));
    close->setToolTip(tr("Dismiss"));
    close->setAccessibleName(tr("Dismiss"));
    auto* row = new QHBoxLayout(note);
    row->setContentsMargins(12, 4, 6, 4);
    row->setSpacing(kGapControls);
    row->addWidget(glyph);
    row->addWidget(text);
    row->addWidget(more);
    row->addWidget(close);
    connect(close, &QToolButton::clicked, note, &QWidget::deleteLater);
    connect(more, &QPushButton::clicked, this, [this, note] {
        note->deleteLater();
        m_pro->showProSheet(licensing::Source::TrialEnd);
    });
    QTimer::singleShot(kTrialNoteMs, note, &QWidget::deleteLater);
    note->adjustSize();
    // Centred over the photo's area, 12 px under its top, once the window has its layout.
    const auto place = [this, note] {
        const QRect area(m_stack->mapTo(this, QPoint(0, 0)), m_stack->size());
        note->move(area.center().x() - (note->width() / 2), area.top() + 12);
        note->raise();
    };
    place();
    QTimer::singleShot(0, note, place);
    note->show();
    note->setAccessibleDescription(text->text());
    return true;
}

QList<licensing::ProUse> MainWindow::exportUses(const QStringList& paths, const QString& watermark)
{
    const QString open = m_document.info().path;
    const auto rawName = [](const QString& path) {
        return pfx::isRawFile(path.toStdString()) ? path : QString();
    };
    if (paths.size() <= 1) {
        return licensing::photoUses(m_document.settings(), rawName(open), watermark);
    }
    const core::SidecarStore store(core::SidecarStore::defaultFolder());
    std::vector<pfx::Settings> edits;
    for (const QString& path : paths) {
        if (path == open) {
            edits.push_back(m_document.settings());
        } else if (m_settings.rememberEdits()) {
            edits.push_back(store.read(core::fileFingerprint(path)).value_or(pfx::Settings{}));
        }
    }
    return licensing::batchUses({}, paths, edits, watermark);
}

void MainWindow::removeFromPhoto(licensing::ProItem item)
{
    const pfx::Settings settings = m_document.settings();
    if (item == licensing::ProItem::Lut) {
        m_document.apply(core::withLook(settings, {}, 0), tr("No look"));
        return;
    }
    if (item != licensing::ProItem::LightroomPreset) {
        return;
    }
    // The preset's units go back to their defaults, as before it was applied.
    const QString name = licensing::lightroomPreset(settings);
    QStringList units = developUnits();
    for (const core::PresetEntry& e : m_library.entries()) {
        if (licensing::lightroomPreset(e.settings) == name) {
            units = core::unitsIn(e.settings);
            break;
        }
    }
    pfx::Settings out;
    for (const std::string& key : settings.keys()) {
        const bool inPreset =
            std::ranges::any_of(units, [&key](const QString& id) { return core::keyInUnit(key, id); });
        if (!inPreset) {
            out.set(key, *settings.find(key));
        }
    }
    if (units.contains(u"look"_s)) {
        out = core::withLook(out, {}, 0);
    }
    m_document.apply(licensing::withoutLightroomPreset(out), tr("Remove %1").arg(name));
}

} // namespace pixelfx::ui
