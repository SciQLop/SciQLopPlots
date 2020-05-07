#ifndef SIMPLEPLOT_H
#define SIMPLEPLOT_H

#include <QMainWindow>

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
};
#endif // SIMPLEPLOT_H
