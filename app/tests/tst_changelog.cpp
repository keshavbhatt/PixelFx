#include "core/changelog.h"

#include <QFile>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

namespace {
const QString kDoc =
    u"# Changelog\n\nintro\n\n## [10.1.0] - unreleased\n\nNew stuff.\n\n### Added\n- **A**: one\n\n"
    "## [10.0.0] - 2026-09-20\n\nRewrite.\n\n### Removed\n- old\n\n[10.1.0]: https://x/compare\n[10.0.0]: https://x/tag\n"_s;
}

class TestChangelog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sectionWithSuffixStopsAtNextHeading()
    {
        QCOMPARE(changelogSection(kDoc, u"10.1.0"_s), u"New stuff.\n\n### Added\n- **A**: one"_s);
    }

    void lastSectionRunsToEndWithoutLinkReferences()
    {
        QCOMPARE(changelogSection(kDoc, u"10.0.0"_s), u"Rewrite.\n\n### Removed\n- old"_s);
    }

    void headingVariants()
    {
        QCOMPARE(changelogSection(u"## [v2.0.0]\nbody\n"_s, u"2.0.0"_s), u"body"_s);
        QCOMPARE(changelogSection(u"## [2.0.0]\r\nline one\r\nline two\r\n"_s, u"2.0.0"_s),
                 u"line one\nline two"_s);
    }

    void missingVersionIsEmpty()
    {
        QVERIFY(changelogSection(kDoc, u"9.0.0"_s).isEmpty());
        QVERIFY(changelogSection(kDoc, u"10.0.0-test"_s).isEmpty());
        QVERIFY(changelogSection(QString(), u"10.0.0"_s).isEmpty());
    }

    void notesSplitTheLeadAndTheGroups()
    {
        const ChangelogNotes notes = changelogNotes(
            u"Rebuilt.\nFrom scratch.\n\nSecond.\n\n### Added\n\n- **A**: one\n  wrapped\n- B\n\n### Fixed\n- C"_s);
        QCOMPARE(notes.lead, (QStringList{u"Rebuilt. From scratch."_s, u"Second."_s}));
        QCOMPARE(notes.groups.size(), 2);
        QCOMPARE(notes.groups.at(0).title, u"Added"_s);
        QCOMPARE(notes.groups.at(0).items, (QStringList{u"**A**: one wrapped"_s, u"B"_s}));
        QCOMPARE(notes.groups.at(1).items, (QStringList{u"C"_s}));
    }

    void inlineMarksBecomeTagsAndTheRestIsEscaped()
    {
        QCOMPARE(changelogInlineHtml(u"**Batch** <b> `pixelfx --batch` & more"_s),
                 u"<b>Batch</b> &lt;b&gt; <code>pixelfx --batch</code> &amp; more"_s);
    }

    void aHighlightIsTheFirstSentence()
    {
        QCOMPARE(changelogFirstSentence(u"Crop (C): crop freely. The crop stays editable."_s),
                 u"Crop (C): crop freely."_s);
        QCOMPARE(changelogFirstSentence(u"Bring .cube LUTs or HaldCLUT pictures"_s),
                 u"Bring .cube LUTs or HaldCLUT pictures"_s);
        QCOMPARE(changelogFirstSentence(u"Run `pixelfx --batch a. B` here. Then more."_s),
                 u"Run `pixelfx --batch a. B` here."_s);
    }

    void realChangelogHasNotesForThisVersion()
    {
        QFile file(QString::fromLatin1(PIXELFX_CHANGELOG_PATH));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());
        QVERIFY2(!changelogSection(text, QString::fromLatin1(PIXELFX_VERSION)).isEmpty(),
                 "CHANGELOG.md has no section for the project version");
        // Owner's rule (CLAUDE.md): accounts and licensing never appear in
        // user-facing release notes, and this file feeds the What's new dialog.
        static const QRegularExpression kForbidden(
            u"licen[cs]e|licensing|\\baccounts?\\b|checkout|evaluation|\\btrial\\b|PixelFx Pro|activation"_s,
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = kForbidden.match(text);
        QVERIFY2(!m.hasMatch(), qPrintable(u"CHANGELOG.md mentions licensing: "_s + m.captured(0)));
    }
};

QTEST_GUILESS_MAIN(TestChangelog)
#include "tst_changelog.moc"
