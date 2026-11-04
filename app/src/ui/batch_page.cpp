#include "ui/batch_page.h"

#include "core/sidecar_store.h"
#include "ui/icons.h"
#include "ui/style.h"
#include "ui/watermark_sheet.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
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

constexpr int kPanelWidth = 380; // mock: grid-template-columns ... 380px

QLabel* sectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setObjectName(u"section"_s);
    label->setContentsMargins(0, 14, 0, 4);
    return label;
}

QString describe(const QString& path)
{
    const auto info = pfx::probe(path.toStdString());
    const QString size = QLocale().formattedDataSize(QFileInfo(path).size(), 1, QLocale::DataSizeSIFormat);
    return info ? u"%1 x %2 · %3"_s.arg(info.value().width).arg(info.value().height).arg(size) : size;
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
        for (QLabel* thumb : m_rowsBox->findChildren<QLabel*>(u"batchThumb"_s)) {
            if (thumb->property("path").toString() == t.path && !t.image.isNull()) {
                thumb->setPixmap(QPixmap::fromImage(t.image.scaled(thumb->size() * devicePixelRatioF(),
                                                                   Qt::KeepAspectRatioByExpanding,
                                                                   Qt::SmoothTransformation)));
            }
        }
    });
    connect(m_thumbnails, &services::Thumbnailer::rendered, this, [this](int key, const QImage& image) {
        if (static_cast<quint64>(key / 2) != (m_previewGeneration % 100000) || image.isNull()) {
            return; // a picture for an older recipe
        }
        QLabel* target = key % 2 == 0 ? m_before : m_after;
        if (target == m_after) {
            m_watermark->setPreview(image); // the watermark sheet previews on the batch's own photo
        }
        target->setPixmap(QPixmap::fromImage(
            image.scaledToHeight(static_cast<int>(56 * devicePixelRatioF()), Qt::SmoothTransformation)));
    });
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(200);
    connect(m_previewTimer, &QTimer::timeout, this, &BatchPage::renderPreview);
    for (const QComboBox* combo : {m_preset, m_second}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &BatchPage::schedulePreview);
    }
    for (const QCheckBox* box : {m_auto, m_keepEdits}) {
        connect(box, &QCheckBox::toggled, this, &BatchPage::schedulePreview);
    }
    connect(&m_runner, &services::BatchRunner::photoChanged, this, &BatchPage::refreshRow);
    connect(&m_runner, &services::BatchRunner::progress, this, &BatchPage::refreshStatus);
    connect(&m_runner, &services::BatchRunner::finished, this, &BatchPage::refreshStatus);
    rebuildRows();
}

BatchPage::~BatchPage() = default;

QWidget* BatchPage::buildList()
{
    auto* box = new QWidget(this);
    auto* title = new QLabel(tr("Photos"), box);
    title->setObjectName(u"recentTitle"_s);
    auto* addFiles = new QPushButton(icons::themed(u"open"_s, tokens().text), tr("Add photos..."), box);
    auto* addFolder =
        new QPushButton(icons::themed(u"folder-plus"_s, tokens().text), tr("Add folder..."), box);
    addFolder->setToolTip(tr("Every photo in a folder, subfolders included"));
    auto* clear = new QPushButton(tr("Clear"), box);
    clear->setObjectName(u"link"_s);
    for (QPushButton* b : {addFiles, addFolder}) {
        b->setObjectName(u"small"_s);
    }
    connect(addFiles, &QPushButton::clicked, this,
            [this] { addPaths(QFileDialog::getOpenFileNames(this, tr("Add photos"))); });
    connect(addFolder, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Add folder"));
        if (!folder.isEmpty()) {
            addPaths({folder});
        }
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        if (!m_runner.isRunning()) {
            m_files.clear();
            rebuildRows();
        }
    });
    auto* head = new QHBoxLayout;
    head->addWidget(title);
    head->addStretch();
    head->addWidget(addFiles);
    head->addWidget(addFolder);
    head->addWidget(clear);

    QHBoxLayout* status = buildStatus(box);

    m_rowsBox = new QWidget(box);
    m_rowsBox->setObjectName(u"adjustContent"_s);
    m_rows = new QVBoxLayout(m_rowsBox);
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(2);
    auto* scroll = new QScrollArea(box);
    scroll->setObjectName(u"adjustScroll"_s);
    scroll->setWidget(m_rowsBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    buildPreview(box);

    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(20, 14, 20, 14);
    column->setSpacing(10);
    column->addLayout(head);
    column->addLayout(status);
    column->addWidget(scroll, 1);
    column->addWidget(m_preview);
    return box;
}

void BatchPage::buildPreview(QWidget* box)
{
    // Mock: "Preview", the first photo before and after the recipe.
    m_preview = new QWidget(box);
    auto* previewLabel = new QLabel(tr("Preview"), m_preview);
    previewLabel->setObjectName(u"muted"_s);
    m_before = new QLabel(m_preview);
    m_after = new QLabel(m_preview);
    auto* arrow = new QLabel(u"\u2192"_s, m_preview);
    arrow->setObjectName(u"muted"_s);
    auto* caption = new QLabel(tr("The first photo, before and after the recipe"), m_preview);
    caption->setObjectName(u"faint"_s);
    auto* previewRow = new QHBoxLayout(m_preview);
    previewRow->setContentsMargins(0, 6, 0, 0);
    previewRow->setSpacing(10);
    previewRow->addWidget(previewLabel);
    previewRow->addWidget(m_before);
    previewRow->addWidget(arrow);
    previewRow->addWidget(m_after);
    previewRow->addWidget(caption, 1);
    m_preview->hide();
}

QHBoxLayout* BatchPage::buildStatus(QWidget* box)
{
    m_status = new QLabel(box);
    m_pause = new QPushButton(tr("Pause"), box);
    m_stop = new QPushButton(tr("Stop"), box);
    m_retry = new QPushButton(tr("Retry failed"), box);
    m_showFolder = new QPushButton(tr("Show in folder"), box);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        m_runner.setPaused(!m_runner.isPaused());
        refreshStatus();
    });
    connect(m_stop, &QPushButton::clicked, this, [this] { m_runner.stop(); });
    connect(m_retry, &QPushButton::clicked, this, [this] { m_runner.retryFailed(); });
    connect(m_showFolder, &QPushButton::clicked, this, [this] {
        for (int i = 0; i < m_runner.total(); ++i) {
            if (!m_runner.target(i).isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_runner.target(i)).absolutePath()));
                return;
            }
        }
    });
    auto* status = new QHBoxLayout;
    status->addWidget(m_status, 1);
    for (QPushButton* b : {m_pause, m_stop, m_retry, m_showFolder}) {
        b->setObjectName(u"small"_s);
        b->hide();
        status->addWidget(b);
    }

    return status;
}

QWidget* BatchPage::buildPanel()
{
    auto* panel = new QWidget(this);
    panel->setObjectName(u"cropPanel"_s);
    panel->setAttribute(Qt::WA_StyledBackground);
    panel->setFixedWidth(kPanelWidth);
    auto* saveRecipe = new QPushButton(tr("Save as batch recipe..."), panel);
    saveRecipe->setToolTip(tr("Keep this page as a file, to run again here or from the command line"));
    auto* openRecipe = new QPushButton(tr("Open recipe..."), panel);
    for (QPushButton* b : {saveRecipe, openRecipe}) {
        b->setObjectName(u"small"_s);
    }
    connect(saveRecipe, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getSaveFileName(
            this, tr("Save as batch recipe"), tr("Batch.pfxbatch"), tr("Batch recipes (*.pfxbatch)"));
        if (!file.isEmpty() && !this->saveRecipe(file)) {
            m_status->setText(tr("The recipe could not be saved there."));
        }
    });
    connect(openRecipe, &QPushButton::clicked, this, [this] {
        const QString file = QFileDialog::getOpenFileName(this, tr("Open batch recipe"), QString(),
                                                          tr("Batch recipes (*.pfxbatch)"));
        if (!file.isEmpty()) {
            const QString error = loadRecipe(file);
            if (!error.isEmpty()) {
                m_status->setText(error);
            }
        }
    });
    auto* recipes = new QHBoxLayout;
    recipes->addWidget(saveRecipe);
    recipes->addWidget(openRecipe);
    recipes->addStretch();
    m_process = new QPushButton(tr("Process"), panel);
    m_process->setObjectName(u"primary"_s);
    connect(m_process, &QPushButton::clicked, this, &BatchPage::process);
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(18, 4, 18, 14);
    column->setSpacing(4);
    column->addWidget(buildRecipe(panel));
    column->addWidget(buildOutput(panel));
    column->addStretch();
    column->addLayout(recipes);
    column->addWidget(m_process);
    return panel;
}

QWidget* BatchPage::buildRecipe(QWidget* parent)
{
    auto* box = new QWidget(parent);
    m_preset = new QComboBox(box);
    m_second = new QComboBox(box);
    m_auto = new QCheckBox(tr("Auto enhance each photo first"), box);
    m_keepEdits = new QCheckBox(tr("Keep edits already made in PixelFx"), box);
    m_keepEdits->setChecked(true);
    m_preset->setAccessibleName(tr("Preset"));
    m_second->setAccessibleName(tr("Then add preset"));
    auto* form = new QFormLayout;
    form->addRow(tr("Preset"), m_preset);
    form->addRow(tr("Then add preset"), m_second);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(sectionTitle(tr("1. Recipe"), box));
    column->addLayout(form);
    column->addWidget(m_auto);
    column->addWidget(m_keepEdits);
    return box;
}

QWidget* BatchPage::buildOutput(QWidget* parent)
{
    auto* box = new QWidget(parent);
    m_size = new QComboBox(box);
    m_size->addItem(tr("Original size"), 0);
    for (const int edge : {3840, 2048, 1600, 1080}) {
        m_size->addItem(tr("%1 px long edge").arg(edge), edge);
    }
    m_size->setCurrentIndex(std::max(0, m_size->findData(m_output.longEdge)));
    m_format = new QComboBox(box);
    for (const std::string& ext : pfx::writableExtensions()) {
        const QString e = QString::fromStdString(ext).mid(1);
        m_format->addItem(e == u"jpg"_s ? u"JPEG"_s : e.toUpper(), e);
    }
    m_format->setCurrentIndex(std::max(0, m_format->findData(m_output.extension)));
    m_quality = new QSpinBox(box);
    m_quality->setRange(1, 100);
    m_quality->setValue(m_output.quality);
    m_quality->setPrefix(tr("Quality "));
    m_folder = new QLabel(QDir::toNativeSeparators(m_output.folder), box);
    m_folder->setObjectName(u"mono"_s);
    m_folder->setWordWrap(true);
    auto* change = new QPushButton(tr("Change..."), box);
    change->setObjectName(u"small"_s);
    connect(change, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Save to"), m_output.folder);
        if (!folder.isEmpty()) {
            m_output.folder = folder;
            m_folder->setText(QDir::toNativeSeparators(folder));
            Q_EMIT outputChanged(recipe().output);
        }
    });
    m_beside = new QCheckBox(tr("Next to each original, in a PixelFx folder"), box);
    connect(m_beside, &QCheckBox::toggled, m_folder, &QWidget::setDisabled);
    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(m_format, 1);
    formatRow->addWidget(m_quality);
    auto* folderRow = new QHBoxLayout;
    folderRow->addWidget(m_folder, 1);
    folderRow->addWidget(change);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(0, 0, 0, 0);
    column->addWidget(sectionTitle(tr("2. Resize"), box));
    column->addWidget(m_size);
    // Mock batch.html: 3. Watermark, 4. Save as.
    column->addWidget(sectionTitle(tr("3. Watermark"), box));
    m_watermark = new WatermarkPicker(m_output.watermark, box);
    connect(m_watermark, &WatermarkPicker::changed, this, [this] { Q_EMIT outputChanged(recipe().output); });
    column->addWidget(m_watermark);
    column->addWidget(sectionTitle(tr("4. Save as"), box));
    column->addLayout(formatRow);
    column->addLayout(folderRow);
    column->addWidget(m_beside);
    return box;
}

void BatchPage::setPresets(const std::vector<core::PresetEntry>& entries)
{
    m_presets = entries;
    for (QComboBox* combo : {m_preset, m_second}) {
        const QString current = combo->currentText();
        combo->clear();
        combo->addItem(tr("None"));
        for (std::size_t i = 0; i < entries.size(); ++i) {
            combo->addItem(entries[i].name, static_cast<int>(i)); // the item knows its preset
        }
        combo->setCurrentIndex(std::max(0, combo->findText(current)));
    }
}

void BatchPage::addPaths(const QStringList& paths)
{
    for (const QString& path : paths) {
        const QFileInfo info(path);
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

services::BatchRecipe BatchPage::recipe() const
{
    services::BatchRecipe r;
    if (m_preset->currentData().toString() == u"recipe"_s) {
        for (const pfx::BatchStep& step : m_loadedStack) {
            r.presets.push_back(step.settings);
        }
    }
    for (const QComboBox* combo : {m_preset, m_second}) {
        const QVariant choice = combo->currentData();
        if (choice.typeId() != QMetaType::Int || !std::cmp_less(choice.toInt(), m_presets.size())) {
            continue; // None, or the presets of an opened recipe
        }
        const core::PresetEntry& e = m_presets[static_cast<std::size_t>(choice.toInt())];
        if (e.autoEnhance) {
            r.autoFirst = true;
        } else {
            r.presets.push_back(e.settings);
        }
    }
    r.autoFirst = r.autoFirst || m_auto->isChecked();
    r.keepEdits = m_keepEdits->isChecked();
    r.sidecarFolder = m_sidecarFolder;
    r.output = m_output;
    r.output.longEdge = m_size->currentData().toInt();
    r.output.extension = m_format->currentData().toString();
    r.output.quality = m_quality->value();
    r.output.watermark = m_watermark->path();
    if (r.output.extension != u"png"_s && r.output.extension != u"tif"_s) {
        r.output.bitsPerSample = 8;
    }
    r.besideOriginals = m_beside->isChecked();
    return r;
}

bool BatchPage::saveRecipe(const QString& path) const
{
    QStringList names;
    if (m_preset->currentData().toString() == u"recipe"_s) {
        for (const pfx::BatchStep& step : m_loadedStack) {
            names << QString::fromStdString(step.name);
        }
    } else {
        for (const QComboBox* combo : {m_preset, m_second}) {
            if (combo->currentIndex() > 0) {
                names << combo->currentText();
            }
        }
    }
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    doc.name = QFileInfo(path).completeBaseName().toStdString();
    doc.batch = services::bodyFromRecipe(recipe(), names);
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
    // The recipe's own presets travel inside it: they show as one choice.
    const int existing = m_preset->findData(u"recipe"_s);
    if (existing >= 0) {
        m_preset->removeItem(existing);
    }
    if (!names.isEmpty()) {
        m_preset->insertItem(1, tr("From recipe: %1").arg(names.join(u", "_s)), u"recipe"_s);
        m_preset->setCurrentIndex(1);
    } else {
        m_preset->setCurrentIndex(0);
    }
    m_second->setCurrentIndex(0);
    const services::BatchRecipe r = services::recipeFromBody(body, m_sidecarFolder);
    m_auto->setChecked(r.autoFirst);
    m_keepEdits->setChecked(r.keepEdits);
    m_size->setCurrentIndex(std::max(0, m_size->findData(r.output.longEdge)));
    m_format->setCurrentIndex(std::max(0, m_format->findData(r.output.extension)));
    m_quality->setValue(r.output.quality);
    m_beside->setChecked(r.besideOriginals);
    m_output.keepMetadata = r.output.keepMetadata;
    m_output.keepLocation = r.output.keepLocation;
    m_output.nameTemplate = r.output.nameTemplate;
    m_output.ifExists = r.output.ifExists;
    if (!r.output.folder.isEmpty()) {
        m_output.folder = r.output.folder;
        m_folder->setText(QDir::toNativeSeparators(m_output.folder));
    }
    return {};
}

void BatchPage::process()
{
    if (m_files.isEmpty() || m_runner.isRunning()) {
        return;
    }
    const services::BatchRecipe r = recipe();
    Q_EMIT outputChanged(r.output);
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
    auto* state = new QLabel(row);
    state->setObjectName(u"batchState"_s);
    state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* reason = new QLabel(row); // why a photo was not saved, in plain words (batch-done.html)
    reason->setObjectName(u"batchReason"_s);
    reason->setWordWrap(true);
    reason->hide();
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(name);
    text->addWidget(meta);
    text->addWidget(reason);
    auto* thumb = new QLabel(row); // mock .thumb 72 by 48
    thumb->setObjectName(u"batchThumb"_s);
    thumb->setProperty("path", file);
    thumb->setFixedSize(72, 48);
    thumb->setScaledContents(false);
    thumb->setAlignment(Qt::AlignCenter);
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
    line->setContentsMargins(8, 6, 10, 6);
    line->setSpacing(10);
    line->addWidget(thumb);
    line->addLayout(text, 1);
    line->addWidget(state);
    line->addWidget(remove);
    m_thumbnails->request(file, 144, m_sidecarFolder);
    return row;
}

void BatchPage::rebuildRows()
{
    while (const QLayoutItem* item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (m_files.isEmpty()) {
        auto* empty =
            new QLabel(tr("Drop photos or folders here, or add them with the buttons above."), m_rowsBox);
        empty->setObjectName(u"muted"_s);
        empty->setContentsMargins(0, 20, 0, 0);
        m_rows->addWidget(empty);
    }
    for (const QString& file : m_files) {
        m_rows->addWidget(makeRow(file));
    }
    m_rows->addStretch();
    m_process->setText(m_files.isEmpty()
                           ? tr("Process")
                           : tr("Process %n photos", nullptr, static_cast<int>(m_files.size())));
    m_process->setEnabled(!m_files.isEmpty());
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
    auto* state = item->widget()->findChild<QLabel*>(u"batchState"_s);
    using S = services::BatchRunner::State;
    switch (m_runner.state(index)) {
    case S::Waiting:
        state->setText(tr("Waiting"));
        break;
    case S::Working:
        state->setText(tr("Saving..."));
        break;
    case S::Saved:
        state->setText(tr("Saved"));
        break;
    case S::Skipped:
        state->setText(tr("Skipped"));
        break;
    case S::Failed:
        state->setText(tr("Not saved"));
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
    if (running) {
        int done = 0;
        for (int i = 0; i < total; ++i) {
            const auto s = m_runner.state(i);
            done += (s != services::BatchRunner::State::Waiting && s != services::BatchRunner::State::Working)
                        ? 1
                        : 0;
        }
        m_status->setText(m_runner.isPaused()
                              ? tr("Paused after %1 of %2").arg(done).arg(total)
                              : tr("Saving %1 of %2").arg(std::min(done + 1, total)).arg(total));
    } else if (total > 0) {
        m_status->setText(failed > 0 ? tr("%1 saved, %2 not saved").arg(saved).arg(failed)
                                     : tr("%n saved", nullptr, saved));
    } else {
        m_status->clear();
    }
    m_pause->setText(m_runner.isPaused() ? tr("Resume") : tr("Pause"));
    m_pause->setVisible(running);
    m_stop->setVisible(running);
    m_retry->setVisible(!running && failed > 0);
    m_showFolder->setVisible(!running && saved > 0);
    m_process->setEnabled(!running && !m_files.isEmpty());
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
