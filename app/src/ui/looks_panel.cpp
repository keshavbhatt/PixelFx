#include "ui/looks_panel.h"

#include "ui/flow_layout.h"
#include "ui/hover_preview.h"
#include "ui/icons.h"
#include "ui/photo_tile.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QGridLayout>
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
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

// The category chips.
struct Chip
{
    const char* label;
    const char* category; // "" all, "mine" the user's
};
constexpr std::size_t kFavoritesChip = 5;
constexpr std::array<Chip, 6> kChips = {{
    {QT_TRANSLATE_NOOP("LooksPanel", "All"), ""},
    {QT_TRANSLATE_NOOP("LooksPanel", "Mine"), "mine"},
    {QT_TRANSLATE_NOOP("LooksPanel", "Color"), "color"},
    {QT_TRANSLATE_NOOP("LooksPanel", "Film"), "film"},
    {QT_TRANSLATE_NOOP("LooksPanel", "B&&W"), "bw"}, // && shows one ampersand on a button
    {QT_TRANSLATE_NOOP("LooksPanel", "Favorites"), "favorites"},
}};

} // namespace

LooksPanel::LooksPanel(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_chips(new QButtonGroup(this))
    , m_listBox(new QWidget(this))
    , m_list(new QVBoxLayout(m_listBox))
    , m_hover(new HoverPreview(this))
{
    connect(m_hover, &HoverPreview::started, this, [this](int index) {
        const QString& id = m_tileIds.at(static_cast<std::size_t>(index));
        Q_EMIT previewRequested(id, id.isEmpty() ? 0 : defaultStrength(id));
    });
    connect(m_hover, &HoverPreview::ended, this, &LooksPanel::previewEnded);
    m_search->setPlaceholderText(tr("Search looks"));
    m_search->setFixedHeight(30); // the mocks' search fields
    m_search->setClearButtonEnabled(true);
    m_search->addAction(icons::themed(u"search"_s, tokens().muted), QLineEdit::LeadingPosition);
    connect(m_search, &QLineEdit::textChanged, this, &LooksPanel::rebuild);
    auto* add = new QToolButton(this);
    add->setObjectName(u"iconButton"_s);
    add->setIcon(icons::themed(u"plus"_s, tokens().text));
    add->setToolTip(tr("New look (Ctrl+Shift+N)"));
    add->setAccessibleName(tr("New look"));
    connect(add, &QToolButton::clicked, this, &LooksPanel::newLookRequested);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(kToolRowPadding, 10, kToolRowPadding, 10);
    head->setSpacing(kGapRelated);
    head->addWidget(m_search, 1);
    head->addWidget(add);

    auto* chipBox = new QWidget(this);
    auto* chips = new FlowLayout(chipBox, 6);
    chips->setContentsMargins(kToolRowPadding, 6, kToolRowPadding, 6);
    for (std::size_t i = 0; i < kChips.size(); ++i) {
        auto* chip = new QPushButton(tr(kChips.at(i).label), this);
        chip->setObjectName(u"chip"_s);
        chip->setCheckable(true);
        chip->setChecked(i == 0);
        m_chips->addButton(chip, static_cast<int>(i));
        chips->addWidget(chip);
    }
    connect(m_chips, &QButtonGroup::idClicked, this, &LooksPanel::rebuild);

    m_list->setContentsMargins(0, 0, 0, 8);
    m_list->setSpacing(0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"panelScroll"_s);
    scroll->setWidget(m_listBox);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    overlayScrollBar(scroll); // over the panel's padding, as the mocks draw it
    m_listBox->setObjectName(u"panelContent"_s);

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addLayout(head);
    column->addWidget(chipBox);
    column->addWidget(scroll, 1);
    column->addWidget(buildStrength());
}

LooksPanel::~LooksPanel() = default;

QSlider* LooksPanel::strengthSlider() const
{
    return m_strength->slider();
}

QWidget* LooksPanel::buildStrength()
{
    auto* box = new QWidget(this);
    box->setObjectName(u"lookStrength"_s);
    box->setAttribute(Qt::WA_StyledBackground);
    // The look the slider belongs to; during a preview it says the applied one has not changed.
    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(kGapRelated);
    auto* label = new QLabel(tr("Applied"), box);
    label->setObjectName(u"adjustName"_s);
    m_appliedName = new QLabel(box);
    m_appliedName->setObjectName(u"itemTitle"_s);
    nameRow->addWidget(label);
    nameRow->addStretch();
    nameRow->addWidget(m_appliedName);
    m_strength = new SliderRow(tr("Look strength"), box);
    m_strength->setRange(0, 100, 100);
    // Live while dragging; one history step for a drag, a key, a click, a typed value or a reset.
    connect(m_strength, &SliderRow::ticksMoved, this, [this](int v) {
        if (m_strength->slider()->isSliderDown()) {
            Q_EMIT strengthMoved(v);
        }
    });
    connect(m_strength, &SliderRow::ticksReleased, this, &LooksPanel::strengthReleased);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(kPanelPadding, 8, kPanelPadding, 4);
    column->setSpacing(0);
    column->addLayout(nameRow);
    column->addWidget(m_strength);
    setCurrent({}, 0);
    return box;
}

int LooksPanel::defaultStrength(const QString& id) const
{
    for (const core::LookEntry& e : m_entries) {
        if (e.id == id && e.look) {
            return e.look->defaultStrength;
        }
    }
    return 100;
}

bool LooksPanel::isPreviewing() const
{
    return m_hover->isPreviewing();
}

void LooksPanel::endPreview()
{
    m_hover->end();
}

void LooksPanel::setEntries(std::vector<core::LookEntry> entries)
{
    m_entries = std::move(entries);
    m_thumbnails.assign(m_entries.size(), QImage());
    rebuild();
}

void LooksPanel::setCurrent(const QString& id, double strength)
{
    const bool changed = id != m_current;
    m_current = id;
    // The look's own strength is the default: the value shows in the accent once it is moved
    // from it, and a double-click on the name goes back to it.
    int standard = 100;
    for (const core::LookEntry& e : m_entries) {
        if (e.id == id && e.look) {
            standard = e.look->defaultStrength;
        }
    }
    m_strength->setRange(0, 100, standard);
    m_strength->setTicks(static_cast<int>(std::lround(strength)));
    m_strength->setEnabled(!id.isEmpty());
    QString name = tr("None");
    for (const core::LookEntry& e : m_entries) {
        if (e.id == id) {
            name = e.name;
        }
    }
    m_appliedName->setText(name);
    if (changed) {
        for (PhotoTile* tile : visibleTiles()) {
            tile->setSelected(tile->id() == id);
        }
    }
}

void LooksPanel::setThumbnail(int index, const QImage& image)
{
    if (index < 0) {
        m_original = image;
    } else if (static_cast<std::size_t>(index) < m_thumbnails.size()) {
        m_thumbnails[static_cast<std::size_t>(index)] = image;
    } else {
        return;
    }
    const QString id = index < 0 ? QString() : m_entries[static_cast<std::size_t>(index)].id;
    for (PhotoTile* tile : visibleTiles()) {
        if (tile->id() == id) {
            tile->setImage(image);
        }
    }
}

std::vector<PhotoTile*> LooksPanel::visibleTiles() const
{
    const QList<PhotoTile*> tiles = m_listBox->findChildren<PhotoTile*>();
    return {tiles.begin(), tiles.end()};
}

bool LooksPanel::matches(const core::LookEntry& e) const
{
    const QString category =
        QLatin1StringView(kChips.at(static_cast<std::size_t>(std::max(0, m_chips->checkedId()))).category);
    if (category == u"favorites"_s) {
        if (!m_favorites.contains(e.id)) {
            return false;
        }
    } else if (category == u"mine"_s ? !e.isMine() : (!category.isEmpty() && e.category != category)) {
        return false;
    }
    const QString needle = m_search->text().trimmed();
    return needle.isEmpty() || e.name.contains(needle, Qt::CaseInsensitive);
}

void LooksPanel::addSection(const QString& title, QWidget* extra, const std::vector<int>& indexes,
                            bool withOriginal)
{
    m_list->addLayout(sectionHeadingRow(sectionLabel(title, m_listBox), extra, kPanelPadding));
    std::vector<PhotoTile*> tiles;
    const auto place = [&](PhotoTile* tile) {
        tile->setSelected(tile->id() == m_current);
        connect(tile, &PhotoTile::clicked, this, [this, tile] {
            m_hover->end();
            Q_EMIT lookChosen(tile->id(), defaultStrength(tile->id()));
        });
        const auto index = static_cast<int>(m_tileIds.size());
        m_tileIds.push_back(tile->id());
        connect(tile, &PhotoTile::hovered, this,
                [this, tile, index](bool inside) { m_hover->hover(tile, index, inside); });
        tiles.push_back(tile);
    };
    if (withOriginal) {
        auto* original = new PhotoTile({}, tr("Original"), PhotoTile::Badge::None, m_listBox);
        original->setImage(m_original);
        place(original);
    }
    for (const int i : indexes) {
        const core::LookEntry& e = m_entries[static_cast<std::size_t>(i)];
        // The star says Favorites; the section already says whose look it is.
        const PhotoTile::Badge badge = m_favorites.contains(e.id) ? PhotoTile::Badge::Favorite
                                       : e.isMine()               ? PhotoTile::Badge::Mine
                                                                  : PhotoTile::Badge::None;
        auto* tile = new PhotoTile(e.id, e.name, badge, m_listBox);
        tile->setImage(m_thumbnails[static_cast<std::size_t>(i)]);
        if (e.isMine()) {
            tile->setToolTip(tr("%1. Right-click to edit, export or delete.").arg(e.name));
        }
        connect(tile, &PhotoTile::menuRequested, this,
                [this, e](const QPoint& where) { showMenu(e, where); });
        connect(tile, &PhotoTile::favoriteRequested, this, [this, id = e.id] { toggleFavorite(id); });
        place(tile);
    }
    m_list->addLayout(tileGrid(tiles, m_listBox));
}

void LooksPanel::rebuild()
{
    m_hover->end(); // its tile goes with the list
    m_tileIds.clear();
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
    std::vector<int> mine;
    std::vector<int> builtIn;
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (matches(m_entries[i])) {
            (m_entries[i].isMine() ? mine : builtIn).push_back(static_cast<int>(i));
        }
    }
    const bool all = m_chips->checkedId() <= 0 && m_search->text().trimmed().isEmpty();
    if (!mine.empty() || all || m_chips->checkedId() == 1) {
        auto* make = new QPushButton(tr("New look"), m_listBox);
        make->setObjectName(u"link"_s);
        make->setToolTip(tr("Make a look of your own from steps and LUTs"));
        connect(make, &QPushButton::clicked, this, &LooksPanel::newLookRequested);
        addSection(tr("My looks"), make, mine, false);
    }
    if (!builtIn.empty() || all) {
        addSection(tr("Built in"), nullptr, builtIn, all);
    }
    if (m_chips->checkedId() == static_cast<int>(kFavoritesChip) && mine.empty() && builtIn.empty() &&
        m_search->text().trimmed().isEmpty()) {
        auto* empty = new QLabel(tr("No favourites yet. Press F on a look, or use its menu, to add it here."),
                                 m_listBox);
        empty->setObjectName(u"muted"_s);
        empty->setWordWrap(true);
        empty->setContentsMargins(kPanelPadding, 12, kPanelPadding, 0);
        m_list->addWidget(empty);
    }
    m_list->addStretch();
}

void LooksPanel::setFavorites(const QStringList& ids)
{
    m_favorites = ids;
    rebuild();
}

void LooksPanel::toggleFavorite(const QString& id)
{
    if (m_favorites.contains(id)) {
        m_favorites.removeAll(id);
    } else {
        m_favorites.append(id);
    }
    Q_EMIT favoritesChanged(m_favorites);
    rebuild(); // the star, and the Favorites chip's list
}

void LooksPanel::showMenu(const core::LookEntry& e, const QPoint& where)
{
    QMenu menu(this);
    const bool favorite = m_favorites.contains(e.id);
    QAction* star = menu.addAction(icons::themed(u"star"_s, tokens().text),
                                   favorite ? tr("Remove from Favorites") : tr("Add to Favorites"), this,
                                   [this, id = e.id] { toggleFavorite(id); });
    star->setShortcut(QKeySequence(Qt::Key_F));
    if (e.isMine()) {
        menu.addSeparator();
        menu.addAction(tr("Edit look..."), this, [this, id = e.id] { Q_EMIT editLookRequested(id); });
        menu.addAction(tr("Export look..."), this, [this, id = e.id] { Q_EMIT exportLookRequested(id); });
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, [this, id = e.id] { Q_EMIT deleteLookRequested(id); });
    }
    menu.exec(where);
}

} // namespace pixelfx::ui
