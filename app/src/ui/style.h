#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <Qt>

class QApplication;
class QButtonGroup;
class QDialog;
class QFrame;
class QImage;
class QPixmap;
class QSize;
class QFormLayout;
class QHBoxLayout;
class QLabel;
class QComboBox;
class QLineEdit;
class QLayout;
class QScrollArea;
class QMenu;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace pixelfx::ui {

/// The space between a sheet's edge and its content; between a panel's edge and its content
/// (sections, groups, sliders, footers); and around a panel's compact rows of tools (tabs, the
/// search row, filter chips, the tile grid).
inline constexpr int kSheetPadding = 20;
inline constexpr int kPanelPadding = 14;
inline constexpr int kToolRowPadding = 10;
/// Rows of a list (spots, steps, photos) sit this far in from the panel's edges.
inline constexpr int kListInset = 8;
/// Between the rows of a list.
inline constexpr int kListRowGap = 1;

/// The gaps between items, by how closely they belong together (0 is for segments that touch).
inline constexpr int kGapTight = 2; ///< rows of one list
inline constexpr int kGapSmall = 4; ///< a label and its control
inline constexpr int kGapRelated =
    6; ///< controls that work together: chips, swatches, a field and its button
inline constexpr int kGapControls = 8; ///< controls in a row, footer buttons
inline constexpr int kGapItems = 10;   ///< a picture or icon and its text; a form's label and field
inline constexpr int kGapGroups = 12;  ///< groups in a column: a sheet's parts, a form's rows
inline constexpr int kGapColumns = 20; ///< columns side by side

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
    QColor warning;
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
/// (switchTheme changes it while PixelFx runs).
void useLightTheme(bool light);
/// Which token `color` is in the current theme (-1 for none), and that token's colour now: how
/// icons follow a theme switch.
[[nodiscard]] int tokenIndex(const QColor& color);
[[nodiscard]] QColor tokenColor(int index);
/// Switches the theme while PixelFx runs: tokens, palette, style sheet, icons and glyphs.
void switchTheme(QApplication& app, bool light);

/// Fusion with a palette and style sheet built from the tokens.
void applyStyle(QApplication& app);
/// Whether `widget` should show that it has the focus: only when the focus came from the
/// keyboard (Tab, a shortcut), not from a click. Style sheet rules match the same with
/// [focusVisible="true"].
[[nodiscard]] bool showsFocusRing(const QWidget* widget);

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
/// How a segmented control takes its row: as wide as its segments, across the row with the
/// segments keeping their own width, or across the row in equal shares.
enum class SegmentFit
{
    Content,
    Row,
    Equal,
};
/// A segmented control: `names` as joined segments in a box, the one at `checked` on, each added
/// to `group` with its index as id; with `glyphs`, each segment shows its icon before its name.
QFrame* segmentBox(const QStringList& names, int checked, QButtonGroup* group, QWidget* parent,
                   SegmentFit fit = SegmentFit::Content, const QStringList& glyphs = {});
/// Lets `list` be narrower than its longest choice, cut with "..." (a choice is shown whole when
/// the list opens): for lists in a side panel of fixed width, whose longest choices would widen it.
void fitToPanel(QComboBox* list);
/// Fills `line` with a warning: the alert icon and `text`, wrapping. Returns the text's label.
QLabel* warningLine(QWidget* line, const QString& text);
/// The magnifier at the start of a search field, named "Search" for screen readers.
void addSearchIcon(QLineEdit* field);
/// `box` (a segmentBox) at the small size, 25 px tall in 12 px type, for choices inside a section.
QFrame* smallSegments(QFrame* box);
/// `text` for a button or a tool button, whose "&" would otherwise mark a shortcut letter.
[[nodiscard]] QString buttonText(QString text);

/// `image` cropped to fill `size` (logical pixels at `ratio`), with rounded corners: the
/// pictures of list rows.
[[nodiscard]] QPixmap roundedPixmap(const QImage& image, const QSize& size, qreal ratio, qreal radius);
/// A section heading of a panel or sheet ("SPOTS (3)", "1. RECIPE"): small, bold, muted capitals.
QLabel* sectionLabel(const QString& text, QWidget* parent);
/// A section heading on a line of its own: 12 px above it, 4 px below, `sides` px from the edges
/// of its container (kPanelPadding in a panel, 0 inside a padded sheet).
QLabel* sectionHeading(const QString& text, QWidget* parent, int sides = 0);
/// A section heading with a control at its right end ("SPOTS (3)  Remove all", "MY LOOKS  New
/// look"), spaced as sectionHeading.
QHBoxLayout* sectionHeadingRow(QLabel* label, QWidget* trailing, int sides = 0);
/// A section heading with `mark` (the Pro mark) right after its text, spaced as sectionHeading.
QWidget* sectionHeadingMarked(const QString& text, QWidget* mark, QWidget* parent);
/// A numbered section of a side panel laid in `box`: its heading, spaced only by its own
/// margins, and the column returned for the section's controls, kGapControls apart.
QVBoxLayout* sectionColumn(const QString& heading, QWidget* box);
/// While `area` scrolls, its scroll bar takes the right padding of `content` (its widget's
/// layout, `edge` px), so the content keeps its width and nothing moves when the bar appears.
void keepScrollBarInPadding(const QScrollArea* area, QLayout* content, int edge);
/// `area`'s vertical scroll bar floats over its right edge instead of taking width: for side
/// panels, whose 14 px padding holds it, with no track; the content keeps its width.
void overlayScrollBar(QScrollArea* area);
/// A heading inside a menu ("History"): Qt draws a menu section as a bare line under a style
/// sheet, so the heading is a label of its own.
void addMenuHeading(QMenu* menu, const QString& text);
/// Draws the labels of `form` muted, in one column `width` px wide (the gap to the fields
/// included), centred on their fields and greyed out with them.
void setLabelColumn(const QFormLayout* form, int width);
/// `image` cut to fill `size` (the middle kept) with rounded corners, for a sharp thumbnail on a
/// screen of `devicePixelRatio`.
[[nodiscard]] QPixmap coverThumbnail(const QImage& image, const QSize& size, int radius,
                                     qreal devicePixelRatio);
/// Styles `panel` as a panel at the side of the window: the panel colour and a border on the edge
/// that faces the photo.
void makeSidePanel(QWidget* panel, Qt::Edge photoSide);
/// The buttons of a footer: `leading` at the left (Copy to clipboard, a note, a check box), then
/// Cancel when asked for, then the primary button, the default one. Both are returned so their
/// actions can be connected (Cancel rejects a sheet, or leaves a mode).
struct FootButtons
{
    QHBoxLayout* row;
    QPushButton* cancel; ///< nullptr without Cancel
    QPushButton* primary;
};
FootButtons footButtons(QWidget* parent, const QString& primary, const QList<QWidget*>& leading = {},
                        bool withCancel = false);
/// A footer on a bar with a top border, edge to edge: under a sheet whose body runs edge to edge
/// (Watermark, the look editor) or a side panel (Heal, Crop, Batch).
QWidget* footBar(QWidget* parent, QLayout* foot, int padding = kSheetPadding, int vertical = 12);
/// A side panel's foot: 10 px above and below its buttons (sheets have 12), the panel's padding at
/// the sides.
QWidget* panelFootBar(QWidget* parent, QLayout* foot);

} // namespace pixelfx::ui
