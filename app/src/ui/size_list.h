#pragma once

#include <QStyledItemDelegate>

class QComboBox;

namespace pixelfx::ui {

/// The size lists of Export and Batch (export-sizes.html): grouped under headings (Keep, Social,
/// Screen, Print at 300 dpi, Mine), each row's name on the left and its pixel size on the right,
/// a tick on the current one.
namespace sizelist {

/// Draws a combo box's list that way; the closed box keeps "name · size".
void install(QComboBox* combo);
void addHeading(QComboBox* combo, const QString& text);
void addItem(QComboBox* combo, const QString& name, const QString& detail, const QVariant& data);
/// A line before the last rows (Custom size...).
void addSeparator(QComboBox* combo);
/// A row's name, without its size.
[[nodiscard]] QString nameOf(const QComboBox* combo, int index);
/// Changes a row's name and size.
void setItem(QComboBox* combo, int index, const QString& name, const QString& detail);

} // namespace sizelist

class SizeListDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SizeListDelegate)

public:
    explicit SizeListDelegate(QComboBox* combo);
    ~SizeListDelegate() override;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    QComboBox* m_combo;
};

} // namespace pixelfx::ui
