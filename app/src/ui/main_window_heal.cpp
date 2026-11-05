// MainWindow's Heal mode, kept apart from the rest of the window.
#include "core/reveal_dust.h"
#include "ui/adjust_panel.h"
#include "ui/canvas.h"
#include "ui/filmstrip.h"
#include "ui/heal_panel.h"
#include "ui/heal_tool.h"
#include "ui/main_window.h"
#include "ui/presets_panel.h"

#include <QAction>
#include <QStackedWidget>
#include <QToolButton>

#include <pfx/heal.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr auto kSpots = "heal.spots";
} // namespace

void MainWindow::setupHeal()
{
    m_healTool = new HealTool(this);
    m_healPanel = new HealPanel(m_healTool, this);
    m_panels->addWidget(m_healPanel);
    // A new spot's patch is picked on the photo as decoded, the frame spots are placed in.
    m_healTool->setSourceFinder([this](const pfx::Spot& spot) {
        const auto source = m_document.previewSource();
        return source ? pfx::findHealSource(*source, spot) : spot;
    });
    connect(m_healTool, &HealTool::spotsChanged, this, &MainWindow::healSpotsChanged);
    connect(m_healTool, &HealTool::selectionChanged, m_canvas, qOverload<>(&QWidget::update));
    connect(m_healPanel, &HealPanel::doneRequested, this, &MainWindow::leaveHeal);
    connect(m_healPanel, &HealPanel::resetRequested, this, [this] {
        if (m_document.settings().spots(kSpots) != m_healStart.spots(kSpots)) {
            m_healTool->setSpots(m_healStart.spots(kSpots));
            healSpotsChanged(m_healTool->spots(), true, tr("Reset spots"));
            m_healPanel->refresh();
        }
    });
    connect(m_healPanel, &HealPanel::showSpotsToggled, this, [this](bool on) {
        m_healTool->setShowSpots(on);
        m_canvas->update();
    });
    connect(m_healPanel, &HealPanel::revealDustToggled, this, [this](bool on) {
        m_revealDust = on;
        if (isHealing()) {
            showHealImage(m_document.preview());
        }
    });
    // Undo and redo work while healing: the spots follow the settings.
    connect(&m_document, &services::Document::settingsReplaced, this, [this](const pfx::Settings& settings) {
        if (isHealing()) {
            m_healTool->setSpots(settings.spots(kSpots));
            m_healPanel->refresh();
            m_canvas->update();
        }
    });
    connect(m_healMode, &QToolButton::clicked, this, &MainWindow::enterHeal);
    setupHealActions();
}

void MainWindow::setupHealActions()
{
    auto* heal = new QAction(tr("Heal"), this);
    heal->setObjectName(u"heal"_s);
    heal->setShortcut(QKeySequence(Qt::Key_R));
    connect(heal, &QAction::triggered, this, [this] { isHealing() ? leaveHeal() : enterHeal(); });
    addAction(heal);
    const auto add = [this](const QString& name, const QKeySequence& key, const std::function<void()>& run) {
        auto* a = new QAction(name, this);
        a->setShortcut(key);
        a->setEnabled(false);
        connect(a, &QAction::triggered, this, run);
        addAction(a);
        m_healActions << a;
    };
    add(tr("Smaller brush or spot"), QKeySequence(Qt::Key_BracketLeft), [this] {
        m_healTool->resizeStep(false);
        m_canvas->update();
    });
    add(tr("Larger brush or spot"), QKeySequence(Qt::Key_BracketRight), [this] {
        m_healTool->resizeStep(true);
        m_canvas->update();
    });
    add(tr("Remove the selected spot"), QKeySequence(Qt::Key_Delete),
        [this] { m_healTool->removeSelected(); });
    add(tr("Show spots"), QKeySequence(Qt::Key_V),
        [this] { m_healPanel->setShowSpots(!m_healTool->showSpots()); });
    add(tr("Reveal dust"), QKeySequence(Qt::Key_D), [this] { m_healPanel->setRevealDust(!m_revealDust); });
    add(tr("Leave Heal"), QKeySequence(Qt::Key_Escape), [this] { leaveHeal(); });
}

void MainWindow::enterHeal()
{
    leaveBatch();
    if (isCropping()) {
        leaveCrop(true);
    }
    if (!m_document.isOpen() || isHealing()) {
        m_healMode->setChecked(isHealing());
        m_editMode->setChecked(!isHealing());
        return;
    }
    showOriginal(false);
    m_canvas->cancelPick();
    m_adjust->setPicking(false);
    m_compare->setChecked(false);
    m_compare->setEnabled(false);
    m_healing = true;
    m_healStart = m_document.settings();
    m_healTool->setSpots(m_healStart.spots(kSpots));
    m_healTool->select(-1);
    m_healPanel->refresh();
    m_document.setHealEditing(true); // the upright photo, no crop: the frame spots live in
    m_canvas->setTool(m_healTool);
    m_canvas->setFocus();
    showHealImage(m_document.preview());
    m_panels->setCurrentWidget(m_healPanel);
    m_presets->hide();
    m_strip->hide();
    showModeStrip(tr("Click to heal · drag to brush · [ and ] change the size · Delete removes the selected "
                     "spot · Escape leaves Heal"));
    for (QAction* a : std::as_const(m_healActions)) {
        a->setEnabled(true);
    }
    m_healMode->setChecked(true);
}

void MainWindow::leaveHeal()
{
    if (!isHealing()) {
        return;
    }
    for (QAction* a : std::as_const(m_healActions)) {
        a->setEnabled(false);
    }
    m_healing = false;
    m_canvas->setTool(nullptr);
    m_document.setHealEditing(false);
    m_canvas->setImage(m_document.preview(), m_document.displaySize());
    m_adjust->setSettings(m_document.settings());
    m_panels->setCurrentWidget(m_adjust);
    m_presets->show();
    m_strip->show();
    hideModeStrip();
    m_compare->setEnabled(true);
    updateHistoryButtons();
    m_editMode->setChecked(true);
}

void MainWindow::leaveTools()
{
    if (isCropping()) {
        leaveCrop(true);
    }
    leaveHeal();
}

void MainWindow::healSpotsChanged(const pfx::Spots& spots, bool finished, const QString& label)
{
    pfx::Settings settings = m_document.settings();
    if (spots.empty()) {
        settings.erase(kSpots);
    } else {
        settings.set(kSpots, spots);
    }
    m_document.setSettings(settings);
    if (finished) {
        m_document.commit(label); // each spot is a history step
    }
    m_canvas->update();
}

void MainWindow::showHealImage(const QImage& image)
{
    m_canvas->setImage(m_revealDust ? core::revealDust(image) : image, m_document.displaySize());
}

} // namespace pixelfx::ui
