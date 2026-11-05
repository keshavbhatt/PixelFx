#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <pfx/settings.h>
#include <string>
#include <vector>

// Presets (FEATURES P1 to P4): saved subsets of
// a photo's settings. A preset holds whole "units" (the rows of Save as preset); applying it
// replaces those units and leaves every other setting of the photo as it is, so presets stack.
namespace pixelfx::core {

/// One row of Save as preset. White balance is its own unit inside Color, and Perspective its
/// own inside the geometry, because they rarely carry from one photo to another.
struct PresetUnit
{
    QString id;    ///< "light", "wb", "crop" ...
    QString label; ///< translated
};

[[nodiscard]] const std::vector<PresetUnit>& presetUnits();
/// Whether `key` belongs to the unit `unitId`.
[[nodiscard]] bool keyInUnit(std::string_view key, const QString& unitId);
/// The units with at least one setting in `settings`, in panel order.
[[nodiscard]] QStringList unitsIn(const pfx::Settings& settings);
/// How many settings of `unitId` differ from their defaults (the "6 changed" of the sheet).
[[nodiscard]] int changedCount(const pfx::Settings& settings, const QString& unitId);
/// The settings of `unitId` that differ from their defaults, in `settings` order (not for "look").
[[nodiscard]] std::vector<std::string> changedKeys(const pfx::Settings& settings, const QString& unitId);

/// A preset of the `units` of `settings`: every value of each unit written out, defaults
/// included, so applying it sets the whole unit.
[[nodiscard]] pfx::Settings presetFrom(const pfx::Settings& settings, const QStringList& units);
/// `current` with the preset's units replaced by the preset's values.
[[nodiscard]] pfx::Settings applyPreset(const pfx::Settings& current, const pfx::Settings& preset);
/// "Light, Color, Curves": the units a preset holds, for its row in the list.
[[nodiscard]] QString presetSummary(const pfx::Settings& preset);
/// `base` with `preset` applied at `amount` percent, 0 to 200 (Lightroom's preset Amount): every
/// number of the preset's units moves from its value in `base` (or its default) toward the
/// preset's and, past 100, beyond it, held in its range; curves blend point by point. Switches
/// and choices are the preset's from any amount above 0.
[[nodiscard]] pfx::Settings blendPreset(const pfx::Settings& base, const pfx::Settings& preset,
                                        double amount);

/// A built-in category of the catalog: its folder name and what the panel calls it.
struct PresetCategory
{
    QString id;   ///< "film-color"
    QString name; ///< "Film, color", translated
};
/// The catalog's categories in the panel's order, Essentials first.
[[nodiscard]] const std::vector<PresetCategory>& presetCategories();

/// A preset in the library.
struct PresetEntry
{
    QString id; ///< the file's path, or "builtin:<name>"; kept for Favorites and Recent
    QString name;
    QString group;    ///< "My presets", "Imported", the user's own, or a built-in category's name
    QString category; ///< a built-in category's id; empty for the user's
    QString path;     ///< empty for built-in presets
    pfx::Settings settings;
    QString description;      ///< what a built-in preset does, in words; empty for the user's
    bool autoEnhance = false; ///< "Auto enhance": works the settings out per photo
    [[nodiscard]] bool isBuiltIn() const { return path.isEmpty(); }
};

/// The user's presets, one .pfxpreset file each in a folder (<AppData>/presets by default),
/// plus the built-in ones. Plain file work, small files: callers may use it on the GUI thread.
class PresetLibrary
{
public:
    explicit PresetLibrary(QString folder);

    [[nodiscard]] const QString& folder() const { return m_folder; }
    /// Every preset: the user's sorted by group then name, then the built-in ones.
    [[nodiscard]] std::vector<PresetEntry> entries() const;
    /// Saves a new preset; a free file name is chosen from `name`. The path, or empty on failure.
    [[nodiscard]] QString save(const QString& name, const QString& group,
                               const pfx::Settings& settings) const;
    /// Replaces the settings of the preset at `path`, keeping its name and group.
    [[nodiscard]] static bool update(const QString& path, const pfx::Settings& settings);
    [[nodiscard]] static bool rename(const QString& path, const QString& name);
    [[nodiscard]] bool remove(const QString& path) const;
    /// What an import did: the new preset's path (empty on failure), the reason it failed, and
    /// for a Lightroom preset the settings PixelFx has no equivalent for.
    struct Imported
    {
        QString path;
        QString error;
        QStringList skipped;
    };
    /// Brings a PixelFx preset or recipe, or a Lightroom or Camera Raw preset (.xmp), into the
    /// library, in the Imported group.
    [[nodiscard]] Imported importFile(const QString& file) const;
    /// Writes the preset at `path` (or a built-in one) to `target` for sharing.
    [[nodiscard]] static bool exportTo(const PresetEntry& entry, const QString& target);

    [[nodiscard]] static QString defaultFolder();
    /// The built-in presets: the essentials made in code, then the catalog shipped with the app.
    [[nodiscard]] static std::vector<PresetEntry> builtIn();
    /// The group a preset goes in unless it names one.
    [[nodiscard]] static QString myPresetsGroup();

private:
    [[nodiscard]] QString freePath(const QString& name) const;
    QString m_folder;
};

} // namespace pixelfx::core

#include <QMetaType>
Q_DECLARE_METATYPE(pixelfx::core::PresetEntry)
