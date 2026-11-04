#pragma once

#include <QDialog>

namespace pixelfx::core {
class Settings;
} // namespace pixelfx::core

namespace pixelfx::ui {

/// Settings (mock settings.html): a sheet with a sidebar where every row writes straight to the
/// settings, no OK button. The General page has what exists today: theme and canvas
/// background, remembering edits and how a photo opens, clipping warnings. The other pages of
/// the mock arrive with their features (Batch, presets and looks, the monitor profile, GPU).
class SettingsSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsSheet)

public:
    explicit SettingsSheet(core::Settings& settings, QWidget* parent = nullptr);
    ~SettingsSheet() override;

private:
    QWidget* buildGeneral();
    core::Settings& m_settings;
};

} // namespace pixelfx::ui
