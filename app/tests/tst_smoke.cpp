#include "core/settings/settings.h"
#include "ui/canvas.h"
#include "ui/main_window.h"
#include "ui/style.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;

class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windowBuildsShowsAndOpensAPhoto()
    {
        pixelfx::ui::applyStyle(*qApp);
        QTemporaryDir dir;
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        pfx::Image img(320, 200);
        img.fill(0.3F, 0.2F, 0.1F);
        const QString path = dir.filePath(u"photo.jpg"_s);
        QVERIFY(pfx::save(img, path.toStdString()).ok());
        QSignalSpy ready(&window.document(), &pixelfx::services::Document::previewReady);
        window.openFiles({dir.filePath(u"notes.txt"_s), path});
        QVERIFY(ready.wait(10000));
        QCOMPARE(window.canvas()->image().size(), QSize(320, 200));
        QVERIFY(window.canvas()->photoRect().isValid());
        QCOMPARE(window.windowTitle(), u"photo.jpg"_s);
        QCOMPARE(settings.recentFiles().first(), path);
        window.close();
    }

    void supportedExtensions()
    {
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"/a/B.JPG"_s));
        QVERIFY(pixelfx::ui::MainWindow::isSupported(u"x.webp"_s));
        QVERIFY(!pixelfx::ui::MainWindow::isSupported(u"x.txt"_s));
    }
};

QTEST_MAIN(TestSmoke)
#include "tst_smoke.moc"
