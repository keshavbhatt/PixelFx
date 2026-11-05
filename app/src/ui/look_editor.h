#pragma once

#include "core/looks.h"

#include <QDialog>
#include <QImage>

#include <memory>
#include <pfx/image.h>
#include <pfx/settings.h>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QVBoxLayout;
class QWidget;

namespace pixelfx::ui {

/// New look / Edit look: a named stack of steps from the same
/// operations as the sliders, plus LUTs; reorder by dragging, switch off, duplicate, remove;
/// the selected step's parameters; a live preview on the photo or a ramp; the look's default
/// strength. Saves into the user's looks; exports a .pfxlook or bakes a .cube.
class LookEditor : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LookEditor)

public:
    /// `source` is the photo before its settings, `settings` the photo's own settings (the look
    /// goes on top of them, as in the editor); `entry` the look to edit, or empty for a new one.
    LookEditor(const std::shared_ptr<const pfx::Image>& source, const pfx::Settings& settings,
               const core::LookEntry& entry, QString folder, QWidget* parent = nullptr);
    ~LookEditor() override;

    [[nodiscard]] const pfx::Look& look() const { return m_look; }
    /// After Save look: the saved look's id.
    [[nodiscard]] const QString& savedId() const { return m_savedId; }
    /// Adds a step of a registry operation with its default values, or "lut" (tests and Add step).
    void addStep(const QString& op);
    [[nodiscard]] QListWidget* stepList() const { return m_steps; }
    bool saveLook();
    /// The preview as last rendered (tests).
    [[nodiscard]] const QImage& previewImage() const { return m_previewImage; }

Q_SIGNALS:
    void previewRendered();

private:
    QWidget* buildSteps();
    QWidget* buildPreview();
    QWidget* buildPreviewBar(QWidget* side);
    QHBoxLayout* buildStepButtons(QWidget* side);
    void showStepMenu(const QPoint& at);
    void addParamSlider(int row, const QString& name, double low, double high, double value, int decimals,
                        const std::string& key);
    void updateStepRow(int row);
    void buildFooter(QVBoxLayout* column);
    void refreshSteps();
    void selectStep(int row);
    void showParams(int row);
    void stepsReordered();
    void importLut();
    void exportLook();
    void bakeCube();
    void schedulePreview();
    void renderPreview();
    [[nodiscard]] static QString stepName(const QString& op);
    [[nodiscard]] QString stepSummary(const pfx::LookStep& step) const;

    pfx::Look m_look;
    QString m_path;
    QString m_folder;
    QString m_savedId;
    pfx::Settings m_photoSettings;
    std::shared_ptr<const pfx::Image> m_photo;
    std::shared_ptr<const pfx::Image> m_ramp;
    bool m_onRamp = false;
    QImage m_previewImage;
    int m_generation = 0;
    bool m_rendering = false;
    bool m_pending = false;
    QLineEdit* m_name = nullptr;
    QComboBox* m_category = nullptr;
    QListWidget* m_steps = nullptr;
    QLabel* m_paramsTitle = nullptr;
    QWidget* m_params = nullptr;
    QVBoxLayout* m_paramsLayout = nullptr;
    QLabel* m_preview = nullptr;
    QSlider* m_strength = nullptr;
    QLabel* m_strengthValue = nullptr;
    class QTimer* m_previewTimer = nullptr;
};

} // namespace pixelfx::ui
