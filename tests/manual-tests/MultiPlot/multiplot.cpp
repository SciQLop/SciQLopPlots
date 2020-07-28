#include "multiplot.h"
#include "ui_multiplot.h"
#include <Graph.hpp>
#include <cpp_utils.hpp>

QString humanize(std::size_t number)
{
    std::array prefixes = { "", "k", "M", "G", "T" };
    double value { static_cast<double>(number) };
    std::size_t prefix_index = 0;
    while (value > 1000.)
    {
        value = value / 1000.;
        prefix_index++;
    }
    return QString { "%1%2" }.arg(value, 0, 'g', 3).arg(prefixes[prefix_index]);
}

MultiPlot::MultiPlot(QWidget* parent) : QMainWindow(parent), ui(new Ui::MultiPlot)
{
    ui->setupUi(this);
    2 * [this]() {
        auto syncPannel = new SciQLopPlots::SyncPannel { this };
        centralWidget()->layout()->addWidget(syncPannel);

        auto plot1 = makePlot(syncPannel);
        auto plot2 = makePlot(syncPannel);
        auto plot3 = makePlot(syncPannel);
    };
}

MultiPlot::~MultiPlot()
{
    delete ui;
}

SciQLopPlots::SciQLopPlot* MultiPlot::makePlot(SciQLopPlots::SyncPannel* panel)
{
    auto plot = new SciQLopPlots::SciQLopPlot { panel };
    panel->addPlot(plot);
    auto colors = std::array { Qt::blue, Qt::red, Qt::darkGreen };
    std::size_t i = 0;
    3 * [plot, this, &colors, &i]() {
        m_gens.emplace_back(
            std::make_unique<DataProducer<SciQLopPlots::SciQLopPlot>>(plot, i, colors[i++]));
    };

    connect(plot, &SciQLopPlots::SciQLopPlot::dataChanged, this, [this]() {
        auto nPoints = std::accumulate(std::cbegin(m_gens), std::cend(m_gens), 0UL,
            [](auto prev, const auto& gen) { return prev + gen->nPoints; });
        ui->totalPointsNumber->setText(humanize(nPoints));
    });

    plot->setXRange({ -100., 100. });
    plot->setYRange({ -2., 2. });
    return plot;
}
