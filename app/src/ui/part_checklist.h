#pragma once

#include <QFrame>
#include <QStringList>

#include <map>
#include <pfx/settings.h>

class QCheckBox;
class QLabel;

namespace pixelfx::ui {

/// The parts of a photo's settings as check boxes in a card (Save as preset, Paste settings),
/// each with what changed in it: one or two sliders by name ("Clarity, Sharpen"), more as a count
/// ("6 changed"), the look with its strength. The parts that changed start ticked, except White
/// balance and Crop and rotate, which rarely carry from one photo to another.
class PartChecklist : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PartChecklist)

public:
    /// `parts` are core::presetUnits ids in panel order; `columns` is 1 or 2.
    PartChecklist(const pfx::Settings& settings, const QStringList& parts, int columns, QWidget* parent);
    ~PartChecklist() override;

    /// The ticked parts, in panel order.
    [[nodiscard]] QStringList ticked() const;
    /// Ticks exactly `parts`.
    void tick(const QStringList& parts);
    /// The parts that changed and start ticked (Changed only).
    [[nodiscard]] const QStringList& changed() const { return m_changed; }
    [[nodiscard]] const QStringList& parts() const { return m_parts; }

Q_SIGNALS:
    void toggled();

private:
    [[nodiscard]] QLabel* note(const pfx::Settings& settings, const QString& part);

    QStringList m_parts;
    QStringList m_changed;
    std::map<QString, QCheckBox*> m_boxes;
};

} // namespace pixelfx::ui
