#pragma once

#include <QWidget>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;

namespace pixelfx::ui {

class HealTool;

/// The Heal panel: Spot or Brush, the brush's size, feather and
/// opacity (the selected spot's while one is selected), Show spots and Reveal dust, the list of
/// spots with Remove all, then Reset and Done.
class HealPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(HealPanel)

public:
    explicit HealPanel(HealTool* tool, QWidget* parent = nullptr);
    ~HealPanel() override;

    /// Shows the tool's spots, selection and brush.
    void refresh();
    [[nodiscard]] QListWidget* spotList() const { return m_list; }
    [[nodiscard]] QSlider* sizeSlider() const { return m_size; }
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
    static QSlider* addSlider(const QString& name, int minimum, int maximum, QWidget* parent, QLabel** value);
    void refreshValues();

    HealTool* m_tool;
    QButtonGroup* m_modes = nullptr;
    QSlider* m_size = nullptr;
    QSlider* m_feather = nullptr;
    QSlider* m_opacity = nullptr;
    QLabel* m_sizeValue = nullptr;
    QLabel* m_featherValue = nullptr;
    QLabel* m_opacityValue = nullptr;
    QCheckBox* m_show = nullptr;
    QCheckBox* m_reveal = nullptr;
    QLabel* m_count = nullptr;
    QPushButton* m_removeAll = nullptr;
    QListWidget* m_list = nullptr;
    bool m_refreshing = false;
};

} // namespace pixelfx::ui
