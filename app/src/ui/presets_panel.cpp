#include "ui/presets_panel.h"

#include "ui/icon_text_button.h"
#include "ui/icons.h"
#include "ui/looks_panel.h"
#include "ui/photo_tile.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kPanelWidth = 272;
constexpr std::size_t kFirstTiles = 6; // per open category: three rows

enum Filter
{
    All,
    Favorites,
    Mine,
    Recent,
};

constexpr std::array<const char*, 4> kChipNames = {
    QT_TRANSLATE_NOOP("PresetsPanel", "All"), QT_TRANSLATE_NOOP("PresetsPanel", "Favorites"),
    QT_TRANSLATE_NOOP("PresetsPanel", "Mine"), QT_TRANSLATE_NOOP("PresetsPanel", "Recent")};

} // namespace

PresetsPanel::PresetsPanel(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_target(new QLabel(this))
    , m_chips(new QButtonGroup(this))
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

    m_list->setContentsMargins(0, 0, 0, 8);
    m_list->setSpacing(kGapTight);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"panelScroll"_s);
    scroll->setWidget(m_listBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listBox->setObjectName(u"panelContent"_s);

    auto* presetsPage = new QWidget(this);
    auto* presetsColumn = new QVBoxLayout(presetsPage);
    presetsColumn->setContentsMargins(0, 0, 0, 0);
    presetsColumn->setSpacing(0);
    presetsColumn->addLayout(buildHead());
    presetsColumn->addWidget(buildChips());
    presetsColumn->addWidget(buildBackRow());
    presetsColumn->addWidget(scroll, 1);
    presetsColumn->addWidget(buildFoot());
    m_pages->addWidget(m_looks);
    m_pages->addWidget(presetsPage);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(tabs);
    column->addWidget(tabLine);
    column->addWidget(m_pages, 1);
}

PresetsPanel::~PresetsPanel() = default;

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
    tabs->setSpacing(kGapSmall);
    for (const QString& name : {tr("Looks"), tr("Presets")}) {
        auto* tab = new QPushButton(name, this);
        tab->setObjectName(u"panelTab"_s);
        tab->setCheckable(true);
        tab->setChecked(m_tabs->buttons().isEmpty());
        m_tabs->addButton(tab, static_cast<int>(m_tabs->buttons().size()));
        tabs->addWidget(tab);
    }
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int index) {
        m_pages->setCurrentIndex(index);
        if (index == 1) {
            Q_EMIT thumbnailsWanted();
        }
    });
    m_target->setObjectName(u"accentText"_s);
    m_target->hide();
    tabs->addStretch();
    tabs->addWidget(m_target);
    return tabs;
}

QHBoxLayout* PresetsPanel::buildHead()
{
    m_search->setObjectName(u"presetSearch"_s);
    m_search->setPlaceholderText(tr("Search presets"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(icons::themed(u"search"_s, tokens().muted), QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &PresetsPanel::rebuild);
    auto* import = new QToolButton(this);
    import->setObjectName(u"iconButton"_s);
    import->setIcon(icons::themed(u"import"_s, tokens().text));
    import->setToolTip(tr("Import presets: PixelFx, Lightroom and Camera Raw presets, .cube LUTs"));
    import->setAccessibleName(tr("Import presets"));
    connect(import, &QToolButton::clicked, this, &PresetsPanel::importRequested);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(kToolRowPadding, 10, kToolRowPadding, 6);
    head->setSpacing(kGapRelated);
    head->addWidget(m_search, 1);
    head->addWidget(import);
    return head;
}

QWidget* PresetsPanel::buildChips()
{
    m_chipRow = new QWidget(this);
    auto* row = new QHBoxLayout(m_chipRow);
    row->setContentsMargins(kToolRowPadding, 0, kToolRowPadding, 6);
    row->setSpacing(kGapRelated);
    for (std::size_t i = 0; i < kChipNames.size(); ++i) {
        auto* chip = new QPushButton(tr(kChipNames.at(i)), m_chipRow);
        chip->setObjectName(u"chip"_s);
        chip->setCheckable(true);
        chip->setChecked(i == 0);
        m_chips->addButton(chip, static_cast<int>(i));
        row->addWidget(chip);
    }
    row->addStretch();
    connect(m_chips, &QButtonGroup::idClicked, this, [this] {
        m_category.clear();
        rebuild();
    });
    return m_chipRow;
}

QWidget* PresetsPanel::buildBackRow()
{
    m_backRow = new QWidget(this);
    auto* row = new QHBoxLayout(m_backRow);
    row->setContentsMargins(kToolRowPadding, 0, kPanelPadding, 4);
    row->setSpacing(kGapSmall);
    auto* back =
        new IconTextButton(icons::themed(u"chevron-left"_s, tokens().text), tr("All presets"), m_backRow);
    back->setObjectName(u"flatSmall"_s);
    back->setToolTip(tr("Back to every category (Backspace)"));
    back->setShortcut(QKeySequence(Qt::Key_Backspace));
    connect(back, &QPushButton::clicked, this, [this] { showCategory({}); });
    m_backCount = new QLabel(m_backRow);
    m_backCount->setObjectName(u"muted"_s);
    row->addWidget(back);
    row->addStretch();
    row->addWidget(m_backCount);
    m_backRow->hide();
    return m_backRow;
}

QWidget* PresetsPanel::buildFoot()
{
    auto* foot = new QWidget(this);
    foot->setObjectName(u"footBar"_s);
    foot->setAttribute(Qt::WA_StyledBackground);
    auto* column = new QVBoxLayout(foot);
    column->setContentsMargins(kPanelPadding, 10, kPanelPadding, 10);
    column->setSpacing(kGapRelated);
    m_appliedBox = new QWidget(foot);
    auto* applied = new QVBoxLayout(m_appliedBox);
    applied->setContentsMargins(0, 0, 0, 0);
    applied->setSpacing(0);
    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(kGapRelated);
    auto* label = new QLabel(tr("Applied"), m_appliedBox);
    label->setObjectName(u"adjustName"_s);
    m_appliedName = new QLabel(m_appliedBox);
    m_appliedName->setObjectName(u"itemTitle"_s);
    m_appliedName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    nameRow->addWidget(label);
    nameRow->addStretch();
    nameRow->addWidget(m_appliedName);
    m_amount = new SliderRow(tr("Amount"), m_appliedBox);
    m_amount->setRange(0, 200, 100);
    m_amount->setFormat([](int ticks) { return QString::number(ticks); });
    m_amount->setToolTip(tr("Blends from the photo before the preset (0) to the preset (100) and past it "
                            "(200). Double-click for 100."));
    connect(m_amount, &SliderRow::ticksMoved, this, [this](int ticks) { Q_EMIT amountMoved(ticks); });
    connect(m_amount, &SliderRow::ticksReleased, this, [this](int ticks) { Q_EMIT amountReleased(ticks); });
    applied->addLayout(nameRow);
    applied->addWidget(m_amount);
    m_appliedBox->hide();
    auto* save =
        new IconTextButton(icons::themed(u"plus"_s, tokens().text), tr("Save current as preset"), foot);
    save->setToolTip(tr("Keep this photo's settings, or some of them, as a preset"));
    connect(save, &QPushButton::clicked, this, &PresetsPanel::saveRequested);
    column->addWidget(m_appliedBox);
    column->addWidget(save);
    return foot;
}

void PresetsPanel::setTargetCount(int count)
{
    m_targetCount = count;
    m_target->setText(tr("Applies to %n photos", nullptr, count));
    m_target->setVisible(count > 1);
}

void PresetsPanel::setEntries(std::vector<core::PresetEntry> entries)
{
    m_entries = std::move(entries);
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id.isEmpty()) {
            m_entries[i].id = u"entry:%1"_s.arg(i); // made in code, never saved
        }
    }
    m_thumbnails.assign(m_entries.size(), QImage());
    rebuild();
}

void PresetsPanel::setThumbnail(std::size_t index, const QImage& image)
{
    if (index >= m_thumbnails.size()) {
        return;
    }
    m_thumbnails[index] = image;
    const QString& id = m_entries[index].id;
    for (PhotoTile* tile : tiles()) {
        if (tile->id() == id) {
            tile->setImage(image);
        }
    }
}

void PresetsPanel::clearThumbnails()
{
    std::ranges::fill(m_thumbnails, QImage());
}

bool PresetsPanel::hasThumbnail(std::size_t index) const
{
    return index < m_thumbnails.size() && !m_thumbnails[index].isNull();
}

std::vector<PhotoTile*> PresetsPanel::tiles() const
{
    return m_tiles;
}

std::vector<std::size_t> PresetsPanel::shownIndexes() const
{
    std::vector<std::size_t> out;
    for (const PhotoTile* tile : tiles()) {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == tile->id()) {
                out.push_back(i);
            }
        }
    }
    return out;
}

void PresetsPanel::setFavorites(const QStringList& ids)
{
    m_favorites = ids;
    for (PhotoTile* tile : tiles()) {
        tile->setBadge(m_favorites.contains(tile->id()) ? PhotoTile::Badge::Favorite
                                                        : PhotoTile::Badge::None);
    }
    if (m_chips->checkedId() == Favorites) {
        rebuild();
    }
}

void PresetsPanel::setRecent(const QStringList& ids)
{
    m_recent = ids;
    if (m_chips->checkedId() == Recent) {
        rebuild();
    }
}

void PresetsPanel::setClosedCategories(const QStringList& names)
{
    m_closed = names;
    rebuild();
}

void PresetsPanel::showCategory(const QString& name)
{
    m_category = name;
    if (!name.isEmpty()) {
        m_chips->button(All)->setChecked(true);
    }
    rebuild();
}

void PresetsPanel::showFilter(int filter)
{
    m_chips->button(filter)->setChecked(true);
    m_category.clear();
    rebuild();
}

void PresetsPanel::setApplied(const QString& id, const QString& name, double amount)
{
    m_appliedId = id;
    m_appliedName->setText(name);
    m_amount->setTicks(qRound(amount));
    m_appliedBox->setVisible(!name.isEmpty());
    for (PhotoTile* tile : tiles()) {
        tile->setSelected(!id.isEmpty() && tile->id() == id);
    }
}

bool PresetsPanel::matches(const core::PresetEntry& e) const
{
    const QString filter = m_search->text().trimmed();
    return filter.isEmpty() || e.name.contains(filter, Qt::CaseInsensitive) ||
           e.group.contains(filter, Qt::CaseInsensitive) ||
           e.description.contains(filter, Qt::CaseInsensitive);
}

std::vector<std::size_t> PresetsPanel::indexesOf(const QStringList& ids) const
{
    std::vector<std::size_t> out;
    for (const QString& id : ids) {
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id == id && matches(m_entries[i])) {
                out.push_back(i);
            }
        }
    }
    return out;
}

void PresetsPanel::clearList()
{
    m_tiles.clear();
    // Later, not now: a tile may be the one whose menu or click led here.
    const auto retire = [](QWidget* w) {
        if (w != nullptr) {
            w->hide();
            w->setParent(nullptr);
            w->deleteLater();
        }
    };
    while (QLayoutItem* item = m_list->takeAt(0)) {
        if (QLayout* layout = item->layout()) {
            while (const QLayoutItem* inner = layout->takeAt(0)) {
                retire(inner->widget());
                delete inner;
            }
        }
        retire(item->widget());
        delete item;
    }
}

void PresetsPanel::rebuild()
{
    clearList();
    const bool inCategory = !m_category.isEmpty();
    const bool searching = !m_search->text().trimmed().isEmpty();
    m_chipRow->setVisible(!inCategory);
    m_backRow->setVisible(inCategory);
    m_search->setPlaceholderText(inCategory
                                     ? tr("Search %1").arg(m_category)
                                     : tr("Search %n presets", nullptr, static_cast<int>(m_entries.size())));
    const int filter = m_chips->checkedId();
    if (filter == Favorites || filter == Recent) {
        addSection(tr(kChipNames.at(static_cast<std::size_t>(filter))),
                   indexesOf(filter == Favorites ? m_favorites : m_recent), false, false);
    } else {
        addGroupSections(filter == Mine, searching);
    }
    if (m_list->count() == 0) {
        addEmptyNote(filter, searching);
    }
    m_list->addStretch();
    Q_EMIT thumbnailsWanted();
}

void PresetsPanel::addGroupSections(bool mineOnly, bool searching)
{
    const bool inCategory = !m_category.isEmpty();
    // In panel order: the user's groups, then the catalog's categories (entries() order).
    QStringList groups;
    for (const core::PresetEntry& e : m_entries) {
        if (!groups.contains(e.group)) {
            groups << e.group;
        }
    }
    for (const QString& group : groups) {
        const bool builtIn = std::ranges::any_of(
            m_entries, [&group](const core::PresetEntry& e) { return e.group == group && e.isBuiltIn(); });
        if ((mineOnly && builtIn) || (inCategory && group != m_category)) {
            continue;
        }
        std::vector<std::size_t> indexes;
        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].group == group && matches(m_entries[i])) {
                indexes.push_back(i);
            }
        }
        if (!indexes.empty()) {
            const bool browsing = !inCategory && !searching;
            addSection(group, indexes, browsing, builtIn && browsing);
        }
    }
    if (inCategory) {
        m_backCount->setText(QString::number(m_tiles.size()));
    }
}

void PresetsPanel::addEmptyNote(int filter, bool searching)
{
    QString text = tr("No preset is called that.");
    if (filter == Favorites && !searching) {
        text = tr("No favourites yet. Press F on a preset, or use its menu, to add it here.");
    } else if (filter == Recent && !searching) {
        text = tr("The presets you apply show here.");
    } else if (filter == Mine && !searching) {
        text = tr("No presets of your own yet. Save current as preset makes one.");
    }
    auto* empty = new QLabel(text, m_listBox);
    empty->setObjectName(u"muted"_s);
    empty->setWordWrap(true);
    empty->setContentsMargins(kPanelPadding, 12, kPanelPadding, 0);
    m_list->addWidget(empty);
}

void PresetsPanel::addSection(const QString& title, const std::vector<std::size_t>& indexes, bool collapsible,
                              bool limited)
{
    const bool open = !collapsible || !m_closed.contains(title);
    QWidget* trailing = nullptr;
    if (limited && open && indexes.size() > kFirstTiles) {
        auto* all = new QPushButton(tr("All %1").arg(indexes.size()), m_listBox);
        all->setObjectName(u"link"_s);
        all->setToolTip(tr("Show every preset in %1").arg(title));
        connect(all, &QPushButton::clicked, this, [this, title] { showCategory(title); });
        trailing = all;
    } else if (collapsible || !m_category.isEmpty()) {
        auto* count = new QLabel(QString::number(indexes.size()), m_listBox);
        count->setObjectName(u"muted"_s);
        trailing = count;
    }
    if (collapsible) {
        // The whole heading opens or closes the category.
        auto* heading = new QPushButton(icons::themed(open ? u"expanded"_s : u"collapsed"_s, tokens().muted),
                                        title.toUpper(), m_listBox);
        heading->setObjectName(u"categoryHeading"_s);
        heading->setIconSize(QSize(14, 14));
        QFont font = heading->font();
        font.setLetterSpacing(QFont::AbsoluteSpacing, 0.66); // as sectionLabel spaces its capitals
        heading->setFont(font);
        heading->setAccessibleName(title);
        heading->setToolTip(open ? tr("Close %1").arg(title) : tr("Open %1").arg(title));
        connect(heading, &QPushButton::clicked, this, [this, title, open] {
            open ? m_closed.append(title) : static_cast<void>(m_closed.removeAll(title));
            Q_EMIT closedCategoriesChanged(m_closed);
            rebuild();
        });
        auto* row = new QHBoxLayout;
        row->setContentsMargins(kToolRowPadding, 8, kPanelPadding, 2);
        row->addWidget(heading);
        row->addStretch();
        if (trailing != nullptr) {
            row->addWidget(trailing);
        }
        m_list->addLayout(row);
    } else {
        m_list->addLayout(sectionHeadingRow(sectionLabel(title, m_listBox), trailing, kPanelPadding));
    }
    if (!open) {
        return;
    }
    std::vector<PhotoTile*> shown;
    const std::size_t limit = limited ? kFirstTiles : indexes.size();
    for (std::size_t k = 0; k < indexes.size() && k < limit; ++k) {
        shown.push_back(makeTile(indexes[k]));
        m_tiles.push_back(shown.back());
    }
    m_list->addLayout(tileGrid(shown, m_listBox));
}

PhotoTile* PresetsPanel::makeTile(std::size_t index)
{
    const core::PresetEntry& e = m_entries[index];
    auto* tile = new PhotoTile(
        e.id, e.name, m_favorites.contains(e.id) ? PhotoTile::Badge::Favorite : PhotoTile::Badge::None,
        m_listBox);
    tile->setTwoLineName(true);
    tile->setDescription(e.description.isEmpty() ? core::presetSummary(e.settings) : e.description);
    tile->setImage(m_thumbnails[index]);
    tile->setSelected(!m_appliedId.isEmpty() && e.id == m_appliedId);
    connect(tile, &PhotoTile::clicked, this, [this, index] { Q_EMIT applyRequested(m_entries[index]); });
    connect(tile, &PhotoTile::hovered, this,
            [this, index](bool inside) { Q_EMIT previewRequested(inside ? &m_entries[index] : nullptr); });
    connect(tile, &PhotoTile::favoriteRequested, this, [this, index] { toggleFavorite(index); });
    connect(tile, &PhotoTile::menuRequested, this,
            [this, index](const QPoint& where) { showMenu(index, where); });
    return tile;
}

void PresetsPanel::toggleFavorite(std::size_t index)
{
    const QString& id = m_entries[index].id;
    if (m_favorites.contains(id)) {
        m_favorites.removeAll(id);
    } else {
        m_favorites.append(id);
    }
    Q_EMIT favoritesChanged(m_favorites);
    setFavorites(m_favorites);
}

void PresetsPanel::showMenu(std::size_t index, const QPoint& where)
{
    const core::PresetEntry e = m_entries.at(index);
    QMenu menu(this);
    menu.addAction(icons::themed(u"check"_s, tokens().text), tr("Apply"), this,
                   [this, e] { Q_EMIT applyRequested(e); });
    if (m_targetCount > 1) {
        menu.addAction(icons::themed(u"images"_s, tokens().text),
                       tr("Apply to %n selected photos", nullptr, m_targetCount), this,
                       [this, e] { Q_EMIT applyRequested(e); });
    }
    menu.addSeparator();
    const bool favorite = m_favorites.contains(e.id);
    QAction* star = menu.addAction(icons::themed(u"star"_s, tokens().text),
                                   favorite ? tr("Remove from Favorites") : tr("Add to Favorites"), this,
                                   [this, index] { toggleFavorite(index); });
    star->setShortcut(QKeySequence(Qt::Key_F));
    if (!e.isBuiltIn()) {
        menu.addAction(tr("Rename..."), this, [this, e] { Q_EMIT renameRequested(e); });
        menu.addAction(tr("Update with current settings"), this, [this, e] { Q_EMIT updateRequested(e); });
    }
    if (!e.autoEnhance) {
        menu.addAction(icons::themed(u"copy"_s, tokens().text),
                       e.isBuiltIn() ? tr("Copy to My presets") : tr("Duplicate"), this,
                       [this, e] { Q_EMIT duplicateRequested(e); });
        menu.addAction(icons::themed(u"share"_s, tokens().text), tr("Export..."), this,
                       [this, e] { Q_EMIT exportRequested(e); });
    }
    if (!e.isBuiltIn()) {
        menu.addAction(tr("Show file"), this, [this, e] { Q_EMIT showFileRequested(e); });
        menu.addSeparator();
        menu.addAction(tr("Delete..."), this, [this, e] { Q_EMIT deleteRequested(e); });
    }
    menu.addSeparator();
    menu.addSection(e.name);
    const QString about = e.description.isEmpty()
                              ? core::presetSummary(e.settings)
                              : u"%1. %2."_s.arg(e.description, core::presetSummary(e.settings));
    QAction* info = menu.addAction(about);
    info->setEnabled(false);
    menu.exec(where);
}

} // namespace pixelfx::ui
