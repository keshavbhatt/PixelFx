#pragma once

#include <QDialog>
#include <QStringList>

#include <map>
#include <pfx/settings.h>

class QCheckBox;

namespace pixelfx::ui {

/// Paste to several photos (apply-to-selected.html: "Paste settings asks which groups"): the
/// parts the copied settings hold, ticked except White balance and Crop and rotate.
class PasteSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PasteSheet)

public:
    PasteSheet(const pfx::Settings& copied, int photos, QWidget* parent = nullptr);
    ~PasteSheet() override;

    [[nodiscard]] QStringList units() const;
    void tick(const QStringList& units);

private:
    std::map<QString, QCheckBox*> m_boxes;
};

} // namespace pixelfx::ui
