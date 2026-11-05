#include "core/changelog.h"

#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

QList<ChangelogRelease> changelogReleases(const QString& markdown)
{
    static const QRegularExpression kHeading(u"^## \\[v?([^\\]]+)\\](.*)$"_s);
    static const QRegularExpression kDate(u"(\\d{4}-\\d{2}-\\d{2})"_s);
    QList<ChangelogRelease> releases;
    for (QString line : markdown.split(u'\n')) {
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        const QRegularExpressionMatch m = kHeading.match(line);
        if (!m.hasMatch()) {
            continue;
        }
        const QRegularExpressionMatch date = kDate.match(m.captured(2));
        releases.append({m.captured(1).trimmed(), date.hasMatch() ? date.captured(1) : QString()});
    }
    return releases;
}

QString changelogSection(const QString& markdown, const QString& version)
{
    static const QRegularExpression kHeading(u"^## \\[v?([^\\]]+)\\]"_s);
    static const QRegularExpression kLinkReference(u"^\\[[^\\]]+\\]:\\s*\\S+"_s);
    QStringList body;
    bool inside = false;
    for (QString line : markdown.split(u'\n')) {
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        if (line.startsWith(u"## "_s)) {
            if (inside) {
                break;
            }
            const QRegularExpressionMatch m = kHeading.match(line);
            inside = m.hasMatch() && m.captured(1).trimmed() == version;
            continue;
        }
        if (inside && !kLinkReference.match(line).hasMatch()) {
            body << line;
        }
    }
    while (!body.isEmpty() && body.first().trimmed().isEmpty()) {
        body.removeFirst();
    }
    while (!body.isEmpty() && body.last().trimmed().isEmpty()) {
        body.removeLast();
    }
    return body.join(u'\n');
}

ChangelogNotes changelogNotes(const QString& section)
{
    ChangelogNotes notes;
    QString paragraph;
    const auto endParagraph = [&] {
        if (!paragraph.isEmpty()) {
            notes.lead.append(paragraph);
            paragraph.clear();
        }
    };
    bool inItem = false; // the last line was part of a list item
    for (QString line : section.split(u'\n')) {
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        const QString trimmed = line.trimmed();
        if (line.startsWith(u"### "_s)) {
            endParagraph();
            notes.groups.append({line.mid(4).trimmed(), {}});
            inItem = false;
        } else if (trimmed.isEmpty()) {
            endParagraph();
            inItem = false;
        } else if ((line.startsWith(u"- "_s) || line.startsWith(u"* "_s)) && !notes.groups.isEmpty()) {
            notes.groups.last().items.append(line.mid(2).trimmed());
            inItem = true;
        } else if (inItem) {
            notes.groups.last().items.last() += u' ' + trimmed; // a wrapped item
        } else if (notes.groups.isEmpty()) {
            paragraph += (paragraph.isEmpty() ? QString() : u" "_s) + trimmed;
        }
    }
    endParagraph();
    return notes;
}

QString changelogFirstSentence(const QString& item)
{
    bool code = false; // a full stop inside `code` ends nothing
    for (qsizetype i = 0; i + 2 < item.size(); ++i) {
        if (item.at(i) == u'`') {
            code = !code;
        } else if (!code && item.at(i) == u'.' && item.at(i + 1) == u' ' && item.at(i + 2).isUpper()) {
            return item.left(i + 1);
        }
    }
    return item;
}

QString changelogInlineHtml(const QString& text)
{
    static const QRegularExpression kBold(u"\\*\\*(.+?)\\*\\*"_s);
    static const QRegularExpression kCode(u"`([^`]+)`"_s);
    QString html = text.toHtmlEscaped();
    html.replace(kBold, u"<b>\\1</b>"_s);
    html.replace(kCode, u"<code>\\1</code>"_s);
    return html;
}

} // namespace pixelfx::core
