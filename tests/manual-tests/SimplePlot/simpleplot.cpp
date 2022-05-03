#include "simpleplot.h"
#include "ui_simpleplot.h"
#include <SciQLopPlots/Qt/Graph.hpp>
#include <SciQLopPlots/Qt/QCustomPlot/SciQLopPlots.hpp>
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

SimplePlot::SimplePlot(QWidget* parent) : QMainWindow(parent), ui(new Ui::SimplePlot)
{
    ui->setupUi(this);
    auto plot = new SciQLopPlots::SciQLopPlot { this };
    plot->addColorMap();
    centralWidget()->layout()->addWidget(plot);
    auto colors = std::array { Qt::blue, Qt::red, Qt::darkGreen };

    3 *
        [plot, this, colors]()
    {
        static std::size_t i = 0;
        m_gens.emplace_back(
            std::make_unique<DataProducer1D<SciQLopPlots::SciQLopPlot>>(plot, i, colors[i++]));
    };

    m_gens.emplace_back(std::make_unique<DataProducer2D<SciQLopPlots::SciQLopPlot>>(plot));

    connect(plot, &SciQLopPlots::SciQLopPlot::dataChanged, this,
        [this]()
        {
            auto nPoints = std::accumulate(std::cbegin(m_gens), std::cend(m_gens), 0UL,
                [](auto prev, const auto& gen) { return prev + gen->nPoints; });
            ui->totalPointsNumber->setText(humanize(nPoints));
        });

    connect(ui->actionAdd_TimeSpan, &QAction::triggered, this,
        [this, plot](bool checked)
        {
            if (checked)
                plot->setInteractionsMode(SciQLopPlots::enums::IteractionsMode::ObjectCreation);
            else
                plot->setInteractionsMode(SciQLopPlots::enums::IteractionsMode::Normal);
        });

    plot->setXRange({ -100., 100. });
    plot->setYRange({ -2., 2. });

    plot->setObjectFactory(SciQLopPlots::interfaces::make_shared_GraphicObjectFactory(
        [](SciQLopPlots::interfaces::IPlotWidget* plot,
            const SciQLopPlots::view::pixel_coordinates<2>& start_position)
        {
            auto x = plot->map_pixels_to_data_coordinates(
                start_position.component(SciQLopPlots::enums::Axis::x).value,
                SciQLopPlots::enums::Axis::x);
            return new SciQLopPlots::TimeSpan { dynamic_cast<SciQLopPlots::SciQLopPlot*>(plot),
                { x, x } };
        }));
}

SimplePlot::~SimplePlot()
{
    delete ui;
}
