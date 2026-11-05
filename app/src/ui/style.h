#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <Qt>

class QApplication;
class QDialog;
class QImage;
class QPixmap;
class QSize;
class QFormLayout;
class QHBoxLayout;
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
    /// The red, green and blue of channels (histogram, curves), the same in both themes.
    QColor channelRed;
    QColor channelGreen;
    QColor channelBlue;
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
/// A sheet's title row: the title (a label named "sheetTitle"), then `extras` (a file name, a
/// name field, a search box), then the close button at the right end.
QHBoxLayout* sheetTitleRow(QDialog* sheet, const QString& title, const QList<QWidget*>& extras = {});
/// The title row on a bar of its own, padded as a sheet's body, for sheets whose body runs edge
/// to edge (Settings, Watermark, the look editor).
QWidget* sheetHeaderBar(QDialog* sheet, QLayout* titleRow);
/// A section heading of a panel or sheet ("SPOTS (3)", "1. RECIPE"): small, bold, muted capitals.
QLabel* sectionLabel(const QString& text, QWidget* parent);
/// A heading inside a menu ("History"): Qt draws a menu section as a bare line under a style
/// sheet, so the heading is a label of its own.
void addMenuHeading(QMenu* menu, const QString& text);
/// Draws the labels of `form` as the mocks' fields: muted, in one column `width` px wide, the gap
/// to the fields included.
void setLabelColumn(const QFormLayout* form, int width);
/// `image` cut to fill `size` (the middle kept) with rounded corners, for a sharp thumbnail on a
/// screen of `devicePixelRatio`.
[[nodiscard]] QPixmap coverThumbnail(const QImage& image, const QSize& size, int radius,
                                     qreal devicePixelRatio);
/// Styles `panel` as a panel at the side of the window: the panel colour and a border on the edge
/// that faces the photo.
void makeSidePanel(QWidget* panel, Qt::Edge photoSide);
/// The footer bar alone, for sheets whose body runs edge to edge (Watermark, the look editor).
QWidget* sheetFootBar(QWidget* sheet, QLayout* foot);

} // namespace pixelfx::ui
