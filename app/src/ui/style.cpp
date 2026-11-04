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

const QString kSheet = uR"(
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
        #recentTitle { font-size: 13px; font-weight: 600; }
        QPushButton#link { border: 0; background: transparent; color: {{link}}; font-size: 12px; padding: 2px 4px; }
        QPushButton#link:hover { text-decoration: underline; }
        #adjustPanel { background: {{panel}}; border-left: 1px solid {{border}}; }
        #cropPanel { background: {{panel}}; border-left: 1px solid {{border}}; }
        #section { color: {{muted}}; font-size: 11px; font-weight: 700; }
        QToolButton#swatch { border: 2px solid transparent; border-radius: 12px; }
        QToolButton#swatch:checked { border-color: {{text}}; }
        QPushButton#chip { border: 1px solid {{border}}; border-radius: 12px; padding: 3px 10px; font-size: 12px; color: {{muted}}; background: transparent; }
        QPushButton#chip:hover { background: {{hover}}; color: {{text}}; }
        QPushButton#chip:checked { background: {{accentSoft}}; color: {{accent}}; border-color: transparent; }
        #rail { background: {{bar}}; border-right: 1px solid {{border}}; }
        QToolButton#railButton { border: 0; border-radius: 9px; background: transparent; color: {{muted}}; font-size: 11px; }
        QToolButton#railButton:hover { background: {{hover}}; color: {{text}}; }
        QToolButton#railButton:checked { background: {{accentSoft}}; color: {{accent}}; }
        QListWidget#settingsPages { background: {{bar}}; border: 0; border-right: 1px solid {{border}}; padding-top: 8px; outline: 0; }
        QListWidget#settingsPages::item { padding: 8px 12px; margin: 0 8px; border-radius: 7px; font-weight: 600; }
        QListWidget#settingsPages::item:selected { background: {{accentSoft}}; color: {{accent}}; }
        #presetsPanel { background: {{panel}}; border-right: 1px solid {{border}}; }
        QPushButton#panelTab { padding: 7px 10px 9px; border: 0; border-bottom: 2px solid transparent; border-radius: 0; background: transparent; color: {{muted}}; font-weight: 500; }
        QPushButton#panelTab:checked { border-bottom-color: {{accent}}; color: {{text}}; }
        QPushButton#panelTab:hover { color: {{text}}; }
        #lookStrength { background: {{panel}}; border-top: 1px solid {{border}}; }
        #panelTabLine { background: {{border}}; }
        #presetName { font-weight: 500; }
        #presetSummary { color: {{muted}}; font-size: 11px; }
        QToolButton#stripAdd { border: 1.5px dashed {{faint}}; border-radius: 6px; background: transparent; }
        QToolButton#stripAdd:hover { background: {{hover}}; }
        #accentText { color: {{accent}}; font-size: 12px; }
        #batchPage { background: {{bg}}; }
        #batchRow { background: {{panel}}; border-radius: 7px; }
        #batchState { color: {{muted}}; font-size: 12px; }
        #batchReason { color: {{danger}}; font-size: 12px; }
        #kbd { font-size: 11px; color: {{text}}; background: {{panel}}; border: 1px solid {{border}}; border-bottom-width: 2px; border-radius: 5px; padding: 1px 6px; }
        #zoomBar { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 10px; }
        #zoomDivider { background: {{border}}; }
        #zoomLevel { font-size: 12px; }
        QToolButton#zoomButton { border: 0; border-radius: 6px; padding: 4px; background: transparent; font-size: 12px; }
        QToolButton#zoomButton:hover { background: {{hover}}; }
        #strip { background: {{bar}}; border-top: 1px solid {{border}}; }
        #adjustScroll, #adjustContent { background: {{panel}}; }
        #adjustGroup { border-top: 1px solid {{border}}; }
        QToolButton#groupToggle { border: 0; background: transparent; font-weight: 600; padding: 2px; }
        QToolButton#groupReset { border: 0; border-radius: 6px; padding: 3px; background: transparent; }
        QToolButton#groupReset:hover { background: {{hover}}; }
        #changedDot { background: {{accent}}; border-radius: 3px; }
        #adjustName { color: {{muted}}; font-size: 12px; }
        #adjustName[changed="true"] { color: {{text}}; }
        #adjustValue { color: {{text}}; font-size: 12px; }
        #adjustValue[changed="true"] { color: {{accent}}; }
        QPushButton#small { padding: 4px 10px; font-size: 12px; }
        QSlider { min-height: 24px; }
        QSlider::groove:horizontal { height: 3px; border-radius: 1px; background: {{track}}; }
        QSlider::handle:horizontal { width: 13px; height: 13px; margin: -5px 0; border-radius: 6px; background: {{text}}; }
        QSlider::handle:horizontal:focus { background: {{accent}}; }
        QSlider#temperatureTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4F86E8, stop:0.5 #DDDDDD, stop:1 #F0B040); }
        QSlider#tintTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4CB060, stop:0.5 #DDDDDD, stop:1 #D050C0); }
        QSlider#saturationTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #888888, stop:0.2 #E04F4F, stop:0.4 #E0C04F, stop:0.6 #4FE070, stop:0.8 #4F9CE0, stop:1 #B04FE0); }
        QDialog { background: {{panel}}; }
        #sheetTitle { font-size: 16px; font-weight: 600; }
        #mono { font-family: monospace; font-size: 12px; }
        QPushButton#segment { border-radius: 0; padding: 5px 12px; background: {{input}}; }
        QPushButton#segment:checked { background: {{accentSoft}}; color: {{accent}}; font-weight: 600; }
        QLineEdit, QComboBox, QSpinBox { background: {{input}}; border: 1px solid {{border}}; border-radius: 7px; padding: 5px 8px; }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: {{accent}}; }
        #toast { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 9px; padding: 7px 12px; }
        #watermarkControls { background: {{panel}}; border-top: 1px solid {{border}}; border-right: 1px solid {{border}}; }
        #watermarkPreview { background: {{canvas}}; border-top: 1px solid {{border}}; }
        #watermarkBar { background: {{panel}}; border-top: 1px solid {{border}}; }
        QToolButton#positionCell { border: 1px solid {{faint}}; border-radius: 5px; background: {{input}}; }
        QToolButton#positionCell:hover { background: {{hover}}; }
        QToolButton#positionCell:checked { background: {{accent}}; border-color: {{accent}}; }
        QListWidget#lookSteps { background: {{panel}}; border: 0; outline: 0; }
        QListWidget#lookSteps::item { padding: 6px 8px; border-radius: 7px; margin: 1px 6px; }
        QListWidget#lookSteps::item:selected { background: {{accentSoft}}; color: {{text}}; }
        QListWidget#lookSteps::item:hover:!selected { background: {{hover}}; }
        QMenu { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 10px; padding: 5px; }
        QMenu::item { padding: 6px 28px 6px 10px; border-radius: 6px; }
        QMenu::item:selected { background: {{hover}}; }
        QMenu::separator { height: 1px; background: {{border}}; margin: 5px 4px; }
    )"_s;

QString sheet(const Tokens& t)
{
    QString s = kSheet;
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
    put(u"input"_s, t.input);
    put(u"muted"_s, t.muted);
    put(u"text"_s, t.text);
    put(u"track"_s, t.track);
    put(u"faint"_s, t.faint);
    put(u"link"_s, t.link);
    put(u"danger"_s, t.danger);
    put(u"canvas"_s, t.canvas);
    return s;
}

} // namespace

const Tokens& darkTokens()
{
    static const Tokens t{hex("#6EA8FE"), hex("#2F6FE0"), hex("#3B7CF0"), hex("#1D2A40"), hex("#161616"),
                          hex("#0F0F0F"), hex("#1C1C1C"), hex("#1E1E1E"), hex("#282828"), hex("#2C2C2C"),
                          hex("#141414"), hex("#303030"), hex("#696969"), hex("#ECECEC"), hex("#A8A8A8"),
                          hex("#8E8E8E"), hex("#FF6B6B"), hex("#8AB8FF")};
    return t;
}

const Tokens& lightTokens()
{
    static const Tokens t{hex("#2560CC"), hex("#2560CC"), hex("#1F52B0"), hex("#E3ECFC"), hex("#EDEDED"),
                          hex("#D6D6D6"), hex("#FAFAFA"), hex("#F5F5F5"), hex("#FFFFFF"), hex("#E6E6E6"),
                          hex("#FFFFFF"), hex("#D4D4D4"), hex("#8E8E8E"), hex("#1A1A1A"), hex("#555555"),
                          hex("#676767"), hex("#C01C28"), hex("#2560CC")};
    return t;
}

namespace {
bool g_light = false; // set once at start, before any window exists
} // namespace

void useLightTheme(bool light)
{
    g_light = light;
}

const Tokens& tokens()
{
    return g_light ? lightTokens() : darkTokens();
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
