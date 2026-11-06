#include "ui/export_sheet.h"

#include "core/describe.h"
#include "services/exporter.h"
#include "ui/elided_label.h"
#include "ui/frame_picker.h"
#include "ui/size_list.h"
#include "ui/slider_row.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

struct Format
{
    const char* extension;
    const char* label;
    bool lossy;
    bool deep; // 16 bits per channel possible
    bool rare; // listed under More
};

constexpr Format kFormats[] = {
    {"jpg", "JPEG", true, false, false},  {"png", "PNG", false, true, false},
    {"webp", "WebP", true, false, false}, {"avif", "AVIF", true, false, false},
    {"tif", "TIFF", false, true, false},  {"jxl", "JPEG XL", true, false, true},
    {"heic", "HEIC", true, false, true},
};

constexpr int kLabelColumn = 160;
constexpr int kEstimatePauseMs = 300;

const Format* formatFor(const QString& extension)
{
    for (const Format& f : kFormats) {
        if (extension == QLatin1StringView(f.extension)) {
            return &f;
        }
    }
    return &kFormats[0];
}

} // namespace

// A line across the form, 8 px from the rows on either side.
QWidget* formDivider(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setObjectName(u"divider"_s);
    line->setFixedHeight(1);
    auto* box = new QWidget(parent);
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(0, 2, 0, 2); // with the form's 12 px between rows
    layout->addWidget(line);
    return box;
}

SliderRow* qualitySlider(int quality, QWidget* parent)
{
    auto* row = new SliderRow(ExportSheet::tr("Smaller file \u00B7 Better image"), parent);
    row->slider()->setAccessibleName(ExportSheet::tr("Quality"));
    row->setRange(1, 100, core::ExportOptions().quality);
    row->setTicks(quality);
    return row;
}

ExportSheet::ExportSheet(const QString& source, const QSize& photoSize, const core::ExportOptions& options,
                         QWidget* parent)
    : Sheet(parent)
    , m_source(source)
    , m_folderPath(options.folder)
    , m_pendingSize(options.sizePreset)
{
    setWindowTitle(tr("Export"));
    setModal(true);

    m_file = new QLabel(QFileInfo(source).fileName(), this);
    m_file->setObjectName(u"muted"_s);
    QHBoxLayout* heading = sheetTitleRow(this, tr("Export"), {m_file});
    m_title = findChild<QLabel*>(u"sheetTitle"_s);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(kGapItems);
    form->setVerticalSpacing(kGapGroups);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Format"), formatRow(options.extension));

    form->addRow(tr("Quality"), qualityRow(options.quality));

    m_sixteen = new QCheckBox(tr("16 bits per channel, for later edits"), this);
    m_sixteen->setChecked(options.bitsPerSample == 16);
    m_sixteen->setToolTip(tr("A larger file, with more room for later edits in another app"));
    form->addRow(QString(), m_sixteen);

    form->addRow(tr("Size"), sizeRow(photoSize, options));
    form->addRow(QString(), framingCard(options));
    refreshSize();

    form->addRow(tr("Sharpen for"), sharpenRow(options.sharpen));
    form->addRow(tr("Color space"), colorSpaceBox(options.colorSpace));

    form->addRow(tr("Watermark"), watermarkRow(options.watermark));

    form->addRow(tr("Metadata"), metadataRow(options));

    form->addRow(formDivider(this));

    form->addRow(tr("Save to"), folderRow());

    form->addRow(tr("File name"), nameRow(options.nameTemplate));

    form->addRow(tr("If it exists"), ifExistsBox(options.ifExists));

    QHBoxLayout* foot = addFooter(form);
    setLabelColumn(form, kLabelColumn);
    QVBoxLayout* column = sheetLayout(this, foot);
    column->setSpacing(kGapGroups);
    addScrollingContent(column, heading, form, this);
    form->setContentsMargins(0, kGapRelated, 0, 0);
    updateForFormat();
    setSize(Size::Regular);

    m_estimateTimer = new QTimer(this);
    m_estimateTimer->setSingleShot(true);
    m_estimateTimer->setInterval(kEstimatePauseMs);
    connect(m_estimateTimer, &QTimer::timeout, this, &ExportSheet::estimate);
    connect(m_quality, &SliderRow::ticksReleased, this, &ExportSheet::scheduleEstimate);
    connect(m_sixteen, &QCheckBox::toggled, this, &ExportSheet::scheduleEstimate);
    connect(m_metadata, &QCheckBox::toggled, this, &ExportSheet::scheduleEstimate);
}

ExportSheet::~ExportSheet() = default;

QHBoxLayout* ExportSheet::addFooter(QFormLayout* form)
{
    m_note = new QLabel(tr("Saved formats and choices are remembered."), this);
    m_note->setObjectName(u"faint"_s);
    m_note->setWordWrap(true);

    m_copyButton = new QPushButton(tr("Copy to clipboard"), this);
    m_copyButton->setToolTip(tr("Copy the photo at this size, with the watermark, instead of saving a file"));
    connect(m_copyButton, &QPushButton::clicked, this, [this] { done(kCopyToClipboard); });
    const FootButtons buttons = footButtons(this, tr("Export"), {m_copyButton}, true);
    m_exportButton = buttons.primary;
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_exportButton, &QPushButton::clicked, this, &QDialog::accept);
    form->addRow(m_note);
    return buttons.row;
}

QWidget* ExportSheet::qualityRow(int quality)
{
    m_quality = qualitySlider(quality, this);
    m_quality->setFlush();
    m_qualityRow = m_quality;
    return m_qualityRow;
}

QWidget* ExportSheet::sizeRow(const QSize& photoSize, const core::ExportOptions& options)
{
    m_photoSize = photoSize;
    m_longEdge = options.longEdge;
    m_custom = core::CustomSize{QString(), options.customSize, options.dpi};
    m_size = new QComboBox(this);
    m_size->setObjectName(u"sizeList"_s);
    m_size->setAccessibleName(tr("Size"));
    sizelist::install(m_size);
    fitToPanel(m_size); // a long size name never widens the sheet
    QString chosen = options.sizePreset;
    if (chosen.isEmpty() && (options.longEdge > 0 || core::hasSize(options.customSize))) {
        chosen = u"custom"_s;
    }
    fillSizes(chosen);
    connect(m_size, &QComboBox::activated, this, [this](int index) {
        if (m_size->itemData(index).toString() == u"new"_s && !askCustomSize()) {
            m_size->setCurrentIndex(m_sizeIndex);
        }
        m_sizeIndex = m_size->currentIndex();
    });
    connect(m_size, &QComboBox::currentIndexChanged, this, &ExportSheet::refreshSize);
    return m_size;
}

void ExportSheet::fillSizes(const QString& chosen)
{
    const QSignalBlocker block(m_size);
    m_size->clear();
    const QString photo = tr("%1 x %2").arg(m_photoSize.width()).arg(m_photoSize.height());
    sizelist::addHeading(m_size, tr("Keep"));
    sizelist::addItem(m_size, tr("Original"), photo, QString());
    QString group;
    for (const core::SizePreset& p : core::sizePresets()) {
        const QString g = QCoreApplication::translate("SizePresets", p.group);
        if (g != group) {
            group = g;
            sizelist::addHeading(m_size, g);
        }
        sizelist::addItem(m_size, QCoreApplication::translate("SizePresets", p.name),
                          core::sizeDetail(p, m_photoSize), QString::fromLatin1(p.id));
    }
    if (!m_mySizes.empty()) {
        sizelist::addHeading(m_size, tr("Mine"));
        for (const core::CustomSize& c : m_mySizes) {
            sizelist::addItem(m_size, c.name, core::sizeDetail(c), core::customSizeId(c));
        }
    }
    // An unnamed custom size in use, from the last export.
    if (m_longEdge > 0 || core::hasSize(m_custom.size)) {
        sizelist::addItem(m_size, tr("Custom"),
                          m_longEdge > 0 ? tr("%1 long edge").arg(m_longEdge) : core::sizeDetail(m_custom),
                          u"custom"_s);
    }
    sizelist::addSeparator(m_size);
    sizelist::addItem(m_size, tr("Custom size..."), QString(), u"new"_s);
    m_size->setCurrentIndex(std::max(1, m_size->findData(chosen)));
    m_sizeIndex = m_size->currentIndex();
}

void ExportSheet::setCustomSizes(const std::vector<core::CustomSize>& sizes)
{
    m_mySizes = sizes;
    const QString chosen = m_size->currentData().toString();
    fillSizes(m_pendingSize.isEmpty() ? chosen : m_pendingSize);
    m_pendingSize.clear();
    refreshSize();
}

QVBoxLayout* ExportSheet::customSizeForm(Sheet& sheet, CustomSizeControls& c, QPushButton* use) const
{
    const auto side = [&sheet](int value, const QString& name) {
        auto* box = new QSpinBox(&sheet);
        box->setRange(0, 65535);
        box->setSuffix(tr(" px"));
        box->setSpecialValueText(tr("Follows the photo"));
        box->setValue(value);
        box->setAccessibleName(name);
        return box;
    };
    c.width = side(m_custom.size.width() > 0 ? m_custom.size.width() : 2000, tr("Width"));
    c.height = side(m_custom.size.height(), tr("Height"));
    c.dpi = new QSpinBox(&sheet);
    c.dpi->setRange(0, 4800);
    c.dpi->setSpecialValueText(tr("Not set"));
    c.dpi->setSuffix(tr(" dpi"));
    c.dpi->setValue(m_custom.dpi);
    c.dpi->setAccessibleName(tr("Pixels per inch"));
    c.keep = new QCheckBox(tr("Keep it under Mine as"), &sheet);
    c.name = new QLineEdit(&sheet);
    c.name->setPlaceholderText(tr("Shop listing"));
    c.name->setAccessibleName(tr("Name of the size"));
    c.name->setEnabled(false);
    connect(c.keep, &QCheckBox::toggled, c.name, &QWidget::setEnabled);
    const auto valid = [c, use] {
        use->setEnabled((c.width->value() > 0 || c.height->value() > 0) &&
                        (!c.keep->isChecked() || !c.name->text().trimmed().isEmpty()));
    };
    for (const QSpinBox* box : {c.width, c.height}) {
        connect(box, &QSpinBox::valueChanged, &sheet, valid);
    }
    connect(c.keep, &QCheckBox::toggled, &sheet, valid);
    connect(c.name, &QLineEdit::textChanged, &sheet, valid);
    auto* column = new QVBoxLayout;
    column->setSpacing(kGapGroups);
    column->addLayout(sheetTitleRow(&sheet, tr("Custom size")));
    auto* note =
        new QLabel(tr("Both sides make an exact frame; one side leaves the other to the photo's shape. "
                      "Resolution is written into the file for printing."),
                   &sheet);
    note->setObjectName(u"muted"_s);
    note->setWordWrap(true);
    column->addWidget(note);
    auto* form = new QFormLayout;
    form->addRow(tr("Width"), c.width);
    form->addRow(tr("Height"), c.height);
    form->addRow(tr("Resolution"), c.dpi);
    column->addLayout(form);
    auto* keepRow = new QHBoxLayout;
    keepRow->addWidget(c.keep);
    keepRow->addWidget(c.name, 1);
    column->addLayout(keepRow);
    return column;
}

bool ExportSheet::askCustomSize()
{
    Sheet sheet(this);
    sheet.setWindowTitle(tr("Custom size"));
    const FootButtons buttons = footButtons(&sheet, tr("Use size"), {}, true);
    connect(buttons.cancel, &QPushButton::clicked, &sheet, &QDialog::reject);
    connect(buttons.primary, &QPushButton::clicked, &sheet, &QDialog::accept);
    CustomSizeControls c;
    sheetLayout(&sheet, buttons.row)->addLayout(customSizeForm(sheet, c, buttons.primary));
    sheet.setSize(Size::Narrow);
    c.width->setFocus();
    c.width->selectAll();
    if (sheet.exec() != QDialog::Accepted) {
        return false;
    }
    m_longEdge = 0;
    const bool keep = c.keep->isChecked();
    m_custom = core::CustomSize{keep ? c.name->text().trimmed() : QString(),
                                QSize(c.width->value(), c.height->value()), c.dpi->value()};
    if (keep) {
        std::erase_if(m_mySizes, [this](const core::CustomSize& s) { return s.name == m_custom.name; });
        m_mySizes.push_back(m_custom);
        m_mySizesChanged = true;
    }
    fillSizes(keep ? core::customSizeId(m_custom) : u"custom"_s);
    refreshSize();
    return true;
}

QWidget* ExportSheet::framingCard(const core::ExportOptions& options)
{
    m_framing = new QFrame(this);
    m_framing->setObjectName(u"card"_s);
    m_frameNote = new QLabel(m_framing);
    m_frameNote->setObjectName(u"muted"_s);
    m_picker = new FramePicker(m_framing);
    m_picker->setAnchor({options.frameX, options.frameY});
    m_fill = new QPushButton(tr("Crop to fill"), m_framing);
    m_fit = new QPushButton(tr("Fit inside"), m_framing);
    m_fill->setToolTip(tr("Keep the part in the frame, cropped to the shape"));
    m_fit->setToolTip(tr("Keep the whole photo and add a border"));
    auto* group = new QButtonGroup(m_framing);
    auto* fitBox = new QFrame(m_framing);
    fitBox->setObjectName(u"segmentBox"_s);
    auto* buttons = new QHBoxLayout(fitBox);
    buttons->setContentsMargins(1, 1, 1, 1);
    buttons->setSpacing(0);
    for (QPushButton* b : {m_fill, m_fit}) {
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        group->addButton(b);
        buttons->addWidget(b, 1);
    }
    m_fill->setProperty("edge", u"first"_s);
    (options.fitInside ? m_fit : m_fill)->setChecked(true);
    connect(group, &QButtonGroup::buttonClicked, this, &ExportSheet::refreshSize);
    auto* help =
        new QLabel(tr("Drag the frame to choose what stays. Fit inside adds a border instead."), m_framing);
    help->setObjectName(u"itemDetail"_s);
    help->setWordWrap(true);
    auto* column = new QVBoxLayout(m_framing);
    column->setContentsMargins(10, 10, 10, 10);
    column->setSpacing(kGapControls);
    column->addWidget(m_frameNote);
    column->addWidget(m_picker);
    column->addWidget(fitBox);
    column->addWidget(help);
    return m_framing;
}

core::ExportOptions ExportSheet::sizeOptions() const
{
    core::ExportOptions o;
    const QString size = m_size->currentData().toString();
    if (core::sizePreset(size) != nullptr) {
        o.sizePreset = size;
        o.dpi = core::sizePreset(size)->followsPhoto ? 300 : 0; // the print sizes are at 300 dpi
    } else if (size.startsWith(u"mine:"_s)) {
        for (const core::CustomSize& c : m_mySizes) {
            if (core::customSizeId(c) == size) {
                o.sizePreset = size;
                o.customSize = c.size;
                o.dpi = c.dpi;
            }
        }
    } else if (size == u"custom"_s) {
        o.longEdge = m_longEdge;
        o.customSize = m_longEdge > 0 ? QSize() : m_custom.size;
        o.dpi = m_custom.dpi;
    }
    o.fitInside = m_fit->isChecked();
    o.frameX = m_picker->anchor().x();
    o.frameY = m_picker->anchor().y();
    return o;
}

void ExportSheet::refreshSize()
{
    scheduleEstimate();
    const core::ExportOptions o = sizeOptions();
    const QSize frame = core::frameSize(o, m_photoSize);
    // Only when the shapes differ is there anything to choose.
    const bool differs = frame.isValid() && m_photoSize.height() > 0 &&
                         std::abs((static_cast<double>(frame.width()) / frame.height()) -
                                  (static_cast<double>(m_photoSize.width()) / m_photoSize.height())) > 0.01;
    m_framing->setVisible(differs);
    if (differs) {
        //: Above the framing picture: "Instagram portrait is 4:5, this photo is 3:2".
        m_frameNote->setText(tr("%1 is %2, this photo is %3")
                                 .arg(m_size->currentData().toString() == u"custom"_s
                                          ? tr("This size")
                                          : sizelist::nameOf(m_size, m_size->currentIndex()),
                                      core::ratioName(frame), core::ratioName(m_photoSize)));
        m_picker->setFrame(frame, o.fitInside);
    }
}

QWidget* ExportSheet::folderRow()
{
    auto* row = new QWidget(this);
    m_folder = new PathLabel(tr("Next to the photo"), row);
    m_folder->setObjectName(u"mono"_s);
    m_folder->setPath(m_folderPath);
    auto* change = new QPushButton(tr("Change..."), row);
    change->setObjectName(u"small"_s);
    connect(change, &QPushButton::clicked, this, &ExportSheet::chooseFolder);
    auto* folderLayout = new QHBoxLayout(row);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->addWidget(m_folder, 1);
    folderLayout->addWidget(change);
    return row;
}

QWidget* ExportSheet::nameRow(const QString& nameTemplate)
{
    auto* row = new QWidget(this);
    m_name = new QLineEdit(nameTemplate, row);
    m_name->setToolTip(tr("{name}: the photo's name, {date}: today, {w} and {h}: the size"));
    m_extension = new QLabel(row);
    m_extension->setObjectName(u"muted"_s);
    auto* nameLayout = new QHBoxLayout(row);
    nameLayout->setContentsMargins(0, 0, 0, 0);
    nameLayout->addWidget(m_name, 1);
    nameLayout->addWidget(m_extension);
    return row;
}

QWidget* ExportSheet::ifExistsBox(core::ExportOptions::IfExists ifExists)
{
    m_ifExists = new QComboBox(this);
    m_ifExists->addItem(tr("Add a number"),
                        QVariant::fromValue(static_cast<int>(core::ExportOptions::IfExists::AddNumber)));
    m_ifExists->addItem(tr("Overwrite"),
                        QVariant::fromValue(static_cast<int>(core::ExportOptions::IfExists::Overwrite)));
    m_ifExists->addItem(tr("Skip"),
                        QVariant::fromValue(static_cast<int>(core::ExportOptions::IfExists::Skip)));
    m_ifExists->setCurrentIndex(m_ifExists->findData(static_cast<int>(ifExists)));
    return m_ifExists;
}

QWidget* ExportSheet::formatRow(const QString& current)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_formats = new QButtonGroup(this);
    auto* more = new QMenu(row);
    QStringList names;
    QStringList extensions;
    const auto writable = pfx::writableExtensions();
    for (const Format& f : kFormats) {
        const std::string ext = std::string(".") + f.extension;
        if (std::ranges::find(writable, ext) == writable.end()) {
            continue; // this build cannot write it (AVIF, HEIC and JPEG XL depend on libvips)
        }
        const QString extension = QString::fromLatin1(f.extension);
        if (f.rare) {
            more->addAction(QString::fromLatin1(f.label), this,
                            [this, extension] { chooseRareFormat(extension); });
            continue;
        }
        names << QString::fromLatin1(f.label);
        extensions << extension;
    }
    if (!more->isEmpty()) {
        names << tr("More");
    }
    const auto checked = static_cast<int>(std::max<qsizetype>(0, extensions.indexOf(current)));
    // Across the row, each segment as wide as its name.
    layout->addWidget(segmentBox(names, checked, m_formats, row, SegmentFit::Row));
    for (qsizetype i = 0; i < extensions.size(); ++i) {
        m_formats->button(static_cast<int>(i))->setProperty("extension", extensions.at(i));
    }
    if (!more->isEmpty()) {
        m_more = qobject_cast<QPushButton*>(m_formats->button(static_cast<int>(extensions.size())));
        // Opened by hand: a button with its own menu draws its name as if pressed.
        connect(m_more, &QPushButton::clicked, this,
                [this, more] { more->popup(m_more->mapToGlobal(QPoint(0, m_more->height()))); });
        connect(more, &QMenu::aboutToHide, this, [this] {
            // After the choice, if any: closed without one, the format shown before stays.
            QTimer::singleShot(0, this, [this] {
                if (m_more->property("extension").toString().isEmpty() && m_shownFormat != nullptr) {
                    m_shownFormat->setChecked(true);
                }
            });
        });
        if (formatFor(current)->rare && current == QLatin1StringView(formatFor(current)->extension)) {
            showRareFormat(current);
        }
    }
    if (m_formats->checkedButton() == nullptr && !m_formats->buttons().isEmpty()) {
        m_formats->buttons().first()->setChecked(true);
    }
    connect(m_formats, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        if (button == m_more && m_more->property("extension").toString().isEmpty()) {
            return; // the menu is open; a choice in it decides
        }
        updateForFormat();
    });
    return row;
}

void ExportSheet::showRareFormat(const QString& extension)
{
    m_more->setText(QString::fromLatin1(formatFor(extension)->label));
    m_more->setProperty("extension", extension);
    m_more->setChecked(true);
}

void ExportSheet::chooseRareFormat(const QString& extension)
{
    showRareFormat(extension);
    updateForFormat();
}

void ExportSheet::updateForFormat()
{
    m_shownFormat = m_formats->checkedButton();
    const QString ext = m_shownFormat->property("extension").toString();
    const Format* f = formatFor(ext);
    m_qualityRow->setEnabled(f->lossy);
    m_sixteen->setVisible(f->deep);
    m_extension->setText(u"."_s + ext);
    scheduleEstimate();
}

void ExportSheet::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Save to"), m_folderPath);
    if (!folder.isEmpty()) {
        m_folderPath = folder;
        m_folder->setPath(folder);
    }
}

void ExportSheet::setPhotoCount(int count)
{
    if (count < 2) {
        return;
    }
    m_photoCount = count;
    m_copyButton->setVisible(count <= 1); // one picture fits on the clipboard
    const QString text = tr("Export %n photos", nullptr, count);
    m_title->setText(text);
    m_exportButton->setText(text);
    m_file->setText(tr("each with its own settings; the size and name apply to all"));
}

QWidget* ExportSheet::metadataRow(const core::ExportOptions& options)
{
    m_metadata = new QCheckBox(tr("Keep camera details and copyright"), this);
    m_metadata->setChecked(options.keepMetadata);
    m_location = new QCheckBox(tr("Keep location"), this);
    m_location->setChecked(options.keepLocation);
    m_location->setToolTip(tr("Where the photo was taken (GPS). Off unless you choose to share it."));
    m_location->setEnabled(options.keepMetadata);
    connect(m_metadata, &QCheckBox::toggled, m_location, &QWidget::setEnabled);
    auto* box = new QWidget(this);
    auto* metadata = new QVBoxLayout(box);
    metadata->setContentsMargins(0, 0, 0, 0);
    metadata->setSpacing(kGapSmall);
    metadata->addWidget(m_metadata);
    metadata->addWidget(m_location);
    return box;
}

QWidget* ExportSheet::watermarkRow(const QString& current)
{
    m_watermark = new WatermarkPicker(current, this);
    return m_watermark;
}

void ExportSheet::setPreview(const QImage& preview)
{
    m_preview = preview;
    m_watermark->setPreview(preview);
    m_picker->setPicture(preview);
    scheduleEstimate();
}

QFrame* sharpenSegments(pfx::OutputSharpen sharpen, QButtonGroup* group, QWidget* parent)
{
    using S = ExportSheet;
    QFrame* box = segmentBox({S::tr("None"), S::tr("Screen"), S::tr("Print")}, static_cast<int>(sharpen),
                             group, parent, SegmentFit::Row);
    group->button(static_cast<int>(pfx::OutputSharpen::Screen))
        ->setToolTip(S::tr("A little crisper on screens, after resizing"));
    group->button(static_cast<int>(pfx::OutputSharpen::Print))
        ->setToolTip(S::tr("Crisper on paper, where ink softens detail"));
    return box;
}

QComboBox* colorSpaceList(pfx::OutputSpace space, QWidget* parent)
{
    using S = ExportSheet;
    auto* list = new QComboBox(parent);
    list->setAccessibleName(S::tr("Color space"));
    list->addItem(S::tr("sRGB (recommended for sharing)"), static_cast<int>(pfx::OutputSpace::Srgb));
    list->addItem(S::tr("Display P3"), static_cast<int>(pfx::OutputSpace::DisplayP3));
    list->addItem(S::tr("Adobe RGB"), static_cast<int>(pfx::OutputSpace::AdobeRgb));
    list->addItem(S::tr("Same as original"), static_cast<int>(pfx::OutputSpace::Original));
    list->setCurrentIndex(std::max(0, list->findData(static_cast<int>(space))));
    list->setToolTip(S::tr("Display P3 and Adobe RGB keep more saturated colours, for screens and "
                           "printers that show them; sRGB looks right everywhere"));
    return list;
}

QWidget* ExportSheet::sharpenRow(pfx::OutputSharpen sharpen)
{
    m_sharpen = new QButtonGroup(this);
    QFrame* box = sharpenSegments(sharpen, m_sharpen, this);
    connect(m_sharpen, &QButtonGroup::idClicked, this, &ExportSheet::scheduleEstimate);
    return box;
}

QWidget* ExportSheet::colorSpaceBox(pfx::OutputSpace space)
{
    m_colorSpace = colorSpaceList(space, this);
    connect(m_colorSpace, &QComboBox::currentIndexChanged, this, &ExportSheet::scheduleEstimate);
    return m_colorSpace;
}

void ExportSheet::scheduleEstimate()
{
    if (m_estimateTimer != nullptr && !m_preview.isNull()) {
        m_estimateTimer->start();
    }
}

void ExportSheet::estimate()
{
    const int ticket = ++m_estimates;
    auto* watcher = new QFutureWatcher<qint64>(this);
    connect(watcher, &QFutureWatcher<qint64>::finished, this, [this, watcher, ticket] {
        watcher->deleteLater();
        if (ticket == m_estimates) {
            showEstimate(watcher->result());
        }
    });
    watcher->setFuture(QtConcurrent::run(&services::estimateExportSize, m_preview, m_photoSize, options()));
}

void ExportSheet::showEstimate(qint64 bytes)
{
    const QString remembered = tr("Saved formats and choices are remembered.");
    if (bytes <= 0) {
        m_note->setText(remembered);
        return;
    }
    const QString size = core::sizeText(bytes);
    //: The estimated size of the file Export writes, with one photo or with several.
    const QString about =
        m_photoCount > 1 ? tr("About %1 for this photo.").arg(size) : tr("About %1.").arg(size);
    m_note->setText(about + u' ' + remembered);
}

void ExportSheet::setWatermarkFolder(const QString& folder)
{
    m_watermark->setFolder(folder);
}

core::ExportOptions ExportSheet::options() const
{
    core::ExportOptions o;
    o.extension = m_formats->checkedButton()->property("extension").toString();
    o.quality = m_quality->ticks();
    o.bitsPerSample = (formatFor(o.extension)->deep && m_sixteen->isChecked()) ? 16 : 8;
    const core::ExportOptions size = sizeOptions();
    o.longEdge = size.longEdge;
    o.sizePreset = size.sizePreset;
    o.customSize = size.customSize;
    o.dpi = size.dpi;
    o.fitInside = size.fitInside;
    o.frameX = size.frameX;
    o.frameY = size.frameY;
    o.sharpen = static_cast<pfx::OutputSharpen>(std::max(0, m_sharpen->checkedId()));
    o.colorSpace = static_cast<pfx::OutputSpace>(m_colorSpace->currentData().toInt());
    o.keepMetadata = m_metadata->isChecked();
    o.keepLocation = m_location->isChecked();
    o.watermark = m_watermark->path();
    o.folder = m_folderPath;
    o.nameTemplate = m_name->text();
    o.ifExists = static_cast<core::ExportOptions::IfExists>(m_ifExists->currentData().toInt());
    return o;
}

} // namespace pixelfx::ui
