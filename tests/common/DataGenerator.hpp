#include <Graph.hpp>
#include <QObject>
#include <SciQLopPlot.hpp>
#include <array>
#include <math.h>
#include <optional>
#include <range/v3/view.hpp>
#include <range/v3/view/iota.hpp>
#include <thread>
#include <tuple>

struct MyCos
{
    std::array<double, 1024 * 32> table;
    MyCos()
    {
        std::generate(std::begin(table), std::end(table),
            [size = table.size()]()
            {
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


struct DataProducer
{
    std::size_t nPoints = 0;
    std::thread genThread;
};

template <typename Plot>
struct DataProducer1D : public DataProducer
{
    using data_t = std::pair<std::vector<double>, std::vector<double>>;
    SciQLopPlots::Graph<data_t, Plot> graph;

    DataProducer1D(Plot* plot, int index = 0, QColor color = Qt::blue)
            : graph { SciQLopPlots::add_graph<data_t>(plot, color) }
    {
        genThread = std::thread(
            [&nPoints = nPoints, &graph = graph, &in = graph.transformations_out,
                graphIndex = index]()
            {
                while (!in.closed())
                {
                    if (auto maybeNewRange = in.take(); maybeNewRange)
                        graph << DataProducer1D::generate(
                            *maybeNewRange, nPoints, graphIndex * 120., (graphIndex * 0.3) + 1.);
                }
            });
    }

    ~DataProducer1D()
    {
        graph.close();
        genThread.join();
    }

    static data_t generate(
        SciQLopPlots::axis::range newRange, std::size_t& nPoints, double shift = 0., double ampl = 1.)
    {
        static MyCos myCos;
        constexpr auto step = .1;
        data_t data;
        nPoints = int((newRange.second - newRange.first) / step);
        data.first = ranges::views::ints(0, ranges::unreachable)
            | ranges::views::transform(
                [&newRange, step = step](int i) { return newRange.first + double(i) * step; })
            | ranges::views::take(nPoints) | ranges::to<std::vector>;
        data.second = data.first
            | ranges::views::transform(
                [shift, ampl](double i) { return ampl * myCos((i + shift) / 100.); })
            | ranges::to<std::vector>;
        return data;
    }
};


template <typename Plot>
struct DataProducer2D : public DataProducer
{
    using data_t = std::tuple<std::vector<double>, std::vector<double>, std::vector<double>>;
    SciQLopPlots::Graph<data_t, Plot> graph;

    DataProducer2D(Plot* plot) : graph { SciQLopPlots::add_colormap<data_t>(plot) }
    {
        genThread = std::thread(
            [&nPoints = nPoints, &graph = graph, &in = graph.transformations_out]()
            {
                while (!in.closed())
                {
                    if (auto maybeNewRange = in.take(); maybeNewRange)
                        graph << DataProducer2D::generate(
                            *maybeNewRange, nPoints, 1 * 120., (1 * 0.3) + 1.);
                }
            });
    }

    ~DataProducer2D()
    {
        graph.close();
        genThread.join();
    }

    static data_t generate(
        SciQLopPlots::axis::range newRange, std::size_t& nPoints, double shift = 0., double ampl = 1.)
    {
        static MyCos myCos;
        constexpr auto step = .1;
        data_t data;
        nPoints = int((newRange.second - newRange.first) / step);
        const auto Ysize = 32;
        std::get<0>(data) = ranges::views::ints(0, ranges::unreachable)
            | ranges::views::transform(
                [&newRange, step = step](int i) { return newRange.first + double(i) * step; })
            | ranges::views::take(nPoints) | ranges::to<std::vector>;
        std::get<1>(data) = ranges::views::ints(0, ranges::unreachable)
            | ranges::views::transform([](int i) { return i / 32.; }) | ranges::views::take(Ysize)
            | ranges::to<std::vector<double>>;
        std::get<2>(data) = ranges::views::ints(0, ranges::unreachable)
            | ranges::views::transform([&newRange, step = step](int i)
                { return newRange.first + double(i) * cos(double(i) / 100) * (i & 31) * step; })
            | ranges::views::take(nPoints * Ysize) | ranges::to<std::vector>;
        nPoints *= Ysize;
        return data;
    }
};
