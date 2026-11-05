#include "ui/presets_panel.h"

#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/looks_panel.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QEnterEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {
constexpr int kPanelWidth = 272;
constexpr int kThumbWidth = 44;
constexpr int kThumbHeight = 30;
} // namespace

PresetsPanel::PresetsPanel(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_target(new QLabel(this))
    , m_listBox(new QWidget(this))
    , m_list(new QVBoxLayout(m_listBox))
    , m_looks(new LooksPanel(this))
    , m_pages(new QStackedWidget(this))
    , m_tabs(new QButtonGroup(this))
{
    makeSidePanel(this, Qt::RightEdge);
    setFixedWidth(kPanelWidth);

    QHBoxLayout* tabs = buildTabs();
    auto* tabLine = new QWidget(this);
    tabLine->setObjectName(u"panelTabLine"_s);
    tabLine->setAttribute(Qt::WA_StyledBackground);
    tabLine->setFixedHeight(1);

    QHBoxLayout* head = buildHead();
    m_list->setContentsMargins(0, 0, 0, 8);
    m_list->setSpacing(1);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"panelScroll"_s);
    scroll->setWidget(m_listBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listBox->setObjectName(u"panelContent"_s);

    auto* save =
        new IconTextButton(icons::themed(u"plus"_s, tokens().text), tr("Save current as preset"), this);
    save->setToolTip(tr("Keep this photo's settings, or some of them, as a preset"));
    connect(save, &QPushButton::clicked, this, &PresetsPanel::saveRequested);
    auto* footLine = new QWidget(this);
    footLine->setObjectName(u"panelTabLine"_s);
    footLine->setAttribute(Qt::WA_StyledBackground);
    footLine->setFixedHeight(1);
    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(kPanelPadding, 10, kPanelPadding, 10);
    foot->addWidget(save, 1);

    auto* presetsPage = new QWidget(this);
    auto* presetsColumn = new QVBoxLayout(presetsPage);
    presetsColumn->setContentsMargins(0, 0, 0, 0);
    presetsColumn->setSpacing(0);
    presetsColumn->addLayout(head);
    presetsColumn->addWidget(scroll, 1);
    presetsColumn->addWidget(footLine);
    presetsColumn->addLayout(foot);
    m_pages->addWidget(m_looks);
    m_pages->addWidget(presetsPage);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(tabs);
    column->addWidget(tabLine);
    column->addWidget(m_pages, 1);
}

void PresetsPanel::showTab(int index)
{
    m_tabs->button(index)->setChecked(true);
    m_pages->setCurrentIndex(index);
}

int PresetsPanel::currentTab() const
{
    return m_pages->currentIndex();
}

QHBoxLayout* PresetsPanel::buildTabs()
{
    auto* tabs = new QHBoxLayout;
    tabs->setContentsMargins(kToolRowPadding, 8, kToolRowPadding, 0);
    tabs->setSpacing(4);
    for (const QString& name : {tr("Looks"), tr("Presets")}) {
        auto* tab = new QPushButton(name, this);
        tab->setObjectName(u"panelTab"_s);
        tab->setCheckable(true);
        tab->setChecked(m_tabs->buttons().isEmpty());
        m_tabs->addButton(tab, static_cast<int>(m_tabs->buttons().size()));
        tabs->addWidget(tab);
    }
    connect(m_tabs, &QButtonGroup::idClicked, m_pages, &QStackedWidget::setCurrentIndex);
    m_target->setObjectName(u"accentText"_s);
    m_target->hide();
    tabs->addStretch();
    tabs->addWidget(m_target);
    return tabs;
}

QHBoxLayout* PresetsPanel::buildHead()
{
    m_search->setPlaceholderText(tr("Search presets"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(icons::themed(u"search"_s, tokens().muted), QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &PresetsPanel::rebuild);
    auto* import = new QToolButton(this);
    import->setObjectName(u"iconButton"_s);
    import->setIcon(icons::themed(u"import"_s, tokens().text));
    import->setToolTip(tr("Import presets and looks: PixelFx, Lightroom and Camera Raw presets, .cube LUTs"));
    import->setAccessibleName(tr("Import presets and looks"));
    connect(import, &QToolButton::clicked, this, &PresetsPanel::importRequested);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(kToolRowPadding, 10, kToolRowPadding, 6);
    head->setSpacing(6);
    head->addWidget(m_search, 1);
    head->addWidget(import);

    return head;
}

PresetsPanel::~PresetsPanel() = default;

void PresetsPanel::setTargetCount(int count)
{
    m_targetCount = count;
    m_target->setText(tr("Applies to %n photos", nullptr, count));
    m_target->setVisible(count > 1);
}

void PresetsPanel::setEntries(std::vector<core::PresetEntry> entries)
{
    m_entries = std::move(entries);
    m_thumbnails.assign(m_entries.size(), QImage());
    rebuild();
}

QList<PresetRow*> PresetsPanel::rows() const
{
    return m_listBox->findChildren<PresetRow*>();
}

void PresetsPanel::setThumbnail(std::size_t index, const QImage& image)
{
    if (index >= m_thumbnails.size()) {
        return;
    }
    m_thumbnails[index] = image;
    for (PresetRow* row : rows()) {
        if (row->index() == index) {
            row->setThumbnail(image);
        }
    }
}

void PresetsPanel::rebuild()
{
    while (const QLayoutItem* item = m_list->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QString filter = m_search->text().trimmed();
    QString group;
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        const core::PresetEntry& e = m_entries[i];
        if (!filter.isEmpty() && !e.name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        if (e.group != group) {
            group = e.group;
            auto* label = new QLabel(group.toUpper(), m_listBox);
            label->setObjectName(u"section"_s);
            label->setContentsMargins(kPanelPadding, 12, kPanelPadding, 4);
            m_list->addWidget(label);
        }
        auto* row = new PresetRow(i, e, m_listBox);
        row->setThumbnail(m_thumbnails[i]);
        connect(row, &PresetRow::clicked, this, [this, i] { Q_EMIT applyRequested(m_entries[i]); });
        connect(row, &PresetRow::hovered, this,
                [this, i](bool inside) { Q_EMIT previewRequested(inside ? &m_entries[i] : nullptr); });
        connect(row, &PresetRow::menuRequested, this, [this, row] { showMenu(row); });
        m_list->addWidget(row);
    }
    if (m_list->count() == 0) {
        auto* empty =
            new QLabel(filter.isEmpty() ? tr("No presets yet.") : tr("No preset is called that."), m_listBox);
        empty->setObjectName(u"muted"_s);
        empty->setContentsMargins(kPanelPadding, 12, kPanelPadding, 0);
        m_list->addWidget(empty);
    }
    m_list->addStretch();
}

void PresetsPanel::showMenu(PresetRow* row)
{
    const core::PresetEntry& e = m_entries.at(row->index());
    QMenu menu(this);
    menu.addAction(m_targetCount > 1 ? tr("Apply to %n photos", nullptr, m_targetCount) : tr("Apply"), this,
                   [this, e] { Q_EMIT applyRequested(e); });
    if (!e.isBuiltIn()) {
        menu.addAction(tr("Rename..."), this, [this, e] { Q_EMIT renameRequested(e); });
        menu.addAction(tr("Update with current settings"), this, [this, e] { Q_EMIT updateRequested(e); });
    }
    if (!e.autoEnhance) {
        menu.addAction(tr("Duplicate"), this, [this, e] { Q_EMIT duplicateRequested(e); });
        menu.addAction(tr("Export..."), this, [this, e] { Q_EMIT exportRequested(e); });
    }
    if (!e.isBuiltIn()) {
        menu.addAction(tr("Show file"), this, [this, e] { Q_EMIT showFileRequested(e); });
        menu.addSeparator();
        menu.addAction(tr("Delete..."), this, [this, e] { Q_EMIT deleteRequested(e); });
    }
    menu.exec(row->menuButton()->mapToGlobal(QPoint(0, row->menuButton()->height())));
}

// ---------------------------------------------------------------------------------------------

PresetRow::PresetRow(std::size_t index, const core::PresetEntry& entry, QWidget* parent)
    : QWidget(parent)
    , m_index(index)
    , m_name(entry.name)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(entry.name);
    const QString summary =
        entry.description.isEmpty() ? core::presetSummary(entry.settings) : entry.description;
    setToolTip(tr("Apply %1. Hover to see it on the photo.").arg(entry.name));
    auto* name = new QLabel(entry.name, this);
    name->setObjectName(u"itemTitle"_s);
    auto* what = new QLabel(summary, this);
    what->setObjectName(u"itemDetail"_s);
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    text->addWidget(name);
    text->addWidget(what);
    auto* more = new QToolButton(this);
    more->setObjectName(u"flatIcon"_s);
    more->setIcon(icons::themed(u"more"_s, tokens().muted));
    more->setToolTip(tr("More for %1").arg(entry.name));
    more->setAccessibleName(tr("More for %1").arg(entry.name));
    connect(more, &QToolButton::clicked, this, &PresetRow::menuRequested);
    m_more = more;
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(18 + kThumbWidth + 10, 8, 14, 9);
    row->setSpacing(10);
    row->addLayout(text, 1);
    row->addWidget(more);
    setMinimumHeight(kThumbHeight + 14);
}

PresetRow::~PresetRow() = default;

void PresetRow::setThumbnail(const QImage& image)
{
    m_thumbnail = image;
    update();
}

void PresetRow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(8, 0, -8, 0);
    if (m_hover || hasFocus()) {
        p.setPen(Qt::NoPen);
        p.setBrush(tokens().hover);
        p.drawRoundedRect(box, 7, 7);
    }
    const QRectF thumb(box.left() + 10, (height() - kThumbHeight) / 2.0, kThumbWidth, kThumbHeight);
    QPainterPath clip;
    clip.addRoundedRect(thumb, 4, 4);
    p.setClipPath(clip);
    p.fillRect(thumb, tokens().input);
    if (!m_thumbnail.isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QSizeF source = QSizeF(thumb.size()).scaled(m_thumbnail.size(), Qt::KeepAspectRatio);
        p.drawImage(thumb, m_thumbnail,
                    QRectF((m_thumbnail.width() - source.width()) / 2,
                           (m_thumbnail.height() - source.height()) / 2, source.width(), source.height()));
    }
}

void PresetRow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked();
    }
}

void PresetRow::enterEvent(QEnterEvent* /*event*/)
{
    m_hover = true;
    update();
    Q_EMIT hovered(true);
}

void PresetRow::leaveEvent(QEvent* /*event*/)
{
    m_hover = false;
    update();
    Q_EMIT hovered(false);
}

void PresetRow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        Q_EMIT clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace pixelfx::ui
