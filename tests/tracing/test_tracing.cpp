#include <QObject>
#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QThread>

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

#include "SciQLopPlots/Tracing.hpp"

namespace tr = SciQLopPlots::tracing;

static QJsonDocument read_trace(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    auto data = f.readAll();
    f.close();
    return QJsonDocument::fromJson(data);
}

static QJsonArray events_of(const QJsonDocument& doc)
{
    return doc.object().value("traceEvents").toArray();
}

class TracingTest : public QObject
{
    Q_OBJECT

private slots:

    void init()
    {
        if (tr::is_enabled())
            tr::disable();
    }

    void single_zone_records_one_event()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        {
            tr::ScopedZone z("hello", "test");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        tr::disable();

        const auto doc = read_trace(path);
        QVERIFY(!doc.isNull());
        QVERIFY(doc.isObject());
        const auto events = events_of(doc);

        QJsonArray complete;
        for (const auto& v : events)
        {
            const auto e = v.toObject();
            if (e.value("ph").toString() == "X")
                complete.append(v);
        }
        QCOMPARE(complete.size(), 1);

        const auto e = complete.first().toObject();
        QCOMPARE(e.value("name").toString(), QString("hello"));
        QCOMPARE(e.value("cat").toString(), QString("test"));
        QVERIFY(e.value("ts").toDouble() >= 0.0);
        QVERIFY(e.value("dur").toDouble() >= 1000.0);  // >=1ms in us
        QVERIFY(e.contains("pid"));
        QVERIFY(e.contains("tid"));
    }

    void disabled_zones_produce_no_events()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        QVERIFY(!tr::is_enabled());
        {
            tr::ScopedZone z("ignored", "noop");
        }

        tr::enable(path.toStdString());
        {
            tr::ScopedZone z("real", "kept");
        }
        tr::disable();

        const auto doc = read_trace(path);
        QVERIFY(!doc.isNull());
        const auto events = events_of(doc);

        int x_count = 0;
        for (const auto& v : events)
        {
            const auto e = v.toObject();
            if (e.value("ph").toString() == "X")
            {
                ++x_count;
                QCOMPARE(e.value("name").toString(), QString("real"));
            }
        }
        QCOMPARE(x_count, 1);
    }

    void nested_zones_preserve_order_and_durations()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        {
            tr::ScopedZone outer("outer", "test");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            {
                tr::ScopedZone inner("inner", "test");
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
        tr::disable();

        const auto events = events_of(read_trace(path));
        QJsonObject outer_e, inner_e;
        for (const auto& v : events)
        {
            const auto e = v.toObject();
            if (e.value("ph").toString() != "X")
                continue;
            if (e.value("name").toString() == "outer") outer_e = e;
            if (e.value("name").toString() == "inner") inner_e = e;
        }
        QVERIFY(!outer_e.isEmpty());
        QVERIFY(!inner_e.isEmpty());

        const double outer_ts  = outer_e.value("ts").toDouble();
        const double outer_dur = outer_e.value("dur").toDouble();
        const double inner_ts  = inner_e.value("ts").toDouble();
        const double inner_dur = inner_e.value("dur").toDouble();

        QVERIFY(inner_ts >= outer_ts);
        QVERIFY(inner_ts + inner_dur <= outer_ts + outer_dur);
        QVERIFY(outer_dur >= inner_dur);
    }

    void zone_with_args_serializes_typed_values()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        {
            tr::ScopedZone z("with_args", "test");
            z.add_arg("n_points", int64_t{12345});
            z.add_arg("ratio", 0.5);
            z.add_arg("ok", true);
            z.add_arg("product", std::string("amda/mms_fgm"));
        }
        tr::disable();

        const auto events = events_of(read_trace(path));
        QJsonObject e;
        for (const auto& v : events)
            if (v.toObject().value("name").toString() == "with_args")
                e = v.toObject();

        QVERIFY(!e.isEmpty());
        const auto args = e.value("args").toObject();
        QCOMPARE(args.value("n_points").toInteger(), 12345LL);
        QCOMPARE(args.value("ratio").toDouble(), 0.5);
        QCOMPARE(args.value("ok").toBool(), true);
        QCOMPARE(args.value("product").toString(), QString("amda/mms_fgm"));
    }

    void multiple_threads_produce_distinct_tids()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());

        constexpr int N_THREADS = 4;
        constexpr int N_ZONES   = 50;
        std::vector<std::thread> threads;
        threads.reserve(N_THREADS);
        for (int t = 0; t < N_THREADS; ++t)
        {
            threads.emplace_back([] {
                for (int i = 0; i < N_ZONES; ++i)
                {
                    tr::ScopedZone z("worker", "thread");
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            });
        }
        for (auto& th : threads) th.join();

        tr::disable();

        const auto events = events_of(read_trace(path));
        std::set<int64_t> tids;
        int worker_count = 0;
        for (const auto& v : events)
        {
            const auto e = v.toObject();
            if (e.value("ph").toString() != "X") continue;
            if (e.value("name").toString() != "worker") continue;
            ++worker_count;
            tids.insert(e.value("tid").toInteger());
        }
        QCOMPARE(worker_count, N_THREADS * N_ZONES);
        QCOMPARE(static_cast<int>(tids.size()), N_THREADS);
    }

    void async_begin_end_pair_emits_b_and_e()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        const auto id = tr::async_begin("fetch", "data");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        tr::async_end(id);
        tr::disable();

        const auto events = events_of(read_trace(path));
        QJsonObject begin_e, end_e;
        for (const auto& v : events)
        {
            const auto e = v.toObject();
            if (e.value("ph").toString() == "b") begin_e = e;
            if (e.value("ph").toString() == "e") end_e = e;
        }
        QVERIFY(!begin_e.isEmpty());
        QVERIFY(!end_e.isEmpty());
        QCOMPARE(begin_e.value("name").toString(), QString("fetch"));
        QCOMPARE(begin_e.value("cat").toString(), QString("data"));
        QCOMPARE(begin_e.value("id").toString(), end_e.value("id").toString());
    }

    void counter_event_emits_C_with_value()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        tr::counter("queue_depth", 7.0, "fetch");
        tr::disable();

        const auto events = events_of(read_trace(path));
        QJsonObject c;
        for (const auto& v : events)
            if (v.toObject().value("ph").toString() == "C")
                c = v.toObject();

        QVERIFY(!c.isEmpty());
        QCOMPARE(c.value("name").toString(), QString("queue_depth"));
        QCOMPARE(c.value("cat").toString(), QString("fetch"));
        QCOMPARE(c.value("args").toObject().value("queue_depth").toDouble(), 7.0);
    }

    void output_is_object_with_displayTimeUnit_ns()
    {
        QTemporaryFile tmp;
        QVERIFY(tmp.open());
        tmp.close();
        const QString path = tmp.fileName();

        tr::enable(path.toStdString());
        { tr::ScopedZone z("x", "x"); }
        tr::disable();

        const auto doc = read_trace(path);
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value("displayTimeUnit").toString(), QString("ns"));
        QVERIFY(doc.object().value("traceEvents").isArray());
    }
};

QTEST_GUILESS_MAIN(TracingTest)
#include "test_tracing.moc"
