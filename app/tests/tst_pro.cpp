#include "core/licensing/pro_features.h"
#include "core/looks.h"
#include "core/presets.h"
#include "services/licensing/license_service.h"
#include "ui/export_sheet.h"
#include "ui/pro_gate.h"
#include "ui/pro_sheet.h"
#include "ui/sheet.h"
#include "ui/style.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <pfx/settings.h>

using namespace Qt::StringLiterals;
using pixelfx::services::LicenseService;
using pixelfx::ui::ProGate;
using pixelfx::ui::ProPrompt;
namespace licensing = pixelfx::core::licensing;

namespace {

// A licence in a chosen state, without the server.
class FakeLicense : public LicenseService
{
public:
    explicit FakeLicense(Tier tier)
        : m_tier(tier)
    {}
    [[nodiscard]] Tier tier() const override { return m_tier; }
    void become(Tier tier)
    {
        m_tier = tier;
        Q_EMIT statusChanged();
    }

private:
    Tier m_tier;
};

pfx::Settings fromLightroom()
{
    pfx::Settings s;
    s.set("light.exposure", 0.4);
    s.set(licensing::kLightroomPresetKey, std::string("Clean Travel"));
    return s;
}

// Answers the next prompt the gate shows with `answer`, once it is up.
void answerWith(ProGate& gate, const std::function<void(ProPrompt*)>& answer)
{
    QObject::connect(
        &gate, &ProGate::prompting, &gate,
        [answer](ProPrompt* prompt) { QTimer::singleShot(0, prompt, [prompt, answer] { answer(prompt); }); },
        Qt::SingleShotConnection);
}

QPushButton* buttonCalled(const QWidget& parent, const QString& text)
{
    for (QPushButton* b : parent.findChildren<QPushButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TestPro : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
    }

    void aPhotoSaysWhatItUsesOfPro()
    {
        QVERIFY(licensing::photoUses(pfx::Settings(), {}, {}).isEmpty());
        const auto uses = licensing::photoUses(fromLightroom(), u"/photos/DSC_0412.NEF"_s, {});
        QCOMPARE(uses.size(), 2);
        QCOMPARE(uses.at(0), (licensing::ProUse{licensing::ProItem::Raw, u"DSC_0412.NEF"_s}));
        QCOMPARE(uses.at(1), (licensing::ProUse{licensing::ProItem::LightroomPreset, u"Clean Travel"_s}));
        // A batch names the first RAW among its photos, and each item once.
        const auto batch = licensing::batchUses({fromLightroom(), fromLightroom()},
                                                {u"a.jpg"_s, u"b.CR2"_s, u"c.nef"_s}, {}, {});
        QCOMPARE(batch.size(), 2);
        QCOMPARE(batch.at(0).name, u"b.CR2"_s);
    }

    void theLightroomMarkFollowsTheSettings()
    {
        // Applied, saved into a preset of the user's, and kept at Amount 0 only when the photo had it.
        const pfx::Settings applied = pixelfx::core::applyPreset(pfx::Settings(), fromLightroom());
        QCOMPARE(licensing::lightroomPreset(applied), u"Clean Travel"_s);
        const pfx::Settings mine = pixelfx::core::presetFrom(applied, {u"light"_s});
        QCOMPARE(licensing::lightroomPreset(mine), u"Clean Travel"_s);
        QVERIFY(licensing::lightroomPreset(pixelfx::core::blendPreset(pfx::Settings(), fromLightroom(), 0))
                    .isEmpty());
        QVERIFY(licensing::lightroomPreset(licensing::withoutLightroomPreset(applied)).isEmpty());
    }

    void aLookMadeOfALutIsPro()
    {
        QTemporaryDir dir;
        const QString cube = dir.filePath(u"warm.cube"_s);
        QFile file(cube);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("LUT_3D_SIZE 2\n0 0 0\n1 0 0\n0 1 0\n1 1 0\n0 0 1\n1 0 1\n0 1 1\n1 1 1\n");
        file.close();
        pixelfx::core::setLookFolder(dir.filePath(u"looks"_s));
        const QString id = pixelfx::core::LookLibrary(dir.filePath(u"looks"_s)).importLut(cube);
        QVERIFY(!id.isEmpty());
        pixelfx::core::lookFolderChanged();
        QVERIFY(licensing::lookUsesLut(id));
        QVERIFY(!licensing::lookUsesLut(pixelfx::core::builtInLooks().front().id));
        const auto uses = licensing::photoUses(pixelfx::core::withLook({}, id, 100), {}, {});
        QCOMPARE(uses.size(), 1);
        QCOMPARE(uses.front().item, licensing::ProItem::Lut);
    }

    void theCheckoutHearsWhichPromptSentTheBuyer()
    {
        const LicenseService license;
        const QUrlQuery query(license.checkoutUrl(licensing::Source::Batch));
        QCOMPARE(query.queryItemValue(u"slug"_s), u"pixelfx"_s);
        QCOMPARE(query.queryItemValue(u"source"_s), u"batch"_s);
        QCOMPARE(QUrlQuery(license.passUrl()).queryItemValue(u"app"_s), u"pixelfx"_s);
        // The Ktechpit Pass is bought with this Account ID, so it unlocks PixelFx at once.
        const QUrlQuery pass(license.passCheckoutUrl());
        QCOMPARE(pass.queryItemValue(u"slug"_s), u"ktechpit-pass"_s);
        QCOMPARE(pass.queryItemValue(u"accountId"_s), license.accountId());
        QCOMPARE(pass.queryItemValue(u"source"_s), u"in_app_pixelfx"_s);
        QCOMPARE(licensing::sourceName(licensing::Source::TrialEnd), u"trial_end"_s);
    }

    void withProOrTheTrialEveryPhotoGoes()
    {
        ProGate none(nullptr, nullptr);
        QCOMPARE(none.admitPhotos(ProGate::Count::Batch, 47), 47);
        FakeLicense trial(LicenseService::Tier::Trial);
        ProGate gate(&trial, nullptr);
        QCOMPARE(gate.admitPhotos(ProGate::Count::Batch, 47), 47);
        QVERIFY(gate.admitUses(licensing::Source::Export, u"Export"_s,
                               licensing::photoUses(fromLightroom(), {}, {})));
    }

    void freeTakesTheFirstTenOrNone()
    {
        FakeLicense free(LicenseService::Tier::Free);
        ProGate gate(&free, nullptr);
        QCOMPARE(gate.admitPhotos(ProGate::Count::Batch, 10), 10); // no prompt up to the free number
        QString title;
        answerWith(gate, [&title](ProPrompt* p) {
            title = p->title();
            p->freeWayButton()->click();
        });
        QCOMPARE(gate.admitPhotos(ProGate::Count::Batch, 47), licensing::kFreePhotos);
        QCOMPARE(title, u"Batch 47 photos with Pro"_s);
        answerWith(gate, [](ProPrompt* p) { p->reject(); });
        QCOMPARE(gate.admitPhotos(ProGate::Count::Paste, 23), 0);
        // Bought already? Check again: once the server says Pro, the whole job goes.
        answerWith(gate, [&free](ProPrompt*) { free.become(LicenseService::Tier::Pro); });
        QCOMPARE(gate.admitPhotos(ProGate::Count::Export, 30), 30);
    }

    void freeIsAskedBeforeSavingProItems()
    {
        FakeLicense free(LicenseService::Tier::Free);
        ProGate gate(&free, nullptr);
        QVERIFY(gate.admitUses(licensing::Source::Batch, u"Batch"_s, {}));
        QString text;
        answerWith(gate, [&text](ProPrompt* p) {
            text = p->text();
            p->reject();
        });
        QVERIFY(!gate.admitUses(licensing::Source::Batch, u"Batch these photos with Pro"_s,
                                licensing::photoUses(fromLightroom(), {}, {})));
        QVERIFY(text.contains(u"Lightroom preset · Clean Travel"_s));
    }

    void theProSheetFollowsTheState()
    {
        FakeLicense license(LicenseService::Tier::Free);
        pixelfx::ui::ProSheet sheet(license);
        QCOMPARE(sheet.statusTitle(), u"Free"_s);
        QVERIFY(sheet.showsPlans());
        license.become(LicenseService::Tier::Pro);
        QCOMPARE(sheet.statusTitle(), u"Pro, Forever"_s);
        QVERIFY(!sheet.showsPlans());
        license.become(LicenseService::Tier::Trial);
        QVERIFY(sheet.statusTitle().startsWith(u"Pro trial: "_s));
        QVERIFY(sheet.showsPlans());
    }

    void aClickOnALinkInASheetReachesIt()
    {
        // A sheet moves when its background is dragged; a click on a link it passes on must not.
        pixelfx::ui::Sheet sheet;
        auto* label = new QLabel(u"<a href=\"#check\">Check again</a>"_s, &sheet);
        auto* layout = new QVBoxLayout(&sheet);
        layout->setContentsMargins(40, 40, 40, 40);
        layout->addWidget(label);
        QSignalSpy activated(label, &QLabel::linkActivated);
        sheet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&sheet));
        const QPoint onLink(label->fontMetrics().horizontalAdvance(u"Check"_s) / 2, label->height() / 2);
        QTest::mouseClick(label, Qt::LeftButton, {}, onLink);
        QCOMPARE(activated.size(), 1);
        QCOMPARE(activated.front().front().toString(), u"#check"_s);
    }

    void exportOnFreeListsTheProItemsAndOffersAPlan()
    {
        FakeLicense free(LicenseService::Tier::Free);
        ProGate gate(&free, nullptr);
        pfx::Settings photo = fromLightroom();
        pixelfx::ui::ExportSheet sheet(u"/photos/beach.jpg"_s, QSize(600, 400),
                                       pixelfx::core::ExportOptions());
        sheet.setPro(
            &gate, [&photo](const QString& watermark) { return licensing::photoUses(photo, {}, watermark); });
        QCOMPARE(sheet.proUses().size(), 1);
        QVERIFY(!buttonCalled(sheet, u"Choose a plan"_s)->isHidden());
        QVERIFY(buttonCalled(sheet, u"Export"_s)->isHidden());
        QVERIFY(buttonCalled(sheet, u"Copy to clipboard"_s)->isHidden());

        // Remove: the window takes the preset off the photo, and Export comes back.
        QSignalSpy removed(&sheet, &pixelfx::ui::ExportSheet::removeRequested);
        QLabel* remove = nullptr;
        for (QLabel* label : sheet.findChildren<QLabel*>()) {
            if (label->text().contains(u"#remove"_s)) {
                remove = label;
            }
        }
        QVERIFY(remove != nullptr);
        Q_EMIT remove->linkActivated(u"#remove"_s);
        QCOMPARE(removed.size(), 1);
        QCOMPARE(removed.front().front().value<licensing::ProItem>(), licensing::ProItem::LightroomPreset);
        photo = licensing::withoutLightroomPreset(photo);
        sheet.refreshPro();
        QVERIFY(sheet.proUses().isEmpty());
        QVERIFY(!buttonCalled(sheet, u"Export"_s)->isHidden());
        QVERIFY(buttonCalled(sheet, u"Choose a plan"_s)->isHidden());

        // With Pro there is no box.
        free.become(LicenseService::Tier::Pro);
        photo = fromLightroom();
        sheet.refreshPro();
        QVERIFY(sheet.proUses().isEmpty());
    }
};

QTEST_MAIN(TestPro)
#include "tst_pro.moc"
