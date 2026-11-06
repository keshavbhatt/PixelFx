#include "ui/compress_panel.h"

#include "ui/elided_label.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/name_field.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kFormatColumn = 52; // the JPEG, PNG and Others labels
constexpr int kSliderIndent = kFormatColumn + 8;

QLabel* formatLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(u"fieldLabel"_s);
    label->setFixedWidth(kFormatColumn);
    return label;
}

SliderRow* indentedSlider(const QString& name, int minimum, int maximum, int defaultTicks, QWidget* parent)
{
    auto* row = new SliderRow(name, parent);
    row->setRange(minimum, maximum, defaultTicks);
    row->setContentsMargins(kSliderIndent, 0, 0, 0);
    return row;
}

} // namespace

CompressPanel::CompressPanel(core::CompressJob job, QWidget* parent)
    : QWidget(parent)
    , m_job(std::move(job))
{
    auto* content = new QWidget(this);
    content->setObjectName(u"batchPanelContent"_s);
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(kPanelPadding, 0, kPanelPadding, 12);
    sections->setSpacing(kGapRelated);
    sections->addWidget(buildCompression(content));
    sections->addWidget(buildSave(content));
    sections->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(scroll, 1);
    column->addWidget(buildFoot(this));
    showJob();
}

CompressPanel::~CompressPanel() = default;

QWidget* CompressPanel::buildCompression(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapControls);
    column->addWidget(sectionHeading(tr("2. Compression"), box));

    m_jpeg = new QButtonGroup(this);
    auto* jpegRow = new QHBoxLayout;
    jpegRow->setSpacing(kGapItems);
    jpegRow->addWidget(formatLabel(tr("JPEG"), box));
    jpegRow->addWidget(segmentBox({tr("Lossless"), tr("Target quality")}, 0, m_jpeg, box), 1);
    m_jpegQuality = indentedSlider(tr("JPEG quality"), 1, 100, pfx::CompressOptions().jpegQuality, box);
    m_jpegQuality->setToolTip(tr("JPEGs are encoded again at this quality when that makes them smaller"));

    m_png = new QButtonGroup(this);
    auto* pngRow = new QHBoxLayout;
    pngRow->setSpacing(kGapItems);
    pngRow->addWidget(formatLabel(tr("PNG"), box));
    pngRow->addWidget(segmentBox({tr("Lossless"), tr("Fewer colours")}, 0, m_png, box), 1);
    m_pngQuality = indentedSlider(tr("PNG quality"), 40, 100, pfx::CompressOptions().pngQuality, box);
    m_pngQuality->setToolTip(tr("The lowest quality a palette of 256 colours may have; "
                                "below it the PNG stays lossless"));

    auto* othersRow = new QHBoxLayout;
    othersRow->setSpacing(kGapItems);
    othersRow->addWidget(formatLabel(tr("Others"), box), 0, Qt::AlignTop);
    auto* others = new QLabel(tr("WebP, GIF and TIFF made smaller without loss when they can be"), box);
    others->setObjectName(u"itemDetail"_s);
    others->setWordWrap(true);
    othersRow->addWidget(others, 1);

    m_extras = new QCheckBox(tr("Remove location and embedded previews"), box);
    auto* extrasNote = new QLabel(tr("Camera details and copyright stay"), box);
    extrasNote->setObjectName(u"itemDetail"_s);
    extrasNote->setContentsMargins(26, 0, 0, 0);

    column->addLayout(jpegRow);
    column->addWidget(m_jpegQuality);
    column->addLayout(pngRow);
    column->addWidget(m_pngQuality);
    column->addLayout(othersRow);
    column->addWidget(m_extras);
    column->addWidget(extrasNote);

    connect(m_jpeg, &QButtonGroup::idClicked, this, &CompressPanel::edited);
    connect(m_png, &QButtonGroup::idClicked, this, &CompressPanel::edited);
    connect(m_jpegQuality, &SliderRow::ticksReleased, this, &CompressPanel::edited);
    connect(m_pngQuality, &SliderRow::ticksReleased, this, &CompressPanel::edited);
    connect(m_extras, &QCheckBox::toggled, this, &CompressPanel::edited);
    return box;
}

QWidget* CompressPanel::buildSave(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(kGapControls);
    column->addWidget(sectionHeading(tr("3. Save"), box));
    m_where = new QButtonGroup(this);
    column->addWidget(
        segmentBox({tr("Into a folder"), tr("Replace the originals")}, 0, m_where, box, SegmentFit::Equal));

    m_folderRow = new QWidget(box);
    m_folder = new PathLabel({}, m_folderRow);
    m_folder->setObjectName(u"mono"_s);
    auto* change = new QPushButton(tr("Change"), m_folderRow);
    change->setObjectName(u"small"_s);
    connect(change, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Save to"), m_job.folder);
        if (!folder.isEmpty()) {
            m_job.folder = folder;
            m_folder->setPath(folder);
            Q_EMIT jobChanged(m_job);
        }
    });
    m_name = new NameField(m_folderRow);
    // Compress only keeps each file's size and has no recipe.
    m_name->setTokens({u"{name}"_s, u"{n}"_s, u"{taken}"_s, u"{date}"_s, u"{camera}"_s});
    connect(m_name, &NameField::edited, this, &CompressPanel::edited);
    auto* folderLine = new QHBoxLayout;
    folderLine->setSpacing(kGapRelated);
    folderLine->addWidget(m_folder, 1);
    folderLine->addWidget(change);
    auto* folderColumn = new QVBoxLayout(m_folderRow);
    folderColumn->setContentsMargins(0, 0, 0, 0);
    folderColumn->setSpacing(kGapControls);
    folderColumn->addLayout(folderLine);
    folderColumn->addWidget(m_name);

    m_keep = new QCheckBox(tr("Keep the original if the result is not smaller"), box);
    column->addWidget(m_folderRow);
    column->addWidget(m_keep);
    connect(m_where, &QButtonGroup::idClicked, this, &CompressPanel::edited);
    connect(m_keep, &QCheckBox::toggled, this, &CompressPanel::edited);
    return box;
}

QWidget* CompressPanel::buildFoot(QWidget* parent)
{
    const FootButtons buttons = footButtons(parent, tr("Compress"));
    m_compress = buttons.primary;
    m_compress->setIcon(icons::themed(u"play"_s, Qt::white));
    connect(m_compress, &QPushButton::clicked, this, &CompressPanel::compressRequested);
    return footBar(parent, buttons.row, kPanelPadding);
}

void CompressPanel::setJob(const core::CompressJob& job)
{
    m_job = job;
    showJob();
}

void CompressPanel::showJob()
{
    const QSignalBlocker a(m_extras);
    const QSignalBlocker b(m_keep);
    m_jpeg->button(m_job.codec.jpeg == pfx::JpegCompression::Quality ? 1 : 0)->setChecked(true);
    m_png->button(m_job.codec.png == pfx::PngCompression::FewerColours ? 1 : 0)->setChecked(true);
    m_where->button(m_job.replaceOriginals ? 1 : 0)->setChecked(true);
    m_jpegQuality->setTicks(m_job.codec.jpegQuality);
    m_pngQuality->setTicks(m_job.codec.pngQuality);
    m_jpegQuality->setVisible(m_job.codec.jpeg == pfx::JpegCompression::Quality);
    m_pngQuality->setVisible(m_job.codec.png == pfx::PngCompression::FewerColours);
    m_extras->setChecked(m_job.codec.removeExtras);
    m_folder->setPath(m_job.folder);
    m_name->setTemplate(m_job.nameTemplate);
    m_folderRow->setVisible(!m_job.replaceOriginals);
    m_keep->setChecked(m_job.keepIfNotSmaller);
}

void CompressPanel::edited()
{
    m_job.codec.jpeg =
        m_jpeg->checkedId() == 1 ? pfx::JpegCompression::Quality : pfx::JpegCompression::Lossless;
    m_job.codec.png =
        m_png->checkedId() == 1 ? pfx::PngCompression::FewerColours : pfx::PngCompression::Lossless;
    m_job.codec.jpegQuality = m_jpegQuality->ticks();
    m_job.codec.pngQuality = m_pngQuality->ticks();
    m_job.codec.removeExtras = m_extras->isChecked();
    m_job.replaceOriginals = m_where->checkedId() == 1;
    m_job.nameTemplate = m_name->nameTemplate().isEmpty() ? u"{name}"_s : m_name->nameTemplate();
    m_job.keepIfNotSmaller = m_keep->isChecked();
    showJob();
    Q_EMIT jobChanged(m_job);
}

void CompressPanel::setFiles(const QStringList& files)
{
    const QDate today = QDate::currentDate();
    m_name->setFiles(files, [this, today](const QString& nameTemplate, const QString& source,
                                          const core::NameFacts& facts) {
        core::CompressJob job = m_job;
        job.nameTemplate = nameTemplate;
        return core::compressFileName(source, job, today, facts);
    });
}

void CompressPanel::setFileCount(int count)
{
    m_count = count;
    refreshButton();
}

void CompressPanel::setState(bool running, bool ranBefore)
{
    m_ranBefore = ranBefore;
    refreshButton();
    m_compress->setEnabled(!running && m_count > 0);
}

void CompressPanel::refreshButton()
{
    if (m_ranBefore) {
        m_compress->setText(tr("Compress again"));
        m_compress->setToolTip(tr("Compress these photos again, replacing what the last run saved"));
    } else {
        m_compress->setText(m_count > 0 ? tr("Compress %n photos", nullptr, m_count) : tr("Compress"));
        m_compress->setToolTip(QString());
    }
    m_compress->setEnabled(m_count > 0);
}

} // namespace pixelfx::ui
