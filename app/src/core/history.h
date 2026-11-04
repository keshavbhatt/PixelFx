#pragma once

#include <QString>
#include <QStringList>

#include <pfx/settings.h>
#include <vector>

namespace pixelfx::core {

/// Undo and redo for one photo's settings (FEATURES E14, ADR-004): each step is a whole
/// settings snapshot with a label. Settings are small, so snapshots cost little and undo is
/// instant. Pure, no Qt objects.
class History
{
public:
    static constexpr int kMaxSteps = 200;

    /// Starts over from `base` (a newly opened photo).
    void reset(const pfx::Settings& base, const QString& label);
    /// Records `after` as a new step; drops any redo steps. A step equal to the current
    /// settings is ignored. Returns whether a step was added.
    bool push(const QString& label, const pfx::Settings& after);

    [[nodiscard]] bool canUndo() const { return m_index > 0; }
    [[nodiscard]] bool canRedo() const { return m_index + 1 < m_steps.size(); }
    const pfx::Settings& undo();
    const pfx::Settings& redo();
    /// Jumps to step `index` (0 is the oldest), like several undos or redos at once.
    const pfx::Settings& goTo(int index);

    [[nodiscard]] const pfx::Settings& current() const;
    /// Label of the step undo would revert, and of the step redo would apply.
    [[nodiscard]] QString undoLabel() const;
    [[nodiscard]] QString redoLabel() const;
    /// Every step's label, oldest first, and the index of the current one.
    [[nodiscard]] QStringList labels() const;
    [[nodiscard]] int currentIndex() const { return static_cast<int>(m_index); }

private:
    struct Step
    {
        QString label;
        pfx::Settings settings;
    };
    std::vector<Step> m_steps{{QString(), pfx::Settings{}}};
    std::size_t m_index = 0;
};

} // namespace pixelfx::core
