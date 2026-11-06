#include "ui/style.h"

#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/menu_style.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QFile>
#include <QFocusEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QPointer>
#include <QProxyStyle>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <memory>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// Thin enough to lie over a panel's 10 to 14 px padding without covering a control.
constexpr int kScrollBarWidth = 8;

QColor hex(const char* value)
{
    return {QString::fromLatin1(value)};
}

// In two parts: MSVC refuses a string literal over 16 KB (error C2026).
const QString kSheet = uR"(
        #header { background: {{bar}}; border-bottom: 1px solid {{border}}; }
        #docName { font-weight: 600; }
        #docMeta { color: {{muted}}; font-size: 11px; }
        QToolButton#headerButton { border: 0; border-radius: 7px; padding: 6px; background: transparent; }
        QToolButton#headerButton:hover { background: {{hover}}; }
        QToolButton#headerButton:checked { background: {{accentSoft}}; }
        QToolButton#headerButton::menu-indicator { image: none; width: 0; }
        QPushButton { border: 1px solid {{border}}; border-radius: 7px; padding: 7px 11px; background: {{panel}}; }
        QPushButton:hover { background: {{hover}}; }
        QPushButton#primary { background: {{accentStrong}}; border-color: {{accentStrong}}; color: white; }
        QPushButton#primary:hover { background: {{accentHover}}; }
        QPushButton[focusVisible="true"]:focus { border-color: {{accent}}; }
        #welcome { background: {{bg}}; }
        #dropZone { background: {{panel}}; border: 2px dashed {{border}}; border-radius: 10px; }
        #dropZone[dragActive="true"] { border-color: {{accent}}; background: {{accentSoft}}; }
        #welcomeTitle { font-size: 20px; font-weight: 600; }
        #batchEmptyTitle { font-size: 18px; font-weight: 600; }
        #welcomeHint, #muted { color: {{muted}}; }
        #faint { color: {{faint}}; font-size: 12px; }
        #faintSmall { color: {{faint}}; font-size: 11px; }
        #monoMuted { font-family: monospace; font-size: 12px; color: {{muted}}; }
        #warningText { color: {{warning}}; font-size: 12px; }
        #messageText { color: {{faint}}; font-size: 12px; }
        #messageText[kind="info"] { color: {{text}}; }
        #messageText[kind="error"] { color: {{danger}}; }
        QLabel#small { font-size: 12px; }
        #recentTitle { font-size: 13px; font-weight: 600; }
        QPushButton#link { border: 1px solid transparent; border-radius: 4px; background: transparent; color: {{link}}; font-size: 12px; padding: 3px; }
        QPushButton#link:hover { text-decoration: underline; }
        QPushButton#link[focusVisible="true"]:focus { border-color: {{accent}}; }
        #sidePanel { background: {{panel}}; }
        #sidePanel[photoSide="left"] { border-left: 1px solid {{border}}; }
        #sidePanel[photoSide="right"] { border-right: 1px solid {{border}}; }
        #section { color: {{muted}}; font-size: 11px; font-weight: 700; }
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
        QListWidget#pickerGroups { background: {{bar}}; border: 0; border-right: 1px solid {{border}}; padding: 6px 0; outline: 0; }
        QListWidget#pickerGroups::item { padding: 0 12px; margin: 0 8px; border-radius: 7px; }
        QListWidget#pickerGroups::item:selected { background: {{accentSoft}}; color: {{accent}}; font-weight: 600; }
        QPushButton#panelTab { padding: 7px 10px 10px; border: 0; border-bottom: 2px solid transparent; border-radius: 0; background: transparent; color: {{muted}}; font-weight: 500; }
        QPushButton#panelTab:checked { border-bottom-color: {{accent}}; color: {{text}}; }
        QPushButton#panelTab:hover { color: {{text}}; }
        #lookStrength { background: {{panel}}; border-top: 1px solid {{border}}; }
        #panelTabLine { background: {{border}}; }
        #itemTitle { font-weight: 500; }
        #itemDetail { color: {{muted}}; font-size: 11px; }
        QWidget#listRow QLabel#itemTitle { min-height: 19px; }
        QWidget#listRow QLabel#itemDetail { min-height: 16px; }
        QLabel#itemDetail[error="true"] { color: {{danger}}; }
        QLabel#recentTitle[error="true"] { color: {{danger}}; }
        QToolButton#stripAdd { border: 1.5px dashed {{faint}}; border-radius: 6px; background: transparent; }
        QToolButton#stripAdd:hover { background: {{hover}}; }
        #accentText { color: {{accent}}; font-size: 12px; }
        #kbd { font-size: 11px; color: {{text}}; background: {{panel}}; border: 1px solid {{border}}; border-bottom-width: 2px; border-radius: 5px; padding: 1px 6px; min-height: 16px; }
        #zoomBar, #zoomChip { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 10px; }
        #zoomBusy { color: {{muted}}; font-size: 12px; }
        #zoomDivider { background: {{border}}; }
        #zoomLevel { font-size: 12px; }
        QToolButton#zoomButton { border: 0; border-radius: 6px; padding: 4px; background: transparent; font-size: 12px; }
        QToolButton#zoomButton:hover { background: {{hover}}; }
        #strip { background: {{bar}}; border-top: 1px solid {{border}}; }
    )"_s + uR"(
        #panelScroll, #panelContent { background: {{panel}}; }
        #adjustGroup { border-top: 1px solid {{border}}; }
        QToolButton#groupToggle { border: 0; background: transparent; font-weight: 600; padding: 0; }
        QToolButton#flatIcon { border: 1px solid transparent; border-radius: 6px; padding: 3px; background: transparent; }
        QToolButton#flatIcon:hover { background: {{hover}}; }
        QToolButton#flatIcon[focusVisible="true"]:focus { border-color: {{accent}}; }
        #viewerStage { background: {{canvas}}; }
        QToolButton#navButton { border: 1px solid {{border}}; border-radius: 20px; background: {{elevated}}; }
        QToolButton#navButton:hover { background: {{hover}}; }
        QToolButton#navButton:disabled { background: {{panel}}; }
        #changedDot { background: {{accent}}; border-radius: 3px; }
        #adjustName { color: {{muted}}; font-size: 12px; }
        #adjustName[changed="true"] { color: {{text}}; }
        #adjustValue { color: {{text}}; font-size: 12px; border: 1px solid transparent; border-radius: 4px; }
        #adjustValue:hover { border-color: {{border}}; padding: 0 4px; }
        #adjustValue[changed="true"] { color: {{accent}}; }
        QLineEdit#adjustValueField { font-size: 12px; padding: 0 4px; min-height: 0; border-radius: 4px; border-color: {{accent}}; }
        QPushButton#small { padding: 4px 9px; font-size: 12px; }
        QPushButton#flatSmall { border: 0; background: transparent; padding: 4px 6px; font-size: 12px; }
        QPushButton#flatSmall:hover { background: {{hover}}; }
        QPushButton#categoryHeading { border: 0; background: transparent; padding: 2px 0; color: {{muted}};
            font-size: 11px; font-weight: 700; text-align: left; }
        QPushButton#categoryHeading:hover { color: {{text}}; }
        QSlider { min-height: 24px; }
        QSlider::groove:horizontal { height: 3px; border-radius: 1px; background: {{track}}; }
        QSlider::handle:horizontal { width: 13px; height: 13px; margin: -5px 0; border-radius: 6px; background: {{text}}; }
        QSlider[focusVisible="true"]::handle:horizontal:focus { background: {{accent}}; }
        QSlider#temperatureTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4F86E8, stop:0.5 #DDDDDD, stop:1 #F0B040); }
        QSlider#tintTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4CB060, stop:0.5 #DDDDDD, stop:1 #D050C0); }
        QSlider#saturationTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #888888, stop:0.2 #E04F4F, stop:0.4 #E0C04F, stop:0.6 #4FE070, stop:0.8 #4F9CE0, stop:1 #B04FE0); }
        QSlider#bwTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1E1E1E, stop:0.5 #8A8A8A, stop:1 #F0F0F0); }
        QSlider#redHueTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #D04FA0, stop:0.5 #E04F4F, stop:1 #E8904A); }
        QSlider#greenHueTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #C8C04F, stop:0.5 #4FC060, stop:1 #4FC0B0); }
        QSlider#blueHueTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4FB0C8, stop:0.5 #4F70E0, stop:1 #8A4FE0); }
        QSlider#redSaturationTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #8A8A8A, stop:1 #E04F4F); }
        QSlider#greenSaturationTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #8A8A8A, stop:1 #4FC060); }
        QSlider#blueSaturationTrack::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #8A8A8A, stop:1 #4F70E0); }
        QSlider#temperatureTrack::groove:horizontal, QSlider#tintTrack::groove:horizontal,
        QSlider#saturationTrack::groove:horizontal, QSlider#bwTrack::groove:horizontal,
        QSlider#redHueTrack::groove:horizontal, QSlider#greenHueTrack::groove:horizontal,
        QSlider#blueHueTrack::groove:horizontal, QSlider#redSaturationTrack::groove:horizontal,
        QSlider#greenSaturationTrack::groove:horizontal, QSlider#blueSaturationTrack::groove:horizontal {
            border: 1px solid {{faint}}; height: 3px; }
        QFrame#hairline { background: {{border}}; border: 0; }
        #subcaption { color: {{muted}}; font-size: 11px; font-weight: 600; padding-top: 12px; }
        #subcaption[underRow="true"] { padding-top: 4px; }
        QToolButton#disclose { border: 0; background: transparent; color: {{muted}}; font-size: 12px; margin-top: 4px; padding: 4px 0 1px; }
        QToolButton#disclose:hover { color: {{text}}; }
        #checkNote { color: {{muted}}; font-size: 12px; margin-left: 24px; }
        QDialog { background: {{panel}}; }
        #sheetTitle { font-size: 16px; font-weight: 600; }
        #mono { font-family: monospace; font-size: 12px; }
        QPushButton#segment { border-radius: 0; padding: 5px 12px; background: {{input}}; }
        QPushButton#segment:checked { background: {{accentSoft}}; color: {{accent}}; font-weight: 600; }
        QPushButton#segment[channel="red"] { color: {{channelRed}}; }
        QPushButton#segment[channel="green"] { color: {{channelGreen}}; }
        QPushButton#segment[channel="blue"] { color: {{channelBlue}}; }
        QPushButton::menu-indicator { image: none; width: 0; }
        QLineEdit, QComboBox, QSpinBox { background: {{input}}; border: 1px solid {{border}}; border-radius: 7px; padding: 5px 8px; }
        QLineEdit:focus, QSpinBox:focus, QComboBox[focusVisible="true"]:focus { border-color: {{accent}}; }
        #toast { background: {{elevated}}; border: 1px solid {{border}}; border-radius: 9px; padding: 7px 12px; }
        #sheetSide { background: {{panel}}; border-top: 1px solid {{border}}; border-right: 1px solid {{border}}; }
        #previewArea { background: {{canvas}}; border-top: 1px solid {{border}}; }
        #previewBar { background: {{panel}}; border-top: 1px solid {{border}}; }
        QToolButton#positionCell { border: 1px solid {{track}}; border-radius: 5px; background: {{input}}; padding: 0; }
        QToolButton#positionCell:hover { background: {{hover}}; }
        QToolButton#positionCell:checked { background: {{accentStrong}}; border-color: {{accentStrong}}; }
        QFrame#positionBox { border: 1px solid {{border}}; border-radius: 8px; background: {{input}}; }
        QListWidget#lookSteps { background: {{panel}}; border: 0; outline: 0; }
        QListWidget#lookSteps::item { padding: 6px 8px; border-radius: 7px; margin: 1px 6px; }
        QListWidget#lookSteps::item:selected { background: {{accentSoft}}; color: {{text}}; }
        QListWidget#lookSteps::item:hover:!selected { background: {{hover}}; }
        #wheelTitle { color: {{muted}}; font-size: 12px; }
        #wheelValues { color: {{text}}; font-size: 12px; }
        #aboutName { font-size: 18px; font-weight: 700; }
        #credit { color: {{muted}}; font-size: 12px; }
        QFrame#divider { background: {{border}}; border: 0; }
        QTextBrowser#whatsNewText { background: {{panel}}; border: 0; }
        QListWidget#noticeList { background: {{input}}; border: 1px solid {{border}}; border-radius: 8px; padding: 4px; }
        QListWidget#noticeList::item { padding: 5px 8px; margin: 1px 0; border-radius: 6px; }
        QListWidget#noticeList::item:hover:!selected { background: {{hover}}; }
        QListWidget#noticeList::item:selected { background: {{accentSoft}}; color: {{text}}; }
        QPlainTextEdit#noticeText, QPlainTextEdit#diagnostics { background: {{input}}; border: 1px solid {{border}}; border-radius: 8px; padding: 6px; font-family: monospace; font-size: 12px; }
    )"_s;

// The standard controls. {{glyphs}} is a folder of small SVGs made in the theme's colours at
// start, because style sheets take images only from files.
const QString kControls = uR"(
        QCheckBox, QRadioButton { spacing: 8px; }
        QCheckBox::indicator { width: 13px; height: 13px; border-radius: 4px; border: 1.5px solid {{faint}}; background: transparent; }
        QCheckBox::indicator:hover { border-color: {{muted}}; }
        QCheckBox::indicator:checked { background: {{accentStrong}}; border-color: {{accentStrong}}; image: url({{glyphs}}/check.svg); }
        QCheckBox::indicator:indeterminate { background: {{accentStrong}}; border-color: {{accentStrong}}; image: url({{glyphs}}/part.svg); }
        QCheckBox::indicator:disabled { border-color: {{border}}; }
        QCheckBox::indicator:checked:disabled { background: {{accentSoft}}; border-color: {{accentSoft}}; }
        QRadioButton::indicator { width: 14px; height: 14px; border-radius: 8px; border: 1.5px solid {{faint}}; background: transparent; }
        QRadioButton::indicator:checked { border: 5px solid {{accentStrong}}; background: white; width: 6px; height: 6px; }
        QCheckBox:disabled, QRadioButton:disabled, QLabel:disabled { color: {{faint}}; }
        QPushButton:disabled { color: {{faint}}; background: {{panel}}; border-color: {{border}}; }
        QPushButton#primary:disabled { background: {{accentSoft}}; border-color: {{accentSoft}}; color: {{muted}}; }
        QToolButton:disabled { color: {{faint}}; }
        QComboBox { padding: 5px 28px 5px 10px; min-height: 20px; color: {{text}}; }
        QComboBox#small { padding: 3px 26px 3px 9px; min-height: 18px; font-size: 12px; }
        QComboBox:hover { border-color: {{faint}}; }
        QComboBox:disabled, QLineEdit:disabled, QSpinBox:disabled { color: {{faint}}; background: {{panel}}; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 24px; border: 0; }
        QComboBox::down-arrow { image: url({{glyphs}}/chevron-down.svg); width: 14px; height: 14px; }
        QComboBox QAbstractItemView { background: {{elevated}}; color: {{text}}; border: 1px solid {{border}}; border-radius: 8px; padding: 4px; outline: 0; selection-background-color: {{hover}}; selection-color: {{text}}; }
        QComboBox QAbstractItemView::item { min-height: 26px; padding: 0 8px; border-radius: 6px; }
        QComboBox QAbstractItemView::item:selected { background: {{hover}}; color: {{text}}; }
        QSpinBox { padding-right: 24px; min-height: 20px; }
        QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 22px; border: 0; background: transparent; }
        QSpinBox::up-button { subcontrol-position: top right; }
        QSpinBox::down-button { subcontrol-position: bottom right; }
        QSpinBox::up-arrow { image: url({{glyphs}}/chevron-up.svg); width: 11px; height: 11px; }
        QSpinBox::down-arrow { image: url({{glyphs}}/chevron-down.svg); width: 11px; height: 11px; }
        QLineEdit { selection-background-color: {{accentStrong}}; min-height: 20px; }
        QLineEdit[search="true"] { min-height: 18px; }
        QListWidget, QTextBrowser, QPlainTextEdit, QTextEdit { background: {{input}}; border: 1px solid {{border}}; border-radius: 8px; color: {{text}}; outline: 0; }
        QListWidget::item:selected { background: {{accentSoft}}; color: {{text}}; }
        QListWidget::item:hover:!selected { background: {{hover}}; }
        QScrollArea { border: 0; background: transparent; }
        QToolTip { background: {{elevated}}; color: {{text}}; border: 1px solid {{border}}; border-radius: 6px; padding: 4px 8px; }
        QLabel#menuHeading { color: {{muted}}; font-size: 11px; font-weight: 700; padding: 6px 15px 4px; }
        QProgressBar { background: {{track}}; border: 0; border-radius: 2px; max-height: 4px; text-align: center; color: transparent; }
        QProgressBar::chunk { background: {{accentStrong}}; border-radius: 2px; }
        QScrollBar:vertical { width: {{scrollbar}}px; background: transparent; margin: 2px 2px 2px 0; }
        QScrollBar:horizontal { height: {{scrollbar}}px; background: transparent; margin: 0 2px 2px 2px; }
        QScrollBar::handle { background: {{border}}; border-radius: 3px; }
        QScrollBar::handle:hover, QScrollBar::handle:pressed { background: {{track}}; }
        QScrollBar::handle:vertical { min-height: 32px; }
        QScrollBar::handle:horizontal { min-width: 32px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: 0; background: none; }
        QScrollBar::add-page, QScrollBar::sub-page { background: none; }
        QWidget#listRow { border-radius: 7px; }
        QWidget#listRow:hover { background: {{hover}}; }
        QWidget#listRow[selected="true"] { background: {{accentSoft}}; }
        QWidget#listRow[selected="true"] QLabel#itemTitle { color: {{accent}}; }
        #footBar { background: {{panel}}; border-top: 1px solid {{border}}; }
        pixelfx--ui--Sheet { background: transparent; }
        pixelfx--ui--Sheet #footBar { border-bottom-left-radius: 13px; border-bottom-right-radius: 13px; }
        QListWidget#settingsPages { border-bottom-left-radius: 13px; }
        QCheckBox#switch::indicator { width: 34px; height: 20px; border: 0; border-radius: 10px; background: transparent; image: url({{glyphs}}/switch-off.svg); }
        QCheckBox#switch::indicator:checked { image: url({{glyphs}}/switch-on.svg); }
        QCheckBox#switch::indicator:disabled { image: url({{glyphs}}/switch-off.svg); }
        QComboBox#compact { padding: 2px 24px 2px 8px; min-height: 18px; font-size: 12px; }
        QToolButton#iconButton { border: 1px solid {{border}}; border-radius: 7px; background: {{panel}}; padding: 0; min-width: 24px; max-width: 24px; min-height: 24px; max-height: 24px; }
        QToolButton#iconButton:hover { background: {{hover}}; }
        QToolButton#iconButton:checked { background: {{accentSoft}}; border-color: transparent; }
        QToolButton#sheetClose { border: 0; border-radius: 7px; padding: 6px; background: transparent; }
        QToolButton#sheetClose:hover { background: {{hover}}; }
        QToolButton#tileClose { border: 0; margin: 2px; border-radius: 10px; padding: 0; background: rgba(0, 0, 0, 153); }
        QToolButton#tileClose:hover { background: rgba(0, 0, 0, 204); }
        #headerSep { background: {{border}}; }
        #wordmark { font-weight: 700; font-size: 14px; }
        #batchPage, #batchList, #batchRows, #batchPanelContent { background: {{panel}}; }
        QListWidget#batchGrid { background: {{panel}}; border: 0; }
        #batchHead { border-bottom: 1px solid {{border}}; }
        QLabel#cardName { font-weight: 600; }
        QWidget#batchPanelContent QCheckBox, QWidget#batchPanelContent QLabel#itemTitle { font-size: 12px; }
        #batchBar { background: {{panel}}; border-top: 1px solid {{border}}; }
        #batchStatus { background: {{bar}}; border-top: 1px solid {{border}}; }
        QLabel#comparePicture { background: {{canvas}}; border-radius: 6px; }
        QLabel#batchThumb { background: {{input}}; border-radius: 4px; }
        QFrame#segmentBox { border: 1px solid {{border}}; border-radius: 7px; background: {{input}}; }
        QFrame#segmentBox QPushButton#segment { border: 1px solid transparent; border-right: 1px solid {{border}}; padding: 3px 12px 4px 11px; background: transparent; }
        QFrame#segmentBox QPushButton#segment[edge="last"] { border-right-color: transparent; }
        QFrame#segmentBox QPushButton#segment::menu-indicator { image: none; width: 0; }
        QFrame#segmentBox QPushButton#segment[focusVisible="true"]:focus { border: 1px solid {{accent}}; }
        QFrame#segmentBox[compact="true"] QPushButton#segment { padding: 2px 10px 2px 9px; font-size: 12px; }
        QFrame#segmentBox QPushButton#segment:checked { background: {{accentSoft}}; }
        QFrame#segmentBox QPushButton#segment[edge="first"] { border-top-left-radius: 6px; border-bottom-left-radius: 6px; }
        QLabel#fieldLabel { color: {{muted}}; }
        QLabel#fieldLabel:disabled { color: {{faint}}; }
        QToolButton#segmentIcon { border: 1px solid transparent; border-radius: 6px; padding: 3px 8px; background: transparent; }
        QToolButton#segmentIcon:checked { background: {{accentSoft}}; }
        QToolButton#segmentIcon[focusVisible="true"]:focus { border-color: {{accent}}; }
        QFrame#card { border: 1px solid {{border}}; border-radius: 10px; background: {{input}}; }
        QLabel#heading { font-weight: 600; }
        QFrame#pill { border-radius: 10px; background: {{hover}}; }
        QFrame#pill[kind="accent"] { background: {{accentSoft}}; }
        QLabel#pillText { font-size: 11px; color: {{muted}}; }
        QFrame#pill[kind="ok"] QLabel#pillText { color: {{success}}; }
        QFrame#pill[kind="bad"] QLabel#pillText { color: {{danger}}; }
        QFrame#pill[kind="accent"] QLabel#pillText { color: {{accent}}; }
        QLineEdit#monoField { font-family: monospace; font-size: 12px; }
    )"_s;

// The glyphs the control rules use, written once per start in the theme's colours.
QString glyphFolder(const Tokens& t)
{
    static std::unique_ptr<QTemporaryDir> folder;
    if (!folder) {
        folder = std::make_unique<QTemporaryDir>();
    }
    const auto write = [](const QString& path, const QString& body, const QColor& colour, double width) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(
                QString(u"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\" fill=\"none\" "
                        "stroke=\"%1\" stroke-width=\"%2\" stroke-linecap=\"round\" "
                        "stroke-linejoin=\"round\">%3</svg>"_s)
                    .arg(colour.name())
                    .arg(width)
                    .arg(body)
                    .toUtf8());
        }
    };
    const QString dir = folder->path();
    write(dir + u"/check.svg"_s, u"<path d=\"M3.5 8.5l3 3 6-7\"/>"_s, Qt::white, 2.2);
    write(dir + u"/part.svg"_s, u"<path d=\"M4 8h8\"/>"_s, Qt::white, 2.2);
    write(dir + u"/chevron-down.svg"_s, u"<path d=\"M4 6l4 4 4-4\"/>"_s, t.muted, 1.8);
    // Switches.
    const auto knob = [&dir](const QString& name, const QColor& track, int x) {
        QFile file(dir + u'/' + name);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(QString(u"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 34 20\">"
                               "<rect width=\"34\" height=\"20\" rx=\"10\" fill=\"%1\"/>"
                               "<circle cx=\"%2\" cy=\"10\" r=\"8\" fill=\"#ffffff\"/></svg>"_s)
                           .arg(track.name())
                           .arg(x)
                           .toUtf8());
        }
    };
    knob(u"switch-off.svg"_s, t.track, 10);
    knob(u"switch-on.svg"_s, t.accentStrong, 24);
    write(dir + u"/chevron-up.svg"_s, u"<path d=\"M4 10l4-4 4 4\"/>"_s, t.muted, 1.8);
    return dir;
}

QString sheet(const Tokens& t)
{
    QString s = kSheet + kControls;
    s.replace(u"{{glyphs}}"_s, glyphFolder(t));
    s.replace(u"{{scrollbar}}"_s, QString::number(kScrollBarWidth));
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
    put(u"success"_s, t.success);
    put(u"warning"_s, t.warning);
    put(u"channelRed"_s, t.channelRed);
    put(u"channelGreen"_s, t.channelGreen);
    put(u"channelBlue"_s, t.channelBlue);
    put(u"canvas"_s, t.canvas);
    return s;
}

} // namespace

const Tokens& darkTokens()
{
    static const Tokens t{hex("#6EA8FE"), hex("#2F6FE0"), hex("#3B7CF0"), hex("#1D2A40"), hex("#161616"),
                          hex("#0F0F0F"), hex("#1C1C1C"), hex("#1E1E1E"), hex("#282828"), hex("#2C2C2C"),
                          hex("#141414"), hex("#303030"), hex("#696969"), hex("#ECECEC"), hex("#A8A8A8"),
                          hex("#8E8E8E"), hex("#FF6B6B"), hex("#8AB8FF"), hex("#5CCB84"), hex("#F5B14C"),
                          hex("#E05050"), hex("#50C060"), hex("#5080F0")};
    return t;
}

const Tokens& lightTokens()
{
    static const Tokens t{hex("#2560CC"), hex("#2560CC"), hex("#1F52B0"), hex("#E3ECFC"), hex("#EDEDED"),
                          hex("#D6D6D6"), hex("#FAFAFA"), hex("#F5F5F5"), hex("#FFFFFF"), hex("#E6E6E6"),
                          hex("#FFFFFF"), hex("#D4D4D4"), hex("#8E8E8E"), hex("#1A1A1A"), hex("#555555"),
                          hex("#676767"), hex("#C01C28"), hex("#2560CC"), hex("#166B2D"), hex("#B45309"),
                          hex("#E05050"), hex("#50C060"), hex("#5080F0")};
    return t;
}

namespace {

// Fusion with transient scroll bars.
class PixelFxStyle : public QProxyStyle
{
public:
    PixelFxStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {}

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
        if (hint == SH_Slider_AbsoluteSetButtons) {
            return Qt::LeftButton; // a click on a slider's track moves it there
        }
        if (hint == SH_ComboBox_Popup) {
            return 0; // a list opens below its box, as tall as its choices
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }

    // Menus have rounded corners, which need a see-through window behind them, and submenus
    // are kept clear of their parent when they are shown. Item views lay their rows out again
    // once styled: a list filled before it was shown placed its rows at the unstyled height, so
    // the padded highlights would overlap. The style sheet applies its item rules after this
    // polish returns, hence the queued call.
    void polish(QWidget* widget) override
    {
        if (qobject_cast<QMenu*>(widget) != nullptr) {
            widget->setAttribute(Qt::WA_TranslucentBackground);
            widget->installEventFilter(this);
        }
        QProxyStyle::polish(widget);
        if (auto* view = qobject_cast<QAbstractItemView*>(widget)) {
            QMetaObject::invokeMethod(view, &QAbstractItemView::doItemsLayout, Qt::QueuedConnection);
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show) {
            if (auto* menu = qobject_cast<QMenu*>(watched)) {
                menu_style::placeSubmenu(menu);
            }
        }
        return QProxyStyle::eventFilter(watched, event);
    }
    using QProxyStyle::polish;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget) const override
    {
        if (element == PE_FrameFocusRect && widget != nullptr && !showsFocusRing(widget)) {
            return; // a click focuses without drawing a ring
        }
        if (menu_style::drawPrimitive(element, option, painter, widget)) {
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const override
    {
        if (element == CE_MenuItem && menu_style::drawItem(option, painter, widget)) {
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& contents,
                           const QWidget* widget) const override
    {
        if (type == CT_MenuItem) {
            if (const std::optional<QSize> size = menu_style::itemSize(option, contents)) {
                return *size;
            }
        }
        return QProxyStyle::sizeFromContents(type, option, contents, widget);
    }

    int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const override
    {
        if (metric == PM_ScrollView_ScrollBarOverlap) {
            return kScrollBarWidth;
        }
        if (const std::optional<int> menu = menu_style::pixelMetric(metric)) {
            return *menu;
        }
        if (metric == PM_LayoutHorizontalSpacing || metric == PM_LayoutVerticalSpacing) {
            return kGapControls; // a layout without a gap of its own spaces its items as controls
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

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

namespace {

constexpr std::array<QColor Tokens::*, 22> kTokenFields = {
    &Tokens::accent,     &Tokens::accentStrong, &Tokens::accentHover, &Tokens::accentSoft, &Tokens::bg,
    &Tokens::canvas,     &Tokens::bar,          &Tokens::panel,       &Tokens::elevated,   &Tokens::hover,
    &Tokens::input,      &Tokens::border,       &Tokens::track,       &Tokens::text,       &Tokens::muted,
    &Tokens::faint,      &Tokens::danger,       &Tokens::link,        &Tokens::success,    &Tokens::warning,
    &Tokens::channelRed, &Tokens::channelGreen};

} // namespace

int tokenIndex(const QColor& color)
{
    const Tokens& t = tokens();
    for (std::size_t i = 0; i < kTokenFields.size(); ++i) {
        if ((t.*kTokenFields.at(i)).rgb() == color.rgb()) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QColor tokenColor(int index)
{
    return index < 0 || static_cast<std::size_t>(index) >= kTokenFields.size()
               ? QColor()
               : tokens().*kTokenFields.at(static_cast<std::size_t>(index));
}

namespace {

// Marks each widget that takes the focus with whether the keyboard gave it ("focusVisible"), as
// browsers do with :focus-visible: a click focuses without drawing a ring.
class FocusVisibility : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::FocusIn || !watched->isWidgetType()) {
            return false;
        }
        const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
        if (reason == Qt::ActiveWindowFocusReason || reason == Qt::PopupFocusReason ||
            reason == Qt::MenuBarFocusReason) {
            return false; // the focus comes back as it was
        }
        const bool visible = reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason ||
                             reason == Qt::ShortcutFocusReason;
        auto* widget = static_cast<QWidget*>(watched);
        if (widget->property("focusVisible").toBool() != visible) {
            widget->setProperty("focusVisible", visible);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
        }
        return false;
    }
};

} // namespace

bool showsFocusRing(const QWidget* widget)
{
    return widget->hasFocus() && widget->property("focusVisible").toBool();
}

namespace {

// The palette and the style sheet of the current tokens.
void applyColours(QApplication& app)
{
    const Tokens& t = tokens();
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

} // namespace

void applyStyle(QApplication& app)
{
    QApplication::setStyle(new PixelFxStyle); // the application owns it
    app.installEventFilter(new FocusVisibility(&app));
    applyColours(app);
}

void switchTheme(QApplication& app, bool light)
{
    if (light == g_light) {
        return;
    }
    // Glyphs remember their token in the theme they were drawn for.
    QHash<QLabel*, int> glyphs;
    for (QWidget* w : QApplication::allWidgets()) {
        if (auto* label = qobject_cast<QLabel*>(w); label != nullptr && label->property("glyph").isValid()) {
            glyphs.insert(label, tokenIndex(label->property("glyphColor").value<QColor>()));
        }
    }
    g_light = light;
    applyColours(app);
    for (auto it = glyphs.constBegin(); it != glyphs.constEnd(); ++it) {
        const QColor colour =
            it.value() >= 0 ? tokenColor(it.value()) : it.key()->property("glyphColor").value<QColor>();
        icons::setGlyph(it.key(), it.key()->property("glyph").toString(), colour,
                        it.key()->property("glyphSize").toInt());
    }
    for (QWidget* w : QApplication::allWidgets()) {
        w->update();
    }
}

QVBoxLayout* sheetLayout(QDialog* sheet, QLayout* foot)
{
    auto* root = new QVBoxLayout(sheet);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* body = new QVBoxLayout;
    body->setContentsMargins(kSheetPadding, 16, kSheetPadding, 16);
    body->setSpacing(kGapGroups);
    root->addLayout(body, 1);
    if (foot != nullptr) {
        root->addWidget(footBar(sheet, foot));
    }
    return body;
}

QWidget* sheetCloseButton(QDialog* sheet)
{
    auto* close = new QToolButton(sheet);
    close->setObjectName(QStringLiteral("sheetClose"));
    close->setIcon(icons::themed(QStringLiteral("close"), tokens().muted));
    close->setIconSize(QSize(18, 18));
    close->setToolTip(QObject::tr("Close (Escape)"));
    close->setAccessibleName(QObject::tr("Close"));
    QObject::connect(close, &QToolButton::clicked, sheet, &QDialog::reject);
    return close;
}

QHBoxLayout* sheetTitleRow(QDialog* sheet, const QString& title, const QList<QWidget*>& extras)
{
    auto* label = new QLabel(title, sheet);
    label->setObjectName(QStringLiteral("sheetTitle"));
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kGapControls);
    row->addWidget(label);
    row->addSpacing(kGapSmall);
    for (QWidget* extra : extras) {
        row->addWidget(extra);
    }
    row->addStretch();
    row->addWidget(sheetCloseButton(sheet));
    return row;
}

QWidget* sheetHeaderBar(QDialog* sheet, QLayout* titleRow)
{
    auto* bar = new QWidget(sheet);
    titleRow->setContentsMargins(kSheetPadding, 16, kSheetPadding, 12);
    bar->setLayout(titleRow);
    return bar;
}

QPixmap roundedPixmap(const QImage& image, const QSize& size, qreal ratio, qreal radius)
{
    const QSize device = (QSizeF(size) * ratio).toSize();
    QPixmap out(device);
    out.fill(Qt::transparent);
    if (image.isNull() || device.isEmpty()) {
        return out;
    }
    const QImage filled = image.scaled(device, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(device)), radius * ratio, radius * ratio);
    p.setClipPath(clip);
    p.drawImage(QPointF((device.width() - filled.width()) / 2.0, (device.height() - filled.height()) / 2.0),
                filled);
    p.end();
    out.setDevicePixelRatio(ratio);
    return out;
}

QString buttonText(QString text)
{
    return text.replace(u'&', QStringLiteral("&&"));
}

QFrame* segmentBox(const QStringList& names, int checked, QButtonGroup* group, QWidget* parent,
                   SegmentFit fit, const QStringList& glyphs)
{
    auto* box = new QFrame(parent);
    box->setObjectName(QStringLiteral("segmentBox"));
    auto* row = new QHBoxLayout(box);
    row->setContentsMargins(1, 1, 1, 1);
    row->setSpacing(0);
    for (int i = 0; i < names.size(); ++i) {
        QPushButton* b = glyphs.isEmpty()
                             ? new QPushButton(buttonText(names.at(i)), box)
                             : new IconTextButton(icons::toggle(glyphs.at(i), tokens().text, tokens().accent),
                                                  buttonText(names.at(i)), box);
        b->setAccessibleName(names.at(i));
        b->setObjectName(QStringLiteral("segment"));
        b->setCheckable(true);
        b->setChecked(i == checked);
        if (i == 0) {
            b->setProperty("edge", QStringLiteral("first"));
        } else if (i == names.size() - 1) {
            b->setProperty("edge", QStringLiteral("last"));
        }
        group->addButton(b, i);
        row->addWidget(b, fit == SegmentFit::Equal ? 1 : 0);
    }
    if (fit == SegmentFit::Row) {
        row->addStretch();
    }
    return box;
}

void addSearchIcon(QLineEdit* field)
{
    QAction* icon =
        field->addAction(icons::themed(QStringLiteral("search"), tokens().muted), QLineEdit::LeadingPosition);
    icon->setText(QCoreApplication::translate("PixelFx", "Search"));
}

void fitToPanel(QComboBox* list)
{
    constexpr int kShortest = 12; // characters shown at the least
    list->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    list->setMinimumContentsLength(kShortest);
}

QFrame* smallSegments(QFrame* box)
{
    box->setProperty("compact", true);
    return box;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setObjectName(QStringLiteral("section"));
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 0.66);
    label->setFont(font);
    return label;
}

namespace {
constexpr int kAboveSection = 12;
constexpr int kBelowSection = 4;
} // namespace

QLabel* sectionHeading(const QString& text, QWidget* parent, int sides)
{
    QLabel* label = sectionLabel(text, parent);
    label->setContentsMargins(sides, kAboveSection, sides, kBelowSection);
    return label;
}

namespace {

// Keeps a floating scroll bar along the right edge of its scroll area.
class OverlayPlacer : public QObject
{
public:
    OverlayPlacer(QScrollArea* area, QScrollBar* bar)
        : QObject(area)
        , m_area(area)
        , m_bar(bar)
    {
        area->installEventFilter(this);
    }

    void place() const
    {
        const int width = m_bar->sizeHint().width();
        m_bar->setGeometry(m_area->width() - width, 0, width, m_area->height());
        m_bar->raise();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_area && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            place();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QScrollArea* m_area;
    QScrollBar* m_bar;
};

} // namespace

void overlayScrollBar(QScrollArea* area)
{
    // The area's own bar is never shown but keeps the range and the position; this one shows them.
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const QScrollBar* inner = area->verticalScrollBar();
    auto* bar = new QScrollBar(Qt::Vertical, area);
    bar->hide();
    QObject::connect(inner, &QScrollBar::rangeChanged, bar, [bar, inner](int min, int max) {
        bar->setRange(min, max);
        bar->setPageStep(inner->pageStep());
        bar->setSingleStep(inner->singleStep());
        bar->setVisible(max > min);
    });
    QObject::connect(inner, &QScrollBar::valueChanged, bar, &QScrollBar::setValue);
    QObject::connect(bar, &QScrollBar::valueChanged, inner, &QScrollBar::setValue);
    const auto* placer = new OverlayPlacer(area, bar);
    placer->place();
}

void keepScrollBarInPadding(const QScrollArea* area, QLayout* content, int edge)
{
    const auto fit = [area, content, edge](int max) {
        const int bar = max > 0 ? area->verticalScrollBar()->sizeHint().width() : 0;
        const QMargins m = content->contentsMargins();
        content->setContentsMargins(m.left(), m.top(), std::max(0, edge - bar), m.bottom());
    };
    fit(area->verticalScrollBar()->maximum());
    QObject::connect(area->verticalScrollBar(), &QScrollBar::rangeChanged, area,
                     [fit](int /*min*/, int max) { fit(max); });
}

QVBoxLayout* sectionColumn(const QString& heading, QWidget* box)
{
    auto* outer = new QVBoxLayout(box);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0); // the heading's own margins space it
    outer->addWidget(sectionHeading(heading, box));
    auto* controls = new QVBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(kGapControls);
    outer->addLayout(controls);
    return controls;
}

QHBoxLayout* sectionHeadingRow(QLabel* label, QWidget* trailing, int sides)
{
    // A trailing link or button is taller than the heading (its 24 px target); it reaches into
    // the row's margins so the heading keeps its place.
    int reach = 0;
    if (trailing != nullptr) {
        label->ensurePolished();
        trailing->ensurePolished();
        reach =
            std::clamp((trailing->sizeHint().height() - label->sizeHint().height()) / 2, 0, kBelowSection);
    }
    auto* row = new QHBoxLayout;
    row->setContentsMargins(sides, kAboveSection - reach, sides, kBelowSection - reach);
    row->setSpacing(kGapControls);
    row->addWidget(label, 1);
    if (trailing != nullptr) {
        row->addWidget(trailing);
    }
    return row;
}

void addMenuHeading(QMenu* menu, const QString& text)
{
    auto* label = new QLabel(text, menu);
    label->setObjectName(QStringLiteral("menuHeading"));
    auto* action = new QWidgetAction(menu);
    action->setDefaultWidget(label);
    action->setEnabled(false);
    menu->addAction(action);
}

namespace {

// Greys a form's label out with its field, and back (EnabledChange has no signal).
class LabelFollowsField : public QObject
{
public:
    LabelFollowsField(QWidget* field, QWidget* label)
        : QObject(field)
        , m_label(label)
    {
        field->installEventFilter(this);
        label->setEnabled(field->isEnabled());
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::EnabledChange && m_label != nullptr) {
            m_label->setEnabled(static_cast<QWidget*>(watched)->isEnabled());
        }
        return false;
    }

private:
    QPointer<QWidget> m_label;
};

// The height a field's label centres on: the field's, less a note under it (a wrapping label
// last in its column).
int labelHeightFor(QLayoutItem* field)
{
    int height = field->sizeHint().height();
    const QLayout* column = field->widget() != nullptr ? field->widget()->layout() : field->layout();
    if (const auto* box = qobject_cast<const QVBoxLayout*>(column); box != nullptr && box->count() > 1) {
        const auto* note = qobject_cast<const QLabel*>(box->itemAt(box->count() - 1)->widget());
        if (note != nullptr && note->wordWrap()) {
            height -= note->sizeHint().height() + std::max(0, box->spacing());
        }
    }
    return height;
}

} // namespace

void setLabelColumn(const QFormLayout* form, int width)
{
    for (int row = 0; row < form->rowCount(); ++row) {
        const QLayoutItem* item = form->itemAt(row, QFormLayout::LabelRole);
        QWidget* label = item != nullptr ? item->widget() : nullptr;
        QLayoutItem* field = form->itemAt(row, QFormLayout::FieldRole);
        if (label == nullptr || field == nullptr) {
            continue;
        }
        label->setObjectName(QStringLiteral("fieldLabel"));
        label->setMinimumWidth(width - std::max(0, form->horizontalSpacing()));
        // As tall as its field, with the name in the middle: a form puts a label at the top of a
        // taller field; here it is centred.
        if (auto* text = qobject_cast<QLabel*>(label)) {
            text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            text->setMinimumHeight(labelHeightFor(field));
        }
        if (field->widget() != nullptr) {
            new LabelFollowsField(field->widget(), label);
        }
    }
}

QPixmap coverThumbnail(const QImage& image, const QSize& size, int radius, qreal devicePixelRatio)
{
    const QSize pixels = size * devicePixelRatio;
    QPixmap out(pixels);
    out.fill(Qt::transparent);
    if (image.isNull()) {
        return out;
    }
    const QImage scaled = image.scaled(pixels, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(pixels)), radius * devicePixelRatio,
                        radius * devicePixelRatio);
    painter.setClipPath(clip);
    painter.drawImage(QPoint((pixels.width() - scaled.width()) / 2, (pixels.height() - scaled.height()) / 2),
                      scaled);
    painter.end();
    out.setDevicePixelRatio(devicePixelRatio);
    return out;
}

void makeSidePanel(QWidget* panel, Qt::Edge photoSide)
{
    panel->setObjectName(QStringLiteral("sidePanel"));
    panel->setProperty("photoSide",
                       photoSide == Qt::LeftEdge ? QStringLiteral("left") : QStringLiteral("right"));
    panel->setAttribute(Qt::WA_StyledBackground);
    // The 1 px border on the photo's side stays clear: a style sheet border takes no room of its
    // own, so the panel's content would paint over it.
    const bool left = photoSide == Qt::LeftEdge;
    panel->setContentsMargins(left ? 1 : 0, 0, left ? 0 : 1, 0);
}

FootButtons footButtons(QWidget* parent, const QString& primary, const QList<QWidget*>& leading,
                        bool withCancel)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(kGapControls);
    // A leading widget that expands (a message line) takes the free space; otherwise a stretch does.
    bool filled = false;
    for (QWidget* w : leading) {
        const bool expands = (w->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag) != 0;
        row->addWidget(w, expands ? 1 : 0);
        filled = filled || expands;
    }
    if (!filled) {
        row->addStretch();
    }
    QPushButton* cancel = nullptr;
    if (withCancel) {
        cancel = new QPushButton(QObject::tr("Cancel"), parent);
        row->addWidget(cancel);
    }
    auto* button = new IconTextButton(QIcon(), primary, parent); // the primary may take an icon later
    button->setObjectName(QStringLiteral("primary"));
    button->setDefault(true);
    row->addWidget(button);
    return {row, cancel, button};
}

QWidget* footBar(QWidget* parent, QLayout* foot, int padding, int vertical)
{
    auto* bar = new QWidget(parent);
    bar->setObjectName(QStringLiteral("footBar"));
    bar->setAttribute(Qt::WA_StyledBackground);
    foot->setContentsMargins(padding, vertical, padding, vertical);
    bar->setLayout(foot);
    return bar;
}

QWidget* panelFootBar(QWidget* parent, QLayout* foot)
{
    return footBar(parent, foot, kPanelPadding, 10);
}

} // namespace pixelfx::ui
