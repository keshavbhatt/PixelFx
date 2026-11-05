#include "core/settings_clipboard.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace pixelfx::core;

class TestSettingsClipboard : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTripLeavesGeometryBehind()
    {
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        s.set("curves.rgb", pfx::Curve{{0, 0.1}, {1, 0.9}});
        s.set("geometry.angle", 3.0);
        const QString text = settingsToClipboard(s);
        QVERIFY(text.contains(u"\"preset\""_s));
        const auto back = settingsFromClipboard(text);
        QVERIFY(back.has_value());
        QCOMPARE(back->number("light.exposure", 0), 0.5);
        QVERIFY(back->contains("curves.rgb"));
        QVERIFY(!back->contains("geometry.angle"));
    }

    void otherTextIsNotSettings()
    {
        QVERIFY(!settingsFromClipboard(u"hello"_s).has_value());
        QVERIFY(!settingsFromClipboard(u"{\"a\": 1}"_s).has_value());
        QVERIFY(!settingsFromClipboard(QString()).has_value());
        QVERIFY(clipboardSource(u"hello"_s).isEmpty());
    }

    void theCopyNamesThePhotoItCameFrom()
    {
        pfx::Settings s;
        s.set("light.exposure", 0.5);
        QCOMPARE(clipboardSource(settingsToClipboard(s, u"sunset-lake.jpg"_s)), u"sunset-lake.jpg"_s);
        QVERIFY(clipboardSource(settingsToClipboard(s)).isEmpty());
    }

    void pasteKeepsThePhotosOwnCrop()
    {
        pfx::Settings current;
        current.set("light.exposure", 1.0);
        current.set("color.vibrance", 20.0);
        current.set("geometry.crop.w", 0.5);
        pfx::Settings copied;
        copied.set("light.contrast", 30.0);
        copied.set("geometry.angle", 9.0);
        const pfx::Settings out = pasted(current, copied);
        QCOMPARE(out.keys(), (std::vector<std::string>{"geometry.crop.w", "light.contrast"}));
    }
};

QTEST_GUILESS_MAIN(TestSettingsClipboard)
#include "tst_settings_clipboard.moc"
