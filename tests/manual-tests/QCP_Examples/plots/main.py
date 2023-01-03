from SciQLopPlots import QCustomPlot, QCP
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
import sys
import numpy as np

def setupQuadraticDemo(plot):
    # generate some data:
    x = np.arange(101)/50.  -1
    y = x**2
    # create graph and assign data to it:
    graph = plot.addGraph()
    graph.setData(x, y)
    # give the axes some labels:
    plot.xAxis.setLabel("x")
    plot.yAxis.setLabel("y")
    # set axes ranges, so we see all data:
    plot.xAxis.setRange(-1, 1)
    plot.yAxis.setRange(0, 1)
    return "Quadratic Demo"

def setupSimpleDemo(plot):
    # add two new graphs and set their look:
    graph0 = plot.addGraph()
    graph0.setPen(QPen(QColorConstants.Blue))
    graph0.setBrush(QBrush(QColor(0, 0, 255, 20)))
    graph1 = plot.addGraph()
    graph1.setPen(QPen(QColorConstants.Red))
    # generate some points of data (y0 for first, y1 for second graph):
    x = np.arange(251)*1.
    y0 = np.exp(-x/150.)*np.cos(x/10.) # exponentially decaying cosine
    y1 = np.exp(-x/150.) # exponential envelope
    # configure right and top axis to show ticks but no labels:
    # (see QCPAxisRect::setupFullAxesBox for a quicker method to do this)
    plot.xAxis2.setVisible(True)
    plot.xAxis2.setTickLabels(False)
    plot.yAxis2.setVisible(True)
    plot.yAxis2.setTickLabels(False)
    # make left and bottom axes always transfer their ranges to right and top axes:
    plot.xAxis.rangeChanged.connect(plot.xAxis2.setRange)
    plot.yAxis.rangeChanged.connect(plot.yAxis2.setRange)
    # pass data points to graphs:
    graph0.setData(x, y0)
    graph1.setData(x, y1)
    # let the ranges scale themselves so graph 0 fits perfectly in the visible area:
    graph0.rescaleAxes()
    # same thing for graph 1, but only enlarge ranges (in case graph 1 is smaller than graph 0):
    graph1.rescaleAxes(True)
    # Note: we could have also just called customPlot->rescaleAxes(); instead
    # Allow user to drag axis ranges with mouse, zoom with mouse wheel and select graphs by clicking:
    plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom | QCP.iSelectPlottables)
    return "Simple Demo"

class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);
        self.setupDemo()

    def _setup_ui(self):
        self.setCentralWidget(QCustomPlot(self))

    def setupDemo(self,demoIndex=0):
        self.centralWidget().replot();
        setupSimpleDemo(self.centralWidget())



if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
