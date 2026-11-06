#pragma once

#include "core/looks.h"
#include "core/presets.h"
#include "ui/sheet.h"

#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <pfx/settings.h>
#include <vector>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace pfx {
class Image;
}

namespace pixelfx::services {
class Thumbnailer;
}

namespace pixelfx::ui {

class PhotoTile;

/// Choose the recipe (batch-recipe-picker.html): a batch's look or preset from a sheet of
/// groups (no look or preset, the looks, Favorites, Recent, the user's own, the catalog's
/// categories), a search through looks and presets together, and tiles of the batch's first
/// photo with each. The same sheet with presets only adds a preset on top.
class RecipePicker : public Sheet
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(RecipePicker)

public:
    enum class Mode
    {
        Recipe, ///< the recipe's first choice
        OnTop,  ///< Then add preset: presets only
    };
    /// What the picker marks and offers first, from the user's settings.
    struct Lists
    {
        QStringList favoritePresets; ///< preset ids
        QStringList favoriteLooks;   ///< look ids
        QStringList recentPresets;   ///< preset ids, newest first
    };
    /// An opened batch recipe's own steps, offered as a choice of their own.
    struct Opened
    {
        QString name;
        pfx::Settings settings;
    };

    /// `current` is the choice now ("", "look:<id>", "preset:<name>", "recipe"); `photo` the
    /// batch's first photo, small (null without photos), named `photoName`.
    RecipePicker(Mode mode, const std::vector<core::PresetEntry>& presets,
                 const std::vector<core::LookEntry>& looks, const Lists& lists,
                 std::shared_ptr<const pfx::Image> photo, QString photoName, QString current,
                 const std::optional<Opened>& opened, QWidget* parent);
    ~RecipePicker() override;

    /// The choice made, in the form of `current`.
    [[nodiscard]] const QString& choice() const { return m_choice; }

    [[nodiscard]] QListWidget* groupList() const { return m_groupList; }
    [[nodiscard]] QLineEdit* searchField() const { return m_search; }
    [[nodiscard]] const std::vector<PhotoTile*>& tiles() const { return m_tiles; }
    [[nodiscard]] QPushButton* useButton() const { return m_use; }
    /// Selects the tile of `choice` as a click does.
    void select(const QString& choice);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Item
    {
        QString choice;
        QString name;
        QString description;
        pfx::Settings settings;
        bool autoEnhance = false;
        bool favorite = false;
        bool mine = false;
        bool look = false;
    };
    struct Group
    {
        QString name;
        std::vector<std::size_t> items;
    };

    void buildItems(const std::vector<core::PresetEntry>& presets, const std::vector<core::LookEntry>& looks,
                    const Lists& lists, const std::optional<Opened>& opened);
    void buildLookGroups();
    void buildPresetGroups(const std::vector<core::PresetEntry>& presets, const Lists& lists);
    /// The items of the presets that pass `test`, in the presets' order.
    [[nodiscard]] std::vector<std::size_t>
    presetItems(const std::vector<core::PresetEntry>& presets,
                const std::function<bool(const core::PresetEntry&)>& test) const;
    QWidget* buildTileScroll();
    QHBoxLayout* buildFoot();
    /// The group to open on: the last one shown, else the one holding `current`, else the first.
    [[nodiscard]] int startGroup(const QString& current) const;
    void addGroup(const QString& name, std::vector<std::size_t> items);
    QWidget* buildGroupList(QWidget* parent);
    void showGroup(int index);
    void showSearch(const QString& text);
    /// Lays the tiles of `sections` (a heading, or none, and its items) and renders their pictures.
    void showTiles(const QString& title, const QString& count,
                   const std::vector<std::pair<QString, std::vector<std::size_t>>>& sections);
    void clearTiles();
    void selectItem(std::size_t index);
    [[nodiscard]] QString countText(const Group& group) const;
    [[nodiscard]] std::optional<std::size_t> itemOf(const QString& choice) const;

    Mode m_mode;
    std::vector<Item> m_items;
    std::vector<Group> m_groups;
    std::shared_ptr<const pfx::Image> m_photo;
    QString m_photoName;
    QString m_choice;
    QListWidget* m_groupList = nullptr;
    QLineEdit* m_search = nullptr;
    QWidget* m_tileArea = nullptr;
    QVBoxLayout* m_tileColumn = nullptr;
    QLabel* m_description = nullptr;
    QPushButton* m_use = nullptr;
    std::vector<PhotoTile*> m_tiles;
    std::vector<std::size_t> m_tileItems; ///< the item of each tile
    services::Thumbnailer* m_thumbnails = nullptr;
    int m_generation = 0;
};

} // namespace pixelfx::ui
