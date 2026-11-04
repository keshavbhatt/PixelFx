#include "ui/export_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::ExportOptions;
using pixelfx::ui::ExportSheet;

class TestExportSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void untouchedSheetReturnsTheRememberedChoices()
    {
        ExportOptions in;
        in.extension = u"png"_s;
        in.quality = 77;
        in.bitsPerSample = 16;
        in.longEdge = 1600;
        in.keepMetadata = false;
        in.folder = u"/tmp/out"_s;
        in.nameTemplate = u"{name}-web"_s;
        in.ifExists = ExportOptions::IfExists::Skip;
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(6000, 4000), in);
        QCOMPARE(sheet.options(), in);
    }

    void switchingToJpegDropsSixteenBits()
    {
        ExportOptions in;
        in.extension = u"png"_s;
        in.bitsPerSample = 16;
        ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), in);
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->property("extension").toString() == u"jpg"_s) {
                b->click();
            }
        }
        const ExportOptions out = sheet.options();
        QCOMPARE(out.extension, u"jpg"_s);
        QCOMPARE(out.bitsPerSample, 8);
    }

    void onlyWritableFormatsAreOffered()
    {
        const ExportSheet sheet(u"/p/photo.jpg"_s, QSize(600, 400), ExportOptions{});
        QStringList offered;
        for (const QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            offered << b->property("extension").toString();
        }
        for (const char* ext : {"jpg", "png", "webp", "tif"}) {
            QVERIFY2(offered.contains(QString::fromLatin1(ext)), ext);
        }
    }
};

QTEST_MAIN(TestExportSheet)
#include "tst_export_sheet.moc"
