#ifndef SIMPLEPLOT_H
#define SIMPLEPLOT_H

#include <QMainWindow>
#include <QCustomPlotWrapper.hpp>
#include <SciQLopPlot.hpp>
#include "DataGenerator.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class SimplePlot; }
QT_END_NAMESPACE

class SimplePlot : public QMainWindow
{
    using Plot = SciQLopPlots::PlotWidget<SciQLopPlots::QCustomPlotWrapper>;
    Q_OBJECT

public:
    SimplePlot(QWidget *parent = nullptr);
    ~SimplePlot();

private:
    Ui::SimplePlot *ui;
    std::vector<std::unique_ptr<DataProducer<Plot>>> m_gens;
};
#endif // SIMPLEPLOT_H
