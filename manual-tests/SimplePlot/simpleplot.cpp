#include "simpleplot.h"
#include "ui_simpleplot.h"
#include <Graph.hpp>
#include <cpp_utils.hpp>

QString humanize(std::size_t number)
{
    std::array prefixes={"","k","M","G","T"};
    double value{static_cast<double>(number)};
    int prefix_index = 0;
    while (value>1000.){value = value/1000.;prefix_index++;}
    return QString{"%1%2"}.arg(value,0, 'g',3).arg(prefixes[prefix_index]);
}

SimplePlot::SimplePlot(QWidget* parent) : QMainWindow(parent), ui(new Ui::SimplePlot)
{
    ui->setupUi(this);
    auto plot = new SciQLopPlots::SciQLopPlot { this };
    centralWidget()->layout()->addWidget(plot);
    auto colors = std::array { Qt::blue, Qt::red, Qt::darkGreen };

    3 * [plot, this, &colors]() {
        static int i = 0;
        m_gens.emplace_back(
            std::make_unique<DataProducer<SciQLopPlots::SciQLopPlot>>(plot, i, colors[i++]));
    };
    connect(plot, &SciQLopPlots::SciQLopPlot::dataChanged, this, [this]() {
        auto nPoints = std::accumulate(std::cbegin(m_gens), std::cend(m_gens), 0UL,
            [](auto prev, const auto& gen) { return prev + gen->nPoints; });
        ui->totalPointsNumber->setText(humanize(nPoints));
    });
}

SimplePlot::~SimplePlot()
{
    delete ui;
}
