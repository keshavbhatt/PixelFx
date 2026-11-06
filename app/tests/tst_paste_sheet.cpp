#include "ui/paste_sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTest>

#include <pfx/settings.h>

using namespace Qt::StringLiterals;
using pixelfx::ui::PasteSheet;

namespace {

QPushButton* pasteButton(const PasteSheet& sheet)
{
    for (QPushButton* b : sheet.findChildren<QPushButton*>()) {
        if (b->text().startsWith(u"Paste to"_s)) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TestPasteSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() { pixelfx::ui::applyStyle(*qApp); }

    void anEditedCopyCanBePasted()
    {
        pfx::Settings copied;
        copied.set("light.exposure", 0.5);
        const PasteSheet sheet(copied, 3);
        QVERIFY(!sheet.units().isEmpty());
        QVERIFY(pasteButton(sheet)->isEnabled());
    }

    void aCopyWithoutEditsSaysThereIsNothingToPaste()
    {
        // Copied from an untouched photo: no empty list and no Paste that does nothing.
        const PasteSheet sheet(pfx::Settings(), 3);
        QVERIFY(sheet.units().isEmpty());
        QVERIFY(!pasteButton(sheet)->isEnabled());
        bool said = false;
        for (const QLabel* label : sheet.findChildren<QLabel*>()) {
            said = said || label->text().contains(u"nothing to paste"_s);
        }
        QVERIFY(said);
    }
};

QTEST_MAIN(TestPasteSheet)
#include "tst_paste_sheet.moc"
