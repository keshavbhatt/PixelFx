// Photo details: the words for what the camera wrote, a long folder fitted to its room, and the
// panel: its rows, the location only after Show, and opening and closing from the window.
#include "core/photo_details.h"
#include "core/settings/settings.h"
#include "services/document.h"
#include "ui/elided_label.h"
#include "ui/main_window.h"
#include "ui/photo_details_panel.h"
#include "ui/style.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFontMetrics>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/io.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::PathLabel;
using pixelfx::ui::PhotoDetailsPanel;

namespace {

pfx::ExifSummary fujifilm()
{
    pfx::ExifSummary exif;
    exif.make = "FUJIFILM";
    exif.model = "X-T4";
    exif.lens = "XF16-80mmF4 R OIS WR";
    exif.dateTaken = "2026:10:03 18:42:10";
    exif.iso = 100;
    exif.exposureTime = 1.0 / 250.0;
    exif.fNumber = 8.0;
    exif.focalLength = 24.0;
    exif.hasLocation = true;
    exif.latitude = 46.376120;
    exif.longitude = -13.836450;
    return exif;
}

pixelfx::services::DocumentInfo photo(const QString& path)
{
    pixelfx::services::DocumentInfo info;
    info.path = path;
    info.size = QSize(6000, 4000);
    info.format = u"jpegload"_s;
    info.bitsPerSample = 8;
    info.hasProfile = true;
    info.profileName = u"Display P3"_s;
    info.exif = fujifilm();
    return info;
}

// The texts of the panel's labels, in order.
QStringList texts(const QWidget& panel)
{
    QStringList out;
    for (const QLabel* label : panel.findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) {
            out << label->text();
        }
    }
    return out;
}

} // namespace

class TestPhotoDetails : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
        pixelfx::ui::applyStyle(*qApp);
    }

    void cameraDetailsReadAsPhotographersWriteThem()
    {
        using namespace pixelfx::core;
        const pfx::ExifSummary exif = fujifilm();
        QCOMPARE(cameraName(exif), u"Fujifilm X-T4"_s);
        QCOMPARE(exposureSettings(exif), u"1/250 s \u00B7 f/8 \u00B7 ISO 100"_s);
        QCOMPARE(focalLength(exif), u"24 mm"_s);
        QCOMPARE(takenAt(exif), QDateTime(QDate(2026, 10, 3), QTime(18, 42, 10)));
        QVERIFY(takenText(takenAt(exif)).startsWith(u"3 October 2026, "_s));
        QVERIFY(takenText(QDateTime()).isEmpty());
        QCOMPARE(coordinatesText(exif.latitude, exif.longitude), u"46.37612\u00B0 N, 13.83645\u00B0 W"_s);
        QCOMPARE(coordinatesForCopy(exif.latitude, exif.longitude), u"46.37612, -13.83645"_s);
        QCOMPARE(sizeWithMegapixels(QSize(6000, 4000)), u"6000 x 4000 (24 MP)"_s);
        QCOMPARE(sizeWithMegapixels(QSize(2048, 1365)), u"2048 x 1365 (2.8 MP)"_s);
        QCOMPARE(formatName(u"jpegload"_s, u"/a/b.jpg"_s, false), u"JPEG"_s);
        QCOMPARE(formatName(u"heifload"_s, u"/a/b.heic"_s, false), u"HEIC"_s);
        QCOMPARE(formatName(u"dcrawload"_s, u"/a/b.raf"_s, true), u"RAW (RAF)"_s);
        // Nothing known: nothing said.
        const pfx::ExifSummary none;
        QVERIFY(exposureSettings(none).isEmpty());
        QVERIFY(focalLength(none).isEmpty());
        QVERIFY(!takenAt(none).isValid());
    }

    void makesAreSaidAsPeopleSayThem_data()
    {
        QTest::addColumn<QString>("make");
        QTest::addColumn<QString>("model");
        QTest::addColumn<QString>("expected");
        QTest::newRow("caps") << u"FUJIFILM"_s << u"X-T4"_s << u"Fujifilm X-T4"_s;
        QTest::newRow("company") << u"NIKON CORPORATION"_s << u"NIKON Z 6_2"_s << u"Nikon Z 6_2"_s;
        QTest::newRow("imaging corp") << u"OLYMPUS IMAGING CORP."_s << u"E-M5"_s << u"Olympus E-M5"_s;
        QTest::newRow("company ltd") << u"RICOH IMAGING COMPANY, LTD."_s << u"GR III"_s << u"Ricoh GR III"_s;
        QTest::newRow("camera ag") << u"LEICA CAMERA AG"_s << u"Q2"_s << u"Leica Q2"_s;
        QTest::newRow("repeated") << u"Canon"_s << u"Canon EOS R5"_s << u"Canon EOS R5"_s;
        QTest::newRow("as written") << u"Apple"_s << u"iPhone 15"_s << u"Apple iPhone 15"_s;
        QTest::newRow("model only") << QString() << u"X100V"_s << u"X100V"_s;
    }

    void makesAreSaidAsPeopleSayThem()
    {
        QFETCH(QString, make);
        QFETCH(QString, model);
        QFETCH(QString, expected);
        pfx::ExifSummary exif;
        exif.make = make.toStdString();
        exif.model = model.toStdString();
        QCOMPARE(pixelfx::core::cameraName(exif), expected);
    }

    void aLongFolderKeepsItsStartAndItsEnd()
    {
        const QFontMetrics metrics(qApp->font());
        const QString path = u"~/Pictures/Archive/Travel/Europe/Slovenia/2026/Lake trip"_s;
        const auto native = [](const QString& s) { return QDir::toNativeSeparators(s); };
        // Room for all: whole.
        QCOMPARE(PathLabel::fitted(path, metrics, 2000), native(path));
        // Less room: folders go from the middle, nearest the start first.
        const int room = metrics.horizontalAdvance(native(u"~/\u2026/Slovenia/2026/Lake trip"_s));
        QCOMPARE(PathLabel::fitted(path, metrics, room), native(u"~/\u2026/Slovenia/2026/Lake trip"_s));
        QCOMPARE(PathLabel::fitted(path, metrics, room - 1), native(u"~/\u2026/2026/Lake trip"_s));
        // Only the start and the last folder left, then the last cut in its middle.
        const int least = metrics.horizontalAdvance(native(u"~/\u2026/Lake trip"_s));
        QCOMPARE(PathLabel::fitted(path, metrics, least), native(u"~/\u2026/Lake trip"_s));
        const QString cut = PathLabel::fitted(path, metrics, least - 10);
        QVERIFY(cut.startsWith(native(u"~/\u2026/"_s)));
        QVERIFY(metrics.horizontalAdvance(cut) <= least - 10);
        // Outside the home folder and on a drive: the start is "/" or "C:".
        QCOMPARE(PathLabel::fitted(u"/media/disk/a/b/Trip"_s, metrics,
                                   metrics.horizontalAdvance(native(u"/\u2026/b/Trip"_s))),
                 native(u"/\u2026/b/Trip"_s));
        QCOMPARE(PathLabel::fitted(u"C:/Users/me/Pictures/Trip"_s, metrics,
                                   metrics.horizontalAdvance(native(u"C:/\u2026/Pictures/Trip"_s))),
                 native(u"C:/\u2026/Pictures/Trip"_s));
    }

    void thePanelSaysWhatTheFileIs()
    {
        QWidget window;
        window.resize(1200, 800);
        PhotoDetailsPanel panel(&window);
        panel.setDetails(photo(u"/photos/sunset-lake.jpg"_s), PhotoDetailsPanel::LocationOnExport::LeftOut);
        const QStringList shown = texts(panel);
        for (const QString& expected :
             {u"sunset-lake.jpg"_s, u"6000 x 4000 (24 MP)"_s, u"JPEG, 8 bits"_s, u"Display P3, embedded"_s,
              u"Fujifilm X-T4"_s, u"XF16-80mmF4 R OIS WR"_s, u"1/250 s \u00B7 f/8 \u00B7 ISO 100"_s,
              u"24 mm"_s, u"Recorded in the file"_s, u"Export leaves it out: Keep location is off."_s}) {
            QVERIFY2(shown.contains(expected), qPrintable(expected));
        }
        // The coordinates only after Show, and Copy details leaves them out until then.
        QVERIFY(!panel.locationShown());
        QVERIFY(!panel.detailsText().contains(u"46.37612"_s));
        QVERIFY(panel.detailsText().contains(u"Lens: XF16-80mmF4 R OIS WR"_s));
        panel.showLocation(true);
        QVERIFY(texts(panel).contains(u"46.37612\u00B0 N, 13.83645\u00B0 W"_s));
        QVERIFY(panel.detailsText().contains(u"46.37612"_s));
        // The same photo again keeps them; the next photo hides them.
        panel.setDetails(photo(u"/photos/sunset-lake.jpg"_s), PhotoDetailsPanel::LocationOnExport::Kept);
        QVERIFY(panel.locationShown());
        QVERIFY(texts(panel).contains(u"Export keeps it: Keep location is on."_s));
        panel.setDetails(photo(u"/photos/next.jpg"_s), PhotoDetailsPanel::LocationOnExport::LeftOut);
        QVERIFY(!panel.locationShown());
    }

    void aFileWithoutCameraDetailsSaysSo()
    {
        QWidget window;
        PhotoDetailsPanel panel(&window);
        pixelfx::services::DocumentInfo info;
        info.path = u"/downloads/web.png"_s;
        info.size = QSize(2048, 1365);
        info.format = u"pngload"_s;
        info.hasAlpha = true;
        panel.setDetails(info, PhotoDetailsPanel::LocationOnExport::LeftOut);
        const QStringList shown = texts(panel);
        QVERIFY(shown.contains(u"PNG, 8 bits, transparency"_s));
        QVERIFY(shown.contains(u"None, read as sRGB"_s));
        QVERIFY(shown.contains(u"No camera details in this file."_s));
        QVERIFY(shown.contains(u"No location in this file."_s));
        QVERIFY(!shown.contains(u"Lens"_s));
    }

    void iOpensItAndEscapeClosesIt()
    {
        QTemporaryDir dir;
        pfx::Image image(64, 48);
        image.fill(0.4F, 0.3F, 0.2F);
        const QString file = dir.filePath(u"photo.jpg"_s);
        QVERIFY(pfx::save(image, file.toStdString()).ok());
        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto* panel = window.findChild<PhotoDetailsPanel*>();
        QVERIFY(panel != nullptr);
        QAction* details = window.namedAction(u"photoDetails"_s);
        // Nothing open: nothing to show.
        details->trigger();
        QVERIFY(!panel->isVisible());
        QSignalSpy opened(&window.document(), &pixelfx::services::Document::opened);
        window.openFiles({file});
        QVERIFY(opened.wait(5000));
        QTest::keyClick(&window, Qt::Key_I);
        QTRY_VERIFY(panel->isVisible());
        QVERIFY(details->isChecked());
        QVERIFY(panel->isAncestorOf(QApplication::focusWidget()));
        // Below the header, on the window.
        QVERIFY(panel->geometry().top() > 50);
        QVERIFY(window.rect().contains(panel->geometry()));
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Escape);
        QTRY_VERIFY(!panel->isVisible());
        QVERIFY(!details->isChecked());
        // The header's name opens it too, and closing the photo closes it.
        auto* name = window.findChild<QPushButton*>(u"docButton"_s);
        QVERIFY(name != nullptr);
        QTest::mouseClick(name, Qt::LeftButton);
        QTRY_VERIFY(panel->isVisible());
        window.namedAction(u"closePhoto"_s)->trigger();
        QTRY_VERIFY(!panel->isVisible());
    }
};

QTEST_MAIN(TestPhotoDetails)
#include "tst_photo_details.moc"
