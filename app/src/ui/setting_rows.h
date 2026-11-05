#pragma once

#include <QWidget>

#include <string>
#include <vector>

class QCheckBox;
class QComboBox;

namespace pixelfx::ui {

/// A setting with a few named choices, as one panel row: the name at the left, a compact list
/// at the right. The values are the registry's choices; the texts are what the list shows.
class ChoiceRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ChoiceRow)

public:
    ChoiceRow(const QString& name, std::vector<std::string> values, const QStringList& texts,
              QWidget* parent = nullptr);
    ~ChoiceRow() override;

    [[nodiscard]] const std::string& value() const;
    /// Shows `value` (the first choice when it is not one) without emitting.
    void setValue(const std::string& value);
    /// Offers or hides one of the choices (Camera, for RAW files only).
    void setAvailable(const std::string& value, bool available);
    [[nodiscard]] QComboBox* list() const { return m_list; }

Q_SIGNALS:
    void chosen(const std::string& value);

private:
    std::vector<std::string> m_values;
    QComboBox* m_list;
};

/// An on or off setting as a check box, with a line under it saying what it does.
class CheckRow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CheckRow)

public:
    CheckRow(const QString& name, const QString& description, QWidget* parent = nullptr);
    ~CheckRow() override;

    [[nodiscard]] bool isChecked() const;
    /// Shows `on` without emitting.
    void setChecked(bool on);
    [[nodiscard]] QCheckBox* box() const { return m_box; }

Q_SIGNALS:
    void toggled(bool on);

private:
    QCheckBox* m_box;
};

} // namespace pixelfx::ui
