#include "ui/crop_panel.h"

#include "core/crop_geometry.h"
#include "ui/adjust_panel.h"
#include "ui/icons.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>

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
constexpr int kSide = 14;

QLabel* section(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setObjectName(u"section"_s);
    QFont f = label->font();
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.66); // .06em of 11 px (mock .sec)
    label->setFont(f);
    label->setContentsMargins(kSide, 12, kSide, 4);
    return label;
}

QPushButton* chip(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(u"chip"_s);
    b->setCheckable(true);
    b->setFocusPolicy(Qt::TabFocus);
    return b;
}

/// Chips in rows that wrap at the panel's width (mock .chips: 6 px gaps, 10 px sides).
QWidget* chipRows(const QList<QAbstractButton*>& chips, QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(10, 6, 10, 6);
    column->setSpacing(6);
    const int width = kPanelWidth - 20;
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
            row->setSpacing(6);
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
    auto* b = new QPushButton(icons::themed(glyph, tokens().text), text, parent);
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
    setObjectName(u"cropPanel"_s);
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(kPanelWidth);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(section(tr("Aspect ratio"), this));
    column->addWidget(buildRatios());
    column->addWidget(buildOrientation());
    column->addWidget(section(tr("Straighten and rotate"), this));
    column->addLayout(buildStraighten());
    column->addWidget(section(tr("Overlay"), this));
    column->addWidget(buildOverlays());
    column->addStretch();
    column->addLayout(buildFoot());
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
    connect(m_ratios, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton* b) { chooseRatio(b->property("ratioName").toString()); });
    return chipRows(ratioChips, this);
}

QWidget* CropPanel::buildOrientation()
{
    auto* orientation = new QWidget(this);
    auto* orientationRow = new QHBoxLayout(orientation);
    orientationRow->setContentsMargins(kSide, 4, kSide, 4);
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
    straighten->setContentsMargins(kSide, 0, kSide, 0);
    straighten->setSpacing(6);
    straighten->addWidget(m_angle);
    auto* turns = new QHBoxLayout;
    turns->setSpacing(6);
    for (QPushButton* b : {turnLeft, turnRight, flipH, flipV}) {
        turns->addWidget(b);
    }
    turns->addStretch();
    straighten->addLayout(turns);
    return straighten;
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

QLayout* CropPanel::buildFoot()
{
    auto* resetButton = new QPushButton(tr("Reset"), this);
    resetButton->setToolTip(tr("Undo the turn, flips, straightening and crop"));
    auto* cancel = new QPushButton(tr("Cancel"), this);
    cancel->setToolTip(tr("Leave without changes (Escape)"));
    auto* done = new QPushButton(tr("Done"), this);
    done->setObjectName(u"primary"_s);
    done->setToolTip(tr("Apply the crop (Enter)"));
    connect(resetButton, &QPushButton::clicked, this, &CropPanel::reset);
    connect(cancel, &QPushButton::clicked, this, [this] { Q_EMIT finished(false); });
    connect(done, &QPushButton::clicked, this, [this] { Q_EMIT finished(true); });
    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(kSide, 10, kSide, 12);
    foot->setSpacing(8);
    foot->addWidget(resetButton);
    foot->addStretch();
    foot->addWidget(cancel);
    foot->addWidget(done);
    return foot;
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
    // The chip of the current ratio; none when the ratio came from a turn of an unlisted one.
    const QSize size = uncropped();
    const double original = static_cast<double>(size.width()) / std::max(1, size.height());
    const QList<QAbstractButton*> chips = m_ratios->buttons();
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
    m_ratios->setExclusive(true);
    const QRectF crop = core::cropOf(m_settings);
    const bool portrait =
        m_ratio > 0.0 ? m_ratio < 1.0 : (crop.width() * size.width()) < (crop.height() * size.height());
    m_orientation->buttons().at(portrait ? 1 : 0)->setChecked(true);
}

} // namespace pixelfx::ui
