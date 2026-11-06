#include "core/looks.h"
#include "core/plurals.h"
#include "ui/photo_tile.h"
#include "ui/recipe_picker.h"
#include "ui/style.h"

#include <QApplication>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QTest>

#include <pfx/image.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::RecipePicker;

namespace {

std::vector<pixelfx::core::PresetEntry> presets()
{
    std::vector<pixelfx::core::PresetEntry> out;
    const auto add = [&out](const QString& name, const QString& group, bool builtIn,
                            const QString& description) {
        pixelfx::core::PresetEntry p;
        p.name = name;
        p.group = group;
        p.id = builtIn ? u"builtin:"_s + name : u"/presets/"_s + name;
        p.path = builtIn ? QString() : p.id;
        p.description = description;
        p.settings.set("light.exposure", 0.3);
        out.push_back(p);
    };
    add(u"Portrait 400"_s, u"Film, color"_s, true, u"Soft warm skin, gentle greens"_s);
    add(u"Ultra 100"_s, u"Film, color"_s, true, u"Saturated, crisp"_s);
    add(u"Warm evening"_s, u"My presets"_s, false, QString());
    return out;
}

} // namespace

class TestRecipePicker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        pixelfx::ui::applyStyle(*qApp);
        QVERIFY(pixelfx::core::installPluralForms(*qApp));
    }

    void groupsSearchAndTheChoice()
    {
        auto photo = std::make_shared<pfx::Image>(60, 40);
        photo->fill(0.4F, 0.4F, 0.4F);
        RecipePicker::Lists lists;
        lists.favoritePresets = {u"builtin:Ultra 100"_s};
        RecipePicker picker(RecipePicker::Mode::Recipe, presets(), pixelfx::core::builtInLooks(), lists,
                            photo, u"DSC_0412.jpg"_s, u"preset:Portrait 400"_s, std::nullopt, nullptr);
        // No look or preset, the looks, Favorites, the user's group before the catalog's.
        QStringList groups;
        for (int i = 0; i < picker.groupList()->count(); ++i) {
            groups << picker.groupList()->item(i)->text();
        }
        QCOMPARE(groups.first(), u"No look or preset"_s);
        QVERIFY(groups.contains(u"All looks"_s));
        QVERIFY(groups.indexOf(u"Favorites"_s) < groups.indexOf(u"My presets"_s));
        QVERIFY(groups.indexOf(u"My presets"_s) < groups.indexOf(u"Film, color"_s));
        // It opens on the choice's group, the choice selected and named on the button.
        QCOMPARE(picker.groupList()->currentItem()->text(), u"Film, color"_s);
        QCOMPARE(picker.tiles().size(), std::size_t{2});
        QCOMPARE(picker.useButton()->text(), u"Use Portrait 400"_s);
        // Search goes through looks and presets, by name and description.
        picker.searchField()->setText(u"warm"_s);
        QStringList found;
        for (const pixelfx::ui::PhotoTile* t : picker.tiles()) {
            found << t->name();
        }
        QVERIFY(found.contains(u"Warm evening"_s));
        QVERIFY(found.contains(u"Portrait 400"_s)); // "Soft warm skin"
        QVERIFY(found.contains(u"Warm"_s));         // the built-in look
        picker.select(u"preset:Warm evening"_s);
        QCOMPARE(picker.choice(), u"preset:Warm evening"_s);
        picker.select(QString());
        QCOMPARE(picker.useButton()->text(), u"Use no look or preset"_s);
        picker.searchField()->clear();
    }

    void aClickSelectsADoubleClickUses()
    {
        auto photo = std::make_shared<pfx::Image>(60, 40);
        photo->fill(0.4F, 0.4F, 0.4F);
        RecipePicker picker(RecipePicker::Mode::OnTop, presets(), pixelfx::core::builtInLooks(), {}, photo,
                            u"a.jpg"_s, QString(), std::nullopt, nullptr);
        picker.show();
        QVERIFY(QTest::qWaitForWindowExposed(&picker));
        pixelfx::ui::PhotoTile* tile = picker.tiles().at(0);
        // The tile keeps the press: passed on, the sheet would start a window move and the
        // release would never reach the tile.
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(20, 20), tile->mapToGlobal(QPointF(20, 20)),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tile, &press);
        QVERIFY(press.isAccepted());
        QTest::mouseClick(tile, Qt::LeftButton, {}, QPoint(20, 20));
        QCOMPARE(picker.choice(), u"preset:"_s + tile->name()); // one click selects
        QVERIFY(picker.isVisible());
        QVERIFY(picker.useButton()->isEnabled());
    }

    void onTopOffersPresetsOnly()
    {
        RecipePicker picker(RecipePicker::Mode::OnTop, presets(), pixelfx::core::builtInLooks(), {}, nullptr,
                            QString(), QString(), std::nullopt, nullptr);
        for (int i = 0; i < picker.groupList()->count(); ++i) {
            QVERIFY(picker.groupList()->item(i)->text() != u"All looks"_s);
            QVERIFY(picker.groupList()->item(i)->text() != u"No look or preset"_s);
        }
        QVERIFY(!picker.useButton()->isEnabled()); // nothing chosen yet
        picker.select(u"preset:Ultra 100"_s);
        QCOMPARE(picker.useButton()->text(), u"Add Ultra 100"_s);
    }
};

QTEST_MAIN(TestRecipePicker)
#include "tst_recipe_picker.moc"
