#include <QObject>
#include <SciQLopPlot.hpp>
#include <array>
#include <future>
#include <math.h>
#include <mutex>
#include <optional>
#include <range/v3/view.hpp>
#include <range/v3/view/iota.hpp>

struct MyCos
{
    std::array<double, 1024 * 32> table;
    MyCos()
    {
        std::generate(std::begin(table), std::end(table), [size = table.size()]() {
            static double x = 0.;
            double res = cos(x * 2 * M_PI / (size));
            x += 1.;
            return res;
        });
    }
    double operator()(double x)
    {
        auto index = int(x * table.size() / (2 * M_PI)) % table.size();
        return table[index];
    }
};

template <typename Plot>
struct DataProducer
{
    using data_t = std::pair<std::vector<double>, std::vector<double>>;
    std::optional<SciQLopPlots::AxisRange> nextRange;
    std::mutex nextRangeMutex;
    Plot* m_plot;
    std::mutex plotMutex;
    std::mutex genMutex;
    int m_graphIndex;
    std::size_t lastGenSize;
    DataProducer(int graphIndex, Plot* plot) : m_plot { plot }, m_graphIndex { graphIndex }
    {
        QObject::connect(
            plot, &Plot::xRangeChanged, [this](auto range) { this->generate_async(range); });
    }

    void generate_async(SciQLopPlots::AxisRange newRange)
    {
        {
            std::lock_guard rangeLock { nextRangeMutex };
            if (nextRange)
                nextRange = newRange;
            if (!genMutex.try_lock())
                nextRange = newRange;
            else
                genMutex.unlock();
        }
        auto _ = std::async(std::launch::async, [&newRange, this] {
            {
                data_t data;
                {
                    std::lock_guard genLock { genMutex };
                    data = DataProducer::generate(
                        newRange, m_graphIndex * 120., (m_graphIndex * 0.3) + 1.);
                }
                lastGenSize = data.first.size();
                std::lock_guard pltLock { plotMutex };
                m_plot->plot(m_graphIndex, std::move(data.first), std::move(data.second));
            }
            if (nextRange)
            {
                SciQLopPlots::AxisRange range;
                {
                    std::lock_guard rangeLock { nextRangeMutex };
                    range = *nextRange;
                    nextRange = std::nullopt;
                }
                generate_async(range);
            }
        });
    }

    static data_t generate(SciQLopPlots::AxisRange newRange, double shift = 0., double ampl = 1.)
    {
        static MyCos myCos;
        constexpr auto step = .1;
        data_t data;
        auto nPoints = int((newRange.second - newRange.first) / step);
        data.first = ranges::views::ints(0, ranges::unreachable)
            | ranges::views::transform(
                [&newRange, step = step](int i) { return newRange.first + double(i) * step; })
            | ranges::views::take(nPoints) | ranges::to<std::vector>;
        data.second = data.first | ranges::views::transform([shift, ampl](double i) {
            return ampl * myCos((i + shift) / 100.);
        }) | ranges::to<std::vector>;
        return data;
    }
};
