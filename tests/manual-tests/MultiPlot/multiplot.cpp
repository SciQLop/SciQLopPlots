#include "multiplot.h"
#include <SciQLopPlots/Graph.hpp>
#include <QDockWidget>
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

QDockWidget* dockify(QWidget* widget, QWidget* parent = nullptr)
{
    auto dock = new QDockWidget { parent };
    widget->setParent(dock);
    dock->setWidget(widget);
    dock->setAllowedAreas(Qt::DockWidgetArea::AllDockWidgetAreas);
    dock->setWindowTitle("Plot Frame");
    SciQLopPlots::removeAllMargins(dock);
    return dock;
}

MultiPlot::MultiPlot(QWidget* parent) : QMainWindow(parent)
{
    m_totalPointNumber = new QLabel;
    statusBar()->addWidget(m_totalPointNumber);
    setDockNestingEnabled(true);
    3 * [this]() {
        auto syncPannel = new SciQLopPlots::SyncPannel {};
        syncPannel->setXRange({ -100., 100. });
        auto dock = dockify(syncPannel, this);
        addDockWidget(Qt::DockWidgetArea::LeftDockWidgetArea, dock);

        makePlot(syncPannel);
        makePlot(syncPannel);
        makePlot(syncPannel);
    };
}

MultiPlot::~MultiPlot() { }

SciQLopPlots::SciQLopPlot* MultiPlot::makePlot(SciQLopPlots::SyncPannel* panel)
{
    auto plot = new SciQLopPlots::SciQLopPlot { panel };
    plot->setMinimumHeight(150);
    panel->addPlot(plot);
    auto colors = std::array { Qt::blue, Qt::red, Qt::darkGreen };
    std::size_t i = 0;
    3 * [plot, this, &colors, &i]() {
        m_gens.emplace_back(
            std::make_unique<DataProducer1D<SciQLopPlots::SciQLopPlot>>(plot, i, colors[i++]));
    };

    connect(plot, &SciQLopPlots::SciQLopPlot::dataChanged, this, [this]() {
        auto nPoints = std::accumulate(std::cbegin(m_gens), std::cend(m_gens), 0UL,
            [](auto prev, const auto& gen) { return prev + gen->nPoints; });
        m_totalPointNumber->setText(QString("Total number of points: %1").arg(humanize(nPoints)));
    });

    plot->setYRange({ -2., 2. });
    return plot;
}
