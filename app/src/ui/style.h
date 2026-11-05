#pragma once

#include <QColor>
#include <QString>

class QApplication;
class QDialog;
class QFormLayout;
class QLabel;
class QLayout;
class QMenu;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace pixelfx::ui {

/// The design tokens of DOCS/DESIGN.md section 1.
struct Tokens
{
    QColor accent;
    QColor accentStrong;
    QColor accentHover;
    QColor accentSoft;
    QColor bg;
    QColor canvas;
    QColor bar;
    QColor panel;
    QColor elevated;
    QColor hover;
    QColor input;
    QColor border;
    QColor track;
    QColor text;
    QColor muted;
    QColor faint;
    QColor danger;
    QColor link;
    QColor success;
};

[[nodiscard]] const Tokens& darkTokens();
[[nodiscard]] const Tokens& lightTokens();
/// The tokens in use. Dark is the default (DESIGN section 1).
[[nodiscard]] const Tokens& tokens();
/// Chooses the light or dark tokens; call before applyStyle and before any window is built
/// (icons are tinted when they are made, so the theme applies at start).
void useLightTheme(bool light);

/// Fusion with a palette and style sheet built from the tokens.
void applyStyle(QApplication& app);

/// Lays out a sheet: the returned layout is the padded body
/// (title row first), and `foot`, the buttons, sits on a bar with a top border, edge to edge.
QVBoxLayout* sheetLayout(QDialog* sheet, QLayout* foot);
/// The flat close button of a sheet's title row; it rejects the sheet.
QWidget* sheetCloseButton(QDialog* sheet);
/// A section heading of a panel or sheet ("SPOTS (3)", "1. RECIPE"): small, bold, muted capitals.
QLabel* sectionLabel(const QString& text, QWidget* parent);
/// A heading inside a menu ("History"): Qt draws a menu section as a bare line under a style
/// sheet, so the heading is a label of its own.
void addMenuHeading(QMenu* menu, const QString& text);
/// Gives the labels of `form` one column `width` px wide, the gap to the fields included.
void setLabelColumn(const QFormLayout* form, int width);
/// The footer bar alone, for sheets whose body runs edge to edge (Watermark, the look editor).
QWidget* sheetFootBar(QWidget* sheet, QLayout* foot);

} // namespace pixelfx::ui
