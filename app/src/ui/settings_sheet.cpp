#include "ui/settings_sheet.h"

#include "core/about.h"
#include "core/export_options.h"
#include "core/log_sink.h"
#include "core/looks.h"
#include "core/presets.h"
#include "core/settings/settings.h"
#include "ui/elided_label.h"
#include "ui/export_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kLabelColumn = 160;
constexpr int kNameWidth = 260;

/// `widget` kept at its own width: the form would stretch it across the column.
template <typename W>
W* naturalWidth(W* widget)
{
    widget->setSizePolicy(QSizePolicy::Fixed, widget->sizePolicy().verticalPolicy());
    return widget;
}

/// A segmented control as wide as its segments, one checked; `chosen(index)` on click.
template <typename Chosen>
QWidget* segments(const QStringList& names, int current, QWidget* parent, Chosen chosen)
{
    auto* group = new QButtonGroup(parent);
    QFrame* box = segmentBox(names, current, group, parent, SegmentFit::Content);
    QObject::connect(group, &QButtonGroup::idClicked, box, chosen);
    return naturalWidth(box);
}

/// A check box styled as a switch, showing `on`; `toggled(bool)` on change.
template <typename Toggled>
QCheckBox* switchBox(const QString& text, bool on, QWidget* parent, Toggled toggled)
{
    auto* box = new QCheckBox(text, parent);
    box->setObjectName(u"switch"_s);
    box->setChecked(on);
    QObject::connect(box, &QCheckBox::toggled, parent, toggled);
    return box;
}

QPushButton* smallButton(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setObjectName(u"small"_s);
    return b;
}

/// `control` with a muted line under it saying what it does.
QWidget* withNote(QWidget* control, const QString& note, QWidget* parent)
{
    auto* line = new QLabel(note, parent);
    line->setObjectName(u"itemDetail"_s);
    line->setWordWrap(true);
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapSmall);
    column->addWidget(control);
    column->addWidget(line);
    return box;
}

/// Widgets side by side, left aligned.
QWidget* rowOf(std::initializer_list<QWidget*> widgets, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kGapItems);
    for (QWidget* w : widgets) {
        layout->addWidget(w);
    }
    layout->addStretch();
    return row;
}

QFormLayout* pageForm(QWidget* page)
{
    auto* form = new QFormLayout(page);
    form->setContentsMargins(kSheetPadding, 4, kSheetPadding, 16);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setHorizontalSpacing(kGapItems);
    form->setVerticalSpacing(kGapGroups);
    return form;
}

void openFolder(const QString& folder)
{
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

} // namespace

SettingsSheet::SettingsSheet(core::Settings& settings, QString looksFolder, QString presetsFolder,
                             QWidget* parent)
    : Sheet(parent)
    , m_settings(settings)
    , m_looksFolder(std::move(looksFolder))
    , m_presetsFolder(std::move(presetsFolder))
    , m_pages(new QListWidget(this))
    , m_stack(new QStackedWidget(this))
{
    setWindowTitle(tr("Settings"));
    QWidget* head = sheetHeaderBar(this, sheetTitleRow(this, tr("Settings")));

    m_pages->setObjectName(u"settingsPages"_s);
    m_pages->setFixedWidth(200);
    m_pages->addItems({tr("General"), tr("Editor"), tr("Export"), tr("Looks and presets"), tr("Performance"),
                       tr("Advanced")});
    m_pages->setAccessibleName(tr("Settings pages"));
    buildPages();
    connect(m_pages, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_pages->setCurrentRow(GeneralPage);
    // As tall as the tallest page, so switching pages never resizes the sheet; scrolling only
    // when the window is too short.
    auto* scroll = new SheetScroll(m_stack, kSheetPadding, this);
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(m_pages);
    body->addWidget(scroll, 1);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(head);
    column->addLayout(body, 1);
    setSize(Size::Wide);
}

SettingsSheet::~SettingsSheet() = default;

void SettingsSheet::showPage(Page page)
{
    m_pages->setCurrentRow(page);
}

SettingsSheet::Page SettingsSheet::page() const
{
    return static_cast<Page>(m_stack->currentIndex());
}

void SettingsSheet::buildPages()
{
    while (m_stack->count() > 0) {
        QWidget* old = m_stack->widget(0);
        m_stack->removeWidget(old);
        old->deleteLater(); // a button on it may be the one being clicked
    }
    for (QWidget* page :
         {buildGeneral(), buildEditor(), buildExport(), buildLooks(), buildPerformance(), buildAdvanced()}) {
        m_stack->addWidget(page);
    }
    refreshCounts();
}

QWidget* SettingsSheet::buildGeneral()
{
    using S = core::Settings;
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);

    form->addRow(sectionHeading(tr("Appearance"), page));
    form->addRow(tr("Theme"),
                 segments({tr("System"), tr("Dark"), tr("Light")}, static_cast<int>(m_settings.theme()), page,
                          [this](int i) { m_settings.setTheme(static_cast<S::Theme>(i)); }));
    form->addRow(tr("Canvas background"),
                 segments({tr("Dark gray"), tr("Middle gray"), tr("Black"), tr("White")},
                          static_cast<int>(m_settings.canvasBackground()), page, [this](int i) {
                              m_settings.setCanvasBackground(static_cast<S::CanvasBackground>(i));
                          }));

    form->addRow(sectionHeading(tr("Notifications"), page));
    form->addRow(tr("Batch finished"),
                 switchBox(tr("Notify me when PixelFx is in the background"), m_settings.notifyBatchDone(),
                           page, [this](bool on) { m_settings.setNotifyBatchDone(on); }));

    form->addRow(sectionHeading(tr("Updates"), page));
    form->addRow(tr("What's new"), switchBox(tr("Show this after each update"), m_settings.showWhatsNew(),
                                             page, [this](bool on) { m_settings.setShowWhatsNew(on); }));
    setLabelColumn(form, kLabelColumn);
    return page;
}

QWidget* SettingsSheet::buildEditor()
{
    using S = core::Settings;
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);

    form->addRow(sectionHeading(tr("Editing"), page));
    QCheckBox* remember =
        switchBox(tr("Keep each photo's settings in the app's library"), m_settings.rememberEdits(), page,
                  [this](bool on) { m_settings.setRememberEdits(on); });
    form->addRow(tr("Remember edits"), remember);
    auto* onOpen = naturalWidth(new QComboBox(page));
    onOpen->addItems({tr("Show the photo as it was left"), tr("Always start from the original")});
    onOpen->setCurrentIndex(m_settings.openAsLeft() ? 0 : 1);
    onOpen->setEnabled(m_settings.rememberEdits());
    connect(remember, &QCheckBox::toggled, onOpen, &QWidget::setEnabled);
    connect(onOpen, &QComboBox::activated, this, [this](int i) { m_settings.setOpenAsLeft(i == 0); });
    form->addRow(tr("On open"), onOpen);

    form->addRow(sectionHeading(tr("Adjustments"), page));
    QCheckBox* solo = switchBox(tr("One group open at a time"), m_settings.oneGroupAtATime(), page,
                                [this](bool on) { m_settings.setOneGroupAtATime(on); });
    connect(&m_settings, &S::oneGroupAtATimeChanged, solo, &QCheckBox::setChecked);
    form->addRow(tr("Groups"), solo);

    form->addRow(sectionHeading(tr("Display"), page));
    QCheckBox* clipping =
        switchBox(tr("Show clipped highlights and shadows (J)"), m_settings.clippingWarnings(), page,
                  [this](bool on) { m_settings.setClippingWarnings(on); });
    connect(&m_settings, &S::clippingWarningsChanged, clipping, &QCheckBox::setChecked);
    form->addRow(tr("Clipping warnings"), clipping);
    setLabelColumn(form, kLabelColumn);
    return page;
}

void SettingsSheet::changeExport(const std::function<void(core::ExportOptions&)>& edit)
{
    core::ExportOptions options = m_settings.exportOptions();
    edit(options);
    m_settings.setExportOptions(options);
}

QWidget* SettingsSheet::buildExport()
{
    using IfExists = core::ExportOptions::IfExists;
    using E = core::ExportOptions;
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);
    const E o = m_settings.exportOptions();

    auto* intro =
        new QLabel(tr("New exports start with these. A change made in Export shows here too."), page);
    intro->setObjectName(u"itemDetail"_s);
    intro->setWordWrap(true);
    form->addRow(intro);

    form->addRow(sectionHeading(tr("Save"), page));
    form->addRow(ExportSheet::tr("Save to"), exportFolderRow(o.folder, page));
    auto* name = new QLineEdit(o.nameTemplate, page);
    name->setMaximumWidth(kNameWidth);
    name->setAccessibleName(ExportSheet::tr("File name"));
    connect(name, &QLineEdit::editingFinished, this, [this, name] {
        const QString text = name->text().trimmed();
        if (text.isEmpty()) {
            name->setText(m_settings.exportOptions().nameTemplate);
            return;
        }
        changeExport([&](E& options) { options.nameTemplate = text; });
    });
    form->addRow(ExportSheet::tr("File name"),
                 withNote(name,
                          ExportSheet::tr("{name}: the photo's name, {date}: today, {w} and {h}: the size"),
                          page));
    form->addRow(
        ExportSheet::tr("If it exists"),
        segments({ExportSheet::tr("Add a number"), ExportSheet::tr("Overwrite"), ExportSheet::tr("Skip")},
                 static_cast<int>(o.ifExists), page, [this](int i) {
                     changeExport([&](E& options) { options.ifExists = static_cast<IfExists>(i); });
                 }));

    form->addRow(sectionHeading(tr("In the file"), page));
    form->addRow(ExportSheet::tr("Metadata"), exportMetadataBox(o, page));
    QComboBox* space = naturalWidth(colorSpaceList(o.colorSpace, page));
    connect(space, &QComboBox::activated, this, [this, space] {
        const auto chosen = static_cast<pfx::OutputSpace>(space->currentData().toInt());
        changeExport([&](E& options) { options.colorSpace = chosen; });
    });
    form->addRow(ExportSheet::tr("Color space"), space);
    setLabelColumn(form, kLabelColumn);
    return page;
}

QWidget* SettingsSheet::exportFolderRow(const QString& path, QWidget* page)
{
    auto* folder = new PathLabel(ExportSheet::tr("Next to the photo"), page);
    folder->setPath(path);
    QPushButton* change = smallButton(ExportSheet::tr("Change..."), page);
    connect(change, &QPushButton::clicked, this, [this, folder] {
        const QString chosen = QFileDialog::getExistingDirectory(this, ExportSheet::tr("Save to"),
                                                                 m_settings.exportOptions().folder);
        if (!chosen.isEmpty()) {
            changeExport([&](core::ExportOptions& options) { options.folder = chosen; });
            folder->setPath(chosen);
        }
    });
    auto* row = new QWidget(page);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kGapItems);
    layout->addWidget(folder, 1);
    layout->addWidget(change);
    return row;
}

QWidget* SettingsSheet::exportMetadataBox(const core::ExportOptions& options, QWidget* page)
{
    using E = core::ExportOptions;
    QCheckBox* metadata =
        switchBox(ExportSheet::tr("Keep camera details and copyright"), options.keepMetadata, page,
                  [this](bool on) { changeExport([&](E& o) { o.keepMetadata = on; }); });
    QCheckBox* location = switchBox(ExportSheet::tr("Keep location"), options.keepLocation, page,
                                    [this](bool on) { changeExport([&](E& o) { o.keepLocation = on; }); });
    location->setToolTip(
        ExportSheet::tr("Where the photo was taken (GPS). Off unless you choose to share it."));
    location->setEnabled(options.keepMetadata);
    connect(metadata, &QCheckBox::toggled, location, &QWidget::setEnabled);
    auto* box = new QWidget(page);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapSmall);
    column->addWidget(metadata);
    column->addWidget(location);
    return box;
}

QWidget* SettingsSheet::buildLooks()
{
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);

    form->addRow(sectionHeading(tr("Yours"), page));
    m_lookCount = new QLabel(page);
    QPushButton* looksFolder = smallButton(tr("Show folder"), page);
    looksFolder->setAccessibleName(tr("Show the folder of my looks"));
    connect(looksFolder, &QPushButton::clicked, this, [this] { openFolder(m_looksFolder); });
    form->addRow(tr("My looks"), rowOf({m_lookCount, looksFolder}, page));
    m_presetCount = new QLabel(page);
    QPushButton* presetsFolder = smallButton(tr("Show folder"), page);
    presetsFolder->setAccessibleName(tr("Show the folder of my presets"));
    connect(presetsFolder, &QPushButton::clicked, this, [this] { openFolder(m_presetsFolder); });
    form->addRow(tr("My presets"), rowOf({m_presetCount, presetsFolder}, page));
    QPushButton* import = smallButton(tr("Import presets and looks..."), page);
    connect(import, &QPushButton::clicked, this, &SettingsSheet::importRequested);
    form->addRow(tr("Import"), withNote(rowOf({import}, page),
                                        tr("PixelFx, Lightroom and Camera Raw presets, .cube LUTs"), page));

    form->addRow(sectionHeading(tr("Lists"), page));
    m_recentCount = new QLabel(page);
    m_clearRecent = smallButton(tr("Clear recent"), page);
    connect(m_clearRecent, &QPushButton::clicked, this, [this] {
        m_settings.clearRecentPresets();
        refreshCounts();
    });
    form->addRow(tr("Recent presets"), rowOf({m_recentCount, m_clearRecent}, page));
    setLabelColumn(form, kLabelColumn);
    return page;
}

void SettingsSheet::refreshCounts()
{
    if (m_lookCount == nullptr) {
        return;
    }
    const auto looks = core::LookLibrary(m_looksFolder).entries();
    const auto mineLooks = std::ranges::count_if(looks, [](const core::LookEntry& e) { return e.isMine(); });
    m_lookCount->setText(tr("%n looks", nullptr, static_cast<int>(mineLooks)));
    const auto presets = core::PresetLibrary(m_presetsFolder).entries();
    const auto minePresets =
        std::ranges::count_if(presets, [](const core::PresetEntry& e) { return !e.isBuiltIn(); });
    m_presetCount->setText(tr("%n presets", nullptr, static_cast<int>(minePresets)));
    const auto recent = static_cast<int>(m_settings.recentPresets().size());
    m_recentCount->setText(tr("%n presets", nullptr, recent));
    m_clearRecent->setEnabled(recent > 0);
}

QWidget* SettingsSheet::buildPerformance()
{
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);

    form->addRow(sectionHeading(tr("Batch"), page));
    auto* atOnce = naturalWidth(new QComboBox(page));
    atOnce->setAccessibleName(tr("Photos at a time"));
    atOnce->addItem(tr("Automatic: 2, or 4 for Compress only"), 0);
    for (const int n : {1, 2, 4, 8}) {
        atOnce->addItem(QString::number(n), n);
    }
    atOnce->setCurrentIndex(std::max(0, atOnce->findData(m_settings.batchAtOnce())));
    connect(atOnce, &QComboBox::activated, this,
            [this, atOnce] { m_settings.setBatchAtOnce(atOnce->currentData().toInt()); });
    form->addRow(tr("Photos at a time"),
                 withNote(atOnce,
                          tr("More at once is faster with many small photos and needs more memory: "
                             "a 24 megapixel photo takes about 1 GB while it is edited."),
                          page));
    setLabelColumn(form, kLabelColumn);
    return page;
}

QWidget* SettingsSheet::buildAdvanced()
{
    auto* page = new QWidget(this);
    QFormLayout* form = pageForm(page);

    form->addRow(sectionHeading(tr("Problems"), page));
    QPushButton* copy = smallButton(tr("Copy"), page);
    copy->setToolTip(
        tr("Version, system and graphics, for a bug report; no photo names and no personal data"));
    connect(copy, &QPushButton::clicked, this, [] {
        QApplication::clipboard()->setText(core::diagnosticsText(
            core::collectDiagnostics(QApplication::applicationVersion(), core::buildRevision())));
    });
    QPushButton* logs = smallButton(tr("Open the log folder"), page);
    const QString logFile = core::LogSink::logFilePath();
    logs->setEnabled(!logFile.isEmpty());
    connect(logs, &QPushButton::clicked, this,
            [logFile] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(logFile).absolutePath())); });
    form->addRow(tr("Diagnostics"), rowOf({copy, logs}, page));

    form->addRow(sectionHeading(tr("Reset"), page));
    QPushButton* restore = smallButton(tr("Restore default settings..."), page);
    connect(restore, &QPushButton::clicked, this, &SettingsSheet::restoreDefaults);
    form->addRow(tr("Settings"),
                 withNote(rowOf({restore}, page),
                          tr("Every setting on these pages goes back to how it was at install. "
                             "Your photos, edits, presets and looks stay."),
                          page));
    setLabelColumn(form, kLabelColumn);
    return page;
}

void SettingsSheet::restoreDefaults()
{
    QMessageBox box(QMessageBox::Question, tr("Restore default settings?"),
                    tr("Every setting on these pages goes back to how it was at install. "
                       "Your photos, edits, presets and looks stay."),
                    QMessageBox::NoButton, this);
    QPushButton* restore = box.addButton(tr("Restore defaults"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(restore);
    box.exec();
    if (box.clickedButton() != restore) {
        return;
    }
    const Page shown = page();
    m_settings.restoreDefaults();
    buildPages();
    m_stack->setCurrentIndex(shown);
}

} // namespace pixelfx::ui
