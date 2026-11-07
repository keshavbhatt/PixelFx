#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

// A bug report put together from what the person wrote and what they chose to attach, as the
// text of an issue on PixelFx's tracker. Pure, tested; nothing here sends anything.
namespace pixelfx::core {

struct BugReport
{
    QString whatHappened;
    QString crash;       ///< what PixelFx was doing when it closed; empty when not attached
    QString diagnostics; ///< empty when not attached
    QStringList log;     ///< the log's last lines; empty when not attached
};

/// `text` with every file and folder path replaced by its kind: "a .jpg file", "a folder". The
/// names of photos and folders say more about a person than a report needs.
[[nodiscard]] QString withoutNames(const QString& text);
/// The last `count` lines of the run before this one in `log` (the lines of a log file, oldest
/// first): each run starts with the line "... info: pixelfx <version> ...".
[[nodiscard]] QStringList previousRun(const QStringList& log, int count);
/// The issue's title: the first line of what happened, at most 80 characters; a plain one when
/// nothing was written.
[[nodiscard]] QString reportTitle(const BugReport& report);
/// The issue's text: what happened, then each attachment under a heading, in code blocks.
[[nodiscard]] QString reportBody(const BugReport& report);
/// The tracker's page for a new issue, filled in with `title` and `body`; empty when the address
/// would be too long for a browser to take, and then `fallbackUrl` has a body asking to paste.
[[nodiscard]] QUrl issueUrl(const QString& title, const QString& body);
[[nodiscard]] QUrl fallbackUrl(const QString& title);

} // namespace pixelfx::core
