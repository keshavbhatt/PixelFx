#include "ui/batch_page.h"

#include "core/batch_resources.h"
#include "core/looks.h"
#include "core/sidecar_store.h"
#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/panel_slider.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <pfx/document.h>
#include <pfx/io.h>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

bool (*isPhotoTest)(const QString&) = nullptr;

constexpr int kPanelWidth = 380;
constexpr int kSide = 14;

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    QLabel* label = sectionLabel(text, parent);
    label->setContentsMargins(0, 12, 0, 4);
    return label;
}

QString describe(const QString& path)
{
    const auto info = pfx::probe(path.toStdString());
    const QString size = QLocale().formattedDataSize(QFileInfo(path).size(), 1, QLocale::DataSizeSIFormat);
    return info ? u"%1 x %2 · %3"_s.arg(info.value().width).arg(info.value().height).arg(size) : size;
}

// "~/Pictures/PixelFx/Batch": the home folder as people write it.
QString shortPath(const QString& path)
{
    const QString home = QDir::homePath();
    const QString native = QDir::toNativeSeparators(path);
    return path.startsWith(home) ? u"~"_s + QDir::toNativeSeparators(path.mid(home.size())) : native;
}

// The formats Save as offers, and whether they take a quality.
bool lossy(const QString& extension)
{
    return extension != u"png"_s && extension != u"tif"_s;
}

QString formatName(const QString& extension)
{
    if (extension == u"jpg"_s) {
        return u"JPEG"_s;
    }
    if (extension == u"jxl"_s) {
        return u"JPEG XL"_s;
    }
    if (extension == u"tif"_s) {
        return u"TIFF"_s;
    }
    if (extension == u"webp"_s) {
        return u"WebP"_s;
    }
    return extension.toUpper();
}

// A status pill: Saved, Waiting, Not saved, Saving.
void setPill(QWidget* pill, const QString& kind, const QString& icon, const QString& text)
{
    auto* glyph = pill->findChild<QLabel*>(u"pillIcon"_s);
    auto* label = pill->findChild<QLabel*>(u"pillText"_s);
    const Tokens& t = tokens();
    QColor colour = t.muted;
    if (kind == u"ok"_s) {
        colour = t.success;
    } else if (kind == u"bad"_s) {
        colour = t.danger;
    } else if (kind == u"accent"_s) {
        colour = t.accent;
    }
    glyph->setVisible(!icon.isEmpty());
    if (!icon.isEmpty()) {
        glyph->setPixmap(icons::pixmap(icon, colour, 12, pill->devicePixelRatioF()));
    }
    label->setText(text);
    pill->setProperty("kind", kind);
    pill->style()->unpolish(pill);
    pill->style()->polish(pill);
    label->style()->unpolish(label);
    label->style()->polish(label);
    pill->setVisible(!text.isEmpty());
}

QWidget* makePill(QWidget* parent)
{
    auto* pill = new QFrame(parent);
    pill->setObjectName(u"pill"_s);
    pill->setFixedHeight(20);
    auto* glyph = new QLabel(pill);
    glyph->setObjectName(u"pillIcon"_s);
    auto* text = new QLabel(pill);
    text->setObjectName(u"pillText"_s);
    auto* row = new QHBoxLayout(pill);
    row->setContentsMargins(8, 2, 8, 2);
    row->setSpacing(4);
    row->addWidget(glyph);
    row->addWidget(text);
    pill->hide();
    return pill;
}

} // namespace

void BatchPage::setPhotoTest(bool (*isPhoto)(const QString&))
{
    isPhotoTest = isPhoto;
}

BatchPage::BatchPage(core::ExportOptions output, QWidget* parent)
    : QWidget(parent)
    , m_output(std::move(output))
    , m_thumbnails(new services::Thumbnailer(this))
    , m_previewTimer(new QTimer(this))
{
    setObjectName(u"batchPage"_s);
    setAttribute(Qt::WA_StyledBackground);
    setAcceptDrops(true);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    row->addWidget(buildList(), 1);
    row->addWidget(buildPanel());
    connect(m_thumbnails, &services::Thumbnailer::ready, this, [this](const services::Thumbnail& t) {
        if (t.image.isNull()) {
            return;
        }
        for (QLabel* thumb : m_rowsBox->findChildren<QLabel*>(u"batchThumb"_s)) {
            if (thumb->property("path").toString() == t.path) {
                thumb->setPixmap(QPixmap::fromImage(t.image.scaled(thumb->size() * devicePixelRatioF(),
                                                                   Qt::KeepAspectRatioByExpanding,
                                                                   Qt::SmoothTransformation)));
            }
        }
        for (int i = 0; i < m_grid->count(); ++i) {
            if (m_grid->item(i)->data(Qt::UserRole).toString() == t.path) {
                m_grid->item(i)->setIcon(QPixmap::fromImage(t.image));
            }
        }
    });
    connect(m_thumbnails, &services::Thumbnailer::rendered, this, [this](int key, const QImage& image) {
        if (static_cast<quint64>(key / 2) != (m_previewGeneration % 100000) || image.isNull()) {
            return; // a picture for an older recipe
        }
        QLabel* target = key % 2 == 0 ? m_before : m_after;
        const QPixmap pixmap = QPixmap::fromImage(
            image.scaledToHeight(static_cast<int>(56 * devicePixelRatioF()), Qt::SmoothTransformation));
        target->setPixmap(pixmap);
        if (target == m_after) {
            m_watermark->setPreview(image); // the watermark sheet previews on the batch's own photo
            m_afterPixmap = pixmap;
            refreshRecipeCard(); // the card shows the recipe on the first photo
        }
    });
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(200);
    connect(m_previewTimer, &QTimer::timeout, this, &BatchPage::renderPreview);
    for (const QCheckBox* box : {m_auto, m_keepEdits, m_secondOn}) {
        connect(box, &QCheckBox::toggled, this, &BatchPage::schedulePreview);
    }
    connect(&m_runner, &services::BatchRunner::photoChanged, this, &BatchPage::refreshRow);
    connect(&m_runner, &services::BatchRunner::progress, this, &BatchPage::refreshStatus);
    connect(&m_runner, &services::BatchRunner::finished, this, &BatchPage::refreshStatus);
    rebuildRows();
    refreshRecipeCard();
    refreshStatus();
}

BatchPage::~BatchPage() = default;

// ---------------------------------------------------------------------------------------------
// The photos

QWidget* BatchPage::buildListHead(QWidget* parent)
{
    auto* head = new QWidget(parent);
    head->setObjectName(u"batchHead"_s);
    head->setAttribute(Qt::WA_StyledBackground);
    auto* title = new QLabel(tr("Photos"), head);
    title->setObjectName(u"recentTitle"_s);
    auto* hint = new QLabel(tr("Drop files or folders here"), head);
    hint->setObjectName(u"muted"_s);
    // List or grid.
    auto* views = new QFrame(head);
    views->setObjectName(u"segmentBox"_s);
    auto* viewRow = new QHBoxLayout(views);
    viewRow->setContentsMargins(0, 0, 0, 0);
    viewRow->setSpacing(0);
    m_viewModes = new QButtonGroup(this);
    const QStringList glyphs = {u"list"_s, u"layout-grid"_s};
    const QStringList names = {tr("List"), tr("Grid")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QToolButton(views);
        b->setObjectName(u"segmentIcon"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        b->setIcon(icons::themed(glyphs.at(i), tokens().muted));
        b->setToolTip(names.at(i));
        b->setAccessibleName(names.at(i));
        m_viewModes->addButton(b, i);
        viewRow->addWidget(b);
    }
    connect(m_viewModes, &QButtonGroup::idClicked, this, [this](int id) { m_views->setCurrentIndex(id); });
    auto* clear = new QPushButton(tr("Clear"), head);
    clear->setObjectName(u"small"_s);
    clear->setToolTip(tr("Take every photo off the list (the files stay where they are)"));
    connect(clear, &QPushButton::clicked, this, [this] {
        if (!m_runner.isRunning()) {
            m_files.clear();
            rebuildRows();
        }
    });
    auto* row = new QHBoxLayout(head);
    row->setContentsMargins(16, 10, 16, 10);
    row->setSpacing(8);
    row->addWidget(title);
    row->addWidget(hint);
    row->addStretch();
    row->addWidget(views);
    row->addWidget(clear);
    return head;
}

QWidget* BatchPage::buildList()
{
    auto* box = new QWidget(this);
    box->setObjectName(u"batchList"_s);
    box->setAttribute(Qt::WA_StyledBackground);
    m_rowsBox = new QWidget(box);
    m_rowsBox->setObjectName(u"batchRows"_s);
    m_rows = new QVBoxLayout(m_rowsBox);
    m_rows->setContentsMargins(16, 8, 16, 8);
    m_rows->setSpacing(0);
    auto* scroll = new QScrollArea(box);
    scroll->setWidget(m_rowsBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_grid = new QListWidget(box);
    m_grid->setObjectName(u"batchGrid"_s);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(QSize(150, 100));
    m_grid->setGridSize(QSize(172, 140));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSelectionMode(QAbstractItemView::NoSelection);
    m_views = new QStackedWidget(box);
    m_views->addWidget(scroll);
    m_views->addWidget(m_grid);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(buildListHead(box));
    column->addWidget(m_views, 1);
    column->addWidget(buildPreview(box));
    column->addWidget(buildStatus(box));
    return box;
}

QWidget* BatchPage::buildPreview(QWidget* parent)
{
    m_preview = new QWidget(parent);
    m_preview->setObjectName(u"batchBar"_s);
    m_preview->setAttribute(Qt::WA_StyledBackground);
    auto* previewLabel = new QLabel(tr("Preview"), m_preview);
    previewLabel->setObjectName(u"muted"_s);
    m_before = new QLabel(m_preview);
    m_after = new QLabel(m_preview);
    auto* arrow = new QLabel(m_preview);
    arrow->setPixmap(icons::pixmap(u"chevron-right"_s, tokens().muted, 15, devicePixelRatioF()));
    auto* caption = new QLabel(tr("The first photo, before and after the recipe"), m_preview);
    caption->setObjectName(u"muted"_s);
    auto* row = new QHBoxLayout(m_preview);
    row->setContentsMargins(16, 10, 16, 10);
    row->setSpacing(10);
    row->addWidget(previewLabel);
    row->addWidget(m_before);
    row->addWidget(arrow);
    row->addWidget(m_after);
    row->addWidget(caption, 1);
    m_preview->hide();
    return m_preview;
}

QWidget* BatchPage::buildStatus(QWidget* parent)
{
    // batch-running.html: "Saving 4 of 6, about 20 s left", Pause, Stop, a progress line;
    // batch-done.html: "5 saved, 1 not saved, in 38 s, 41 MB", Retry failed, Show in folder.
    m_status = new QWidget(parent);
    m_status->setObjectName(u"batchBar"_s);
    m_status->setAttribute(Qt::WA_StyledBackground);
    m_statusIcon = new QLabel(m_status);
    m_statusText = new QLabel(m_status);
    m_statusText->setObjectName(u"recentTitle"_s);
    m_statusDetail = new QLabel(m_status);
    m_statusDetail->setObjectName(u"muted"_s);
    m_pause = new IconTextButton(icons::themed(u"pause"_s, tokens().text), tr("Pause"), m_status);
    m_stop = new QPushButton(tr("Stop"), m_status);
    m_retry = new QPushButton(tr("Retry failed"), m_status);
    m_showFolder =
        new IconTextButton(icons::themed(u"folder"_s, tokens().text), tr("Show in folder"), m_status);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        m_runner.setPaused(!m_runner.isPaused());
        refreshStatus();
    });
    connect(m_stop, &QPushButton::clicked, this, [this] { m_runner.stop(); });
    connect(m_retry, &QPushButton::clicked, this, [this] {
        m_clock.start();
        m_runner.retryFailed();
    });
    connect(m_showFolder, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_runner.total(); ++i) {
            if (!m_runner.target(i).isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_runner.target(i)).absolutePath()));
                return;
            }
        }
    });
    m_progress = new QProgressBar(m_status);
    m_progress->setTextVisible(false);
    auto* line = new QHBoxLayout;
    line->setSpacing(8);
    line->addWidget(m_statusIcon);
    line->addWidget(m_statusText);
    line->addWidget(m_statusDetail);
    line->addStretch();
    for (QPushButton* b : {m_pause, m_stop, m_retry, m_showFolder}) {
        b->setObjectName(u"small"_s);
        line->addWidget(b);
    }
    auto* column = new QVBoxLayout(m_status);
    column->setContentsMargins(16, 10, 16, 12);
    column->setSpacing(8);
    column->addLayout(line);
    column->addWidget(m_progress);
    m_status->hide();
    return m_status;
}

// ---------------------------------------------------------------------------------------------
// The recipe and the output

QWidget* BatchPage::buildPanel()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(u"cropPanel"_s);
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setFixedWidth(kPanelWidth);
    auto* content = new QWidget(panel);
    content->setObjectName(u"batchPanelContent"_s);
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(kSide, 0, kSide, 12);
    sections->setSpacing(6);
    sections->addWidget(buildRecipe(content));
    sections->addWidget(buildResize(content));
    sections->addWidget(sectionTitle(tr("3. Watermark"), content));
    m_watermark = new WatermarkPicker(m_output.watermark, content);
    connect(m_watermark, &WatermarkPicker::changed, this, &BatchPage::outputEdited);
    sections->addWidget(m_watermark);
    sections->addWidget(buildSaveAs(content));
    sections->addStretch();
    auto* scroll = new QScrollArea(panel);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(scroll, 1);
    column->addWidget(buildFoot(panel));
    return panel;
}

QWidget* BatchPage::buildRecipe(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);
    column->addWidget(sectionTitle(tr("1. Recipe"), box));
    auto* sources = new QHBoxLayout;
    sources->setSpacing(0);
    m_sources = new QButtonGroup(this);
    const QStringList names = {tr("Preset or look"), tr("Settings from a photo")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(names.at(i), box);
        b->setObjectName(u"segment"_s);
        b->setCheckable(true);
        b->setChecked(i == 0);
        m_sources->addButton(b, i);
        sources->addWidget(b, 1);
    }
    connect(m_sources, &QButtonGroup::idClicked, this, [this](int id) { setFromPhoto(id == 1); });
    column->addLayout(sources);
    column->addWidget(buildRecipeCard(box));
    column->addLayout(buildSecondRow(box));
    m_auto = new QCheckBox(tr("Auto enhance each photo first"), box);
    m_keepEdits = new QCheckBox(tr("Keep edits already made in PixelFx"), box);
    m_keepEdits->setToolTip(tr("Start from each photo's remembered edits instead of the original"));
    column->addWidget(m_auto);
    column->addWidget(m_keepEdits);
    return box;
}

QWidget* BatchPage::buildRecipeCard(QWidget* box)
{
    auto* card = new QFrame(box);
    card->setObjectName(u"recipeCard"_s);
    m_cardThumb = new QLabel(card);
    m_cardThumb->setFixedSize(54, 36);
    m_cardThumb->setObjectName(u"cardThumb"_s);
    m_cardName = new QLabel(card);
    m_cardName->setObjectName(u"presetName"_s);
    m_cardDetail = new QLabel(card);
    m_cardDetail->setObjectName(u"presetSummary"_s);
    m_cardChange = new QPushButton(tr("Change"), card);
    m_cardChange->setObjectName(u"small"_s);
    connect(m_cardChange, &QPushButton::clicked, this, [this] { showRecipeMenu(m_cardChange, false); });
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(m_cardName);
    text->addWidget(m_cardDetail);
    auto* cardRow = new QHBoxLayout(card);
    cardRow->setContentsMargins(8, 8, 8, 8);
    cardRow->setSpacing(10);
    cardRow->addWidget(m_cardThumb);
    cardRow->addLayout(text, 1);
    cardRow->addWidget(m_cardChange);
    return card;
}

QHBoxLayout* BatchPage::buildSecondRow(QWidget* box)
{
    // "Then add preset Product white   Change"
    m_secondOn = new QCheckBox(tr("Then add preset"), box);
    m_secondName = new QLabel(box);
    m_secondName->setObjectName(u"presetName"_s);
    m_secondChange = new QPushButton(tr("Change"), box);
    m_secondChange->setObjectName(u"link"_s);
    connect(m_secondChange, &QPushButton::clicked, this, [this] { showRecipeMenu(m_secondChange, true); });
    connect(m_secondOn, &QCheckBox::toggled, this, [this](bool on) {
        if (on && m_second.isEmpty()) {
            showRecipeMenu(m_secondChange, true);
        }
        refreshRecipeCard();
    });
    auto* secondRow = new QHBoxLayout;
    secondRow->setSpacing(6);
    secondRow->addWidget(m_secondOn);
    secondRow->addWidget(m_secondName);
    secondRow->addStretch();
    secondRow->addWidget(m_secondChange);
    return secondRow;
}

QComboBox* BatchPage::buildSizes(QWidget* parent)
{
    auto* sizes = new QComboBox(parent);
    sizes->setAccessibleName(tr("Resize"));
    sizes->addItem(tr("Original size"), 0);
    for (const int edge : {3840, 2048, 1600, 1080}) {
        sizes->addItem(tr("%1 px long edge").arg(edge), edge);
    }
    // The Export sheet's named sizes too.
    for (const core::SizePreset& p : core::sizePresets()) {
        const QString size =
            p.longEdge > 0 ? tr("%1 long edge").arg(p.longEdge) : tr("%1 x %2").arg(p.width).arg(p.height);
        //: A size choice of Batch: where the photos go, then the size, "Web · 2048 long edge".
        sizes->addItem(tr("%1 · %2").arg(QCoreApplication::translate("SizePresets", p.name), size),
                       QString(u"preset:"_s + QString::fromLatin1(p.id)));
    }
    return sizes;
}

QWidget* BatchPage::buildResize(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);
    column->addWidget(sectionTitle(tr("2. Resize"), box));
    m_size = buildSizes(box);
    showSize(m_output.sizePreset, m_output.longEdge);
    connect(m_size, &QComboBox::currentIndexChanged, this, &BatchPage::outputEdited);
    m_noEnlarge = new QCheckBox(tr("Never enlarge"), box);
    m_noEnlarge->setChecked(m_output.noEnlarge);
    m_noEnlarge->setToolTip(tr("A photo smaller than the long edge keeps its own size"));
    connect(m_noEnlarge, &QCheckBox::toggled, this, &BatchPage::outputEdited);
    column->addWidget(m_size);
    column->addWidget(m_noEnlarge);
    return box;
}

QWidget* BatchPage::buildSaveAs(QWidget* parent)
{
    auto* box = new QWidget(parent);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);
    column->addWidget(sectionTitle(tr("4. Save as"), box));
    m_format = new QComboBox(box);
    m_format->setAccessibleName(tr("Format"));
    refreshFormats();
    connect(m_format, &QComboBox::activated, this, [this] {
        m_output.extension = m_format->currentData().toString();
        outputEdited();
    });
    auto* more = new QPushButton(tr("More..."), box);
    more->setObjectName(u"small"_s);
    more->setToolTip(tr("Quality, bit depth, metadata, and what to do when a file is already there"));
    connect(more, &QPushButton::clicked, this, &BatchPage::editSaveOptions);
    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(m_format);
    formatRow->addWidget(more);
    formatRow->addStretch();
    m_folder = new QLabel(shortPath(m_output.folder), box);
    m_folder->setObjectName(u"mono"_s);
    m_folder->setToolTip(QDir::toNativeSeparators(m_output.folder));
    auto* change = new QPushButton(tr("Change"), box);
    change->setObjectName(u"small"_s);
    connect(change, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Save to"), m_output.folder);
        if (!folder.isEmpty()) {
            m_output.folder = folder;
            m_folder->setText(shortPath(folder));
            m_folder->setToolTip(QDir::toNativeSeparators(folder));
            outputEdited();
        }
    });
    auto* folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(change);
    m_name = new QLineEdit(m_output.nameTemplate, box);
    m_name->setObjectName(u"monoField"_s);
    m_name->setAccessibleName(tr("File name"));
    m_name->setToolTip(tr("{name} the photo's name, {preset} the recipe, {date}, {w} and {h} the size"));
    connect(m_name, &QLineEdit::editingFinished, this, [this] {
        m_output.nameTemplate =
            m_name->text().trimmed().isEmpty() ? u"{name}-edit"_s : m_name->text().trimmed();
        outputEdited();
    });
    m_beside = new QCheckBox(tr("Next to each original, in a subfolder"), box);
    m_beside->setToolTip(tr("A folder named PixelFx beside each photo, instead of the folder above"));
    connect(m_beside, &QCheckBox::toggled, m_folder, &QWidget::setDisabled);
    column->addLayout(formatRow);
    column->addLayout(folderRow);
    column->addWidget(m_name);
    column->addWidget(m_beside);
    return box;
}

QWidget* BatchPage::buildFoot(QWidget* parent)
{
    auto* save =
        new IconTextButton(icons::themed(u"save"_s, tokens().text), tr("Save as batch recipe"), parent);
    save->setToolTip(tr("Keep this page as a file, to run again here or from the command line. "
                        "Open one from the main menu, or drop it here."));
    connect(save, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(
            this, tr("Save as batch recipe"), tr("Batch.pfxbatch"), tr("Batch recipes (*.pfxbatch)"));
        if (!file.isEmpty() && !saveRecipe(file)) {
            m_statusText->setText(tr("The recipe could not be saved there."));
            m_status->show();
        }
    });
    m_process = new IconTextButton(icons::themed(u"play"_s, Qt::white), tr("Process"), parent);
    m_process->setObjectName(u"primary"_s);
    connect(m_process, &QPushButton::clicked, this, &BatchPage::process);
    auto* foot = new QHBoxLayout;
    foot->addWidget(save);
    foot->addStretch();
    foot->addWidget(m_process);
    return sheetFootBar(parent, foot);
}

void BatchPage::refreshFormats()
{
    const QSignalBlocker block(m_format);
    m_format->clear();
    for (const std::string& ext : pfx::writableExtensions()) {
        const QString e = QString::fromStdString(ext).mid(1);
        //: Batch's Save as: a format and its quality, "JPEG, quality 90".
        m_format->addItem(
            lossy(e) ? tr("%1, quality %2").arg(formatName(e)).arg(m_output.quality) : formatName(e), e);
    }
    m_format->setCurrentIndex(std::max(0, m_format->findData(m_output.extension)));
}

QFormLayout* BatchPage::saveOptionsForm(QDialog* sheet, const core::ExportOptions& output,
                                        SaveOptionsControls& c)
{
    c.quality = new PanelSlider(Qt::Horizontal, sheet);
    c.quality->setRange(1, 100);
    c.quality->setValue(output.quality);
    c.quality->setEnabled(lossy(output.extension));
    auto* qualityValue = new QLabel(QString::number(output.quality), sheet);
    connect(c.quality, &QSlider::valueChanged, qualityValue,
            [qualityValue](int v) { qualityValue->setNum(v); });
    auto* qualityRow = new QHBoxLayout;
    qualityRow->addWidget(c.quality, 1);
    qualityRow->addWidget(qualityValue);
    c.deep = new QCheckBox(tr("16 bits per channel"), sheet);
    c.deep->setChecked(output.bitsPerSample == 16);
    c.deep->setEnabled(!lossy(output.extension));
    c.camera = new QCheckBox(tr("Keep camera details and copyright"), sheet);
    c.camera->setChecked(output.keepMetadata);
    c.location = new QCheckBox(tr("Keep location"), sheet);
    c.location->setChecked(output.keepLocation);
    c.exists = new QComboBox(sheet);
    c.exists->addItem(tr("Add a number"), static_cast<int>(core::ExportOptions::IfExists::AddNumber));
    c.exists->addItem(tr("Replace it"), static_cast<int>(core::ExportOptions::IfExists::Overwrite));
    c.exists->addItem(tr("Skip the photo"), static_cast<int>(core::ExportOptions::IfExists::Skip));
    c.exists->setCurrentIndex(std::max(0, c.exists->findData(static_cast<int>(output.ifExists))));
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(tr("Quality"), qualityRow);
    form->addRow(tr("Bit depth"), c.deep);
    auto* metadata = new QVBoxLayout;
    metadata->addWidget(c.camera);
    metadata->addWidget(c.location);
    form->addRow(tr("Metadata"), metadata);
    form->addRow(tr("If it exists"), c.exists);
    return form;
}

void BatchPage::editSaveOptions()
{
    // More...: what the Export sheet has beyond the format and the folder.
    QDialog sheet(this);
    sheet.setWindowTitle(tr("Save options"));
    SaveOptionsControls c;
    QFormLayout* form = saveOptionsForm(&sheet, m_output, c);
    auto* cancel = new QPushButton(tr("Cancel"), &sheet);
    auto* done = new QPushButton(tr("Done"), &sheet);
    done->setObjectName(u"primary"_s);
    done->setDefault(true);
    connect(cancel, &QPushButton::clicked, &sheet, &QDialog::reject);
    connect(done, &QPushButton::clicked, &sheet, &QDialog::accept);
    auto* foot = new QHBoxLayout;
    foot->addStretch();
    foot->addWidget(cancel);
    foot->addWidget(done);
    QVBoxLayout* column = sheetLayout(&sheet, foot);
    auto* title = new QLabel(tr("Save options"), &sheet);
    title->setObjectName(u"sheetTitle"_s);
    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(sheetCloseButton(&sheet));
    column->addLayout(titleRow);
    column->addLayout(form);
    sheet.setMinimumWidth(440);
    if (sheet.exec() != QDialog::Accepted) {
        return;
    }
    m_output.quality = c.quality->value();
    m_output.bitsPerSample = c.deep->isChecked() ? 16 : 8;
    m_output.keepMetadata = c.camera->isChecked();
    m_output.keepLocation = c.location->isChecked();
    m_output.ifExists = static_cast<core::ExportOptions::IfExists>(c.exists->currentData().toInt());
    refreshFormats();
    outputEdited();
}

void BatchPage::outputEdited()
{
    Q_EMIT outputChanged(recipe().output);
}

// ---------------------------------------------------------------------------------------------
// The recipe's choices

void BatchPage::setPresets(const std::vector<core::PresetEntry>& entries)
{
    m_presets = entries;
    refreshRecipeCard();
}

void BatchPage::setLooks(const std::vector<core::LookEntry>& entries)
{
    m_looks = entries;
    refreshRecipeCard();
}

void BatchPage::setPhotoSettings(const QString& name, const pfx::Settings& settings)
{
    m_photoName = name;
    m_photoSettings = settings;
    if (m_fromPhoto) {
        refreshRecipeCard();
        schedulePreview();
    }
}

void BatchPage::chooseRecipe(const QString& choice)
{
    m_first = choice;
    m_fromPhoto = false;
    m_sources->button(0)->setChecked(true);
    refreshRecipeCard();
    schedulePreview();
}

void BatchPage::chooseSecond(const QString& presetName)
{
    m_second = presetName;
    {
        const QSignalBlocker block(m_secondOn);
        m_secondOn->setChecked(!presetName.isEmpty());
    }
    refreshRecipeCard();
    schedulePreview();
}

void BatchPage::setFromPhoto(bool on)
{
    m_fromPhoto = on;
    m_sources->button(on ? 1 : 0)->setChecked(true);
    refreshRecipeCard();
    schedulePreview();
}

void BatchPage::showRecipeMenu(QWidget* anchor, bool second)
{
    QMenu menu(this);
    const auto add = [&menu](const QString& text, const std::function<void()>& run) {
        connect(menu.addAction(text), &QAction::triggered, &menu, run);
    };
    if (second) {
        add(tr("None"), [this] { chooseSecond({}); });
    } else {
        add(tr("None: resize and save only"), [this] { chooseRecipe({}); });
        if (!m_loadedStack.empty()) {
            add(tr("From the opened recipe: %1").arg(m_loadedName), [this] { chooseRecipe(u"recipe"_s); });
        }
    }
    menu.addSeparator();
    addMenuHeading(&menu, tr("Presets"));
    for (const core::PresetEntry& p : m_presets) {
        if (second && p.autoEnhance) {
            continue; // Auto enhance has its own check box
        }
        const QString name = p.name;
        add(name, [this, name, second] { second ? chooseSecond(name) : chooseRecipe(u"preset:"_s + name); });
    }
    if (!second && !m_looks.empty()) {
        menu.addSeparator();
        addMenuHeading(&menu, tr("Looks"));
        for (const core::LookEntry& l : m_looks) {
            const QString id = l.id;
            add(l.name, [this, id] { chooseRecipe(u"look:"_s + id); });
        }
    }
    menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
    if (second && m_second.isEmpty()) {
        const QSignalBlocker block(m_secondOn);
        m_secondOn->setChecked(false);
    }
}

QString BatchPage::recipeTitle() const
{
    if (m_first.startsWith(u"look:"_s)) {
        for (const core::LookEntry& l : m_looks) {
            if (l.id == m_first.mid(5)) {
                return l.name;
            }
        }
    }
    if (m_first.startsWith(u"preset:"_s)) {
        return m_first.mid(7);
    }
    if (m_first == u"recipe"_s) {
        return m_loadedName;
    }
    return {};
}

std::pair<QString, QString> BatchPage::recipeCardText() const
{
    QString name;
    QString detail;
    if (m_fromPhoto) {
        name = m_photoName.isEmpty() ? tr("No photo is open") : m_photoName;
        detail = m_photoName.isEmpty() ? tr("Open a photo in the editor, then come back")
                                       : tr("Its settings, crop and spots left out");
    } else if (m_first.startsWith(u"look:"_s)) {
        name = recipeTitle();
        for (const core::LookEntry& l : m_looks) {
            if (l.id == m_first.mid(5) && l.look) {
                detail = tr("Look, strength %1").arg(l.look->defaultStrength);
            }
        }
    } else if (m_first.startsWith(u"preset:"_s)) {
        name = recipeTitle();
        detail = tr("Preset");
        for (const core::PresetEntry& p : m_presets) {
            if (p.name == name) {
                detail = p.autoEnhance ? tr("Worked out for each photo")
                                       : tr("Preset: %1").arg(core::presetSummary(p.settings));
            }
        }
    } else if (m_first == u"recipe"_s) {
        name = m_loadedName;
        detail = tr("From a batch recipe, %n steps", nullptr, static_cast<int>(m_loadedStack.size()));
    } else {
        name = tr("No preset or look");
        detail = tr("The photos are only resized and saved");
    }
    return {name, detail};
}

void BatchPage::refreshRecipeCard()
{
    const auto [name, detail] = recipeCardText();
    m_cardName->setText(name);
    m_cardDetail->setText(detail);
    m_cardChange->setVisible(!m_fromPhoto);
    if (!m_afterPixmap.isNull()) {
        m_cardThumb->setPixmap(m_afterPixmap.scaled(m_cardThumb->size() * devicePixelRatioF(),
                                                    Qt::KeepAspectRatioByExpanding,
                                                    Qt::SmoothTransformation));
    } else {
        m_cardThumb->setPixmap(icons::pixmap(u"sliders"_s, tokens().faint, 20, devicePixelRatioF()));
        m_cardThumb->setAlignment(Qt::AlignCenter);
    }
    m_secondName->setText(m_second);
    m_secondChange->setText(m_second.isEmpty() ? tr("Choose") : tr("Change"));
}

// ---------------------------------------------------------------------------------------------
// Photos in and out

void BatchPage::addPaths(const QStringList& paths)
{
    for (const QString& path : paths) {
        const QFileInfo info(path);
        if (info.suffix().toLower() == u"pfxbatch"_s) {
            const QString error = loadRecipe(info.absoluteFilePath()); // a dropped batch recipe
            if (!error.isEmpty()) {
                m_statusText->setText(error);
                m_status->show();
            }
            continue;
        }
        QStringList found;
        if (info.isDir()) {
            QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                found << it.next();
            }
            found.sort();
        } else {
            found << info.absoluteFilePath();
        }
        for (const QString& file : found) {
            if ((isPhotoTest == nullptr || isPhotoTest(file)) && !m_files.contains(file)) {
                m_files << file;
            }
        }
    }
    rebuildRows();
}

void BatchPage::chooseFiles()
{
    addPaths(QFileDialog::getOpenFileNames(this, tr("Add photos")));
}

void BatchPage::chooseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Add folder"));
    if (!folder.isEmpty()) {
        addPaths({folder});
    }
}

QString BatchPage::summary() const
{
    qint64 bytes = 0;
    for (const QString& file : m_files) {
        bytes += QFileInfo(file).size();
    }
    //: The header while Batch is up: "6 photos · 129 MB".
    return m_files.isEmpty() ? tr("No photos yet")
                             : tr("%n photos · %1", nullptr, static_cast<int>(m_files.size()))
                                   .arg(QLocale().formattedDataSize(bytes, 0, QLocale::DataSizeSIFormat));
}

void BatchPage::showSize(const QString& sizePreset, int longEdge)
{
    const QSignalBlocker block(m_size);
    int index = sizePreset.isEmpty() ? -1 : m_size->findData(QString(u"preset:"_s + sizePreset));
    if (index < 0) {
        index = m_size->findData(longEdge);
    }
    m_size->setCurrentIndex(std::max(0, index));
}

// The photo's own settings as a preset, its crop, turns and spots left out (they belong to it).
pfx::Settings BatchPage::photoPreset(const pfx::Settings& settings)
{
    QStringList units;
    for (const core::PresetUnit& u : core::presetUnits()) {
        if (u.id != u"crop"_s && u.id != u"perspective"_s) {
            units << u.id;
        }
    }
    return core::presetFrom(settings, units);
}

void BatchPage::addFirstChoice(services::BatchRecipe& r) const
{
    if (m_fromPhoto) {
        if (!m_photoName.isEmpty()) {
            r.presets.push_back(photoPreset(m_photoSettings));
        }
    } else if (m_first == u"recipe"_s) {
        for (const pfx::BatchStep& step : m_loadedStack) {
            r.presets.push_back(step.settings);
        }
    } else if (m_first.startsWith(u"look:"_s)) {
        for (const core::LookEntry& e : m_looks) {
            if (e.id == m_first.mid(5) && e.look) {
                r.presets.push_back(core::withLook({}, e.id, e.look->defaultStrength));
            }
        }
    } else if (m_first.startsWith(u"preset:"_s)) {
        for (const core::PresetEntry& e : m_presets) {
            if (e.name == m_first.mid(7)) {
                if (e.autoEnhance) {
                    r.autoFirst = true;
                } else {
                    r.presets.push_back(e.settings);
                }
            }
        }
    }
}

services::BatchRecipe BatchPage::recipe() const
{
    services::BatchRecipe r;
    addFirstChoice(r);
    if (m_secondOn->isChecked()) {
        for (const core::PresetEntry& e : m_presets) {
            if (e.name == m_second && !e.autoEnhance) {
                r.presets.push_back(e.settings);
            }
        }
    }
    r.autoFirst = r.autoFirst || m_auto->isChecked();
    r.keepEdits = m_keepEdits->isChecked();
    r.sidecarFolder = m_sidecarFolder;
    r.output = m_output;
    const QString size = m_size->currentData().toString();
    r.output.sizePreset = size.startsWith(u"preset:"_s) ? size.mid(7) : QString();
    r.output.longEdge = r.output.sizePreset.isEmpty() ? m_size->currentData().toInt() : 0;
    r.output.noEnlarge = m_noEnlarge->isChecked();
    r.output.extension = m_format->currentData().toString();
    r.output.watermark = m_watermark->path();
    r.output.recipeName = m_fromPhoto ? QFileInfo(m_photoName).completeBaseName() : recipeTitle();
    if (lossy(r.output.extension)) {
        r.output.bitsPerSample = 8;
    }
    r.besideOriginals = m_beside->isChecked();
    return r;
}

bool BatchPage::saveRecipe(const QString& path) const
{
    QStringList names;
    if (m_first == u"recipe"_s && !m_fromPhoto) {
        for (const pfx::BatchStep& step : m_loadedStack) {
            names << QString::fromStdString(step.name);
        }
    } else {
        names << (m_fromPhoto ? m_photoName : recipeTitle());
    }
    if (m_secondOn->isChecked() && !m_second.isEmpty()) {
        names << m_second;
    }
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    doc.name = QFileInfo(path).completeBaseName().toStdString();
    const services::BatchRecipe r = recipe();
    doc.batch = services::bodyFromRecipe(r, names);
    core::embedBatchResources(doc.batch, r.output.watermark); // so the file runs anywhere
    return pfx::saveDocument(doc, path.toStdString()).ok();
}

QString BatchPage::loadRecipe(const QString& path)
{
    pfx::ParseReport report;
    const auto doc = pfx::loadDocument(path.toStdString(), &report);
    if (!doc) {
        return QString::fromStdString(doc.error().message);
    }
    if (doc.value().kind != pfx::DocumentKind::Batch) {
        return tr("This file is not a batch recipe.");
    }
    const pfx::BatchBody& body = doc.value().batch;
    m_loadedStack = body.stack;
    QStringList names;
    for (const pfx::BatchStep& step : body.stack) {
        names << (step.name.empty() ? tr("a preset") : QString::fromStdString(step.name));
    }
    m_loadedName = names.join(u", "_s);
    // The recipe's own steps travel inside it: they show as one choice.
    chooseSecond({});
    chooseRecipe(m_loadedStack.empty() ? QString() : u"recipe"_s);
    const core::AdoptedResources adopted =
        core::adoptBatchResources(body, core::lookFolder(), m_watermark->folder());
    if (!adopted.addedLooks.isEmpty()) {
        Q_EMIT looksAdded();
    }
    m_watermark->setPath(adopted.watermarkPath);
    const services::BatchRecipe r = services::recipeFromBody(body, m_sidecarFolder);
    m_auto->setChecked(r.autoFirst);
    m_keepEdits->setChecked(r.keepEdits);
    m_output.fitInside = r.output.fitInside;
    showSize(r.output.sizePreset, r.output.longEdge);
    m_noEnlarge->setChecked(r.output.noEnlarge);
    m_output.extension = r.output.extension;
    m_output.quality = r.output.quality;
    refreshFormats();
    m_beside->setChecked(r.besideOriginals);
    m_output.keepMetadata = r.output.keepMetadata;
    m_output.keepLocation = r.output.keepLocation;
    m_output.nameTemplate = r.output.nameTemplate;
    m_name->setText(m_output.nameTemplate);
    m_output.ifExists = r.output.ifExists;
    if (!r.output.folder.isEmpty()) {
        m_output.folder = r.output.folder;
        m_folder->setText(shortPath(m_output.folder));
    }
    return adopted.problems.join(u' ');
}

void BatchPage::process()
{
    if (m_files.isEmpty() || m_runner.isRunning()) {
        return;
    }
    const services::BatchRecipe r = recipe();
    Q_EMIT outputChanged(r.output);
    m_clock.start();
    m_runner.start(m_files, r);
    refreshStatus();
}

QWidget* BatchPage::makeRow(const QString& file)
{
    auto* row = new QWidget(m_rowsBox);
    row->setObjectName(u"batchRow"_s);
    auto* name = new QLabel(QFileInfo(file).fileName(), row);
    name->setObjectName(u"presetName"_s);
    auto* meta = new QLabel(describe(file), row);
    meta->setObjectName(u"presetSummary"_s);
    auto* reason = new QLabel(row); // why a photo was not saved, in plain words (batch-done.html)
    reason->setObjectName(u"batchReason"_s);
    reason->setWordWrap(true);
    reason->hide();
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(name);
    text->addWidget(meta);
    text->addWidget(reason);
    auto* thumb = new QLabel(row);
    thumb->setObjectName(u"batchThumb"_s);
    thumb->setProperty("path", file);
    thumb->setFixedSize(72, 48);
    thumb->setAlignment(Qt::AlignCenter);
    QWidget* state = makePill(row);
    state->setObjectName(u"pill"_s);
    auto* remove = new QToolButton(row);
    remove->setObjectName(u"groupReset"_s);
    remove->setIcon(icons::themed(u"close"_s, tokens().muted));
    remove->setToolTip(tr("Leave %1 out").arg(QFileInfo(file).fileName()));
    remove->setAccessibleName(tr("Leave %1 out").arg(QFileInfo(file).fileName()));
    connect(remove, &QToolButton::clicked, this, [this, file] {
        if (!m_runner.isRunning()) {
            m_files.removeAll(file);
            rebuildRows();
        }
    });
    auto* line = new QHBoxLayout(row);
    line->setContentsMargins(0, 7, 0, 7);
    line->setSpacing(10);
    line->addWidget(thumb);
    line->addLayout(text, 1);
    line->addWidget(state, 0, Qt::AlignVCenter);
    line->addWidget(remove, 0, Qt::AlignVCenter);
    m_thumbnails->request(file, 160, m_sidecarFolder);
    return row;
}

void BatchPage::rebuildRows()
{
    while (const QLayoutItem* item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_grid->clear();
    if (m_files.isEmpty()) {
        auto* empty = new QLabel(
            tr("Drop photos or folders here, or use Add photos and Add folder at the top."), m_rowsBox);
        empty->setObjectName(u"muted"_s);
        empty->setContentsMargins(0, 20, 0, 0);
        m_rows->addWidget(empty);
    }
    for (const QString& file : m_files) {
        m_rows->addWidget(makeRow(file));
        auto* item = new QListWidgetItem(QFileInfo(file).fileName(), m_grid);
        item->setData(Qt::UserRole, file);
        item->setToolTip(describe(file));
    }
    m_rows->addStretch();
    m_process->setText(m_files.isEmpty()
                           ? tr("Process")
                           : tr("Process %n photos", nullptr, static_cast<int>(m_files.size())));
    m_process->setEnabled(!m_files.isEmpty() && !m_runner.isRunning());
    m_watermark->setBatchPhotos(m_files.mid(0, 50)); // the sheet's "Preview on: Batch photos"
    Q_EMIT summaryChanged(summary());
    schedulePreview();
}

void BatchPage::schedulePreview()
{
    m_previewTimer->start();
}

void BatchPage::renderPreview()
{
    m_preview->setVisible(!m_files.isEmpty());
    if (m_files.isEmpty()) {
        m_afterPixmap = QPixmap();
        refreshRecipeCard();
        return;
    }
    ++m_previewGeneration;
    const auto render = [this] {
        const services::BatchRecipe r = recipe();
        pfx::Settings own;
        if (r.keepEdits && !m_sidecarFolder.isEmpty()) {
            own = core::SidecarStore(m_sidecarFolder)
                      .read(core::fileFingerprint(m_previewPath))
                      .value_or(pfx::Settings{});
        }
        const auto key = static_cast<int>((m_previewGeneration % 100000) * 2);
        m_thumbnails->requestRender(key, m_previewImage, own);
        m_thumbnails->requestRender(key + 1, m_previewImage,
                                    services::BatchRunner::settingsFor(r, own, *m_previewImage));
    };
    if (m_previewImage && m_files.contains(m_previewPath)) {
        render();
        return;
    }
    // A small copy of the first photo that reads, decoded once on a worker.
    using Found = std::pair<QString, std::shared_ptr<const pfx::Image>>;
    auto* watcher = new QFutureWatcher<Found>(this);
    connect(watcher, &QFutureWatcher<Found>::finished, this, [this, watcher, render] {
        const Found found = watcher->result();
        watcher->deleteLater();
        if (!found.second || !m_files.contains(found.first)) {
            return;
        }
        m_previewPath = found.first;
        m_previewImage = found.second;
        render();
    });
    watcher->setFuture(QtConcurrent::run([files = m_files.mid(0, 8)]() -> Found {
        for (const QString& file : files) {
            if (auto loaded = pfx::load(file.toStdString(), {240})) {
                return {file, std::make_shared<const pfx::Image>(std::move(loaded).value().image)};
            }
        }
        return {};
    }));
}

void BatchPage::refreshRow(int index)
{
    const QLayoutItem* item = m_rows->itemAt(index);
    if (item == nullptr || item->widget() == nullptr || index >= m_runner.total()) {
        return;
    }
    auto* pill = item->widget()->findChild<QFrame*>(u"pill"_s);
    using S = services::BatchRunner::State;
    switch (m_runner.state(index)) {
    case S::Waiting:
        setPill(pill, u"plain"_s, {}, tr("Waiting"));
        break;
    case S::Working:
        setPill(pill, u"accent"_s, {}, tr("Saving"));
        break;
    case S::Saved:
        setPill(pill, u"ok"_s, u"check"_s, tr("Saved"));
        break;
    case S::Skipped:
        setPill(pill, u"plain"_s, {}, tr("Skipped"));
        break;
    case S::Failed:
        setPill(pill, u"bad"_s, u"alert"_s, tr("Not saved"));
        break;
    }
    auto* reason = item->widget()->findChild<QLabel*>(u"batchReason"_s);
    const bool explain = m_runner.state(index) == S::Failed || m_runner.state(index) == S::Skipped;
    reason->setText(explain ? m_runner.reason(index) : QString());
    reason->setVisible(explain);
}

void BatchPage::refreshStatus()
{
    const int total = m_runner.total();
    const int saved = m_runner.saved();
    const int failed = m_runner.failed();
    const bool running = m_runner.isRunning();
    int done = 0;
    for (int i = 0; i < total; ++i) {
        const auto s = m_runner.state(i);
        done += (s != services::BatchRunner::State::Waiting && s != services::BatchRunner::State::Working)
                    ? 1
                    : 0;
    }
    m_statusIcon->clear();
    m_statusDetail->clear();
    if (running) {
        showRunning(done, total);
    } else if (total > 0) {
        showDone(saved, failed);
    }
    m_status->setVisible(total > 0);
    m_progress->setVisible(running);
    m_pause->setText(m_runner.isPaused() ? tr("Resume") : tr("Pause"));
    m_pause->setVisible(running);
    m_stop->setVisible(running);
    m_retry->setVisible(!running && failed > 0);
    m_showFolder->setVisible(!running && saved > 0);
    m_process->setEnabled(!running && !m_files.isEmpty());
}

void BatchPage::showRunning(int done, int total)
{
    m_statusText->setText(m_runner.isPaused()
                              ? tr("Paused after %1 of %2").arg(done).arg(total)
                              : tr("Saving %1 of %2").arg(std::min(done + 1, total)).arg(total));
    if (done > 0 && !m_runner.isPaused()) {
        const qint64 left = m_clock.elapsed() / done * (total - done) / 1000;
        m_statusDetail->setText(left < 60 ? tr("about %1 s left").arg(std::max<qint64>(1, left))
                                          : tr("about %1 min left").arg((left + 30) / 60));
    }
    m_progress->setRange(0, std::max(1, total));
    m_progress->setValue(done);
}

void BatchPage::showDone(int saved, int failed)
{
    const int total = m_runner.total();
    m_statusIcon->setPixmap(icons::pixmap(failed > 0 ? u"alert"_s : u"check"_s,
                                          failed > 0 ? tokens().danger : tokens().success, 16,
                                          devicePixelRatioF()));
    m_statusText->setText(failed > 0 ? tr("%1 saved, %2 not saved").arg(saved).arg(failed)
                                     : tr("%n saved", nullptr, saved));
    qint64 bytes = 0;
    for (int i = 0; i < total; ++i) {
        bytes += m_runner.target(i).isEmpty() ? 0 : QFileInfo(m_runner.target(i)).size();
    }
    if (m_clock.isValid()) {
        //: After a batch: how long it took and how much it wrote, "in 38 s, 41 MB".
        m_statusDetail->setText(tr("in %1 s, %2")
                                    .arg(std::max<qint64>(1, m_clock.elapsed() / 1000))
                                    .arg(QLocale().formattedDataSize(bytes, 0, QLocale::DataSizeSIFormat)));
    }
}

void BatchPage::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BatchPage::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }
    addPaths(paths);
    event->acceptProposedAction();
}

} // namespace pixelfx::ui
