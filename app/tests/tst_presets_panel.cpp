#include "ui/paste_sheet.h"
#include "ui/preset_save_sheet.h"
#include "ui/presets_panel.h"
#include "ui/style.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::PresetEntry;
using pixelfx::ui::PresetsPanel;

class TestPresetsPanel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void rowsBySearchAndClickApplies()
    {
        PresetsPanel panel;
        std::vector<PresetEntry> entries;
        for (const char* name : {"Warm evening", "Cool morning", "Punchy"}) {
            PresetEntry e;
            e.name = QString::fromLatin1(name);
            e.group = u"My presets"_s;
            e.path = u"/x/"_s + e.name;
            e.settings.set("light.exposure", 0.2);
            entries.push_back(e);
        }
        panel.setEntries(entries);
        QCOMPARE(panel.rows().size(), 3);
        panel.findChild<QLineEdit*>()->setText(u"warm"_s);
        QCOMPARE(panel.rows().size(), 1);
        QSignalSpy apply(&panel, &PresetsPanel::applyRequested);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QTest::mouseClick(panel.rows().first(), Qt::LeftButton, {}, QPoint(80, 10));
        QCOMPARE(apply.count(), 1);
        QCOMPARE(apply.first().first().value<PresetEntry>().name, u"Warm evening"_s);
    }

    void pasteSheetOffersWhatWasCopied()
    {
        pfx::Settings copied;
        copied.set("light.exposure", 0.4);
        copied.set("color.wb.temperature", 12.0);
        pixelfx::ui::PasteSheet sheet(copied, 3);
        QCOMPARE(sheet.units(), QStringList{u"light"_s}); // White balance offered but not ticked
        sheet.tick({u"light"_s, u"wb"_s});
        QCOMPARE(sheet.units().size(), 2);
    }

    void saveSheetTicksChangedPartsButWhiteBalanceAndCrop()
    {
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        s.set("color.wb.temperature", 10.0);
        s.set("geometry.crop.w", 0.5);
        s.set("detail.clarity", 20.0);
        pixelfx::ui::PresetSaveSheet sheet(s, {u"Clients"_s});
        QCOMPARE(sheet.units(), (QStringList{u"light"_s, u"detail"_s}));
        QCOMPARE(sheet.group(), u"My presets"_s);
        sheet.tick({u"wb"_s});
        QCOMPARE(sheet.units(), QStringList{u"wb"_s});
    }

    void saveSheetNamesOneOrTwoChangedSlidersAndCountsMore()
    {
        pfx::Settings s;
        s.set("detail.clarity", 20.0);
        s.set("detail.sharpen.amount", 30.0);
        s.set("light.exposure", 0.5);
        s.set("light.contrast", 10.0);
        s.set("light.shadows", 10.0);
        s.set("curves.rgb", pfx::Curve{{0.0, 0.1}, {1.0, 0.9}});
        const pixelfx::ui::PresetSaveSheet sheet(s, {});
        QStringList notes;
        for (const QLabel* label : sheet.findChildren<QLabel*>(u"faint"_s)) {
            notes << label->text();
        }
        QVERIFY(notes.contains(u"Clarity, Sharpen"_s));
        QVERIFY(notes.contains(u"3 changed"_s));
        QVERIFY(!notes.contains(u"1 changed"_s)); // the curve is not a slider: no note
    }
};

QTEST_MAIN(TestPresetsPanel)
#include "tst_presets_panel.moc"
