#pragma once

#include "ui/canvas.h"

#include <QObject>
#include <QPointF>

#include <functional>
#include <pfx/settings.h>
#include <vector>

namespace pixelfx::ui {

/// Heal on the canvas: a click heals a spot the brush's size, a drag paints a
/// stroke healed along its length; every spot stays editable. The selected spot shows its
/// source (hollow, joined by a dashed line): drag the solid circle to move the spot, the hollow
/// one to choose another patch. Positions are fractions of the upright photo as shown.
class HealTool : public QObject, public CanvasTool
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HealTool)

public:
    enum class Mode
    {
        Spot,
        Brush
    };

    explicit HealTool(QObject* parent = nullptr);
    ~HealTool() override;

    /// Picks a new spot's source (the library's findHealSource on the photo).
    void setSourceFinder(std::function<pfx::Spot(const pfx::Spot&)> finder) { m_finder = std::move(finder); }

    /// Shows `spots` without emitting; the selection is kept when it still exists.
    void setSpots(const pfx::Spots& spots);
    [[nodiscard]] const pfx::Spots& spots() const { return m_spots; }
    /// The selected spot's index, or -1.
    [[nodiscard]] int selected() const { return m_selected; }
    void select(int index);

    void setMode(Mode mode) { m_mode = mode; }
    [[nodiscard]] Mode mode() const { return m_mode; }
    /// The brush: size is the diameter as a share of the long edge; feather and opacity 0 to 100.
    /// With a spot selected these change that spot instead (`finished` makes it a history step).
    void setSize(double size, bool finished);
    void setFeather(double feather, bool finished);
    void setOpacity(double opacity, bool finished);
    [[nodiscard]] double size() const;
    [[nodiscard]] double feather() const;
    [[nodiscard]] double opacity() const;
    /// [ and ]: the brush or the selected spot a step smaller or larger.
    void resizeStep(bool larger);
    /// Moves the selected spot by `delta` (fractions), the arrow keys.
    void nudge(QPointF delta);
    /// A spot of the brush's size at `fraction` of the photo, its patch picked, as a click makes.
    void addSpotAt(QPointF fraction);
    void removeSelected();
    void removeAll();
    /// Show spots (V): off hides the circles but keeps the healing.
    void setShowSpots(bool on);
    [[nodiscard]] bool showSpots() const { return m_show; }

    void paint(QPainter& painter, const QRectF& photo) override;
    bool press(const QMouseEvent& event, const QRectF& photo) override;
    void move(const QMouseEvent& event, const QRectF& photo) override;
    void release(const QMouseEvent& event, const QRectF& photo) override;
    void leave() override;

    static constexpr double kMinSize = 0.005;
    static constexpr double kMaxSize = 0.25;

Q_SIGNALS:
    /// The spots changed; `finished` with a history label when a gesture ended.
    void spotsChanged(const pfx::Spots& spots, bool finished, const QString& label);
    void selectionChanged(int index);
    /// The brush's size, feather or opacity changed (for the panel's sliders).
    void brushChanged();

private:
    enum class Drag
    {
        None,
        Brush,
        Target,
        Source
    };
    [[nodiscard]] static QPointF toFraction(QPointF pos, const QRectF& photo);
    [[nodiscard]] static QPointF toWidget(QPointF fraction, const QRectF& photo);
    [[nodiscard]] static double radiusOnScreen(const pfx::Spot& spot, const QRectF& photo);
    [[nodiscard]] int spotAt(QPointF pos, const QRectF& photo) const;
    [[nodiscard]] bool onSource(QPointF pos, const QRectF& photo) const;
    void addSpot(pfx::Spot spot);
    static void paintSpot(QPainter& painter, const pfx::Spot& spot, const QRectF& photo, bool selected);
    void changeSelected(const std::function<void(pfx::Spot&)>& change, bool finished, const QString& label);

    pfx::Spots m_spots;
    int m_selected = -1;
    Mode m_mode = Mode::Spot;
    double m_size = 0.04;
    double m_feather = 50.0;
    double m_opacity = 100.0;
    bool m_show = true;
    std::function<pfx::Spot(const pfx::Spot&)> m_finder;
    Drag m_drag = Drag::None;
    QPointF m_pressFraction;
    pfx::Spot m_pressSpot;
    std::vector<QPointF> m_stroke; // fractions, while brushing
    QPointF m_hover{-1, -1};       // widget coordinates; negative when away
    QRectF m_lastPhoto;
};

} // namespace pixelfx::ui
