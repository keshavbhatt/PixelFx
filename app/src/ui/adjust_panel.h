#pragma once

#include <QWidget>

#include <pfx/settings.h>
#include <vector>

class QLabel;
class QSlider;

namespace pixelfx::ui {

class AdjustGroup;
class AdjustSlider;

/// The adjustments panel (mock editor.html, right side): groups in a photographer's order,
/// built from the library's operation registry (ADR-005). It edits a copy of the photo's
/// settings and reports every change; it holds no image logic.
class AdjustPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustPanel)

public:
    explicit AdjustPanel(QWidget* parent = nullptr);
    ~AdjustPanel() override;

    /// Shows `settings` without emitting anything (open, undo, redo).
    void setSettings(const pfx::Settings& settings);
    [[nodiscard]] const pfx::Settings& settings() const { return m_settings; }
    [[nodiscard]] AdjustSlider* slider(const QString& key) const;
    /// Clears every setting (Ctrl+R) and reports it as one step.
    void resetAll();

Q_SIGNALS:
    /// Every change, live while a slider moves.
    void settingsChanged(const pfx::Settings& settings);
    /// A change is complete (slider released, a reset): a history step named `label`.
    void editFinished(const QString& label);

private:
    void setValue(const QString& key, double value, bool finished);
    void resetGroup(AdjustGroup* group);
    void refreshGroups();

    pfx::Settings m_settings;
    std::vector<AdjustGroup*> m_groups;
    std::vector<AdjustSlider*> m_sliders;
};

/// One labelled slider for one numeric setting. Values are in the registry's units.
class AdjustSlider : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AdjustSlider)

public:
    AdjustSlider(const QString& key, const QString& label, QWidget* parent = nullptr);
    ~AdjustSlider() override;

    [[nodiscard]] const QString& key() const { return m_key; }
    [[nodiscard]] const QString& label() const { return m_label; }
    [[nodiscard]] double value() const;
    [[nodiscard]] double defaultValue() const { return m_default; }
    [[nodiscard]] bool isChanged() const;
    /// Shows `value` without emitting.
    void setValue(double value);
    /// The value as the panel shows it: "+12", "-0.35".
    [[nodiscard]] QString valueText() const;

Q_SIGNALS:
    void moved(double value);
    void released(double value);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();
    [[nodiscard]] int toTicks(double value) const;
    [[nodiscard]] double fromTicks(int ticks) const;

    QString m_key;
    QString m_label;
    double m_default = 0.0;
    double m_scale = 1.0;
    QSlider* m_slider = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_value = nullptr;
};

} // namespace pixelfx::ui
