#include "simpleplot.h"
#include "ui_simpleplot.h"
#include <SciQLopPlot.hpp>

SimplePlot::SimplePlot(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::SimplePlot)
{
    ui->setupUi(this);
    setCentralWidget(new SciQLopPlots::PlotWidget<SciQLopPlots::QCustomPlotWrapper>{this});
}

SimplePlot::~SimplePlot()
{
    delete ui;
}

