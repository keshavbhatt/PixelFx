#pragma once

#include <QWidget>

#include <pfx/settings.h>
#include <string>
#include <vector>

class QLabel;
class QToolButton;
class QVBoxLayout;

namespace pixelfx::ui {

class AdjustSlider;

/// One collapsible group of the adjust panel: a header (chevron, icon, name, changed dot,
/// reset) and a body of sliders and other controls, with an optional "More" section at its end
/// for the finer settings, closed until asked for. The group owns the settings keys of its
/// sliders and of any other control in it (registered with addKeys), so whether it is changed
/// and what Reset clears are known in one place.
class AdjustGroup : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustGroup)

public:
    /// `id` names the group in saved state and does not change with the language; `name` is shown.
    AdjustGroup(QString id, const QString& name, const QString& glyph, QWidget* parent);
    ~AdjustGroup() override;

    void addContent(QWidget* widget);
    void addSlider(AdjustSlider* slider);
    /// The same, into the "More" section (made on first use).
    void addMoreContent(QWidget* widget);
    void addMoreSlider(AdjustSlider* slider);
    /// A sub-heading inside the body or the More section ("Sharpening").
    void addCaption(const QString& text, bool more);
    [[nodiscard]] bool hasMore() const { return m_moreBody != nullptr; }
    void setMoreOpen(bool open);
    [[nodiscard]] bool isMoreOpen() const;
    /// The name shown and said ("B&W mix" for the Color mixer in black and white); the id stays.
    void setName(const QString& name);
    /// Settings keys of controls that are not sliders (curves, wheels, turns).
    void addKeys(const std::vector<std::string>& keys);
    [[nodiscard]] const std::vector<AdjustSlider*>& sliders() const { return m_sliders; }
    [[nodiscard]] const QString& id() const { return m_id; }
    [[nodiscard]] const QString& name() const { return m_name; }
    [[nodiscard]] QToolButton* resetButton() const { return m_reset; }

    void setExpanded(bool open);
    [[nodiscard]] bool isExpanded() const;
    /// Whether any of the group's settings differs from its default in `settings`.
    [[nodiscard]] bool isChanged(const pfx::Settings& settings) const;
    /// Removes the group's settings from `settings` and puts its sliders back to their defaults.
    void reset(pfx::Settings& settings) const;
    /// Shows or hides the changed dot and the reset button.
    void setChanged(bool changed);

private:
    void updateChevron();
    void makeMore();

    QString m_id;
    QString m_name;
    QToolButton* m_toggle;
    QLabel* m_chevron;
    QLabel* m_dot;
    QToolButton* m_reset;
    QWidget* m_body;
    QVBoxLayout* m_bodyLayout;
    QToolButton* m_moreToggle = nullptr;
    QWidget* m_moreBody = nullptr;
    QVBoxLayout* m_moreLayout = nullptr;
    std::vector<AdjustSlider*> m_sliders;
    std::vector<std::string> m_keys;
};

} // namespace pixelfx::ui
