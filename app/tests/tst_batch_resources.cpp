#include "core/batch_resources.h"
#include "core/looks.h"
#include "core/watermark.h"
#include "services/batch_runner.h"
#include "ui/batch_page.h"
#include "ui/style.h"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <pfx/document.h>

using namespace Qt::StringLiterals;
using pixelfx::core::Watermark;
using pixelfx::core::WatermarkLibrary;

namespace {

pfx::Look contrastLook()
{
    pfx::Look look;
    look.defaultStrength = 70;
    pfx::LookStep step;
    step.op = "light";
    step.params.set("light.contrast", 25.0);
    look.steps.push_back(step);
    return look;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// What a batch file holds after writing and reading it back.
pfx::BatchBody throughFile(const pfx::BatchBody& body)
{
    pfx::Document doc;
    doc.kind = pfx::DocumentKind::Batch;
    doc.batch = body;
    auto again = pfx::parseDocument(pfx::serializeDocument(doc));
    return again ? again.value().batch : pfx::BatchBody{};
}

} // namespace

class TestBatchResources : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
    }

    void userLooksTravelInsideTheRecipeAndAreAddedOnce()
    {
        QTemporaryDir here;
        QTemporaryDir there;
        pixelfx::core::setLookFolder(here.path());
        const QString id = pixelfx::core::LookLibrary(here.path()).save(u"Dusk walk"_s, contrastLook());
        QVERIFY(!id.isEmpty());
        pfx::BatchBody body;
        body.stack.push_back({"Dusk walk", pixelfx::core::withLook({}, id, 60)});
        body.stack.push_back({"Golden", pixelfx::core::withLook({}, u"builtin:golden-hour"_s, 72)});
        pixelfx::core::embedBatchResources(body, {});
        QCOMPARE(body.looks.size(), std::size_t{1}); // built-in looks stay references
        QCOMPARE(QString::fromStdString(body.looks.front().id), id);
        QCOMPARE(body.looks.front().name, std::string("Dusk walk"));

        const pfx::BatchBody read = throughFile(body);
        auto adopted = pixelfx::core::adoptBatchResources(read, there.path(), there.filePath(u"marks"_s));
        QCOMPARE(adopted.addedLooks, QStringList{u"Dusk walk"_s});
        QVERIFY(adopted.problems.isEmpty());
        QVERIFY(adopted.watermarkPath.isEmpty());
        bool found = false;
        for (const auto& e : pixelfx::core::LookLibrary(there.path()).entries()) {
            if (e.isMine() && e.id == id) {
                found = true;
                QCOMPARE(e.look->steps, contrastLook().steps);
            }
        }
        QVERIFY(found);
        // Opening the same recipe again adds nothing.
        adopted = pixelfx::core::adoptBatchResources(read, there.path(), there.filePath(u"marks"_s));
        QVERIFY(adopted.addedLooks.isEmpty());
        pixelfx::core::setLookFolder({});
    }

    void aLogoWatermarkTravelsWithItsLogoAndIsNotDuplicated()
    {
        QTemporaryDir dir;
        QImage logo(40, 20, QImage::Format_ARGB32);
        logo.fill(Qt::red);
        QVERIFY(logo.save(dir.filePath(u"logo.png"_s)));
        Watermark w;
        w.name = u"Studio"_s;
        w.kind = Watermark::Kind::Logo;
        w.logo = dir.filePath(u"logo.png"_s);
        w.anchor = 2;
        const QString saved = WatermarkLibrary(dir.filePath(u"here"_s)).save(w);
        QVERIFY(!saved.isEmpty());

        pfx::BatchBody body;
        pixelfx::core::embedBatchResources(body, saved);
        QVERIFY(body.watermark.find("logoData") != std::string::npos);
        const pfx::BatchBody read = throughFile(body);
        const QString there = dir.filePath(u"there"_s);
        const auto adopted = pixelfx::core::adoptBatchResources(read, dir.filePath(u"looks"_s), there);
        QVERIFY(adopted.problems.isEmpty());
        QVERIFY(QFile::exists(adopted.watermarkPath));
        const auto entries = WatermarkLibrary(there).entries();
        QCOMPARE(entries.size(), std::size_t{1});
        QCOMPARE(entries.front().watermark.anchor, 2);
        QCOMPARE(readAll(entries.front().watermark.logo), readAll(dir.filePath(u"logo.png"_s)));
        // The same recipe again finds the watermark it added.
        const auto again = pixelfx::core::adoptBatchResources(read, dir.filePath(u"looks"_s), there);
        QCOMPARE(again.watermarkPath, adopted.watermarkPath);
        QCOMPARE(WatermarkLibrary(there).entries().size(), std::size_t{1});
        // And the folder it came from already has it.
        QCOMPARE(pixelfx::core::adoptBatchResources(read, {}, dir.filePath(u"here"_s)).watermarkPath, saved);
    }

    void aBrokenWatermarkIsAProblemNotACrash()
    {
        QTemporaryDir dir;
        pfx::BatchBody body;
        body.watermark = R"({"format":"pixelfx","kind":"watermark","version":1,"body":{"type":"logo"}})";
        const auto adopted = pixelfx::core::adoptBatchResources(body, dir.path(), dir.path());
        QVERIFY(adopted.watermarkPath.isEmpty());
        QCOMPARE(adopted.problems.size(), 1);
    }

    void sizePresetsRoundTripThroughTheRecipe()
    {
        pixelfx::services::BatchRecipe r;
        r.output.sizePreset = u"instagram-portrait"_s;
        r.output.fitInside = true;
        const pfx::BatchBody body = throughFile(pixelfx::services::bodyFromRecipe(r));
        QCOMPARE(body.sizePreset, std::string("instagram-portrait"));
        const auto back = pixelfx::services::recipeFromBody(body, {});
        QCOMPARE(back.output.sizePreset, u"instagram-portrait"_s);
        QVERIFY(back.output.fitInside);
        // An id this version does not know is not written.
        r.output.sizePreset = u"no-such-size"_s;
        QVERIFY(pixelfx::services::bodyFromRecipe(r).sizePreset.empty());
    }

    void batchPageSavesAndOpensTheSizeAndTheWatermark()
    {
        QTemporaryDir dir;
        Watermark w;
        w.name = u"Proof"_s;
        w.text = u"PROOF"_s;
        w.repeat = true;
        const QString marks = WatermarkLibrary::defaultFolder();
        const QString saved = WatermarkLibrary(marks).save(w);
        QVERIFY(!saved.isEmpty());
        pixelfx::core::ExportOptions options;
        options.sizePreset = u"web"_s;
        options.watermark = saved;
        pixelfx::ui::BatchPage page(options);
        QCOMPARE(page.recipe().output.sizePreset, u"web"_s);
        QCOMPARE(page.recipe().output.longEdge, 0);
        QCOMPARE(page.recipe().output.watermark, saved);
        const QString file = dir.filePath(u"proof.pfxbatch"_s);
        QVERIFY(page.saveRecipe(file));
        // A page with other choices takes the recipe's.
        pixelfx::ui::BatchPage other(pixelfx::core::ExportOptions{});
        QVERIFY(other.recipe().output.sizePreset.isEmpty());
        QVERIFY(other.loadRecipe(file).isEmpty());
        QCOMPARE(other.recipe().output.sizePreset, u"web"_s);
        QCOMPARE(other.recipe().output.watermark, saved); // the identical one already here
        // Choosing a long edge clears the size preset.
        auto* size = other.findChildren<QComboBox*>().at(0);
        for (QComboBox* c : other.findChildren<QComboBox*>()) {
            if (c->findData(2048) >= 0) {
                size = c;
            }
        }
        size->setCurrentIndex(size->findData(2048));
        QVERIFY(other.recipe().output.sizePreset.isEmpty());
        QCOMPARE(other.recipe().output.longEdge, 2048);
        QVERIFY(WatermarkLibrary(marks).remove(saved));
    }
};

QTEST_MAIN(TestBatchResources)
#include "tst_batch_resources.moc"
