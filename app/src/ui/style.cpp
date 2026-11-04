#include "ui/style.h"

#include <QApplication>
#include <QPalette>
#include <QStyleFactory>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

QColor hex(const char* value)
{
    return {QString::fromLatin1(value)};
}

QString sheet(const Tokens& t)
{
    QString s = uR"(
        #header { background: {{bar}}; border-bottom: 1px solid {{border}}; }
        #docName { font-weight: 600; }
        #docMeta { color: {{muted}}; font-size: 11px; }
        QToolButton#headerButton { border: 0; border-radius: 7px; padding: 6px; background: transparent; }
        QToolButton#headerButton:hover { background: {{hover}}; }
        QToolButton#headerButton::menu-indicator { image: none; width: 0; }
        QPushButton { border: 1px solid {{border}}; border-radius: 7px; padding: 7px 14px; background: {{panel}}; }
        QPushButton:hover { background: {{hover}}; }
        QPushButton#primary { background: {{accentStrong}}; border-color: {{accentStrong}}; color: white; }
        QPushButton#primary:hover { background: {{accentHover}}; }
        QPushButton:focus { border-color: {{accent}}; }
        #welcome { background: {{bg}}; }
        #dropZone { background: {{panel}}; border: 2px dashed {{border}}; border-radius: 10px; }
        #dropZone[dragActive="true"] { border-color: {{accent}}; background: {{accentSoft}}; }
        #welcomeTitle { font-size: 20px; font-weight: 600; }
        #welcomeHint, #muted { color: {{muted}}; }
        #faint { color: {{faint}}; font-size: 12px; }
        QMenu { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 10px; padding: 5px; }
        QMenu::item { padding: 6px 28px 6px 10px; border-radius: 6px; }
        QMenu::item:selected { background: {{hover}}; }
        QMenu::separator { height: 1px; background: {{border}}; margin: 5px 4px; }
    )"_s;
    const auto put = [&s](const QString& name, const QColor& c) {
        s.replace(u"{{"_s + name + u"}}"_s, c.name());
    };
    put(u"accent"_s, t.accent);
    put(u"accentStrong"_s, t.accentStrong);
    put(u"accentHover"_s, t.accentHover);
    put(u"accentSoft"_s, t.accentSoft);
    put(u"bg"_s, t.bg);
    put(u"bar"_s, t.bar);
    put(u"panel"_s, t.panel);
    put(u"elevated"_s, t.elevated);
    put(u"hover"_s, t.hover);
    put(u"border"_s, t.border);
    put(u"muted"_s, t.muted);
    put(u"faint"_s, t.faint);
    return s;
}

} // namespace

const Tokens& darkTokens()
{
    static const Tokens t{hex("#6EA8FE"), hex("#2F6FE0"), hex("#3B7CF0"), hex("#1D2A40"), hex("#161616"),
                          hex("#0F0F0F"), hex("#1C1C1C"), hex("#1E1E1E"), hex("#282828"), hex("#2C2C2C"),
                          hex("#141414"), hex("#303030"), hex("#696969"), hex("#ECECEC"), hex("#A8A8A8"),
                          hex("#8E8E8E"), hex("#FF6B6B")};
    return t;
}

const Tokens& lightTokens()
{
    static const Tokens t{hex("#2560CC"), hex("#2560CC"), hex("#1F52B0"), hex("#E3ECFC"), hex("#EDEDED"),
                          hex("#D6D6D6"), hex("#FAFAFA"), hex("#F5F5F5"), hex("#FFFFFF"), hex("#E6E6E6"),
                          hex("#FFFFFF"), hex("#D4D4D4"), hex("#8E8E8E"), hex("#1A1A1A"), hex("#555555"),
                          hex("#676767"), hex("#C01C28")};
    return t;
}

const Tokens& tokens()
{
    return darkTokens();
}

void applyStyle(QApplication& app)
{
    const Tokens& t = tokens();
    QApplication::setStyle(QStyleFactory::create(u"Fusion"_s));
    QPalette p;
    p.setColor(QPalette::Window, t.bg);
    p.setColor(QPalette::WindowText, t.text);
    p.setColor(QPalette::Base, t.input);
    p.setColor(QPalette::AlternateBase, t.panel);
    p.setColor(QPalette::Text, t.text);
    p.setColor(QPalette::Button, t.panel);
    p.setColor(QPalette::ButtonText, t.text);
    p.setColor(QPalette::Highlight, t.accentStrong);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, t.elevated);
    p.setColor(QPalette::ToolTipText, t.text);
    p.setColor(QPalette::PlaceholderText, t.faint);
    p.setColor(QPalette::Link, t.accent);
    p.setColor(QPalette::Disabled, QPalette::Text, t.faint);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.faint);
    p.setColor(QPalette::Disabled, QPalette::WindowText, t.faint);
    QApplication::setPalette(p);
    app.setStyleSheet(sheet(t));
}

} // namespace pixelfx::ui
