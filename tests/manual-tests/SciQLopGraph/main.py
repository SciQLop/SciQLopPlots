from SciQLopPlots import QCustomPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, QCPColorGradient, QCPMarginGroup
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
import sys
import math
import numpy as np
from types import SimpleNamespace


class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);

    def _setup_ui(self):
        plot: QCustomPlot = QCustomPlot(self)
        plot.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom|QCP.iSelectPlottables)
        self.setCentralWidget(plot)
        self.graph = plot.addSciQLopGraph(plot.xAxis, plot.yAxis, ["X","Y","Z"])
        x=np.arange(3e7)*1.
        y=np.ones((3,len(x)))
        y[0]=np.cos(x/60)
        y[1]=np.cos(x/600)*1.3
        y[2]=np.cos(x/6000)*1.7
        self.graph.setData(x,y)

        self.graph.graphAt(0).setPen(QPen(QColorConstants.Red))
        self.graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
        self.graph.graphAt(2).setPen(QPen(QColorConstants.Green))

        plot.legend.setVisible(True)

        plot.rescaleAxes()
        plot.setOpenGl(True)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
