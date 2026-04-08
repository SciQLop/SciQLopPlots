#include <QtTest/QtTest>

#include <SciQLopPlots/DSP/Segments.hpp>

#include <random>
#include <vector>

class BenchSegments : public QObject
{
    Q_OBJECT

private:
    std::vector<double> make_timestamps(std::size_t n, int n_gaps, double gap_size = 100.0)
    {
        const double dt = 0.001;
        std::vector<double> t(n);
        for (std::size_t i = 0; i < n; ++i)
            t[i] = static_cast<double>(i) * dt;

        std::mt19937 rng(42);
        std::uniform_int_distribution<std::size_t> dist(100, n - 100);
        std::vector<std::size_t> positions;
        for (int g = 0; g < n_gaps; ++g)
            positions.push_back(dist(rng));
        std::sort(positions.begin(), positions.end());

        for (auto pos : positions)
            for (std::size_t i = pos; i < n; ++i)
                t[i] += gap_size * dt;

        return t;
    }

private slots:
    void compute_median_dt_1M()
    {
        auto t = make_timestamps(1'000'000, 0);
        std::span<const double> ts { t.data(), t.size() };
        QBENCHMARK { auto m = sqp::dsp::detail::compute_median_dt(ts); }
    }

    void find_gap_indices_1M_no_gaps()
    {
        auto t = make_timestamps(1'000'000, 0);
        std::span<const double> ts { t.data(), t.size() };
        QBENCHMARK { auto g = sqp::dsp::detail::find_gap_indices(ts, 3.0); }
    }

    void find_gap_indices_1M_5_gaps()
    {
        auto t = make_timestamps(1'000'000, 5);
        std::span<const double> ts { t.data(), t.size() };
        QBENCHMARK { auto g = sqp::dsp::detail::find_gap_indices(ts, 3.0); }
    }

    void split_segments_1M_no_gaps()
    {
        auto t = make_timestamps(1'000'000, 0);
        std::vector<double> y(t.size(), 1.0);
        QBENCHMARK
        {
            auto s = sqp::dsp::split_segments<double>(
                { t.data(), t.size() }, { y.data(), y.size() }, 1, 3.0);
        }
    }

    void split_segments_1M_5_gaps()
    {
        auto t = make_timestamps(1'000'000, 5);
        std::vector<double> y(t.size(), 1.0);
        QBENCHMARK
        {
            auto s = sqp::dsp::split_segments<double>(
                { t.data(), t.size() }, { y.data(), y.size() }, 1, 3.0);
        }
    }

    void split_segments_1M_50_gaps()
    {
        auto t = make_timestamps(1'000'000, 50);
        std::vector<double> y(t.size(), 1.0);
        QBENCHMARK
        {
            auto s = sqp::dsp::split_segments<double>(
                { t.data(), t.size() }, { y.data(), y.size() }, 1, 3.0);
        }
    }

    void split_segments_10M_5_gaps()
    {
        auto t = make_timestamps(10'000'000, 5);
        std::vector<double> y(t.size(), 1.0);
        QBENCHMARK
        {
            auto s = sqp::dsp::split_segments<double>(
                { t.data(), t.size() }, { y.data(), y.size() }, 1, 3.0);
        }
    }
};

QTEST_GUILESS_MAIN(BenchSegments)
#include "bench_segments.moc"
