#pragma once

#include <QDialog>

class QLineEdit;

namespace pixelfx::ui {

/// Keyboard shortcuts (mock shortcuts.html): every shortcut by section, searchable; F1 and
/// Ctrl+/ open it.
class ShortcutsSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ShortcutsSheet)

public:
    explicit ShortcutsSheet(QWidget* parent = nullptr);
    ~ShortcutsSheet() override;
    /// Shows only rows whose name contains `text`; how many are shown.
    int filter(const QString& text);

private:
    QLineEdit* m_search = nullptr;
};

} // namespace pixelfx::ui
