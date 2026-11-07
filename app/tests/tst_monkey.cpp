// Every action, slider and tool on odd photos (one pixel, a thin strip, grey, transparent,
// sixteen bits, a broken file, an empty one): no crash, and no warning from Qt.
#include "core/settings/settings.h"
#include "services/document.h"
#include "ui/adjust_panel.h"
#include "ui/main_window.h"
#include "ui/slider_row.h"
#include "ui/style.h"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QSignalSpy>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <cstdio>
#include <pfx/io.h>

using namespace Qt::StringLiterals;

namespace {

QStringList g_warnings;

void collect(QtMsgType type, const QMessageLogContext& /*context*/, const QString& message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        // Qt's offscreen platform, not PixelFx: plugins without a feature, and its default
        // "Sans Serif" family, which macOS does not have.
        if (!message.contains(u"This plugin does not support"_s) &&
            !message.contains(u"Populating font family aliases"_s)) {
            g_warnings << message;
        }
    }
}

// Closes whatever opens: a sheet, a message, a file dialog, a menu.
void closeWhatOpens(QObject* owner)
{
    auto* timer = new QTimer(owner);
    QObject::connect(timer, &QTimer::timeout, owner, [] {
        if (QWidget* modal = QApplication::activeModalWidget()) {
            if (auto* dialog = qobject_cast<QDialog*>(modal)) {
                dialog->reject();
            } else {
                modal->close();
            }
        }
        if (QWidget* popup = QApplication::activePopupWidget()) {
            popup->close();
        }
    });
    timer->start(30);
}

} // namespace

class TestMonkey : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        pixelfx::ui::applyStyle(*qApp);
        qInstallMessageHandler(collect);
    }

    void everythingOnOddPhotos()
    {
        QTemporaryDir dir;
        // Odd photos, written here.
        QStringList files;
        const auto write = [&dir, &files](const QString& name, int w, int h) {
            pfx::Image img(w, h);
            img.fill(0.4F, 0.3F, 0.2F);
            const QString path = dir.filePath(name);
            QVERIFY(pfx::save(img, path.toStdString()).ok());
            files << path;
        };
        write(u"one pixel.png"_s, 1, 1);
        write(u"strip.jpg"_s, 4000, 3);
        write(u"tall.png"_s, 2, 3000);
        write(u"fotó ü 写真 #1.jpg"_s, 64, 48);
        QImage grey(300, 200, QImage::Format_Grayscale8);
        grey.fill(128);
        QVERIFY(grey.save(dir.filePath(u"grey.png"_s)));
        files << dir.filePath(u"grey.png"_s);
        QImage clear(120, 80, QImage::Format_ARGB32);
        clear.fill(Qt::transparent);
        QVERIFY(clear.save(dir.filePath(u"clear.png"_s)));
        files << dir.filePath(u"clear.png"_s);
        QImage deep(200, 100, QImage::Format_RGBA64);
        deep.fill(QColor(200, 100, 50));
        QVERIFY(deep.save(dir.filePath(u"deep.png"_s)));
        files << dir.filePath(u"deep.png"_s);
        QFile broken(dir.filePath(u"broken.jpg"_s));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write(QByteArray("\xff\xd8\xff\xe0 not really a jpeg", 30));
        broken.close();
        files << broken.fileName();
        QFile empty(dir.filePath(u"empty.png"_s));
        QVERIFY(empty.open(QIODevice::WriteOnly));
        empty.close();
        files << empty.fileName();

        pixelfx::core::Settings settings(dir.filePath(u"s.ini"_s));
        pixelfx::ui::MainWindow window(settings);
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        closeWhatOpens(&window);
        QStringList skipped = {u"quit"_s, u"onlineGuide"_s, u"reportBug"_s, u"showPresetsFolder"_s};
#ifdef Q_OS_WIN
        // Qt's own file dialog (the offscreen platform has no native one) crashes on Windows in
        // its file-information thread, in QSystemLocale, when closed the moment it opens; users
        // get the native dialog. Every other action still runs.
        skipped << u"open"_s << u"openFolder"_s << u"openBatchRecipe"_s << u"importPresets"_s;
#endif
        for (const QString& file : files) {
            g_warnings << u"--- "_s + QFileInfo(file).fileName();
            QSignalSpy ready(&window.document(), &pixelfx::services::Document::previewReady);
            window.openFiles({file});
            ready.wait(4000);
            QTest::qWait(200);
            {
                for (QAction* a : window.findChildren<QAction*>()) {
                    if (a->objectName().isEmpty() || a->objectName().startsWith(u"_q_"_s) ||
                        skipped.contains(a->objectName())) {
                        continue;
                    }
                    g_warnings << u"> "_s + a->objectName();
                    a->trigger();
                    QTest::qWait(40);
                    if (window.findChildren<QAction*>().isEmpty()) {
                        break;
                    }
                }
                // Back to editing for the sliders.
                window.namedAction(u"edit"_s)->trigger();
                QTest::qWait(50);
                for (auto* row : window.adjustPanel()->findChildren<pixelfx::ui::SliderRow*>()) {
                    if (!row->isVisible() || !row->isEnabled()) {
                        continue;
                    }
                    QSlider* slider = row->slider();
                    slider->setValue(slider->maximum());
                    slider->setValue(slider->minimum());
                }
                QTest::qWait(300);
                for (int i = 0; i < 80; ++i) {
                    window.namedAction(u"undo"_s)->trigger();
                }
                for (int i = 0; i < 80; ++i) {
                    window.namedAction(u"redo"_s)->trigger();
                }
                QTest::qWait(300);
            }
            window.namedAction(u"closePhoto"_s)->trigger();
            QTest::qWait(100);
        }
        // Only the warnings, with the step before each.
        QStringList report;
        QString step;
        QString photo;
        for (const QString& w : std::as_const(g_warnings)) {
            if (w.startsWith(u"--- "_s)) {
                photo = w.mid(4);
                step = w;
            } else if (w.startsWith(u"> "_s)) {
                step = photo + u" "_s + w;
            } else {
                report << step + u": "_s + w;
            }
        }
        report.removeDuplicates();
        // The broken and the empty file are logged as not opened; anything else is a fault.
        report.removeIf([](const QString& line) { return line.contains(u": could not open "_s); });
        for (const QString& line : std::as_const(report)) {
            fprintf(stderr, "WARN %s\n", qPrintable(line));
        }
        QVERIFY2(report.isEmpty(), qPrintable(report.join(u'\n')));
    }
};

QTEST_MAIN(TestMonkey)
#include "tst_monkey.moc"
