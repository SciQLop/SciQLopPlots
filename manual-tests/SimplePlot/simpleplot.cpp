#include "simpleplot.h"
#include "ui_simpleplot.h"



SimplePlot::SimplePlot(QWidget* parent) : QMainWindow(parent), ui(new Ui::SimplePlot)
{
    ui->setupUi(this);
    auto plot = new Plot { this };
    centralWidget()->layout()->addWidget(plot);
    auto colors = std::array{Qt::blue, Qt::red, Qt::darkGreen};
    for (int i = 0; i < 3; i++)
    {
        plot->addGraph(colors[i%colors.size()]);
        m_gens.emplace_back(std::make_unique<DataProducer<Plot>>(i, plot));
    }

}

SimplePlot::~SimplePlot()
{
    delete ui;
}
