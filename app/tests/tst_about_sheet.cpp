#include "core/about.h"
#include "core/settings/settings.h"
#include "ui/about_sheet.h"
#include "ui/main_window.h"
#include "ui/style.h"
#include "ui/whats_new_sheet.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

using namespace Qt::StringLiterals;
using pixelfx::ui::AboutSheet;
using pixelfx::ui::WhatsNewSheet;

namespace {

const QString kChangelog =
    u"# Changelog\n\n## [3.1.0] - unreleased\n\nBetter.\n\nSmaller things.\n\n"
    "### Added\n\n- **One**: 1\n- Two\n- Three\n\n### Fixed\n\n- Four\n- Five\n- Six\n\n"
    "## [3.0.0] - 2026-01-02\n\nFirst.\n\n### Added\n\n- Only\n"_s;

// Closes the sheet that opens next, as a person would.
void closeNextSheet()
{
    QTimer::singleShot(0, qApp, [] {
        if (QWidget* w = QApplication::activeModalWidget()) {
            w->close();
        }
    });
}

} // namespace

class TestAboutSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QApplication::setApplicationVersion(QString::fromLatin1(PIXELFX_VERSION)); // as Application does
        pixelfx::ui::applyStyle(*qApp);
    }

    void noticeRowsAreSpacedSoTheirHighlightsDoNotTouch()
    {
        AboutSheet sheet;
        auto* list = sheet.findChild<QListWidget*>(u"noticeList"_s);
        QVERIFY(list != nullptr);
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        QCoreApplication::processEvents();
        const QRect first = list->visualItemRect(list->item(0));
        const QRect second = list->visualItemRect(list->item(1));
        QVERIFY(first.height() >= list->fontMetrics().height() + 10);
        QVERIFY(second.top() > first.bottom()); // a row each; the margin keeps the highlights apart
    }

    void whatsNewShowsTheFirstFewThenEverything()
    {
        WhatsNewSheet sheet(kChangelog, u"3.1.0"_s, true);
        QString html = sheet.html();
        QVERIFY(html.contains(u"Better."_s));
        QVERIFY(html.contains(u"One"_s));
        QVERIFY(html.contains(u"Five"_s));
        QVERIFY(!html.contains(u"Six"_s)); // the sixth item waits for the full list
        auto* more = sheet.findChild<QLabel*>(u"credit"_s);
        QVERIFY(more != nullptr && !more->isHidden());
        sheet.showAll();
        QVERIFY(sheet.html().contains(u"Six"_s));
        QVERIFY(more->isHidden());
        // Older versions come from the same changelog; a short section needs no link.
        auto* versions = sheet.findChild<QComboBox*>();
        QCOMPARE(versions->count(), 2);
        sheet.showVersion(u"3.0.0"_s);
        QVERIFY(sheet.html().contains(u"Only"_s));
        QVERIFY(!sheet.showingAll());
        QVERIFY(more->isHidden());
    }

    void whatsNewHidesTheVersionListWithOneRelease()
    {
        WhatsNewSheet sheet(u"## [1.0.0]\n\nHello.\n"_s, u"1.0.0"_s, false);
        QVERIFY(sheet.findChild<QComboBox*>()->isHidden());
        QVERIFY(!sheet.showAfterUpdates());
    }

    void bundledChangelogHasThisVersion()
    {
        WhatsNewSheet sheet(WhatsNewSheet::bundledChangelog(), QApplication::applicationVersion(), true);
        QVERIFY(!sheet.html().contains(u"No notes for this version"_s));
    }

    void aboutPagesNoticesAndDiagnostics()
    {
        AboutSheet sheet;
        QCOMPARE(sheet.page(), AboutSheet::AboutPage);
        sheet.showPage(AboutSheet::NoticesPage);
        QCOMPARE(sheet.page(), AboutSheet::NoticesPage);
        QVERIFY(sheet.noticeShown().contains(u"GNU GENERAL PUBLIC LICENSE"_s)); // PixelFx first
        const auto& list = pixelfx::core::notices();
        for (int i = 0; i < list.size(); ++i) {
            sheet.showNotice(i);
            QVERIFY2(sheet.noticeShown().size() > 500, qPrintable(list.at(i).name));
        }
        sheet.showPage(AboutSheet::DiagnosticsPage);
        QVERIFY(sheet.diagnostics().startsWith(u"PixelFx "_s));
        // Each page is as tall as it needs, at one width; the notices get the most room.
        const QSize diagnostics = sheet.size();
        sheet.showPage(AboutSheet::AboutPage);
        QCOMPARE(sheet.width(), diagnostics.width());
        const int about = sheet.height();
        sheet.showPage(AboutSheet::NoticesPage);
        QVERIFY(sheet.height() > about);
        QVERIFY(sheet.height() > diagnostics.height());
    }

    void whatsNewOpensOnceAfterAnUpdate()
    {
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        // A first install records the version and shows nothing.
        QVERIFY(!window.showWhatsNewIfUpdated());
        QCOMPARE(settings.whatsNewSeen(), QApplication::applicationVersion());
        // After an update it opens once.
        settings.setWhatsNewSeen(u"1.9.0"_s);
        closeNextSheet();
        QVERIFY(window.showWhatsNewIfUpdated());
        QVERIFY(!window.showWhatsNewIfUpdated());
        // Turned off, an update only records the version.
        settings.setWhatsNewSeen(u"1.9.0"_s);
        settings.setShowWhatsNew(false);
        QVERIFY(!window.showWhatsNewIfUpdated());
        QCOMPARE(settings.whatsNewSeen(), QApplication::applicationVersion());
    }
};

QTEST_MAIN(TestAboutSheet)
#include "tst_about_sheet.moc"
