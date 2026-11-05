#include "ui/looks_panel.h"

#include "ui/flow_layout.h"
#include "ui/icons.h"
#include "ui/panel_slider.h"
#include "ui/style.h"

#include <QButtonGroup>
#include <QContextMenuEvent>
#include <QGridLayout>
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

// Two columns that share the panel's width; about 116 px
// each beside the scroll bar.
constexpr int kImageHeight = 77;
constexpr int kTileHeight = kImageHeight + 24;

// The category chips.
struct Chip
{
    const char* label;
    const char* category; // "" all, "mine" the user's
};
constexpr std::array<Chip, 5> kChips = {{
    {QT_TRANSLATE_NOOP("LooksPanel", "All"), ""},
    {QT_TRANSLATE_NOOP("LooksPanel", "Mine"), "mine"},
    {QT_TRANSLATE_NOOP("LooksPanel", "Color"), "color"},
    {QT_TRANSLATE_NOOP("LooksPanel", "Film"), "film"},
    {QT_TRANSLATE_NOOP("LooksPanel", "B&&W"), "bw"}, // && shows one ampersand on a button
}};

} // namespace

LookTile::LookTile(QString id, QString name, bool mine, QWidget* parent)
    : QWidget(parent)
    , m_id(std::move(id))
    , m_name(std::move(name))
    , m_mine(mine)
{
    setFixedHeight(kTileHeight);
    setMinimumWidth(90);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(m_name);
    setToolTip(m_name);
}

LookTile::~LookTile() = default;

void LookTile::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void LookTile::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void LookTile::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens& t = tokens();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
    QPainterPath clip;
    clip.addRoundedRect(box, 8, 8);
    p.fillPath(clip, t.hover);
    p.save();
    p.setClipPath(clip);
    const QRectF imageRect(box.left(), box.top(), box.width(), kImageHeight);
    if (!m_image.isNull()) {
        // object-fit: cover
        const QSizeF scaled = QSizeF(m_image.size()).scaled(imageRect.size(), Qt::KeepAspectRatioByExpanding);
        const QRectF source((scaled.width() - imageRect.width()) / 2 * m_image.width() / scaled.width(),
                            (scaled.height() - imageRect.height()) / 2 * m_image.height() / scaled.height(),
                            imageRect.width() * m_image.width() / scaled.width(),
                            imageRect.height() * m_image.height() / scaled.height());
        p.drawImage(imageRect, m_image, source);
    }
    p.restore();
    QFont small = font();
    small.setPixelSize(12);
    p.setFont(small);
    p.setPen(t.text);
    p.drawText(QRectF(box.left() + 7, imageRect.bottom(), box.width() - 14, kTileHeight - kImageHeight),
               Qt::AlignLeft | Qt::AlignVCenter,
               fontMetrics().elidedText(m_name, Qt::ElideRight, width() - 16));
    if (m_mine) {
        // The "Mine" pill, top right.
        QFont pill = small;
        pill.setPixelSize(11);
        pill.setWeight(QFont::DemiBold);
        const QString text = tr("Mine");
        const QFontMetrics fm(pill);
        const QRectF r(box.right() - fm.horizontalAdvance(text) - 17, box.top() + 5,
                       fm.horizontalAdvance(text) + 12, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(t.accentSoft);
        p.drawRoundedRect(r, 9, 9);
        p.setFont(pill);
        p.setPen(t.accent);
        p.drawText(r, Qt::AlignCenter, text);
    }
    if (m_selected || showsFocusRing(this)) {
        p.setPen(QPen(m_selected ? t.accent : t.muted, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 8, 8);
    }
}

void LookTile::contextMenuEvent(QContextMenuEvent* event)
{
    Q_EMIT menuRequested(event->globalPos());
}

void LookTile::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        Q_EMIT clicked();
    }
}

void LookTile::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space) {
        Q_EMIT clicked();
        return;
    }
    QWidget::keyPressEvent(event);
}

LooksPanel::LooksPanel(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_chips(new QButtonGroup(this))
    , m_listBox(new QWidget(this))
    , m_list(new QVBoxLayout(m_listBox))
{
    m_search->setPlaceholderText(tr("Search looks"));
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
    head->setContentsMargins(kToolRowPadding, 10, kToolRowPadding, 4);
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

QWidget* LooksPanel::buildStrength()
{
    auto* box = new QWidget(this);
    box->setObjectName(u"lookStrength"_s);
    box->setAttribute(Qt::WA_StyledBackground);
    m_strengthName = new QLabel(tr("Look strength"), box);
    m_strengthName->setObjectName(u"adjustName"_s);
    m_strengthValue = new QLabel(box);
    m_strengthValue->setObjectName(u"adjustValue"_s);
    m_strength = new PanelSlider(Qt::Horizontal, box);
    m_strength->setRange(0, 100);
    m_strength->setAccessibleName(tr("Look strength"));
    connect(m_strength, &QSlider::valueChanged, this, [this](int v) {
        m_strengthValue->setText(QString::number(v));
        if (m_strength->isSliderDown()) {
            Q_EMIT strengthMoved(v);
        }
    });
    connect(m_strength, &QSlider::sliderReleased, this,
            [this] { Q_EMIT strengthReleased(m_strength->value()); });
    // Keys and clicks on the track move it without a drag: one step each.
    connect(m_strength, &QSlider::actionTriggered, this, [this](int action) {
        if (action != QAbstractSlider::SliderMove) {
            QMetaObject::invokeMethod(
                this, [this] { Q_EMIT strengthReleased(m_strength->value()); }, Qt::QueuedConnection);
        }
    });
    auto* top = new QHBoxLayout;
    top->addWidget(m_strengthName);
    top->addStretch();
    top->addWidget(m_strengthValue);
    auto* column = new QVBoxLayout(box);
    column->setContentsMargins(kPanelPadding, 8, kPanelPadding, 10);
    column->setSpacing(0);
    column->addLayout(top);
    column->addWidget(m_strength);
    setCurrent({}, 0);
    return box;
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
    {
        const QSignalBlocker block(m_strength);
        m_strength->setValue(static_cast<int>(std::lround(strength)));
    }
    m_strengthValue->setText(QString::number(m_strength->value()));
    m_strength->setEnabled(!id.isEmpty());
    m_strengthName->setProperty("changed", !id.isEmpty());
    m_strengthValue->setProperty("changed", !id.isEmpty());
    for (QLabel* l : {m_strengthName, m_strengthValue}) {
        l->style()->unpolish(l);
        l->style()->polish(l);
    }
    if (changed) {
        for (LookTile* tile : visibleTiles()) {
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
    for (LookTile* tile : visibleTiles()) {
        if (tile->id() == id) {
            tile->setImage(image);
        }
    }
}

std::vector<LookTile*> LooksPanel::visibleTiles() const
{
    const QList<LookTile*> tiles = m_listBox->findChildren<LookTile*>();
    return {tiles.begin(), tiles.end()};
}

bool LooksPanel::matches(const core::LookEntry& e) const
{
    const QString category =
        QLatin1StringView(kChips.at(static_cast<std::size_t>(std::max(0, m_chips->checkedId()))).category);
    if (category == u"mine"_s ? !e.isMine() : (!category.isEmpty() && e.category != category)) {
        return false;
    }
    const QString needle = m_search->text().trimmed();
    return needle.isEmpty() || e.name.contains(needle, Qt::CaseInsensitive);
}

void LooksPanel::addSection(const QString& title, QWidget* extra, const std::vector<int>& indexes,
                            bool withOriginal)
{
    m_list->addLayout(sectionHeadingRow(sectionLabel(title, m_listBox), extra, kPanelPadding));
    auto* grid = new QGridLayout;
    grid->setContentsMargins(kToolRowPadding, 2, kToolRowPadding, 6);
    grid->setHorizontalSpacing(kGapControls);
    grid->setVerticalSpacing(kGapControls);
    int at = 0;
    const auto place = [&](LookTile* tile) {
        tile->setSelected(tile->id() == m_current);
        connect(tile, &LookTile::clicked, this, [this, tile] {
            int strength = 100;
            for (const core::LookEntry& e : m_entries) {
                if (e.id == tile->id()) {
                    strength = e.look->defaultStrength;
                }
            }
            Q_EMIT lookChosen(tile->id(), strength);
        });
        grid->addWidget(tile, at / 2, at % 2);
        ++at;
    };
    if (withOriginal) {
        auto* original = new LookTile({}, tr("Original"), false, m_listBox);
        original->setImage(m_original);
        place(original);
    }
    for (const int i : indexes) {
        const core::LookEntry& e = m_entries[static_cast<std::size_t>(i)];
        auto* tile = new LookTile(e.id, e.name, e.isMine(), m_listBox);
        tile->setImage(m_thumbnails[static_cast<std::size_t>(i)]);
        if (e.isMine()) {
            tile->setToolTip(tr("%1. Right-click to edit, export or delete.").arg(e.name));
            connect(tile, &LookTile::menuRequested, this, [this, id = e.id](const QPoint& where) {
                QMenu menu(this);
                connect(menu.addAction(tr("Edit look...")), &QAction::triggered, this,
                        [this, id] { Q_EMIT editLookRequested(id); });
                connect(menu.addAction(tr("Export look...")), &QAction::triggered, this,
                        [this, id] { Q_EMIT exportLookRequested(id); });
                menu.addSeparator();
                connect(menu.addAction(tr("Delete")), &QAction::triggered, this,
                        [this, id] { Q_EMIT deleteLookRequested(id); });
                menu.exec(where);
            });
        }
        place(tile);
    }
    m_list->addLayout(grid);
}

void LooksPanel::rebuild()
{
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
    m_list->addStretch();
}

} // namespace pixelfx::ui
