#ifndef SIMPLEPLOT_H
#define SIMPLEPLOT_H

#include <QMainWindow>
#include <SciQLopPlots/SciQLopPlot.hpp>
#include <SciQLopPlots/Qt/QCustomPlot/QCustomPlotWrapper.hpp>
#include "DataGenerator.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class SimplePlot; }
QT_END_NAMESPACE

class SimplePlot : public QMainWindow
{
    Q_OBJECT

public:
    SimplePlot(QWidget *parent = nullptr);
    ~SimplePlot();

private:
    Ui::SimplePlot *ui;
    std::vector<std::unique_ptr<DataProducer>> m_gens;
};
#endif // SIMPLEPLOT_H
