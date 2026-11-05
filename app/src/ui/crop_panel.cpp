#include "ui/crop_panel.h"

#include "core/crop_geometry.h"
#include "ui/adjust_panel.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/sheet.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

struct RatioChip
{
    const char* name;
    double ratio; // width over height; 0 free, -1 the photo's own
};

constexpr std::array kRatios = {RatioChip{QT_TRANSLATE_NOOP("CropPanel", "Free"), 0.0},
                                RatioChip{QT_TRANSLATE_NOOP("CropPanel", "Original"), -1.0},
                                RatioChip{"1:1", 1.0},
                                RatioChip{"4:5", 4.0 / 5.0},
                                RatioChip{"5:7", 5.0 / 7.0},
                                RatioChip{"3:2", 3.0 / 2.0},
                                RatioChip{"16:9", 16.0 / 9.0},
                                RatioChip{"9:16", 9.0 / 16.0}};

struct OverlayChip
{
    const char* name;
    CropOverlay overlay;
};

constexpr std::array kOverlays = {
    OverlayChip{QT_TRANSLATE_NOOP("CropPanel", "Thirds"), CropOverlay::Thirds},
    OverlayChip{QT_TRANSLATE_NOOP("CropPanel", "Grid"), CropOverlay::Grid},
    OverlayChip{QT_TRANSLATE_NOOP("CropPanel", "Golden"), CropOverlay::Golden},
    OverlayChip{QT_TRANSLATE_NOOP("CropPanel", "Diagonal"), CropOverlay::Diagonal},
    OverlayChip{QT_TRANSLATE_NOOP("CropPanel", "None"), CropOverlay::None}};

constexpr int kPanelWidth = 316;

QPushButton* chip(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(u"chip"_s);
    b->setCheckable(true);
    b->setFocusPolicy(Qt::TabFocus);
    return b;
}

/// Chips in rows that wrap at the panel's width.
QWidget* chipRows(const QList<QAbstractButton*>& chips, QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(kToolRowPadding, 6, kToolRowPadding, 6);
    column->setSpacing(kGapRelated);
    const int width = kPanelWidth - (2 * kToolRowPadding);
    QHBoxLayout* row = nullptr;
    int used = 0;
    for (QAbstractButton* c : chips) {
        c->ensurePolished();
        const int w = c->sizeHint().width();
        if (row == nullptr || used + 6 + w > width) {
            if (row != nullptr) {
                row->addStretch();
            }
            row = new QHBoxLayout;
            row->setSpacing(kGapRelated);
            column->addLayout(row);
            used = -6;
        }
        row->addWidget(c);
        used += 6 + w;
    }
    if (row != nullptr) {
        row->addStretch();
    }
    return box;
}

QPushButton* smallButton(const QString& glyph, const QString& text, const QString& tip, QWidget* parent)
{
    auto* b = new IconTextButton(icons::themed(glyph, tokens().text), text, parent);
    b->setObjectName(u"small"_s);
    b->setToolTip(tip);
    b->setAccessibleName(tip);
    return b;
}

} // namespace

CropPanel::CropPanel(QWidget* parent)
    : QWidget(parent)
    , m_ratios(new QButtonGroup(this))
    , m_orientation(new QButtonGroup(this))
    , m_overlays(new QButtonGroup(this))
{
    makeSidePanel(this, Qt::LeftEdge);
    setFixedWidth(kPanelWidth);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(sectionHeading(tr("Aspect ratio"), this, kPanelPadding));
    column->addWidget(buildRatios());
    column->addWidget(buildOrientation());
    column->addWidget(sectionHeading(tr("Straighten and rotate"), this, kPanelPadding));
    column->addLayout(buildStraighten());
    column->addWidget(sectionHeading(tr("Perspective"), this, kPanelPadding));
    column->addLayout(buildPerspective());
    column->addWidget(sectionHeading(tr("Overlay"), this, kPanelPadding));
    column->addWidget(buildOverlays());
    column->addStretch();
    column->addWidget(buildFoot());
}

QWidget* CropPanel::buildRatios()
{
    QList<QAbstractButton*> ratioChips;
    for (const RatioChip& r : kRatios) {
        auto* c = chip(tr(r.name), this);
        c->setProperty("ratioName", QString::fromLatin1(r.name));
        m_ratios->addButton(c);
        ratioChips << c;
    }
    connect(m_ratios, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* b) {
        if (b == m_customChip) {
            askCustomRatio();
        } else {
            chooseRatio(b->property("ratioName").toString());
        }
    });
    m_customChip = chip(tr("Custom"), this);
    m_customChip->setToolTip(tr("A ratio of your own, such as 7:5"));
    m_ratios->addButton(m_customChip);
    ratioChips << m_customChip;
    return chipRows(ratioChips, this);
}

QWidget* CropPanel::buildOrientation()
{
    auto* orientation = new QWidget(this);
    auto* orientationRow = new QHBoxLayout(orientation);
    orientationRow->setContentsMargins(kPanelPadding, 4, kPanelPadding, 4);
    orientationRow->setSpacing(0);
    auto* orientationLabel = new QLabel(tr("Orientation"), orientation);
    orientationLabel->setObjectName(u"adjustName"_s);
    orientationRow->addWidget(orientationLabel);
    orientationRow->addStretch();
    for (const QString& name : {tr("Landscape"), tr("Portrait")}) {
        auto* b = new QPushButton(name, orientation);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setToolTip(tr("Swap landscape and portrait (X)"));
        m_orientation->addButton(b);
        orientationRow->addWidget(b);
    }
    connect(m_orientation, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* b) {
        const bool portrait = m_orientation->buttons().indexOf(b) == 1;
        const QSize size = uncropped();
        const QRectF crop = core::cropOf(m_settings);
        const bool isPortrait = (crop.width() * size.width()) < (crop.height() * size.height());
        if (portrait != isPortrait) {
            swapOrientation();
        } else {
            refreshControls();
        }
    });
    return orientation;
}

QLayout* CropPanel::buildStraighten()
{
    m_angle = new AdjustSlider(u"geometry.angle"_s, tr("Straighten"), this);
    m_angle->setStepsPerUnit(10.0);
    connect(m_angle, &AdjustSlider::moved, this, &CropPanel::setAngle);
    auto* turnLeft = smallButton(u"rotate-ccw"_s, u"90°"_s, tr("Turn left"), this);
    auto* turnRight = smallButton(u"rotate-cw"_s, u"90°"_s, tr("Turn right"), this);
    auto* flipH = smallButton(u"flip-h"_s, tr("Flip"), tr("Flip horizontally"), this);
    auto* flipV = smallButton(u"flip-v"_s, tr("Flip"), tr("Flip vertically"), this);
    connect(turnLeft, &QPushButton::clicked, this, [this] { turn(false); });
    connect(turnRight, &QPushButton::clicked, this, [this] { turn(true); });
    connect(flipH, &QPushButton::clicked, this, [this] { flip(Qt::Horizontal); });
    connect(flipV, &QPushButton::clicked, this, [this] { flip(Qt::Vertical); });
    auto* straighten = new QVBoxLayout;
    straighten->setContentsMargins(kPanelPadding, 0, kPanelPadding, 0);
    straighten->setSpacing(kGapRelated);
    straighten->addWidget(m_angle);
    auto* turns = new QHBoxLayout;
    turns->setSpacing(kGapRelated);
    for (QPushButton* b : {turnLeft, turnRight, flipH, flipV}) {
        turns->addWidget(b);
    }
    turns->addStretch();
    straighten->addLayout(turns);
    auto* level = smallButton(u"ruler"_s, tr("Draw a level line"),
                              tr("Drag along something that should be level or upright"), this);
    connect(level, &QPushButton::clicked, this, &CropPanel::levelLineRequested);
    auto* levelRow = new QHBoxLayout;
    levelRow->addWidget(level);
    levelRow->addStretch();
    straighten->addLayout(levelRow);
    return straighten;
}

QLayout* CropPanel::buildPerspective()
{
    auto* box = new QVBoxLayout;
    box->setContentsMargins(kPanelPadding, 0, kPanelPadding, 0);
    box->setSpacing(0);
    const std::array<std::pair<const char*, QString>, 2> sliders = {
        std::pair{"geometry.perspective.vertical", tr("Vertical")},
        std::pair{"geometry.perspective.horizontal", tr("Horizontal")}};
    for (const auto& [key, label] : sliders) {
        auto* s = new AdjustSlider(QString::fromLatin1(key), label, this);
        s->setToolTip(tr("Straighten lines that lean together, such as a building shot from below"));
        connect(s, &AdjustSlider::moved, this, [this, s](double v) { setPerspective(s->key(), v); });
        box->addWidget(s);
        m_perspective.push_back(s);
    }
    return box;
}

void CropPanel::setPerspective(const QString& key, double value)
{
    if (value == 0.0) {
        m_settings.erase(key.toStdString());
    } else {
        m_settings.set(key.toStdString(), value);
    }
    changed();
}

QWidget* CropPanel::buildOverlays()
{
    QList<QAbstractButton*> overlayChips;
    for (const OverlayChip& o : kOverlays) {
        auto* c = chip(tr(o.name), this);
        c->setChecked(o.overlay == m_overlay);
        m_overlays->addButton(c, static_cast<int>(o.overlay));
        overlayChips << c;
    }
    connect(m_overlays, &QButtonGroup::idClicked, this, [this](int id) {
        m_overlay = static_cast<CropOverlay>(id);
        Q_EMIT overlayChanged(m_overlay);
    });
    return chipRows(overlayChips, this);
}

QWidget* CropPanel::buildFoot()
{
    auto* resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setToolTip(tr("Undo the turn, flips, straightening and crop"));
    connect(resetButton, &QPushButton::clicked, this, &CropPanel::reset);
    const FootButtons buttons = footButtons(this, tr("Done"), {resetButton}, true);
    buttons.cancel->setToolTip(tr("Leave without changes (Escape)"));
    buttons.primary->setToolTip(tr("Apply the crop (Enter)"));
    connect(buttons.cancel, &QPushButton::clicked, this, [this] { Q_EMIT finished(false); });
    connect(buttons.primary, &QPushButton::clicked, this, [this] { Q_EMIT finished(true); });
    return footBar(this, buttons.row, kPanelPadding);
}

CropPanel::~CropPanel() = default;

void CropPanel::begin(const pfx::Settings& settings, const QSize& photo)
{
    m_settings = settings;
    m_photo = photo;
    m_ratio = 0.0;
    refreshControls();
    Q_EMIT ratioChanged(m_ratio);
}

QSize CropPanel::uncropped() const
{
    return core::uncroppedSize(m_settings, m_photo);
}

void CropPanel::setCrop(const QRectF& crop)
{
    core::setCrop(m_settings, crop);
    changed();
}

void CropPanel::chooseRatio(const QString& name)
{
    for (const RatioChip& r : kRatios) {
        if (name == QLatin1StringView(r.name)) {
            const QSize size = uncropped();
            const double ratio =
                r.ratio < 0 ? static_cast<double>(size.width()) / std::max(1, size.height()) : r.ratio;
            if (r.ratio == 0.0) {
                setRatio(0.0);
                refreshControls();
                return;
            }
            core::setCrop(m_settings, core::fitRatio(core::cropOf(m_settings), size, ratio));
            setRatio(ratio);
            changed();
            return;
        }
    }
}

void CropPanel::askCustomRatio()
{
    // "Custom asks for numbers": width and height, in a small sheet.
    Sheet sheet(this);
    sheet.setWindowTitle(tr("Custom ratio"));
    auto* width = new QSpinBox(&sheet);
    auto* height = new QSpinBox(&sheet);
    for (QSpinBox* box : {width, height}) {
        box->setRange(1, 1000);
    }
    width->setValue(m_customWidth);
    height->setValue(m_customHeight);
    width->setAccessibleName(tr("Width"));
    height->setAccessibleName(tr("Height"));
    const FootButtons buttons = footButtons(&sheet, tr("Use ratio"), {}, true);
    connect(buttons.cancel, &QPushButton::clicked, &sheet, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, &sheet, &QDialog::accept);
    auto* ratio = new QHBoxLayout;
    ratio->addWidget(width, 1);
    ratio->addWidget(new QLabel(u":"_s, &sheet));
    ratio->addWidget(height, 1);
    QVBoxLayout* column = sheetLayout(&sheet, buttons.row);
    column->addLayout(sheetTitleRow(&sheet, tr("Custom ratio")));
    column->addWidget(new QLabel(tr("Width to height, for example 16 : 9 or 5 : 4."), &sheet));
    column->addLayout(ratio);
    sheet.setSize(Sheet::Size::Narrow);
    width->setFocus();
    width->selectAll();
    if (sheet.exec() == QDialog::Accepted) {
        setCustomRatio(width->value(), height->value());
    } else {
        refreshControls();
    }
}

void CropPanel::setCustomRatio(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    m_customWidth = width;
    m_customHeight = height;
    const double ratio = static_cast<double>(width) / height;
    core::setCrop(m_settings, core::fitRatio(core::cropOf(m_settings), uncropped(), ratio));
    setRatio(ratio);
    changed();
}

void CropPanel::levelTo(double lineDegrees)
{
    // The line is drawn on the photo as shown (already straightened by the current angle). A
    // line within 45 degrees of upright is made vertical, otherwise horizontal.
    double tilt = std::fmod(lineDegrees, 180.0);
    if (tilt > 90.0) {
        tilt -= 180.0;
    } else if (tilt < -90.0) {
        tilt += 180.0;
    }
    if (std::abs(tilt) > 45.0) {
        tilt -= std::copysign(90.0, tilt);
    }
    setAngle(std::clamp(m_settings.number("geometry.angle", 0.0) - tilt, -45.0, 45.0));
}

void CropPanel::setRatio(double ratio)
{
    if (ratio != m_ratio) {
        m_ratio = ratio;
        Q_EMIT ratioChanged(m_ratio);
    }
}

void CropPanel::swapOrientation()
{
    const QSize size = uncropped();
    if (m_ratio > 0.0) {
        setRatio(1.0 / m_ratio);
        core::setCrop(m_settings, core::fitRatio(core::cropOf(m_settings), size, m_ratio));
    } else {
        core::setCrop(m_settings, core::swapCrop(core::cropOf(m_settings), size));
    }
    changed();
}

void CropPanel::turn(bool clockwise)
{
    core::turn(m_settings, clockwise);
    if (m_ratio > 0.0) {
        setRatio(1.0 / m_ratio);
    }
    changed();
}

void CropPanel::flip(Qt::Orientation orientation)
{
    core::flip(m_settings, orientation);
    changed();
}

void CropPanel::setAngle(double degrees)
{
    core::setAngle(m_settings, degrees);
    if (m_ratio > 0.0) { // the straightened photo changed shape: keep the box's ratio
        core::setCrop(m_settings, core::conformRatio(core::cropOf(m_settings), uncropped(), m_ratio));
    }
    changed();
}

void CropPanel::reset()
{
    pfx::Settings next;
    for (const std::string& key : m_settings.keys()) {
        if (pfx::groupOf(key) != "geometry") {
            next.set(key, *m_settings.find(key));
        }
    }
    m_settings = std::move(next);
    setRatio(0.0);
    changed();
}

void CropPanel::changed()
{
    refreshControls();
    Q_EMIT settingsChanged(m_settings);
}

void CropPanel::refreshControls()
{
    m_angle->setValue(m_settings.number("geometry.angle", 0.0));
    for (AdjustSlider* s : m_perspective) {
        s->setValue(m_settings.number(s->key().toStdString(), 0.0));
    }
    // The chip of the current ratio; none when the ratio came from a turn of an unlisted one.
    const QSize size = uncropped();
    const double original = static_cast<double>(size.width()) / std::max(1, size.height());
    QList<QAbstractButton*> chips = m_ratios->buttons();
    chips.removeOne(m_customChip);
    m_ratios->setExclusive(false);
    for (qsizetype i = 0; i < chips.size(); ++i) {
        const RatioChip& r = kRatios.at(static_cast<std::size_t>(i));
        const double value = r.ratio < 0 ? original : r.ratio;
        const bool on =
            (m_ratio == 0.0) ? r.ratio == 0.0 : (r.ratio != 0.0 && std::abs(value - m_ratio) < 1e-6);
        chips.at(i)->setChecked(on);
    }
    // Original and a listed ratio can match the same photo; show only the first.
    bool seen = false;
    for (QAbstractButton* c : chips) {
        if (c->isChecked()) {
            c->setChecked(!seen);
            seen = true;
        }
    }
    m_customChip->setChecked(!seen && m_ratio > 0.0); // a ratio no chip names: the user's own
    m_ratios->setExclusive(true);
    const QRectF crop = core::cropOf(m_settings);
    const bool portrait =
        m_ratio > 0.0 ? m_ratio < 1.0 : (crop.width() * size.width()) < (crop.height() * size.height());
    m_orientation->buttons().at(portrait ? 1 : 0)->setChecked(true);
}

} // namespace pixelfx::ui
