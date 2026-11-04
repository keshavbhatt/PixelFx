#include "ui/export_sheet.h"

#include "ui/style.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
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
};

constexpr Format kFormats[] = {
    {"jpg", "JPEG", true, false},  {"png", "PNG", false, true},  {"webp", "WebP", true, false},
    {"avif", "AVIF", true, false}, {"tif", "TIFF", false, true}, {"jxl", "JPEG XL", true, false},
    {"heic", "HEIC", true, false},
};

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
    : QDialog(parent)
    , m_source(source)
    , m_folderPath(options.folder)
{
    setWindowTitle(tr("Export"));
    setModal(true);
    setMinimumWidth(560);

    auto* title = new QLabel(tr("Export"), this);
    title->setObjectName(u"sheetTitle"_s);
    auto* file = new QLabel(QFileInfo(source).fileName(), this);
    file->setObjectName(u"muted"_s);
    auto* heading = new QHBoxLayout;
    heading->addWidget(title);
    heading->addSpacing(8);
    heading->addWidget(file);
    heading->addStretch();

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Format"), formatRow(options.extension));

    form->addRow(tr("Quality"), qualityRow(options.quality));

    m_sixteen = new QCheckBox(tr("16 bits per channel (larger file, more room for later edits)"), this);
    m_sixteen->setChecked(options.bitsPerSample == 16);
    form->addRow(QString(), m_sixteen);

    form->addRow(tr("Size"), sizeRow(photoSize, options.longEdge));

    m_metadata = new QCheckBox(tr("Keep camera details and copyright"), this);
    m_metadata->setChecked(options.keepMetadata);
    form->addRow(tr("Metadata"), m_metadata);

    form->addRow(tr("Save to"), folderRow());

    form->addRow(tr("File name"), nameRow(options.nameTemplate));

    form->addRow(tr("If it exists"), ifExistsBox(options.ifExists));

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(24, 20, 24, 18);
    column->setSpacing(14);
    column->addLayout(heading);
    column->addLayout(form);
    addFooter(column);
    updateForFormat();
}

ExportSheet::~ExportSheet() = default;

void ExportSheet::addFooter(QVBoxLayout* column)
{
    auto* note = new QLabel(tr("The original photo is never changed. Your choices are remembered."), this);
    note->setObjectName(u"faint"_s);
    note->setWordWrap(true);

    auto* cancel = new QPushButton(tr("Cancel"), this);
    auto* exportButton = new QPushButton(tr("Export"), this);
    exportButton->setObjectName(u"primary"_s);
    exportButton->setDefault(true);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(exportButton, &QPushButton::clicked, this, &QDialog::accept);
    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(exportButton);
    column->addWidget(note);
    column->addLayout(buttons);
}

QWidget* ExportSheet::qualityRow(int quality)
{
    m_qualityRow = new QWidget(this);
    m_quality = new QSlider(Qt::Horizontal, m_qualityRow);
    m_quality->setRange(1, 100);
    m_quality->setValue(quality);
    m_quality->setAccessibleName(tr("Quality"));
    m_qualityValue = new QLabel(m_qualityRow);
    m_qualityValue->setMinimumWidth(28);
    m_qualityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* qualityLayout = new QHBoxLayout(m_qualityRow);
    qualityLayout->setContentsMargins(0, 0, 0, 0);
    auto* smaller = new QLabel(tr("Smaller file"), m_qualityRow);
    smaller->setObjectName(u"faint"_s);
    auto* better = new QLabel(tr("Better image"), m_qualityRow);
    better->setObjectName(u"faint"_s);
    qualityLayout->addWidget(smaller);
    qualityLayout->addWidget(m_quality, 1);
    qualityLayout->addWidget(better);
    qualityLayout->addWidget(m_qualityValue);
    connect(m_quality, &QSlider::valueChanged, m_qualityValue, qOverload<int>(&QLabel::setNum));
    m_qualityValue->setNum(quality);
    return m_qualityRow;
}

QWidget* ExportSheet::sizeRow(const QSize& photoSize, int longEdge)
{
    auto* row = new QWidget(this);
    m_size = new QComboBox(row);
    m_size->addItem(tr("Original (%1 x %2)").arg(photoSize.width()).arg(photoSize.height()));
    m_size->addItem(tr("Long edge"));
    m_longEdge = new QSpinBox(row);
    m_longEdge->setRange(16, 65535);
    m_longEdge->setSuffix(tr(" px"));
    m_longEdge->setValue(longEdge > 0 ? longEdge : 2048);
    m_size->setCurrentIndex(longEdge > 0 ? 1 : 0);
    m_longEdge->setEnabled(longEdge > 0);
    connect(m_size, &QComboBox::currentIndexChanged, m_longEdge,
            [this](int i) { m_longEdge->setEnabled(i == 1); });
    auto* sizeLayout = new QHBoxLayout(row);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    sizeLayout->addWidget(m_size, 1);
    sizeLayout->addWidget(m_longEdge);
    return row;
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
    const auto writable = pfx::writableExtensions();
    for (const Format& f : kFormats) {
        const std::string ext = std::string(".") + f.extension;
        if (std::ranges::find(writable, ext) == writable.end()) {
            continue; // this build cannot write it (AVIF, HEIC and JPEG XL depend on libvips)
        }
        auto* button = new QPushButton(QString::fromLatin1(f.label), row);
        button->setCheckable(true);
        button->setObjectName(u"segment"_s);
        button->setProperty("extension", QString::fromLatin1(f.extension));
        button->setChecked(current == QLatin1StringView(f.extension));
        m_formats->addButton(button);
        layout->addWidget(button);
    }
    if (m_formats->checkedButton() == nullptr && !m_formats->buttons().isEmpty()) {
        m_formats->buttons().first()->setChecked(true);
    }
    layout->addStretch();
    connect(m_formats, &QButtonGroup::buttonClicked, this, &ExportSheet::updateForFormat);
    return row;
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

core::ExportOptions ExportSheet::options() const
{
    core::ExportOptions o;
    o.extension = m_formats->checkedButton()->property("extension").toString();
    o.quality = m_quality->value();
    o.bitsPerSample = (formatFor(o.extension)->deep && m_sixteen->isChecked()) ? 16 : 8;
    o.longEdge = m_size->currentIndex() == 1 ? m_longEdge->value() : 0;
    o.keepMetadata = m_metadata->isChecked();
    o.folder = m_folderPath;
    o.nameTemplate = m_name->text();
    o.ifExists = static_cast<core::ExportOptions::IfExists>(m_ifExists->currentData().toInt());
    return o;
}

} // namespace pixelfx::ui
