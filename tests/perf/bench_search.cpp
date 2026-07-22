#include <QtTest/QtTest>

#include <SciQLopPlots/Products/SubsequenceMatcher.hpp>

#include <random>
#include <vector>

class BenchSearch : public QObject
{
    Q_OBJECT

private:
    // Representative of ProductsFlatFilterModel::path_text -- path segments
    // joined by ' ', e.g. "mms1 fgm survey l2 b_gse".
    static QList<QString> make_short_candidates(int count)
    {
        static const char* missions[] = { "mms1", "mms2", "mms3", "cluster1", "themisA",
            "solo", "psp" };
        static const char* instruments[]
            = { "fgm", "cis", "hia", "edp", "epd", "swa", "rpw" };
        static const char* products[] = { "survey", "burst", "onboard_moments", "l2",
            "b_gse", "b_gsm", "density", "velocity", "energy_spectrum" };

        QList<QString> out;
        out.reserve(count);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> mi(0, 6), ii(0, 6), pi(0, 8);
        for (int i = 0; i < count; ++i)
            out.append(QString("%1 %2 %3")
                           .arg(missions[mi(rng)])
                           .arg(instruments[ii(rng)])
                           .arg(products[pi(rng)]));
        return out;
    }

    // Representative of ProductsFlatFilterModel::meta_text / raw_text --
    // longer free-text description with dates/uid metadata appended.
    static QList<QString> make_long_candidates(int count)
    {
        auto short_candidates = make_short_candidates(count);
        QList<QString> out;
        out.reserve(count);
        for (const auto& s : short_candidates)
            out.append(s
                + " Instrument description with additional metadata fields uid start_date "
                  "2018-01-01T00:00:00Z stop_date 2024-12-31T23:59:59Z components x y z "
                  "sampling_rate 128Hz calibration v3 mission_phase commissioning");
        return out;
    }

private slots:
    void score_short_candidates_matching_token()
    {
        auto candidates = make_short_candidates(2000);
        QString token = QStringLiteral("fgm");
        QBENCHMARK
        {
            int total = 0;
            for (const auto& c : candidates)
                total += subsequence_score(token, c);
        }
    }

    void score_short_candidates_no_match()
    {
        auto candidates = make_short_candidates(2000);
        QString token = QStringLiteral("xyz9");
        QBENCHMARK
        {
            int total = 0;
            for (const auto& c : candidates)
                total += subsequence_score(token, c);
        }
    }

    void score_long_candidates_matching_token()
    {
        auto candidates = make_long_candidates(2000);
        QString token = QStringLiteral("fgm");
        QBENCHMARK
        {
            int total = 0;
            for (const auto& c : candidates)
                total += subsequence_score(token, c);
        }
    }

    void score_long_candidates_metadata_token()
    {
        auto candidates = make_long_candidates(2000);
        QString token = QStringLiteral("calib");
        QBENCHMARK
        {
            int total = 0;
            for (const auto& c : candidates)
                total += subsequence_score(token, c);
        }
    }

    void score_multi_token_query_realistic_batch()
    {
        // Mimics free_text_score(): several query tokens, each scored
        // against both a short (path) and long (meta) text per candidate.
        auto path_candidates = make_short_candidates(2000);
        auto meta_candidates = make_long_candidates(2000);
        QList<QString> tokens
            = { QStringLiteral("mms1"), QStringLiteral("fgm"), QStringLiteral("survey"),
                  QStringLiteral("b_gse"), QStringLiteral("2018") };

        QBENCHMARK
        {
            int total = 0;
            for (qsizetype i = 0; i < path_candidates.size(); ++i)
                for (const auto& token : tokens)
                    total += std::max(subsequence_score(token, path_candidates[i]),
                        subsequence_score(token, meta_candidates[i]));
        }
    }
};

QTEST_GUILESS_MAIN(BenchSearch)
#include "bench_search.moc"
