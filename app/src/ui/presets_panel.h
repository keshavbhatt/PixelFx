#pragma once

#include "core/presets.h"

#include <QImage>
#include <QWidget>

#include <vector>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

namespace pixelfx::ui {

class LooksPanel;
class PhotoTile;
class SliderRow;

/// The left panel's two tabs. Looks; and Presets, the catalog of `editor-presets-catalog.html`:
/// search, the All, Favorites, Mine and Recent chips, the user's presets and the built-in
/// categories as tiles of the photo (six per open category, "All N" for the rest), the Amount
/// of the preset just applied, and Save current as preset. A click applies, hovering previews
/// on the photo, a tile's menu manages it.
class PresetsPanel : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresetsPanel)

public:
    explicit PresetsPanel(QWidget* parent = nullptr);
    ~PresetsPanel() override;

    /// Shows `entries`: the user's by group, then the built-in ones by category.
    void setEntries(std::vector<core::PresetEntry> entries);
    [[nodiscard]] const std::vector<core::PresetEntry>& entries() const { return m_entries; }
    /// The picture of the photo with the preset at `index` of entries() applied.
    void setThumbnail(std::size_t index, const QImage& image);
    /// Forgets every picture (the photo or its edit changed); shownIndexes() then wants them all.
    void clearThumbnails();
    [[nodiscard]] bool hasThumbnail(std::size_t index) const;
    /// The entries whose tiles are on show, so only their pictures are made.
    [[nodiscard]] std::vector<std::size_t> shownIndexes() const;
    [[nodiscard]] std::vector<PhotoTile*> tiles() const;
    /// With several photos selected the panel says where a click goes ("Applies to 4 photos").
    void setTargetCount(int count);
    /// Preset ids in Favorites, and the recent ones, newest first.
    void setFavorites(const QStringList& ids);
    [[nodiscard]] const QStringList& favorites() const { return m_favorites; }
    void setRecent(const QStringList& ids);
    /// Categories closed by the user (their names), restored at start.
    void setClosedCategories(const QStringList& names);
    [[nodiscard]] const QStringList& closedCategories() const { return m_closed; }
    /// The whole of one category (its name), or every category again with an empty name.
    void showCategory(const QString& name);
    [[nodiscard]] const QString& shownCategory() const { return m_category; }
    /// 0 All, 1 Favorites, 2 Mine, 3 Recent.
    void showFilter(int filter);
    /// The preset just applied and its Amount; an empty name hides the Amount.
    void setApplied(const QString& id, const QString& name, double amount);
    [[nodiscard]] SliderRow* amountRow() const { return m_amount; }

    /// The Looks tab's panel.
    [[nodiscard]] LooksPanel* looksPanel() const { return m_looks; }
    /// 0 Looks, 1 Presets.
    void showTab(int index);
    [[nodiscard]] int currentTab() const;

Q_SIGNALS:
    void applyRequested(const pixelfx::core::PresetEntry& entry);
    /// Hovering a tile (null `entry`: the pointer left the tiles).
    void previewRequested(const pixelfx::core::PresetEntry* entry);
    /// Other tiles came into view: make the pictures shownIndexes() lacks.
    void thumbnailsWanted();
    void saveRequested();
    void importRequested();
    void renameRequested(const pixelfx::core::PresetEntry& entry);
    void updateRequested(const pixelfx::core::PresetEntry& entry);
    void exportRequested(const pixelfx::core::PresetEntry& entry);
    void duplicateRequested(const pixelfx::core::PresetEntry& entry);
    void showFileRequested(const pixelfx::core::PresetEntry& entry);
    void deleteRequested(const pixelfx::core::PresetEntry& entry);
    void favoritesChanged(const QStringList& ids);
    void closedCategoriesChanged(const QStringList& names);
    /// The Amount moves (live) and is let go (a history step).
    void amountMoved(double amount);
    void amountReleased(double amount);

private:
    QHBoxLayout* buildHead();
    QHBoxLayout* buildTabs();
    QWidget* buildChips();
    QWidget* buildBackRow();
    QWidget* buildFoot();
    void rebuild();
    void clearList();
    void addGroupSections(bool mineOnly, bool searching);
    void addEmptyNote(int filter, bool searching);
    /// A heading and its tiles; `collapsible` headings open and close, `limited` ones show the
    /// first six with "All N" for the rest.
    void addSection(const QString& title, const std::vector<std::size_t>& indexes, bool collapsible,
                    bool limited);
    [[nodiscard]] PhotoTile* makeTile(std::size_t index);
    [[nodiscard]] bool matches(const core::PresetEntry& e) const;
    [[nodiscard]] std::vector<std::size_t> indexesOf(const QStringList& ids) const;
    void toggleFavorite(std::size_t index);
    void showMenu(std::size_t index, const QPoint& where);

    std::vector<core::PresetEntry> m_entries;
    std::vector<QImage> m_thumbnails;
    // The tiles of the list as built: Qt reports new children of a shown panel as hidden until
    // the next event loop pass, so their visibility cannot tell which are on show.
    std::vector<PhotoTile*> m_tiles;
    QStringList m_favorites;
    QStringList m_recent;
    QStringList m_closed;
    QString m_category;
    QString m_appliedId;
    QLineEdit* m_search = nullptr;
    QLabel* m_target = nullptr;
    QButtonGroup* m_chips = nullptr;
    QWidget* m_chipRow = nullptr;
    QWidget* m_backRow = nullptr;
    QLabel* m_backCount = nullptr;
    QWidget* m_listBox = nullptr;
    QVBoxLayout* m_list = nullptr;
    QWidget* m_appliedBox = nullptr;
    QLabel* m_appliedName = nullptr;
    SliderRow* m_amount = nullptr;
    LooksPanel* m_looks = nullptr;
    QStackedWidget* m_pages = nullptr;
    QButtonGroup* m_tabs = nullptr;
    int m_targetCount = 1;
};

} // namespace pixelfx::ui
