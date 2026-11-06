#include "core/plurals.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <iterator>

using namespace Qt::StringLiterals;

namespace {

// The sources of app/i18n/pixelfx_en.ts; empty when it cannot be read or a form is not filled in.
QSet<QString> knownCounts()
{
    QFile ts(QStringLiteral(PIXELFX_PLURALS_PATH));
    if (!ts.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString forms = QString::fromUtf8(ts.readAll());
    if (forms.contains(u"type=\"unfinished\""_s)) {
        return {};
    }
    QSet<QString> known;
    const QRegularExpression source(u"<source>([^<]*)</source>"_s);
    for (auto it = source.globalMatch(forms); it.hasNext();) {
        known.insert(it.next().captured(1).replace(u"&amp;"_s, u"&"_s).replace(u"&quot;"_s, u"\""_s));
    }
    return known;
}

// Every count the app's code writes, as tr("...%n...", nullptr, n) in one string literal.
QStringList countsInCode()
{
    const QRegularExpression count(uR"re(tr\("((?:[^"\\]|\\.)*%n(?:[^"\\]|\\.)*)",\s*nullptr)re"_s);
    QStringList out;
    QDirIterator files(QStringLiteral(PIXELFX_SOURCE_PATH), {u"*.cpp"_s}, QDir::Files,
                       QDirIterator::Subdirectories);
    while (files.hasNext()) {
        QFile file(files.next());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        const QString code = QString::fromUtf8(file.readAll());
        for (auto it = count.globalMatch(code); it.hasNext();) {
            out << it.next().captured(1).replace(u"\\\""_s, u"\""_s);
        }
    }
    return out;
}

} // namespace

// Every count in the app's texts has its English singular (ADR-014): a tr("... %n ...") with no
// entry in app/i18n/pixelfx_en.ts would read "1 photos".
class TestPlurals : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    static void everyCountHasItsSingular()
    {
        const QSet<QString> known = knownCounts();
        QVERIFY2(!known.isEmpty(), "the plural forms cannot be read, or one is not filled in");
        const QStringList counts = countsInCode();
        QVERIFY(counts.size() > 20); // the pattern finds the app's counts
        QStringList missing;
        std::ranges::copy_if(counts, std::back_inserter(missing),
                             [&known](const QString& t) { return !known.contains(t); });
        QVERIFY2(missing.isEmpty(), qPrintable(u"no plural forms for: "_s + missing.join(u" | "_s)));
    }

    static void oneIsSaidInTheSingular()
    {
        QVERIFY(pixelfx::core::installPluralForms(*QCoreApplication::instance()));
        QCOMPARE(QCoreApplication::translate("pixelfx::ui::RecipePicker", "%n presets", nullptr, 1),
                 u"1 preset"_s);
        QCOMPARE(QCoreApplication::translate("pixelfx::ui::RecipePicker", "%n presets", nullptr, 4),
                 u"4 presets"_s);
    }
};

QTEST_GUILESS_MAIN(TestPlurals)
#include "tst_plurals.moc"
