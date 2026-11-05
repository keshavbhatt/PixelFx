#pragma once

#include "ui/sheet.h"

#include <QStringList>

#include <pfx/settings.h>

class QLabel;

namespace pixelfx::ui {

class PartChecklist;

/// Paste to several photos (apply-to-selected.html: "Paste settings asks which groups"): the
/// parts the copied settings hold, with what changed in each, ticked except White balance and
/// Crop and rotate.
class PasteSheet : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PasteSheet)

public:
    PasteSheet(const pfx::Settings& copied, int photos, QWidget* parent = nullptr);
    ~PasteSheet() override;

    /// Where the settings were copied from ("sunset-lake.jpg"); nothing is shown when empty.
    void setSource(const QString& source);
    [[nodiscard]] QStringList units() const;
    void tick(const QStringList& units);

private:
    PartChecklist* m_parts = nullptr;
    QLabel* m_source = nullptr;
};

} // namespace pixelfx::ui
