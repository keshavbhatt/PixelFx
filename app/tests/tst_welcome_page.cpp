#include "core/sidecar_store.h"
#include "ui/style.h"
#include "ui/welcome_page.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::RecentCard;
using pixelfx::ui::WelcomePage;

class TestWelcomePage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void formatsLineNamesWhatTheBuildReads()
    {
        QCOMPARE(WelcomePage::formatsText({".jpg", ".png"}), u"JPEG and PNG."_s);
        QCOMPARE(WelcomePage::formatsText({".jpg", ".png", ".tif", ".webp", ".heic", ".jxl", ".dng"}),
                 u"JPEG, PNG, TIFF, WebP, HEIC, JPEG XL and camera RAW."_s);
        QCOMPARE(WelcomePage::formatsText({".jpg"}), u"JPEG."_s);
        QVERIFY(WelcomePage::formatsText({}).isEmpty());
    }

    void recentShowsExistingPhotosWithTheirEdits()
    {
        QTemporaryDir dir;
        pfx::Image img(300, 200);
        img.fill(0.18F, 0.18F, 0.18F);
        const QString a = dir.filePath(u"a.png"_s);
        const QString b = dir.filePath(u"b.png"_s);
        QVERIFY(pfx::save(img, a.toStdString()).ok());
        img.fill(0.3F, 0.2F, 0.1F); // different pixels: a different fingerprint
        QVERIFY(pfx::save(img, b.toStdString()).ok());
        // b was edited: brighter.
        const QString edits = dir.filePath(u"edits"_s);
        pfx::Settings brighter;
        brighter.set("light.exposure", 2.0);
        const pixelfx::core::SidecarStore store(edits);
        QVERIFY(store.write(b, pixelfx::core::fileFingerprint(b), brighter));

        WelcomePage page;
        page.setSidecarFolder(edits);
        page.setRecentFiles({b, dir.filePath(u"gone.png"_s), a});
        const QList<RecentCard*> cards = page.recentCards();
        QCOMPARE(cards.size(), 2);
        RecentCard* edited = cards.first()->path() == b ? cards.first() : cards.last();
        RecentCard* plain = edited == cards.first() ? cards.last() : cards.first();
        QTRY_VERIFY_WITH_TIMEOUT(edited->dateText().startsWith(u"Edited"_s), 10000);
        QVERIFY(!plain->dateText().startsWith(u"Edited"_s));
        QCOMPARE(plain->dateText(), u"Today"_s); // no translation loaded in tests

        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QSignalSpy opened(&page, &WelcomePage::recentOpened);
        QTest::mouseClick(edited, Qt::LeftButton, {}, QPoint(20, 20));
        QCOMPARE(opened.count(), 1);
        QCOMPARE(opened.first().first().toString(), b);

        page.setRecentFiles({});
        QVERIFY(page.recentCards().isEmpty());
    }

    void atMostFourCards()
    {
        QTemporaryDir dir;
        pfx::Image img(30, 20);
        img.fill(0.5F, 0.5F, 0.5F);
        QStringList files;
        for (int i = 0; i < 6; ++i) {
            files << dir.filePath(u"p%1.png"_s.arg(i));
            QVERIFY(pfx::save(img, files.last().toStdString()).ok());
        }
        WelcomePage page;
        page.setRecentFiles(files);
        QCOMPARE(page.recentCards().size(), WelcomePage::kRecentShown);
    }

private:
    static QString tr(const char* text)
    {
        return QCoreApplication::translate("pixelfx::ui::RecentCard", text);
    }
};

QTEST_MAIN(TestWelcomePage)
#include "tst_welcome_page.moc"
