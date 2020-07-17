#include "multiplot.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    MultiPlot w;
    w.show();
    return a.exec();
}
