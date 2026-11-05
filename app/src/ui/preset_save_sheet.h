#pragma once

#include "ui/sheet.h"

#include <QStringList>

#include <map>
#include <pfx/settings.h>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace pixelfx::ui {

/// Save as preset: a name, a group, and which parts of the settings the
/// preset holds. Changed parts are ticked, except White balance and Crop and rotate, which
/// rarely carry from one photo to another. Save to file also writes a copy to share.
class PresetSaveSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresetSaveSheet)

public:
    PresetSaveSheet(const pfx::Settings& settings, const QStringList& groups, QWidget* parent = nullptr);
    ~PresetSaveSheet() override;

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString group() const;
    /// The ticked units (core::presetUnits ids).
    [[nodiscard]] QStringList units() const;
    /// Where Save to file asked to write a copy; empty when it was not used.
    [[nodiscard]] const QString& fileCopy() const { return m_fileCopy; }

    void setName(const QString& name);
    /// The photo as edited, shown beside the name.
    void setPhoto(const QImage& photo);
    /// Ticks exactly `units` (tests, and the Changed only / All / None links).
    void tick(const QStringList& units);

private:
    /// The thumbnail beside the name and group.
    QHBoxLayout* buildTop(const QStringList& groups);
    void buildInclude(QVBoxLayout* column, const pfx::Settings& settings);
    /// A check box per part, in two columns, with what changed in each.
    QWidget* buildParts(const pfx::Settings& settings);
    [[nodiscard]] QLabel* unitNote(const QString& unit, const pfx::Settings& settings, int changed);
    [[nodiscard]] static QString changedText(const pfx::Settings& settings, const QString& unit, int changed);
    QHBoxLayout* buildButtons();
    void refreshSave();
    QLineEdit* m_name = nullptr;
    QComboBox* m_group = nullptr;
    QPushButton* m_save = nullptr;
    QLabel* m_thumbnail = nullptr;
    std::map<QString, QCheckBox*> m_boxes;
    QStringList m_changed;
    QString m_fileCopy;
};

} // namespace pixelfx::ui
