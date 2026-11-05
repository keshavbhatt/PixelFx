#include "ui/paste_sheet.h"
#include "ui/photo_tile.h"
#include "ui/preset_save_sheet.h"
#include "ui/presets_panel.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
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
        panel.showTab(1);
        QCOMPARE(panel.tiles().size(), 3);
        panel.findChild<QLineEdit*>(u"presetSearch"_s)->setText(u"warm"_s);
        QCOMPARE(panel.tiles().size(), 1);
        QSignalSpy apply(&panel, &PresetsPanel::applyRequested);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QTest::mouseClick(panel.tiles().front(), Qt::LeftButton, {}, QPoint(40, 10));
        QCOMPARE(apply.count(), 1);
        QCOMPARE(apply.first().first().value<PresetEntry>().name, u"Warm evening"_s);
    }

    static std::vector<PresetEntry> catalog()
    {
        std::vector<PresetEntry> entries;
        PresetEntry mine;
        mine.id = u"/x/Moody"_s;
        mine.name = u"Moody"_s;
        mine.group = u"My presets"_s;
        mine.path = mine.id;
        entries.push_back(mine);
        for (int i = 0; i < 9; ++i) {
            PresetEntry e;
            e.id = u"builtin:film-color/%1"_s.arg(i);
            e.name = u"Film %1"_s.arg(i);
            e.group = u"Film, color"_s;
            e.category = u"film-color"_s;
            e.settings.set("color.wb.temperature", 10.0 + i);
            entries.push_back(e);
        }
        return entries;
    }

    void categoriesShowSixTilesAndOpenInFull()
    {
        PresetsPanel panel;
        panel.setEntries(catalog());
        QCOMPARE(panel.tiles().size(), 7u); // one of mine, six of the nine
        QPushButton* all = nullptr;
        for (auto* b : panel.findChildren<QPushButton*>(u"link"_s)) {
            if (b->text() == u"All 9"_s) {
                all = b;
            }
        }
        QVERIFY(all != nullptr);
        all->click();
        QCOMPARE(panel.shownCategory(), u"Film, color"_s);
        QCOMPARE(panel.tiles().size(), 9u);
        panel.showCategory({});
        QCOMPARE(panel.tiles().size(), 7u);
        // Closing a category hides its tiles and is reported to be remembered.
        QSignalSpy closed(&panel, &PresetsPanel::closedCategoriesChanged);
        for (auto* b : panel.findChildren<QPushButton*>(u"categoryHeading"_s)) {
            if (b->accessibleName() == u"Film, color"_s) {
                b->click();
            }
        }
        QCOMPARE(closed.count(), 1);
        QCOMPARE(panel.tiles().size(), 1u);
        QVERIFY(panel.shownIndexes() == std::vector<std::size_t>{0});
    }

    void favoritesMineAndRecentFilter()
    {
        PresetsPanel panel;
        panel.setEntries(catalog());
        QSignalSpy favorites(&panel, &PresetsPanel::favoritesChanged);
        pixelfx::ui::PhotoTile* film = panel.tiles().at(2);
        const QString id = film->id();
        QTest::keyClick(film, Qt::Key_F);
        QCOMPARE(favorites.count(), 1);
        QCOMPARE(panel.favorites(), QStringList{id});
        panel.showFilter(1);
        QCOMPARE(panel.tiles().size(), 1u);
        QCOMPARE(panel.tiles().front()->id(), id);
        QVERIFY(panel.tiles().front()->accessibleName().endsWith(u", in Favorites"_s));
        panel.showFilter(2);
        QCOMPARE(panel.tiles().size(), 1u);
        QCOMPARE(panel.tiles().front()->name(), u"Moody"_s);
        panel.setRecent({u"builtin:film-color/4"_s, u"builtin:film-color/1"_s});
        panel.showFilter(3);
        QCOMPARE(panel.tiles().size(), 2u);
        QCOMPARE(panel.tiles().front()->name(), u"Film 4"_s); // newest first
    }

    void theAmountShowsOnceAPresetIsApplied()
    {
        PresetsPanel panel;
        panel.setEntries(catalog());
        auto* amount = panel.amountRow();
        QVERIFY(amount->parentWidget()->isHidden());
        panel.setApplied(u"builtin:film-color/0"_s, u"Film 0"_s, 100.0);
        QVERIFY(!amount->parentWidget()->isHidden());
        QCOMPARE(amount->ticks(), 100);
        QSignalSpy released(&panel, &PresetsPanel::amountReleased);
        amount->slider()->setValue(80); // a keyboard step: complete at once
        QCOMPARE(released.count(), 1);
        QCOMPARE(released.first().first().toDouble(), 80.0);
        panel.setApplied({}, {}, 100.0);
        QVERIFY(amount->parentWidget()->isHidden());
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
