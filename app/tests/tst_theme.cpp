#include "ui/icons.h"
#include "ui/style.h"

#include <QApplication>
#include <QLabel>
#include <QTest>

using namespace Qt::StringLiterals;

// The theme switches while PixelFx runs: tokens, palette, icons and glyphs follow.
class TestTheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        pixelfx::ui::useLightTheme(false);
        pixelfx::ui::applyStyle(*qApp);
    }

    void iconsAndGlyphsFollowTheSwitch()
    {
        using pixelfx::ui::tokens;
        const QColor darkText = tokens().text;
        const QIcon icon = pixelfx::ui::icons::themed(u"check"_s, tokens().text);
        QLabel label;
        pixelfx::ui::icons::setGlyph(&label, u"alert"_s, tokens().danger, 16);
        const QColor darkDanger = tokens().danger;

        // The colour of the most opaque pixel of an icon.
        const auto ink = [](const QImage& image) {
            QColor best(Qt::transparent);
            for (int y = 0; y < image.height(); ++y) {
                for (int x = 0; x < image.width(); ++x) {
                    const QColor c = image.pixelColor(x, y);
                    if (c.alpha() > best.alpha()) {
                        best = c;
                    }
                }
            }
            return best;
        };
        QCOMPARE(ink(icon.pixmap(24).toImage()).rgb(), darkText.rgb());

        pixelfx::ui::switchTheme(*qApp, true);
        QVERIFY(tokens().text != darkText);
        QCOMPARE(ink(icon.pixmap(24).toImage()).rgb(), tokens().text.rgb());
        QCOMPARE(ink(label.pixmap().toImage()).rgb(), tokens().danger.rgb());
        QVERIFY(tokens().danger != darkDanger);
        QCOMPARE(QApplication::palette().color(QPalette::Window), tokens().bg);

        pixelfx::ui::switchTheme(*qApp, false);
        QCOMPARE(ink(icon.pixmap(24).toImage()).rgb(), darkText.rgb());
    }
};

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
