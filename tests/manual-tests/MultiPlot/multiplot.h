#ifndef SIMPLEPLOT_H
#define SIMPLEPLOT_H

#include <QMainWindow>
#include <SciQLopPlot.hpp>
#include <QCustomPlotWrapper.hpp>
#include "DataGenerator.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MultiPlot; }
QT_END_NAMESPACE

class MultiPlot : public QMainWindow
{
    Q_OBJECT

public:
    MultiPlot(QWidget *parent = nullptr);
    ~MultiPlot();

private:
    Ui::MultiPlot *ui;
    std::vector<std::unique_ptr<DataProducer<SciQLopPlots::SciQLopPlot>>> m_gens;
};
#endif // SIMPLEPLOT_H
