#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace pixelfx::core {

/// One `## [<version>]` heading of a Keep-a-Changelog document.
struct ChangelogRelease
{
    QString version; ///< without a `v` prefix
    QString date;    ///< the `YYYY-MM-DD` after the dash; empty when unreleased or absent
};

/// Every release heading in document order (newest first in a Keep-a-Changelog
/// file). Pure, tested.
[[nodiscard]] QList<ChangelogRelease> changelogReleases(const QString& markdown);

/// The body of the `## [<version>]` section of a Keep-a-Changelog document:
/// everything after that heading up to the next `## ` heading, trimmed.
/// The heading may carry a suffix (`- unreleased`, `- 2026-09-20`) and a `v`
/// prefix on the version; `### ` subheadings stay in the body; Keep-a-Changelog
/// link-reference lines (`[1.0.0]: https://…`) are dropped. Empty when the
/// version has no section. Pure, tested.
[[nodiscard]] QString changelogSection(const QString& markdown, const QString& version);

/// One `### ` group of a changelog section and its list items, each joined onto one line.
struct ChangelogGroup
{
    QString title; ///< "Added", "Changed", "Fixed"
    QStringList items;
};

/// A changelog section split for the What's new sheet: the paragraphs before the first `### `
/// heading, then each group. Markdown inline marks stay in the text. Pure, tested.
struct ChangelogNotes
{
    QStringList lead;
    QList<ChangelogGroup> groups;
};
[[nodiscard]] ChangelogNotes changelogNotes(const QString& section);

/// The first sentence of an item, for a short highlight: up to the first full stop that ends a
/// sentence (". " before a capital letter), or the whole item. Pure, tested.
[[nodiscard]] QString changelogFirstSentence(const QString& item);

/// `text` as HTML: escaped, with `**bold**` and `` `code` `` turned into tags. Pure, tested.
[[nodiscard]] QString changelogInlineHtml(const QString& text);

} // namespace pixelfx::core
