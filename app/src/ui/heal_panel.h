#pragma once

#include <QWidget>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace pixelfx::ui {

class HealTool;
class ListRow;
class SliderRow;

/// The Heal panel: Spot or Brush; the brush's size, feather and opacity (the selected spot's
/// while one is selected); Show spots and Reveal dust; the list of spots with Remove all; then
/// Reset and Done.
class HealPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HealPanel)

public:
    explicit HealPanel(HealTool* tool, QWidget* parent = nullptr);
    ~HealPanel() override;

    /// Shows the tool's spots, selection and brush.
    void refresh();
    [[nodiscard]] const std::vector<ListRow*>& spotRows() const { return m_rows; }
    [[nodiscard]] SliderRow* sizeSlider() const { return m_size; }
    void setShowSpots(bool on);
    void setRevealDust(bool on);
    [[nodiscard]] bool revealDust() const;

Q_SIGNALS:
    void showSpotsToggled(bool on);
    void revealDustToggled(bool on);
    /// Reset: the spots back to how they were when Heal opened.
    void resetRequested();
    void doneRequested();

private:
    QWidget* buildBrush();
    QWidget* buildSpots();
    QWidget* buildSwitch(const QString& label, const QString& key, QCheckBox** box);
    void refreshValues();

    HealTool* m_tool;
    QButtonGroup* m_modes = nullptr;
    SliderRow* m_size = nullptr;
    SliderRow* m_feather = nullptr;
    SliderRow* m_opacity = nullptr;
    QCheckBox* m_show = nullptr;
    QCheckBox* m_reveal = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_removeAll = nullptr;
    QVBoxLayout* m_list = nullptr;
    std::vector<ListRow*> m_rows;
    bool m_refreshing = false;
};

} // namespace pixelfx::ui
