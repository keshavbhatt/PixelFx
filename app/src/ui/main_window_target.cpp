// Picking a colour on the photo for the Color mixer and the B&W mix.
#include "ui/adjust_panel.h"
#include "ui/canvas.h"
#include "ui/color_target_tool.h"
#include "ui/main_window.h"
#include "ui/presets_panel.h"

#include <QAction>

#include <pfx/mixer.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

void MainWindow::connectColorTarget()
{
    m_colorTarget = new ColorTargetTool(this);
    auto* target = new QAction(tr("Adjust a colour on the photo"), this);
    target->setObjectName(u"colorTarget"_s);
    target->setShortcut(QKeySequence(Qt::Key_T));
    target->setCheckable(true);
    connect(target, &QAction::triggered, this,
            [this](bool on) { on ? startColorTarget() : stopColorTarget(); });
    addAction(target);
    auto* stop = new QAction(tr("Stop adjusting a colour"), this);
    stop->setShortcut(QKeySequence(Qt::Key_Escape));
    stop->setEnabled(false);
    connect(stop, &QAction::triggered, this, &MainWindow::stopColorTarget);
    addAction(stop);
    m_colorTargetStop = stop;
    connect(m_adjust, &AdjustPanel::targetToggled, this,
            [this](bool on) { on ? startColorTarget() : stopColorTarget(); });
    connect(m_adjust, &AdjustPanel::targetModeChanged, this, [this] {
        if (isColorTargeting()) {
            m_colorTarget->setBanner(m_adjust->targetBanner());
            m_canvas->update();
        }
    });
    // What is under the pointer before a press, and the colour a press picks.
    const auto bandsAt = [this](QPointF fraction) {
        const auto colour = m_document.colorBeforeMixer(fraction);
        return colour ? pfx::hueBands((*colour)[0], (*colour)[1], (*colour)[2]) : pfx::HueBands{};
    };
    connect(m_colorTarget, &ColorTargetTool::hovered, this, [this, bandsAt](QPointF fraction) {
        m_colorTarget->setHint(m_adjust->targetName(bandsAt(fraction)), m_adjust->targetDragWords());
    });
    connect(m_colorTarget, &ColorTargetTool::pressed, this, [this, bandsAt](QPointF fraction) {
        const pfx::HueBands bands = bandsAt(fraction);
        m_colorTarget->setHint(m_adjust->targetName(bands), m_adjust->targetDragWords());
        m_adjust->pickTarget(bands);
    });
    connect(m_colorTarget, &ColorTargetTool::dragged, m_adjust, &AdjustPanel::dragTarget);
}

bool MainWindow::isColorTargeting() const
{
    return m_adjust->isTargeting();
}

void MainWindow::startColorTarget()
{
    if (!m_document.isOpen() || isInTool()) {
        stopColorTarget();
        return;
    }
    m_presets->endPreview(); // Escape is the picker's now
    m_canvas->cancelPick();  // one picker at a time: not the white balance one too
    m_adjust->setPicking(false);
    m_adjust->setTargeting(true);
    m_colorTarget->setBanner(m_adjust->targetBanner());
    m_colorTarget->setHint({}, {});
    m_canvas->setTool(m_colorTarget);
    m_colorTargetStop->setEnabled(true);
    namedAction(u"colorTarget"_s)->setChecked(true);
}

void MainWindow::stopColorTarget()
{
    if (m_colorTarget == nullptr) {
        return;
    }
    const bool was = isColorTargeting();
    m_adjust->setTargeting(false);
    m_colorTargetStop->setEnabled(false);
    namedAction(u"colorTarget"_s)->setChecked(false);
    if (was) {
        m_canvas->setTool(nullptr);
    }
}

} // namespace pixelfx::ui
