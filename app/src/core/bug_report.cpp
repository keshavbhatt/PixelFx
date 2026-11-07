#include "core/bug_report.h"

#include "core/links.h"

#include <QObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace pixelfx::core {

namespace {

constexpr qsizetype kTitleLength = 80;
constexpr qsizetype kLongestUrl = 6000; // browsers and the tracker take about this much

// "a .jpg file" or "a folder", from a path's last part.
QString kindOf(const QString& path)
{
    const QString last = path.section(QRegularExpression(u"[/\\\\]"_s), -1, -1, QString::SectionSkipEmpty);
    static const QRegularExpression kExtension(u"\\.([A-Za-z0-9]{1,5})$"_s);
    const QRegularExpressionMatch m = kExtension.match(last);
    return m.hasMatch() ? u"a .%1 file"_s.arg(m.captured(1).toLower()) : u"a folder"_s;
}

QString fenced(const QString& text)
{
    return u"```\n"_s + text.trimmed() + u"\n```\n"_s;
}

} // namespace

QString withoutNames(const QString& text)
{
    // A path in quotes may hold spaces; one without ends at a space or a bracket. A path starts
    // at "/", "~/" or a drive ("C:\"), not after a letter, a digit, a dot or a colon (f/8, 1/250 s,
    // https://).
    static const QRegularExpression kQuoted(u"\"((?:~|[A-Za-z]:)?[/\\\\][^\"]*)\""_s);
    static const QRegularExpression kBare(
        u"(?<![\\w.:/\\\\~])((?:~|[A-Za-z]:)?(?:[/\\\\][^\\s\"'<>()\\[\\],;]+)+)"_s);
    QString out;
    qsizetype from = 0;
    auto quoted = kQuoted.globalMatch(text);
    while (quoted.hasNext()) {
        const QRegularExpressionMatch m = quoted.next();
        out += QStringView(text).mid(from, m.capturedStart() - from);
        out += kindOf(m.captured(1));
        from = m.capturedEnd();
    }
    out += QStringView(text).mid(from);
    QString done;
    from = 0;
    auto bare = kBare.globalMatch(out);
    while (bare.hasNext()) {
        const QRegularExpressionMatch m = bare.next();
        done += QStringView(out).mid(from, m.capturedStart() - from);
        done += kindOf(m.captured(1));
        from = m.capturedEnd();
    }
    done += QStringView(out).mid(from);
    return done;
}

QStringList previousRun(const QStringList& log, int count)
{
    static const QRegularExpression kStart(u"\\] info: pixelfx \\d"_s);
    QList<qsizetype> starts;
    for (qsizetype i = 0; i < log.size(); ++i) {
        if (kStart.match(log.at(i)).hasMatch()) {
            starts << i;
        }
    }
    // The run before the last start: from the start before it (or the top), to the last start.
    const qsizetype end = starts.isEmpty() ? log.size() : starts.last();
    const qsizetype begin = starts.size() >= 2 ? starts.at(starts.size() - 2) : 0;
    const qsizetype first = std::max(begin, end - count);
    return log.mid(first, end - first);
}

QString reportTitle(const BugReport& report)
{
    QString first;
    for (const QString& line : report.whatHappened.split(u'\n')) {
        if (!line.trimmed().isEmpty()) {
            first = line.trimmed();
            break;
        }
    }
    if (first.isEmpty()) {
        return report.crash.isEmpty() ? QObject::tr("Bug report")
                                      : QObject::tr("PixelFx closed unexpectedly");
    }
    return first.size() > kTitleLength ? first.left(kTitleLength - 3).trimmed() + u"..."_s : first;
}

QString reportBody(const BugReport& report)
{
    QString body = u"### What happened\n\n"_s;
    body += report.whatHappened.trimmed().isEmpty() ? u"(not written)\n"_s
                                                    : report.whatHappened.trimmed() + u'\n';
    if (!report.crash.isEmpty()) {
        body += u"\n### What PixelFx was doing when it closed\n\n"_s + fenced(report.crash);
    }
    if (!report.diagnostics.isEmpty()) {
        body += u"\n### Diagnostics\n\n"_s + fenced(report.diagnostics);
    }
    if (!report.log.isEmpty()) {
        body += u"\n### Log\n\n"_s + fenced(report.log.join(u'\n'));
    }
    return body;
}

QUrl issueUrl(const QString& title, const QString& body)
{
    // Encoded once, by hand: "&", "=", "#" and "+" in the text must not end a field.
    QUrl url(issuesUrl() + u"/new"_s);
    const QByteArray query =
        "title=" + QUrl::toPercentEncoding(title) + "&body=" + QUrl::toPercentEncoding(body);
    url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
    return url.toEncoded().size() > kLongestUrl ? QUrl() : url;
}

QUrl fallbackUrl(const QString& title)
{
    //: The text of a new issue when the report was too long to fill in: it is on the clipboard.
    return issueUrl(title, QObject::tr("The report is on the clipboard: paste it here."));
}

} // namespace pixelfx::core
