#include "core/history.h"

namespace pixelfx::core {

void History::reset(const pfx::Settings& base, const QString& label)
{
    m_steps.assign(1, Step{label, base});
    m_index = 0;
}

bool History::push(const QString& label, const pfx::Settings& after)
{
    if (after == current()) {
        return false;
    }
    m_steps.resize(m_index + 1);
    m_steps.push_back(Step{label, after});
    if (m_steps.size() > static_cast<std::size_t>(kMaxSteps)) {
        m_steps.erase(m_steps.begin()); // the oldest step becomes the new base
    }
    m_index = m_steps.size() - 1;
    return true;
}

const pfx::Settings& History::undo()
{
    if (canUndo()) {
        --m_index;
    }
    return current();
}

const pfx::Settings& History::redo()
{
    if (canRedo()) {
        ++m_index;
    }
    return current();
}

const pfx::Settings& History::goTo(int index)
{
    if (index >= 0 && static_cast<std::size_t>(index) < m_steps.size()) {
        m_index = static_cast<std::size_t>(index);
    }
    return current();
}

const pfx::Settings& History::current() const
{
    return m_steps[m_index].settings;
}

QString History::undoLabel() const
{
    return canUndo() ? m_steps[m_index].label : QString();
}

QString History::redoLabel() const
{
    return canRedo() ? m_steps[m_index + 1].label : QString();
}

QStringList History::labels() const
{
    QStringList out;
    for (const Step& step : m_steps) {
        out << step.label;
    }
    return out;
}

} // namespace pixelfx::core
