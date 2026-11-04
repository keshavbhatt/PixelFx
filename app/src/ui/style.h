#pragma once

#include <QColor>
#include <QString>

class QApplication;

namespace pixelfx::ui {

/// The design tokens of DOCS/DESIGN.md section 1 (the mocks' mock.css holds the same values).
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
};

[[nodiscard]] const Tokens& darkTokens();
[[nodiscard]] const Tokens& lightTokens();
/// The tokens in use. Dark is the default (DESIGN section 1); the theme setting arrives in M3.
[[nodiscard]] const Tokens& tokens();

/// Fusion with a palette and style sheet built from the tokens.
void applyStyle(QApplication& app);

} // namespace pixelfx::ui
