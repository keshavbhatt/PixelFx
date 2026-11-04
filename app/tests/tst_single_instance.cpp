#include "app/single_instance.h"

#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTest>

#include <future>

using namespace Qt::StringLiterals;
using pixelfx::app::SingleInstance;

class TestSingleInstance : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        // Unique key per test so parallel ctest runs never collide.
        m_key = u"pixelfx-test-%1"_s.arg(QRandomGenerator::global()->generate());
    }

    void firstIsPrimarySecondIsNot()
    {
        SingleInstance primary(m_key);
        QVERIFY(primary.isPrimary());
        SingleInstance secondary(m_key);
        QVERIFY(!secondary.isPrimary());
    }

    void commandsReachThePrimary()
    {
        SingleInstance primary(m_key);
        QSignalSpy spy(&primary, &SingleInstance::commandReceived);
        SingleInstance secondary(m_key);

        // The secondary sends from another thread, as a second process would: on Windows a local
        // socket is a named pipe, and a blocking write from the primary's own thread waits for an
        // accept that thread cannot run.
        auto sent = std::async(std::launch::async, [&secondary] {
            return secondary.sendToPrimary({{u"cmd"_s, u"raise"_s}}) &&
                   secondary.sendToPrimary({{u"cmd"_s, u"new-chat"_s}, {u"url"_s, u"wa.me/1"_s}});
        });
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 5000);
        QVERIFY(sent.get());
        const QJsonObject second = spy.at(1).first().toJsonObject();
        QCOMPARE(second.value(u"cmd"_s).toString(), u"new-chat"_s);
        QCOMPARE(second.value(u"url"_s).toString(), u"wa.me/1"_s);
    }

    void primaryRefusesToSend()
    {
        SingleInstance primary(m_key);
        QVERIFY(!primary.sendToPrimary({{u"cmd"_s, u"raise"_s}}));
    }

    void keyIsReleasedOnDestruction()
    {
        {
            SingleInstance primary(m_key);
            QVERIFY(primary.isPrimary());
        }
        SingleInstance next(m_key);
        QVERIFY(next.isPrimary());
    }

    void keyDependsOnProfile()
    {
        QVERIFY(pixelfx::app::instanceKeyFor(QString()).contains(u"default"_s));
        QVERIFY(pixelfx::app::instanceKeyFor(u"work"_s).contains(u"work"_s));
        QVERIFY(pixelfx::app::instanceKeyFor(u"work"_s) != pixelfx::app::instanceKeyFor(u"home"_s));
    }

private:
    QString m_key;
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_single_instance.moc"
