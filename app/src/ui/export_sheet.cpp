#include "ui/export_sheet.h"

#include "ui/panel_slider.h"
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
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

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

const Format* formatFor(const QString& extension)
{
    for (const Format& f : kFormats) {
        if (extension == QLatin1StringView(f.extension)) {
            return &f;
        }
    }
    return &kFormats[0];
}

QString shortPath(const QString& path)
{
    if (path.isEmpty()) {
        return QObject::tr("Next to the photo");
    }
    const QString home = QDir::homePath();
    return path.startsWith(home) ? u"~"_s + path.mid(home.size()) : path;
}

} // namespace

ExportSheet::ExportSheet(const QString& source, const QSize& photoSize, const core::ExportOptions& options,
                         QWidget* parent)
    : Sheet(parent)
    , m_source(source)
    , m_folderPath(options.folder)
{
    setWindowTitle(tr("Export"));
    setModal(true);

    m_file = new QLabel(QFileInfo(source).fileName(), this);
    m_file->setObjectName(u"muted"_s);
    QHBoxLayout* heading = sheetTitleRow(this, tr("Export"), {m_file});
    m_title = findChild<QLabel*>(u"sheetTitle"_s);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Format"), formatRow(options.extension));

    form->addRow(tr("Quality"), qualityRow(options.quality));

    m_sixteen = new QCheckBox(tr("16 bits per channel (larger file, more room for later edits)"), this);
    m_sixteen->setChecked(options.bitsPerSample == 16);
    form->addRow(QString(), m_sixteen);

    form->addRow(tr("Size"), sizeRow(photoSize, options));
    form->addRow(QString(), fitRow(options.fitInside));
    refreshSize(photoSize);

    form->addRow(tr("Watermark"), watermarkRow(options.watermark));

    form->addRow(tr("Metadata"), metadataRow(options));

    auto* divider = new QFrame(this);
    divider->setObjectName(u"divider"_s);
    divider->setFixedHeight(1);
    form->addRow(divider);

    form->addRow(tr("Save to"), folderRow());

    form->addRow(tr("File name"), nameRow(options.nameTemplate));

    form->addRow(tr("If it exists"), ifExistsBox(options.ifExists));

    QHBoxLayout* foot = addFooter(form);
    setLabelColumn(form, kLabelColumn);
    QVBoxLayout* column = sheetLayout(this, foot);
    column->setSpacing(14);
    column->addLayout(heading);
    column->addLayout(form);
    updateForFormat();
    setSize(Size::Regular);
}

ExportSheet::~ExportSheet() = default;

QHBoxLayout* ExportSheet::addFooter(QFormLayout* form)
{
    auto* note = new QLabel(tr("The original photo is never changed. Your choices are remembered."), this);
    note->setObjectName(u"faint"_s);
    note->setWordWrap(true);

    auto* cancel = new QPushButton(tr("Cancel"), this);
    auto* exportButton = new QPushButton(tr("Export"), this);
    m_exportButton = exportButton;
    exportButton->setObjectName(u"primary"_s);
    exportButton->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, &QDialog::accept);
    m_copyButton = new QPushButton(tr("Copy to clipboard"), this);
    m_copyButton->setToolTip(tr("Copy the photo at this size, with the watermark, instead of saving a file"));
    connect(m_copyButton, &QPushButton::clicked, this, [this] { done(kCopyToClipboard); });
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_copyButton);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(exportButton);
    form->addRow(note);
    return buttons;
}

QWidget* ExportSheet::qualityRow(int quality)
{
    m_qualityRow = new QWidget(this);
    m_quality = new PanelSlider(Qt::Horizontal, m_qualityRow);
    m_quality->setRange(1, 100);
    m_quality->setValue(quality);
    m_quality->setAccessibleName(tr("Quality"));
    m_qualityValue = new QLabel(m_qualityRow);
    m_qualityValue->setObjectName(u"adjustValue"_s);
    m_qualityValue->setProperty("changed", true);
    m_qualityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* scale = new QLabel(tr("Smaller file \u00B7 Better image"), m_qualityRow);
    scale->setObjectName(u"small"_s);
    auto* top = new QHBoxLayout;
    top->addWidget(scale, 1);
    top->addWidget(m_qualityValue);
    auto* qualityLayout = new QVBoxLayout(m_qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    qualityLayout->setSpacing(2);
    qualityLayout->addLayout(top);
    qualityLayout->addWidget(m_quality);
    connect(m_quality, &QSlider::valueChanged, m_qualityValue, qOverload<int>(&QLabel::setNum));
    m_qualityValue->setNum(quality);
    return m_qualityRow;
}

QWidget* ExportSheet::sizeRow(const QSize& photoSize, const core::ExportOptions& options)
{
    auto* row = new QWidget(this);
    m_size = new QComboBox(row);
    m_size->setObjectName(u"sizeList"_s);
    m_size->setAccessibleName(tr("Size"));
    m_size->addItem(tr("Original \u00B7 %1 x %2").arg(photoSize.width()).arg(photoSize.height()), QString());
    QString group;
    for (const core::SizePreset& p : core::sizePresets()) {
        const QString g = QCoreApplication::translate("SizePresets", p.group);
        if (g != group) {
            group = g;
            m_size->insertSeparator(m_size->count());
            m_size->addItem(g);
            const auto* model = qobject_cast<const QStandardItemModel*>(m_size->model());
            if (model != nullptr) {
                model->item(m_size->count() - 1)->setEnabled(false); // a heading, not a choice
            }
        }
        const QSize exact = core::exactSize(p, photoSize);
        const QString pixels = exact.isValid() ? tr("%1 x %2").arg(exact.width()).arg(exact.height())
                                               : tr("%1 long edge").arg(p.longEdge);
        m_size->addItem(u"%1 \u00B7 %2"_s.arg(QCoreApplication::translate("SizePresets", p.name), pixels),
                        QString::fromLatin1(p.id));
    }
    m_size->insertSeparator(m_size->count());
    m_size->addItem(QString(), u"custom"_s);
    setLongEdge(options.longEdge > 0 ? options.longEdge : 2048);
    int index = 0;
    if (!options.sizePreset.isEmpty()) {
        index = std::max(0, m_size->findData(options.sizePreset));
    } else if (options.longEdge > 0) {
        index = m_size->findData(u"custom"_s);
    }
    m_size->setCurrentIndex(index);
    m_sizeIndex = index;
    auto* sizeLayout = new QHBoxLayout(row);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->addWidget(m_size, 1);
    connect(m_size, &QComboBox::activated, this, [this](int chosen) {
        if (m_size->itemData(chosen).toString() == u"custom"_s && !askLongEdge()) {
            m_size->setCurrentIndex(m_sizeIndex);
        }
        m_sizeIndex = m_size->currentIndex();
    });
    connect(m_size, &QComboBox::currentIndexChanged, this, [this, photoSize] { refreshSize(photoSize); });
    return row;
}

void ExportSheet::setLongEdge(int pixels)
{
    m_longEdge = pixels;
    m_size->setItemText(m_size->findData(u"custom"_s), tr("Long edge \u00B7 %1 px").arg(pixels));
}

bool ExportSheet::askLongEdge()
{
    Sheet sheet(this);
    sheet.setWindowTitle(tr("Custom size"));
    auto* edge = new QSpinBox(&sheet);
    edge->setRange(16, 65535);
    edge->setSuffix(tr(" px"));
    edge->setValue(m_longEdge);
    edge->setAccessibleName(tr("Long edge"));
    auto* cancel = new QPushButton(tr("Cancel"), &sheet);
    auto* use = new QPushButton(tr("Use size"), &sheet);
    use->setObjectName(u"primary"_s);
    use->setDefault(true);
    connect(cancel, &QPushButton::clicked, &sheet, &QDialog::reject);
    connect(use, &QPushButton::clicked, &sheet, &QDialog::accept);
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    foot->addWidget(cancel);
    foot->addWidget(use);
    QVBoxLayout* column = sheetLayout(&sheet, foot);
    auto* form = new QFormLayout;
    form->addRow(tr("Long edge"), edge);
    column->addLayout(sheetTitleRow(&sheet, tr("Custom size")));
    column->addWidget(new QLabel(tr("The longer side of the photo; the other follows its shape."), &sheet));
    column->addLayout(form);
    sheet.setSize(Size::Narrow);
    edge->setFocus();
    edge->selectAll();
    if (sheet.exec() != QDialog::Accepted) {
        return false;
    }
    setLongEdge(edge->value());
    return true;
}

QWidget* ExportSheet::fitRow(bool fitInside)
{
    m_fitRow = new QWidget(this);
    m_fitNote = new QLabel(m_fitRow);
    m_fitNote->setObjectName(u"faint"_s);
    m_fill = new QPushButton(tr("Crop to fill"), m_fitRow);
    m_fit = new QPushButton(tr("Fit inside"), m_fitRow);
    m_fill->setToolTip(tr("Keep the middle of the photo, cropped to the shape"));
    m_fit->setToolTip(tr("Keep the whole photo and add a border"));
    auto* group = new QButtonGroup(m_fitRow);
    for (QPushButton* b : {m_fill, m_fit}) {
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        group->addButton(b);
    }
    (fitInside ? m_fit : m_fill)->setChecked(true);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(0);
    buttons->addWidget(m_fill);
    buttons->addWidget(m_fit);
    buttons->addStretch();
    auto* column = new QVBoxLayout(m_fitRow);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(4);
    column->addWidget(m_fitNote);
    column->addLayout(buttons);
    return m_fitRow;
}

void ExportSheet::refreshSize(const QSize& photoSize)
{
    const QString id = m_size->currentData().toString();
    const core::SizePreset* preset = core::sizePreset(id);
    const QSize exact = preset != nullptr ? core::exactSize(*preset, photoSize) : QSize();
    // Only when the shapes differ is there anything to choose.
    const bool differs = exact.isValid() && photoSize.height() > 0 &&
                         std::abs((static_cast<double>(exact.width()) / exact.height()) -
                                  (static_cast<double>(photoSize.width()) / photoSize.height())) > 0.01;
    m_fitRow->setVisible(differs);
    if (differs) {
        // The nearest common shape ("3:2"), as people name them; otherwise the number.
        const auto ratio = [](const QSize& s) {
            const double r = static_cast<double>(s.width()) / s.height();
            static const std::array<std::pair<int, int>, 8> common = {
                {{1, 1}, {5, 4}, {4, 3}, {3, 2}, {16, 10}, {16, 9}, {2, 1}, {21, 9}}};
            for (const auto& [a, b] : common) {
                if (std::abs(r - (static_cast<double>(a) / b)) < 0.02 * r) {
                    return u"%1:%2"_s.arg(a).arg(b);
                }
                if (std::abs(r - (static_cast<double>(b) / a)) < 0.02 * r) {
                    return u"%1:%2"_s.arg(b).arg(a);
                }
            }
            return u"%1:1"_s.arg(r, 0, 'f', 2);
        };
        m_fitNote->setText(tr("%1 is %2, this photo is %3")
                               .arg(QCoreApplication::translate("SizePresets", preset->name), ratio(exact),
                                    ratio(photoSize)));
    }
}

QWidget* ExportSheet::folderRow()
{
    auto* row = new QWidget(this);
    m_folder = new QLabel(shortPath(m_folderPath), row);
    m_folder->setObjectName(u"mono"_s);
    m_folder->setToolTip(m_folderPath);
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
    row->setObjectName(u"segmented"_s);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_formats = new QButtonGroup(this);
    auto* more = new QMenu(row);
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
        auto* button = new QPushButton(QString::fromLatin1(f.label), row);
        button->setCheckable(true);
        button->setObjectName(u"segment"_s);
        button->setProperty("extension", extension);
        button->setChecked(current == extension);
        m_formats->addButton(button);
        layout->addWidget(button, 1);
    }
    if (!more->isEmpty()) {
        m_more = new QPushButton(tr("More"), row);
        m_more->setObjectName(u"segment"_s);
        m_more->setCheckable(true);
        m_more->setMenu(more);
        m_formats->addButton(m_more);
        layout->addWidget(m_more, 1);
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
    const QString ext = m_formats->checkedButton()->property("extension").toString();
    const Format* f = formatFor(ext);
    m_qualityRow->setEnabled(f->lossy);
    m_sixteen->setVisible(f->deep);
    m_extension->setText(u"."_s + ext);
}

void ExportSheet::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Save to"), m_folderPath);
    if (!folder.isEmpty()) {
        m_folderPath = folder;
        m_folder->setText(shortPath(folder));
        m_folder->setToolTip(folder);
    }
}

void ExportSheet::setPhotoCount(int count)
{
    if (count < 2) {
        return;
    }
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
    metadata->setSpacing(4);
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
    m_watermark->setPreview(preview);
}

void ExportSheet::setWatermarkFolder(const QString& folder)
{
    m_watermark->setFolder(folder);
}

core::ExportOptions ExportSheet::options() const
{
    core::ExportOptions o;
    o.extension = m_formats->checkedButton()->property("extension").toString();
    o.quality = m_quality->value();
    o.bitsPerSample = (formatFor(o.extension)->deep && m_sixteen->isChecked()) ? 16 : 8;
    const QString size = m_size->currentData().toString();
    o.longEdge = size == u"custom"_s ? m_longEdge : 0;
    o.sizePreset = core::sizePreset(size) != nullptr ? size : QString();
    o.fitInside = m_fit->isChecked();
    o.keepMetadata = m_metadata->isChecked();
    o.keepLocation = m_location->isChecked();
    o.watermark = m_watermark->path();
    o.folder = m_folderPath;
    o.nameTemplate = m_name->text();
    o.ifExists = static_cast<core::ExportOptions::IfExists>(m_ifExists->currentData().toInt());
    return o;
}

} // namespace pixelfx::ui
