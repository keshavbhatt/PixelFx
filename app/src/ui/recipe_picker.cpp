#include "ui/recipe_picker.h"

#include "services/thumbnailer.h"
#include "ui/icons.h"
#include "ui/photo_tile.h"
#include "ui/style.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <pfx/auto.h>
#include <utility>

using namespace Qt::StringLiterals;

namespace pixelfx::ui {

namespace {

constexpr int kColumns = 5;       // tiles as wide as the editor's
constexpr int kKeySpan = 10000;   // render keys: generation * span + tile
constexpr int kMostResults = 120; // a search shows at most this many tiles
constexpr int kCountRole = Qt::UserRole + 1;
constexpr int kHeadingRole = Qt::UserRole + 2;

// What a picker of each mode last showed, kept while the app runs.
struct Remembered
{
    QString group;
    QString search;
};

Remembered& remembered(RecipePicker::Mode mode)
{
    static std::array<Remembered, 2> kept;
    return kept.at(mode == RecipePicker::Mode::Recipe ? 0 : 1);
}

// The groups' list: a count at the right of each group, and the LOOKS and PRESETS headings.
class GroupDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const Tokens& t = tokens();
        if (index.data(kHeadingRole).toBool()) {
            QFont caps = option.font;
            caps.setPixelSize(11);
            caps.setBold(true);
            caps.setLetterSpacing(QFont::AbsoluteSpacing, 0.66);
            painter->setFont(caps);
            painter->setPen(t.muted);
            painter->drawText(option.rect.adjusted(14, 0, -10, -2), Qt::AlignLeft | Qt::AlignBottom,
                              index.data().toString().toUpper());
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
        if (const QString count = index.data(kCountRole).toString(); !count.isEmpty()) {
            QFont small = option.font;
            small.setPixelSize(12);
            painter->setFont(small);
            painter->setPen(t.muted);
            painter->drawText(option.rect.adjusted(0, 0, -16, 0), Qt::AlignRight | Qt::AlignVCenter, count);
        }
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(index.data(kHeadingRole).toBool() ? 30 : 32);
        return size;
    }
};

} // namespace

RecipePicker::RecipePicker(Mode mode, const std::vector<core::PresetEntry>& presets,
                           const std::vector<core::LookEntry>& looks, const Lists& lists,
                           std::shared_ptr<const pfx::Image> photo, QString photoName, QString current,
                           const std::optional<Opened>& opened, QWidget* parent)
    : Sheet(parent)
    , m_mode(mode)
    , m_photo(std::move(photo))
    , m_photoName(std::move(photoName))
    , m_choice(std::move(current))
    , m_search(new QLineEdit(this))
    , m_tileArea(new QWidget(this))
    , m_tileColumn(new QVBoxLayout(m_tileArea))
    , m_description(new QLabel(this))
    , m_thumbnails(new services::Thumbnailer(this))
{
    setModal(true);
    const QString title = mode == Mode::Recipe ? tr("Choose the recipe") : tr("Add a preset on top");
    setWindowTitle(title);
    buildItems(presets, looks, lists, opened);
    if (mode == Mode::Recipe) {
        buildLookGroups();
    }
    buildPresetGroups(presets, lists);

    m_search->setPlaceholderText(mode == Mode::Recipe ? tr("Search looks and presets")
                                                      : tr("Search presets"));
    m_search->setClearButtonEnabled(true);
    addSearchIcon(m_search);
    m_search->setFixedWidth(260);
    connect(m_search, &QLineEdit::textChanged, this, &RecipePicker::showSearch);
    QHBoxLayout* heading = sheetTitleRow(this, title, {m_search});
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildGroupList(this));
    body->addWidget(buildTileScroll(), 1);
    QHBoxLayout* foot = buildFoot();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(sheetHeaderBar(this, heading));
    auto* line = new QFrame(this);
    line->setObjectName(u"divider"_s);
    line->setFixedHeight(1);
    root->addWidget(line);
    root->addLayout(body, 1);
    root->addWidget(footBar(this, foot));

    connect(m_thumbnails, &services::Thumbnailer::rendered, this, [this](int key, const QImage& image) {
        const int tile = key % kKeySpan;
        if (key / kKeySpan == m_generation && tile >= 0 && std::cmp_less(tile, m_tiles.size())) {
            m_tiles.at(static_cast<std::size_t>(tile))->setImage(image);
        }
    });

    setSize(Size::Picker);
    showGroup(startGroup(m_choice));
    if (!remembered(mode).search.isEmpty()) {
        m_search->setText(remembered(mode).search);
    }
    if (const auto at = itemOf(m_choice)) {
        selectItem(*at);
    } else {
        m_use->setEnabled(false);
    }
}

QWidget* RecipePicker::buildTileScroll()
{
    m_tileColumn->setContentsMargins(6, 10, 6, 10);
    m_tileColumn->setSpacing(kGapSmall);
    auto* scroll = new QScrollArea(this);
    scroll->setWidget(m_tileArea);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    keepScrollBarInPadding(scroll, m_tileColumn, 6);
    return scroll;
}

QHBoxLayout* RecipePicker::buildFoot()
{
    m_description->setObjectName(u"itemDetail"_s);
    m_description->setWordWrap(true);
    const FootButtons buttons = footButtons(this, m_mode == Mode::OnTop ? tr("Add") : tr("Use"), {}, true);
    m_use = buttons.primary;
    connect(buttons.cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_use, &QPushButton::clicked, this, &QDialog::accept);
    buttons.row->insertWidget(0, m_description, 1);
    return buttons.row;
}

int RecipePicker::startGroup(const QString& current) const
{
    // The group the last picker showed, else the one with the choice, else the first.
    const auto groupWith = [this](const std::function<bool(const Group&)>& test) {
        for (std::size_t g = 0; g < m_groups.size(); ++g) {
            if (test(m_groups.at(g))) {
                return static_cast<int>(g);
            }
        }
        return -1;
    };
    if (const int last = groupWith([this](const Group& g) { return g.name == remembered(m_mode).group; });
        last >= 0) {
        return last;
    }
    if (const auto at = itemOf(current)) {
        return std::max(
            0, groupWith([at](const Group& g) { return std::ranges::find(g.items, *at) != g.items.end(); }));
    }
    return 0;
}

RecipePicker::~RecipePicker()
{
    remembered(m_mode).search = m_search->text();
}

void RecipePicker::buildItems(const std::vector<core::PresetEntry>& presets,
                              const std::vector<core::LookEntry>& looks, const Lists& lists,
                              const std::optional<Opened>& opened)
{
    if (m_mode == Mode::Recipe) {
        m_items.push_back({.choice = QString(),
                           .name = tr("No look or preset"),
                           .description = tr("Each photo is only resized and saved, as set on the right"),
                           .settings = {}});
        if (opened) {
            m_items.push_back({.choice = u"recipe"_s,
                               .name = opened->name,
                               .description = tr("The steps of the batch recipe you opened"),
                               .settings = opened->settings});
        }
        for (const core::LookEntry& l : looks) {
            m_items.push_back(
                {.choice = u"look:"_s + l.id,
                 .name = l.name,
                 .description = tr("Look, strength %1").arg(l.look ? l.look->defaultStrength : 100),
                 .settings = core::withLook({}, l.id, l.look ? l.look->defaultStrength : 100),
                 .favorite = lists.favoriteLooks.contains(l.id),
                 .mine = l.isMine(),
                 .look = true});
        }
    }
    for (const core::PresetEntry& p : presets) {
        if (m_mode == Mode::OnTop && p.autoEnhance) {
            continue; // Auto enhance has its own check box
        }
        m_items.push_back(
            {.choice = u"preset:"_s + p.name,
             .name = p.name,
             .description = p.description.isEmpty() ? core::presetSummary(p.settings) : p.description,
             .settings = core::applyPreset({}, p.settings),
             .autoEnhance = p.autoEnhance,
             .favorite = lists.favoritePresets.contains(p.id)});
    }
}

void RecipePicker::buildLookGroups()
{
    addGroup(tr("No look or preset"), {0});
    if (m_items.size() > 1 && m_items.at(1).choice == u"recipe"_s) {
        addGroup(m_items.at(1).name, {1});
    }
    std::vector<std::size_t> all;
    std::vector<std::size_t> mine;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).look) {
            all.push_back(i);
        }
        if (m_items.at(i).look && m_items.at(i).mine) {
            mine.push_back(i);
        }
    }
    addGroup(u"#"_s + tr("Looks"), {});
    addGroup(tr("All looks"), all);
    if (!mine.empty()) {
        addGroup(tr("Mine"), mine);
    }
    addGroup(u"#"_s + tr("Presets"), {});
}

std::vector<std::size_t>
RecipePicker::presetItems(const std::vector<core::PresetEntry>& presets,
                          const std::function<bool(const core::PresetEntry&)>& test) const
{
    std::vector<std::size_t> out;
    for (const core::PresetEntry& p : presets) {
        if (const auto at = itemOf(u"preset:"_s + p.name); at && test(p)) {
            out.push_back(*at);
        }
    }
    return out;
}

void RecipePicker::buildPresetGroups(const std::vector<core::PresetEntry>& presets, const Lists& lists)
{
    if (auto favorites = presetItems(
            presets, [&lists](const core::PresetEntry& p) { return lists.favoritePresets.contains(p.id); });
        !favorites.empty()) {
        addGroup(tr("Favorites"), std::move(favorites));
    }
    std::vector<std::size_t> recent;
    for (const QString& id : lists.recentPresets) {
        const auto one = presetItems(presets, [&id](const core::PresetEntry& p) { return p.id == id; });
        recent.insert(recent.end(), one.begin(), one.end());
    }
    if (!recent.empty()) {
        addGroup(tr("Recent"), std::move(recent));
    }
    // The user's groups first, then the catalog's categories, each in the order they come.
    QStringList order;
    for (const bool builtIn : {false, true}) {
        for (const core::PresetEntry& p : presets) {
            if (p.isBuiltIn() == builtIn && !order.contains(p.group) && itemOf(u"preset:"_s + p.name)) {
                order << p.group;
            }
        }
    }
    for (const QString& group : order) {
        addGroup(group,
                 presetItems(presets, [&group](const core::PresetEntry& p) { return p.group == group; }));
    }
}

void RecipePicker::addGroup(const QString& name, std::vector<std::size_t> items)
{
    m_groups.push_back({name, std::move(items)});
}

QWidget* RecipePicker::buildGroupList(QWidget* parent)
{
    m_groupList = new QListWidget(parent);
    m_groupList->setObjectName(u"pickerGroups"_s);
    m_groupList->setFixedWidth(210);
    m_groupList->setItemDelegate(new GroupDelegate(m_groupList));
    m_groupList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (const Group& g : m_groups) {
        auto* item = new QListWidgetItem(m_groupList);
        if (g.name.startsWith(u'#')) {
            item->setText(g.name.mid(1));
            item->setData(kHeadingRole, true);
            item->setFlags(Qt::NoItemFlags);
            continue;
        }
        item->setText(g.name);
        item->setData(kCountRole, countText(g));
    }
    connect(m_groupList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && m_search->text().isEmpty()) {
            showGroup(row);
        } else if (row >= 0) {
            m_search->clear(); // a group chosen ends the search, and shows the group
        }
    });
    return m_groupList;
}

QString RecipePicker::countText(const Group& group) const
{
    if (group.items.size() == 1 && m_items.at(group.items.front()).choice.isEmpty()) {
        return {}; // No look or preset
    }
    return QString::number(group.items.size());
}

void RecipePicker::showGroup(int index)
{
    if (index < 0 || std::cmp_greater_equal(index, m_groups.size())) {
        return;
    }
    // A heading is not a group: the one after it is.
    while (std::cmp_less(index, m_groups.size()) &&
           m_groups.at(static_cast<std::size_t>(index)).name.startsWith(u'#')) {
        ++index;
    }
    const Group& g = m_groups.at(static_cast<std::size_t>(index));
    {
        const QSignalBlocker block(m_groupList);
        m_groupList->setCurrentRow(index);
    }
    remembered(m_mode).group = g.name;
    const auto n = static_cast<int>(g.items.size());
    QString count;
    if (!countText(g).isEmpty()) {
        count = m_items.at(g.items.front()).look ? tr("%n looks", nullptr, n) : tr("%n presets", nullptr, n);
    }
    showTiles(g.name, count, {{QString(), g.items}});
}

void RecipePicker::showSearch(const QString& text)
{
    remembered(m_mode).search = text;
    const QString words = text.trimmed();
    if (words.isEmpty()) {
        showGroup(std::max(0, m_groupList->currentRow()));
        return;
    }
    std::vector<std::size_t> looks;
    std::vector<std::size_t> presets;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        const Item& item = m_items.at(i);
        if (item.choice.isEmpty() || item.choice == u"recipe"_s) {
            continue;
        }
        if (item.name.contains(words, Qt::CaseInsensitive) ||
            item.description.contains(words, Qt::CaseInsensitive)) {
            (item.look ? looks : presets).push_back(i);
        }
    }
    std::vector<std::pair<QString, std::vector<std::size_t>>> sections;
    if (!looks.empty()) {
        sections.emplace_back(tr("Looks"), looks);
    }
    if (!presets.empty()) {
        sections.emplace_back(tr("Presets"), presets);
    }
    const auto found = static_cast<int>(looks.size() + presets.size());
    //: The title over search results: "Results for warm".
    showTiles(tr("Results for %1").arg(words),
              found > 0 ? tr("%n found", nullptr, found) : tr("Nothing found"), sections);
}

void RecipePicker::clearTiles()
{
    while (QLayoutItem* item = m_tileColumn->takeAt(0)) {
        if (item->layout() != nullptr) {
            while (const QLayoutItem* inner = item->layout()->takeAt(0)) {
                delete inner->widget();
                delete inner;
            }
        }
        delete item->widget();
        delete item;
    }
    m_tiles.clear();
    m_tileItems.clear();
}

void RecipePicker::showTiles(const QString& title, const QString& count,
                             const std::vector<std::pair<QString, std::vector<std::size_t>>>& sections)
{
    clearTiles();
    ++m_generation;
    auto* top = new QHBoxLayout;
    top->setContentsMargins(kToolRowPadding, 0, kToolRowPadding, 4);
    top->setSpacing(kGapRelated);
    auto* name = new QLabel(title, m_tileArea);
    name->setObjectName(u"itemTitle"_s);
    auto* counted = new QLabel(count, m_tileArea);
    counted->setObjectName(u"muted"_s);
    auto* on = new QLabel(m_photoName.isEmpty() ? tr("Add photos to see each on the first one")
                                                : tr("On %1, the first photo").arg(m_photoName),
                          m_tileArea);
    on->setObjectName(u"muted"_s);
    top->addWidget(name);
    top->addWidget(counted);
    top->addStretch();
    top->addWidget(on);
    m_tileColumn->addLayout(top);
    int shown = 0;
    for (const auto& [heading, items] : sections) {
        if (!heading.isEmpty()) {
            m_tileColumn->addWidget(
                sectionHeading(u"%1 (%2)"_s.arg(heading).arg(items.size()), m_tileArea, kToolRowPadding));
        }
        std::vector<PhotoTile*> tiles;
        for (const std::size_t i : items) {
            if (shown++ >= kMostResults) {
                break;
            }
            const Item& item = m_items.at(i);
            auto badge = PhotoTile::Badge::None;
            if (item.favorite) {
                badge = PhotoTile::Badge::Favorite;
            } else if (item.mine) {
                badge = PhotoTile::Badge::Mine;
            }
            auto* tile = new PhotoTile(item.choice, item.name, badge, m_tileArea);
            tile->setDescription(item.description);
            tile->setSelected(item.choice == m_choice);
            tile->installEventFilter(this);
            const std::size_t index = i;
            connect(tile, &PhotoTile::clicked, this, [this, index] { selectItem(index); });
            if (m_photo) {
                const pfx::Settings s = item.autoEnhance ? pfx::autoAdjust(*m_photo) : item.settings;
                m_thumbnails->requestRender((m_generation * kKeySpan) + static_cast<int>(m_tiles.size()),
                                            m_photo, s);
            }
            m_tiles.push_back(tile);
            m_tileItems.push_back(i);
            tiles.push_back(tile);
        }
        m_tileColumn->addLayout(tileGrid(tiles, m_tileArea, kColumns));
    }
    m_tileColumn->addStretch();
}

void RecipePicker::select(const QString& choice)
{
    if (const auto at = itemOf(choice)) {
        selectItem(*at);
    }
}

void RecipePicker::selectItem(std::size_t index)
{
    const Item& item = m_items.at(index);
    m_choice = item.choice;
    for (std::size_t t = 0; t < m_tiles.size(); ++t) {
        m_tiles.at(t)->setSelected(m_tileItems.at(t) == index);
    }
    // "Portrait 400: soft warm skin, ...": the description goes on in lower case, unless it opens
    // with a name in capitals ("B&W").
    QString description = item.description;
    if (description.size() > 1 && description.at(1).isLower()) {
        description[0] = description.at(0).toLower();
    }
    m_description->setText(description.isEmpty() ? item.name : tr("%1: %2").arg(item.name, description));
    if (m_mode == Mode::OnTop) {
        m_use->setText(buttonText(tr("Add %1").arg(item.name)));
    } else {
        m_use->setText(
            buttonText(item.choice.isEmpty() ? tr("Use no look or preset") : tr("Use %1").arg(item.name)));
    }
    m_use->setEnabled(true);
}

std::optional<std::size_t> RecipePicker::itemOf(const QString& choice) const
{
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).choice == choice) {
            return i;
        }
    }
    return std::nullopt;
}

bool RecipePicker::eventFilter(QObject* watched, QEvent* event)
{
    // A double-click, or Enter, on a tile uses it.
    const auto* tile = qobject_cast<PhotoTile*>(watched);
    const bool enter =
        event->type() == QEvent::KeyPress && (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Return ||
                                              static_cast<QKeyEvent*>(event)->key() == Qt::Key_Enter);
    if (tile != nullptr && (event->type() == QEvent::MouseButtonDblClick || enter)) {
        select(tile->id());
        accept();
        return true;
    }
    return Sheet::eventFilter(watched, event);
}

} // namespace pixelfx::ui
