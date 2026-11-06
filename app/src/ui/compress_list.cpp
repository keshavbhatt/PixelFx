#include "ui/compress_list.h"

#include "core/describe.h"
#include "ui/file_row.h"
#include "ui/list_row.h"
#include "ui/status_pill.h"
#include "ui/style.h"

#include <QEvent>
#include <QFileInfo>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QProgressBar>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kBarWidth = 180;

QString bytesText(qint64 bytes)
{
    return core::sizeText(bytes);
}

QString formatOf(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == u"jpg"_s || ext == u"jpeg"_s) {
        return u"JPEG"_s;
    }
    if (ext == u"tif"_s || ext == u"tiff"_s) {
        return u"TIFF"_s;
    }
    if (ext == u"webp"_s) {
        return u"WebP"_s;
    }
    return ext.toUpper();
}

} // namespace

CompressList::CompressList(QWidget* parent)
    : QWidget(parent)
    , m_column(new QVBoxLayout(this))
{
    setObjectName(u"batchRows"_s);
    m_column->setContentsMargins(8, 6, 8, 6);
    m_column->setSpacing(1);
}

CompressList::~CompressList() = default;

QString CompressList::describe(const services::CompressResult& result, const QString& path)
{
    using M = pfx::CompressMethod;
    using N = pfx::CompressNote;
    // A format left alone says why; a result no smaller than the original says that.
    if (result.method == M::Unchanged && result.note != N::None) {
        return unchangedReason(result.note, path);
    }
    if (result.keptOriginal || result.after >= result.before) {
        return tr("Already as small as it gets");
    }
    switch (result.method) {
    case M::JpegLossless:
        return tr("JPEG, lossless");
    case M::JpegQuality:
        return tr("JPEG, encoded again");
    case M::PngLossless:
        return result.note == N::BelowQuality ? tr("PNG, lossless: fewer colours would show")
                                              : tr("PNG, lossless");
    case M::PngFewerColours:
        //: A PNG reduced to a palette, with the palette's quality: "PNG, fewer colours (quality 87)".
        return tr("PNG, fewer colours (quality %1)").arg(result.quality);
    case M::TiffLossless:
        return tr("TIFF, better compression");
    case M::WebpLossless:
        return tr("WebP, lossless");
    case M::WebpMetadata:
        return tr("WebP, location and previews removed");
    case M::GifLossless:
        return tr("GIF, lossless");
    case M::Unchanged:
        break;
    }
    return tr("Already as small as it gets");
}

QString CompressList::unchangedReason(pfx::CompressNote note, const QString& path)
{
    using N = pfx::CompressNote;
    switch (note) {
    case N::LossyWebp:
        return tr("A lossy WebP stays as it is");
    case N::Animated:
        return tr("An animated PNG stays as it is");
    case N::TooManyColours:
        return tr("Too many colours for GIF: kept as it is");
    case N::NotSupported:
        //: Compress only leaves this format alone: "HEIC is kept as it is".
        return tr("%1 is kept as it is").arg(formatOf(path));
    case N::BelowQuality:
    case N::None:
        break;
    }
    return tr("Already as small as it gets");
}

void CompressList::setFiles(const QStringList& files)
{
    m_files = files;
    if (!files.contains(m_selected)) {
        m_selected.clear();
    }
    build();
}

void CompressList::build()
{
    while (const QLayoutItem* item = m_column->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_rows.clear();
    if (m_files.isEmpty()) {
        auto* empty =
            new QLabel(tr("Drop photos or folders here, or use Add photos and Add folder at the top."), this);
        empty->setObjectName(u"muted"_s);
        empty->setContentsMargins(kPanelPadding - 8, 20, 0, 0);
        m_column->addWidget(empty);
        m_column->addStretch();
        return;
    }
    m_rows.reserve(static_cast<std::size_t>(m_files.size()));
    for (const QString& file : m_files) {
        Row r;
        r.path = file;
        r.before = QFileInfo(file).size();
        r.sizes = new QWidget;
        r.sizeText = new QLabel(r.sizes);
        r.sizeText->setObjectName(u"mono"_s);
        r.sizeText->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        r.bar = new QProgressBar(r.sizes);
        r.bar->setTextVisible(false);
        r.bar->setFixedWidth(kBarWidth);
        r.bar->setRange(0, 1000);
        auto* sizes = new QVBoxLayout(r.sizes);
        sizes->setContentsMargins(0, 0, 8, 0);
        sizes->setSpacing(4);
        sizes->addWidget(r.sizeText);
        sizes->addWidget(r.bar);
        r.sizes->hide();
        r.file = makeFileRow(this, file, u"%1 \u00B7 %2"_s.arg(formatOf(file), bytesText(r.before)), r.sizes);
        r.row = r.file.row;
        r.pill = r.file.pill;
        r.row->setSelected(file == m_selected);
        r.row->installEventFilter(this);
        connect(r.row, &ListRow::clicked, this, [this, file] { select(file); });
        connect(r.row, &ListRow::actionClicked, this, [this, file] { Q_EMIT removeRequested(file); });
        m_rows.push_back(r);
    }
    order();
}

void CompressList::order()
{
    if (m_rows.empty()) {
        return; // the empty list's note stays as build() placed it
    }
    std::vector<const Row*> sorted;
    sorted.reserve(m_rows.size());
    for (const Row& r : m_rows) {
        sorted.push_back(&r);
    }
    switch (m_sort) {
    case Sort::BiggestSaving:
        std::ranges::stable_sort(sorted, [](const Row* a, const Row* b) { return a->saved > b->saved; });
        break;
    case Sort::Size:
        std::ranges::stable_sort(sorted, [](const Row* a, const Row* b) { return a->before > b->before; });
        break;
    case Sort::Name:
        std::ranges::stable_sort(sorted, [](const Row* a, const Row* b) {
            return QFileInfo(a->path).fileName().localeAwareCompare(QFileInfo(b->path).fileName()) < 0;
        });
        break;
    }
    while (const QLayoutItem* item = m_column->takeAt(0)) {
        delete item; // the rows stay; only their places change
    }
    for (const Row* r : sorted) {
        m_column->addWidget(r->row);
    }
    m_column->addStretch();
}

void CompressList::setSort(Sort sort)
{
    m_sort = sort;
    order();
}

void CompressList::setThumbnail(const QString& path, const QImage& image)
{
    for (const Row& r : m_rows) {
        if (r.path == path) {
            setFileThumb(r.file, image);
        }
    }
}

void CompressList::showResult(Row& r, const services::BatchRunner& runner, int index)
{
    using S = services::BatchRunner::State;
    using K = StatusPill::Kind;
    r.sizes->setVisible(runner.state(index) == S::Saved);
    r.saved = -1;
    switch (runner.state(index)) {
    case S::Waiting:
        r.pill->setStatus(K::Plain, tr("Waiting"));
        return;
    case S::Working:
        r.pill->setStatus(K::Accent, tr("Compressing"));
        return;
    case S::Skipped:
    case S::Failed:
        r.pill->setStatus(K::Bad, tr("Not saved"), u"alert"_s);
        r.row->setSubtitle(runner.reason(index));
        r.row->setSubtitleError(true);
        return;
    case S::Saved:
        break;
    }
    const services::CompressResult& c = runner.compressed(index);
    r.saved = std::max<qint64>(0, c.before - c.after);
    r.row->setSubtitle(describe(c, r.path));
    r.row->setSubtitleError(false);
    //: Sizes before and after: "8.4 MB → 5.1 MB".
    r.sizeText->setText(tr("%1 → <b>%2</b>").arg(bytesText(c.before), bytesText(c.after)));
    r.bar->setValue(c.before > 0 ? static_cast<int>(1000 * c.after / c.before) : 1000);
    r.bar->setAccessibleName(
        tr("New size, %1% of the original").arg(c.before > 0 ? 100 * c.after / c.before : 100));
    const qint64 percent = c.before > 0 ? (100 * r.saved + (c.before / 2)) / c.before : 0;
    if (r.saved > 0 && percent > 0) {
        r.pill->setStatus(K::Ok, tr("Saved %1%").arg(percent));
    } else {
        r.pill->setStatus(K::Plain, tr("No change"));
    }
}

void CompressList::refresh(const services::BatchRunner& runner)
{
    const bool ours = runner.isCompressing() && runner.files() == m_files;
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        Row& r = m_rows[i];
        if (!ours || static_cast<int>(i) >= runner.total()) {
            r.sizes->hide();
            r.pill->setStatus(StatusPill::Kind::Plain, {});
            r.row->setSubtitle(u"%1 · %2"_s.arg(formatOf(r.path), bytesText(r.before)));
            r.row->setSubtitleError(false);
            r.saved = -1;
            continue;
        }
        showResult(r, runner, static_cast<int>(i));
    }
    if (m_sort == Sort::BiggestSaving) {
        order();
    }
}

void CompressList::select(const QString& path)
{
    if (path == m_selected) {
        return;
    }
    m_selected = path;
    for (const Row& r : m_rows) {
        r.row->setSelected(r.path == path);
    }
    Q_EMIT selectionChanged(path);
}

bool CompressList::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    const int key = static_cast<QKeyEvent*>(event)->key();
    if (key != Qt::Key_Up && key != Qt::Key_Down) {
        return QWidget::eventFilter(watched, event);
    }
    // Up and Down move through the rows in the order shown.
    const int at = m_column->indexOf(static_cast<QWidget*>(watched));
    const int next = at + (key == Qt::Key_Down ? 1 : -1);
    if (const QLayoutItem* item = m_column->itemAt(next); item != nullptr && item->widget() != nullptr) {
        for (const Row& r : m_rows) {
            if (r.row == item->widget()) {
                r.row->setFocus(Qt::TabFocusReason);
                select(r.path);
            }
        }
    }
    return true;
}

} // namespace pixelfx::ui
