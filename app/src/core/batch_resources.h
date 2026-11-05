#pragma once

#include <QString>
#include <QStringList>

#include <pfx/document.h>

// What a batch recipe file carries so it runs on another machine (DOCS/presets.md section 4,
// version 2): the user's looks its stack refers to, and the watermark with its logo.
namespace pixelfx::core {

/// Adds to `body` the user's looks its stack refers to (built-in ones stay references) and the
/// saved watermark at `watermarkPath` (empty for none), its logo's bytes included.
void embedBatchResources(pfx::BatchBody& body, const QString& watermarkPath);

struct AdoptedResources
{
    /// The watermark's file in `watermarkFolder`, empty when the recipe has none.
    QString watermarkPath;
    /// Names of the looks added to the user's looks.
    QStringList addedLooks;
    /// What could not be added, for people.
    QStringList problems;
};

/// Makes what `body` carries usable here: each embedded look whose id is not among the user's
/// looks yet is saved into `lookFolder` under its own id; the watermark is saved into
/// `watermarkFolder` unless an identical one is there already.
[[nodiscard]] AdoptedResources adoptBatchResources(const pfx::BatchBody& body, const QString& lookFolder,
                                                   const QString& watermarkFolder);

} // namespace pixelfx::core
