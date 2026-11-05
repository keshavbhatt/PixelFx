#include "core/settings/settings.h"

#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::Settings;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void generalPageDefaultsAndChanges()
    {
        QTemporaryDir dir;
        pixelfx::core::Settings s(dir.filePath(u"g.ini"_s));
        using S = pixelfx::core::Settings;
        QCOMPARE(s.theme(), S::Theme::Dark);
        QCOMPARE(s.canvasBackground(), S::CanvasBackground::DarkGray);
        QVERIFY(s.rememberEdits());
        QVERIFY(s.openAsLeft());
        QVERIFY(!s.clippingWarnings());
        QSignalSpy clipping(&s, &S::clippingWarningsChanged);
        s.setClippingWarnings(true);
        s.setClippingWarnings(true); // no change, no signal
        QCOMPARE(clipping.count(), 1);
        s.setTheme(S::Theme::Light);
        s.setCanvasBackground(S::CanvasBackground::White);
        s.setRememberEdits(false);
        s.setOpenAsLeft(false);
        s.sync();
        const pixelfx::core::Settings again(dir.filePath(u"g.ini"_s));
        QCOMPARE(again.theme(), S::Theme::Light);
        QCOMPARE(again.canvasBackground(), S::CanvasBackground::White);
        QVERIFY(!again.rememberEdits());
        QVERIFY(!again.openAsLeft());
        QVERIFY(again.clippingWarnings());
    }

    void defaultsAreSensible()
    {
        QTemporaryDir dir;
        const Settings s(dir.filePath(u"s.ini"_s));
        QVERIFY(s.windowGeometry().isEmpty());
        QVERIFY(s.recentFiles().isEmpty());
        QVERIFY(QFileInfo(s.lastOpenFolder()).isDir()); // Pictures or home
        QVERIFY(!s.openAdjustGroups().has_value());     // the panel keeps its own until then
    }

    void valuesPersistAndSignal()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(u"s.ini"_s);
        {
            Settings s(file);
            QSignalSpy folderSpy(&s, &Settings::lastOpenFolderChanged);
            s.setLastOpenFolder(dir.path());
            s.setLastOpenFolder(dir.path()); // unchanged: no second signal
            QCOMPARE(folderSpy.count(), 1);
            s.setWindowGeometry(QByteArray("geometry"));
            s.setOpenAdjustGroups({});
            s.sync();
        }
        const Settings again(file);
        QCOMPARE(again.openAdjustGroups(), std::optional<QStringList>(QStringList())); // all closed
        QCOMPARE(again.lastOpenFolder(), dir.path());
        QCOMPARE(again.windowGeometry(), QByteArray("geometry"));
    }

    void missingFolderFallsBack()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath(u"s.ini"_s));
        s.setLastOpenFolder(dir.filePath(u"gone"_s));
        QVERIFY(s.lastOpenFolder() != dir.filePath(u"gone"_s));
    }

    void recentFilesAreMostRecentFirstUniqueAndCapped()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath(u"s.ini"_s));
        QSignalSpy spy(&s, &Settings::recentFilesChanged);
        for (int i = 0; i < Settings::kMaxRecentFiles + 3; ++i) {
            s.addRecentFile(u"/photos/%1.jpg"_s.arg(i));
        }
        s.addRecentFile(u"/photos/5.jpg"_s);
        const QStringList files = s.recentFiles();
        QCOMPARE(files.size(), Settings::kMaxRecentFiles);
        QCOMPARE(files.first(), u"/photos/5.jpg"_s);
        QCOMPARE(files.count(u"/photos/5.jpg"_s), 1);
        QCOMPARE(spy.count(), Settings::kMaxRecentFiles + 4);
        s.clearRecentFiles();
        QVERIFY(s.recentFiles().isEmpty());
    }

    void version1FolderIsMigratedOnce()
    {
        QTemporaryDir dir;
        const QString legacy = dir.filePath(u"pixelFx.conf"_s);
        {
            QSettings old(legacy, QSettings::IniFormat);
            old.setValue(u"last_image_dir_path"_s, dir.path());
        }
        Settings s(dir.filePath(u"s.ini"_s));
        QVERIFY(s.migrateFromVersion1(legacy));
        QCOMPARE(s.lastOpenFolder(), dir.path());
        s.setLastOpenFolder(QDir::homePath());
        QVERIFY(!s.migrateFromVersion1(legacy)); // only once
        QCOMPARE(s.lastOpenFolder(), QDir::homePath());
    }

    void missingVersion1FileIsFine()
    {
        QTemporaryDir dir;
        Settings s(dir.filePath(u"s.ini"_s));
        QVERIFY(!s.migrateFromVersion1(dir.filePath(u"none.conf"_s)));
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
