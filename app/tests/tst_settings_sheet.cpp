#include "core/export_options.h"
#include "core/plurals.h"
#include "core/presets.h"
#include "core/settings/settings.h"
#include "ui/canvas.h"
#include "ui/settings_sheet.h"
#include "ui/shortcuts_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pixelfx::core::Settings;

class TestSettingsSheet : public QObject
{
    Q_OBJECT

    static QCheckBox* box(const QWidget& sheet, const QString& text)
    {
        for (QCheckBox* c : sheet.findChildren<QCheckBox*>()) {
            if (c->text() == text) {
                return c;
            }
        }
        qFatal("no check box %s", qPrintable(text));
    }

private Q_SLOTS:
    void initTestCase()
    {
        pixelfx::ui::applyStyle(*qApp);
        QVERIFY(pixelfx::core::installPluralForms(*qApp)); // "1 preset", as the app says it
    }

    void everyRowWritesStraightThrough()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::SettingsSheet sheet(settings, dir.filePath(u"looks"_s), dir.filePath(u"presets"_s));
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"Light"_s || b->text() == u"Black"_s) {
                b->click();
            }
        }
        QCOMPARE(settings.theme(), Settings::Theme::Light);
        QCOMPARE(settings.canvasBackground(), Settings::CanvasBackground::Black);
        for (const QString& text :
             {u"Keep each photo's settings in the app's library"_s,
              u"Show clipped highlights and shadows (J)"_s, u"One group open at a time"_s,
              u"Notify me when PixelFx is in the background"_s, u"Show this after each update"_s}) {
            box(sheet, text)->click();
        }
        QVERIFY(!settings.rememberEdits());
        QVERIFY(settings.clippingWarnings());
        QVERIFY(settings.oneGroupAtATime());
        QVERIFY(!settings.notifyBatchDone());
        QVERIFY(!settings.showWhatsNew());
        // The adjust panel's own switch shows here as it changes.
        settings.setOneGroupAtATime(false);
        QVERIFY(!box(sheet, u"One group open at a time"_s)->isChecked());
        QComboBox* onOpen = nullptr;
        for (QComboBox* list : sheet.findChildren<QComboBox*>()) {
            if (list->itemText(0) == u"Show the photo as it was left"_s) {
                onOpen = list;
            }
        }
        QVERIFY(onOpen != nullptr);
        QVERIFY(!onOpen->isEnabled()); // nothing remembered: nothing to open as left
        // Its name greys out with it, and comes back with it.
        const QLabel* name = nullptr;
        for (const QLabel* l : sheet.findChildren<QLabel*>(u"fieldLabel"_s)) {
            if (l->text() == u"On open"_s) {
                name = l;
            }
        }
        QVERIFY(name != nullptr);
        QVERIFY(!name->isEnabled());
        onOpen->setEnabled(true);
        QVERIFY(name->isEnabled());
    }

    void exportPageWritesTheExportChoices()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::SettingsSheet sheet(settings, dir.filePath(u"looks"_s), dir.filePath(u"presets"_s));
        sheet.showPage(pixelfx::ui::SettingsSheet::ExportPage);
        auto* name = sheet.findChild<QLineEdit*>();
        name->setText(u"{name}-{date}"_s);
        Q_EMIT name->editingFinished();
        for (QPushButton* b : sheet.findChildren<QPushButton*>(u"segment"_s)) {
            if (b->text() == u"Skip"_s) {
                b->click();
            }
        }
        box(sheet, u"Keep camera details and copyright"_s)->click();
        QVERIFY(!box(sheet, u"Keep location"_s)->isEnabled()); // nothing kept, so no location either
        const auto o = settings.exportOptions();
        QCOMPARE(o.nameTemplate, u"{name}-{date}"_s);
        QCOMPARE(o.ifExists, pixelfx::core::ExportOptions::IfExists::Skip);
        QVERIFY(!o.keepMetadata);
        name->clear();
        Q_EMIT name->editingFinished(); // an empty name is not kept
        QCOMPARE(name->text(), u"{name}-{date}"_s);
    }

    void looksPageCountsAndClearsRecent()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        const pixelfx::core::PresetLibrary presets(dir.filePath(u"presets"_s));
        pfx::Settings values;
        values.set("light.exposure", 0.5);
        QVERIFY(!presets.save(u"Mine"_s, pixelfx::core::PresetLibrary::myPresetsGroup(), values).isEmpty());
        settings.notePresetUsed(u"builtin:essentials/punchy"_s);
        pixelfx::ui::SettingsSheet sheet(settings, dir.filePath(u"looks"_s), dir.filePath(u"presets"_s));
        QStringList texts;
        for (const QLabel* l : sheet.findChildren<QLabel*>()) {
            texts << l->text();
        }
        QVERIFY(texts.contains(u"0 looks"_s));
        QCOMPARE(texts.count(u"1 preset"_s), 2); // My presets and Recent presets
        QPushButton* clear = nullptr;
        for (QPushButton* b : sheet.findChildren<QPushButton*>()) {
            if (b->text() == u"Clear recent"_s) {
                clear = b;
            }
        }
        QVERIFY(clear != nullptr && clear->isEnabled());
        clear->click();
        QVERIFY(settings.recentPresets().isEmpty());
        QVERIFY(!clear->isEnabled());
    }

    void restoreDefaultsKeepsTheLists()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        settings.setTheme(Settings::Theme::Light);
        settings.setNotifyBatchDone(false);
        settings.setBatchAtOnce(8);
        auto o = settings.exportOptions();
        o.nameTemplate = u"{name}-web"_s;
        o.quality = 70;
        settings.setExportOptions(o);
        settings.setFavoriteLooks({u"builtin:film"_s});
        QSignalSpy theme(&settings, &Settings::themeChanged);
        settings.restoreDefaults();
        QCOMPARE(settings.theme(), Settings::Theme::Dark);
        QCOMPARE(theme.count(), 1); // the window takes the theme back at once
        QVERIFY(settings.notifyBatchDone());
        QCOMPARE(settings.batchAtOnce(), 0);
        QCOMPARE(settings.exportOptions().nameTemplate, u"{name}-edit"_s);
        QCOMPARE(settings.exportOptions().quality, 70); // a choice of the Export sheet, not shown here
        QCOMPARE(settings.favoriteLooks(), QStringList{u"builtin:film"_s});
    }

    void shortcutsSheetFilters()
    {
        pixelfx::ui::ShortcutsSheet sheet;
        const int all = sheet.filter(QString());
        QVERIFY(all > 20);
        QCOMPARE(sheet.filter(u"zoom"_s), 1);
        QCOMPARE(sheet.filter(u"nothing like this"_s), 0);
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
