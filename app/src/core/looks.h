#pragma once

#include <QString>
#include <QStringList>

#include <memory>
#include <pfx/look.h>
#include <pfx/pipeline.h>
#include <pfx/result.h>
#include <vector>

// Looks (FEATURES L1 to L7, mock editor.html): one-click styles with a strength. A photo's
// settings name its look in the `look` group (presets.md section 2): `look.id` and
// `look.strength` (0 to 100). Built-in looks are `builtin:<slug>`; the user's are .pfxlook
// files in <AppData>/looks, named by their document id.
namespace pixelfx::core {

struct LookEntry
{
    QString id;       ///< "builtin:golden-hour", or the document's id
    QString name;     ///< translated for built-in looks
    QString category; ///< "color", "film", "bw"
    QString path;     ///< empty for built-in looks
    std::shared_ptr<const pfx::Look> look;
    [[nodiscard]] bool isMine() const { return !path.isEmpty(); }
};

/// The looks that ship with PixelFx, in the mock's order ("Original" is no look and not here).
[[nodiscard]] const std::vector<LookEntry>& builtInLooks();

class LookLibrary
{
public:
    explicit LookLibrary(QString folder);
    [[nodiscard]] static QString defaultFolder();
    [[nodiscard]] const QString& folder() const { return m_folder; }

    /// The user's looks sorted by name, then the built-in ones.
    [[nodiscard]] std::vector<LookEntry> entries() const;
    /// A .cube LUT or a HaldCLUT picture as a look of its own (one LUT step), named after the
    /// file. The new look's id, or empty with `error` set.
    [[nodiscard]] QString importLut(const QString& file, QString* error = nullptr) const;
    [[nodiscard]] bool remove(const QString& path) const;
    /// Saves a look of the user's under `name`: a new file when `path` is empty, otherwise that
    /// file, keeping its id. The look's id, or empty on failure; `savedPath` gets the file.
    [[nodiscard]] QString save(const QString& name, const pfx::Look& look, const QString& path = {},
                               QString* savedPath = nullptr) const;
    /// Writes the user's look at `path` (or a built-in one, by id) to `target` for sharing.
    [[nodiscard]] static bool exportTo(const LookEntry& entry, const QString& target);

private:
    QString m_folder;
};

/// Where findLook looks for the user's looks (set once at start; tests point it elsewhere).
void setLookFolder(const QString& folder);
/// The look `id` names, built-in or the user's; null when there is none. Thread-safe: renders
/// call it from workers. The user's looks are read again when their folder changes.
[[nodiscard]] std::shared_ptr<const pfx::Look> findLook(const QString& id);
/// Forget what findLook has read (after an import or a removal).
void lookFolderChanged();

/// pfx::render with the photo's look resolved from its settings: every render in the app goes
/// through here, so the editor, thumbnails, Export and Batch agree.
[[nodiscard]] pfx::Result<pfx::Image> renderPhoto(const pfx::Image& input, const pfx::Settings& settings,
                                                  pfx::RenderOptions options = {});
[[nodiscard]] pfx::Result<pfx::Image> renderPhoto(pfx::Image&& input, const pfx::Settings& settings,
                                                  pfx::RenderOptions options = {});

/// The look's name for people ("Golden hour"), built-in or the user's; empty when unknown.
[[nodiscard]] QString lookName(const QString& id);

/// The settings with `id` as their look at `strength` (0 to 100), or with no look for an empty id.
[[nodiscard]] pfx::Settings withLook(const pfx::Settings& settings, const QString& id, double strength);

} // namespace pixelfx::core
