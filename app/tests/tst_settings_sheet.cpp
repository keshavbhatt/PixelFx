#include "core/settings/settings.h"
#include "ui/canvas.h"
#include "ui/settings_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::Settings;

class TestSettingsSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void everyRowWritesStraightThrough()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::SettingsSheet sheet(settings);
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"Light"_s || b->text() == u"Black"_s) {
                b->click();
            }
        }
        QCOMPARE(settings.theme(), Settings::Theme::Light);
        QCOMPARE(settings.canvasBackground(), Settings::CanvasBackground::Black);
        for (QCheckBox* c : sheet.findChildren<QCheckBox*>()) {
            c->click(); // remember edits off, clipping warnings on
        }
        QVERIFY(!settings.rememberEdits());
        QVERIFY(settings.clippingWarnings());
        auto* onOpen = sheet.findChild<QComboBox*>();
        QVERIFY(!onOpen->isEnabled()); // nothing remembered: nothing to open as left
    }

    void clippingOverlayMarksWhiteAndBlack()
    {
        QImage img(3, 1, QImage::Format_RGB32);
        img.setPixel(0, 0, qRgb(255, 200, 200)); // a channel at white
        img.setPixel(1, 0, qRgb(0, 0, 1));       // black
        img.setPixel(2, 0, qRgb(120, 120, 120)); // fine
        const QImage overlay = pixelfx::ui::Canvas::clippingOverlay(img);
        QCOMPARE(qRed(overlay.pixel(0, 0)), 255);
        QCOMPARE(qBlue(overlay.pixel(1, 0)), 255);
        QCOMPARE(qAlpha(overlay.pixel(2, 0)), 0);
    }
};

QTEST_MAIN(TestSettingsSheet)
#include "tst_settings_sheet.moc"
