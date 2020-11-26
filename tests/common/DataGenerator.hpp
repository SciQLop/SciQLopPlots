#include <Graph.hpp>
#include <QObject>
#include <SciQLopPlot.hpp>
#include <array>
#include <math.h>
#include <optional>
#include <range/v3/view.hpp>
#include <range/v3/view/iota.hpp>
#include <thread>

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
    SciQLopPlots::Graph<data_t, Plot> graph;
    std::size_t nPoints=0;
    std::thread genThread;
    DataProducer(Plot* plot, int index = 0, QColor color = Qt::blue)
            : graph { SciQLopPlots::add_graph<data_t>(plot, color) }
    {
        genThread = std::thread([&nPoints = nPoints, &out = graph.data_in,
                                    &in = graph.transformations_out, graphIndex = index]() {
            while (!in.closed())
            {
                if (auto newRange = in.take(); newRange)
                    out << DataProducer::generate(
                        *newRange, nPoints, graphIndex * 120., (graphIndex * 0.3) + 1.);
            }
        });
    }

    ~DataProducer()
    {
        graph.data_in.close();
        graph.transformations_out.close();
        genThread.join();
    }

    static data_t generate(
        SciQLopPlots::AxisRange newRange, std::size_t& nPoints, double shift = 0., double ampl = 1.)
    {
        static MyCos myCos;
        constexpr auto step = .1;
        data_t data;
        nPoints = int((newRange.second - newRange.first) / step);
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
