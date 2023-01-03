from SciQLopPlots import QCustomPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, QCPColorGradient, QCPMarginGroup
from PySide6.QtWidgets import QMainWindow, QApplication
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
import sys
import math
import numpy as np
from types import SimpleNamespace

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
    # Note: we could have also just called plot.rescaleAxes(); instead
    # Allow user to drag axis ranges with mouse, zoom with mouse wheel and select graphs by clicking:
    plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom | QCP.iSelectPlottables)
    return "Simple Demo"

def setupColorMapDemo(plot):

    # configure axis rect:
    plot.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom) # this will also allow rescaling the color scale by dragging/zooming
    plot.axisRect().setupFullAxesBox(True);
    plot.xAxis.setLabel("x");
    plot.yAxis.setLabel("y");

    # set up the QCPColorMap:
    colorMap = plot.addColorMap(plot.xAxis, plot.yAxis)
    nx = 200
    ny = 200
    colorMap.data().setSize(nx, ny) # we want the color map to have nx * ny data points
    colorMap.data().setRange(QCPRange(-4, 4), QCPRange(-4, 4)); # and span the coordinate range -4..4 in both key (x) and value (y) dimensions
    # now we assign some data, by accessing the QCPColorMapData instance of the color map:
    for  xIndex in range(nx):
        for yIndex in range(ny):
          x, y = colorMap.data().cellToCoord(xIndex, yIndex)
          r = 3*math.sqrt(x**2+y**2)+1e-2
          z = 2*x*(math.cos(r+2)/r-math.sin(r+2)/r) # the B field strength of dipole radiation (modulo physical constants)
          colorMap.data().setCell(xIndex, yIndex, z)

    # add a color scale:
    plot.colorScale = QCPColorScale(plot)
    plot.plotLayout().addElement(0, 1, plot.colorScale); # add it to the right of the main axis rect
    plot.colorScale.setType(QCPAxis.atRight) # scale shall be vertical bar with tick/axis labels right (actually atRight is already the default)
    colorMap.setColorScale(plot.colorScale) # associate the color map with the color scale
    plot.colorScale.axis().setLabel("Magnetic Field Strength");

    # set the color gradient of the color map to one of the presets:
    colorMap.setGradient(QCPColorGradient.GradientPreset.gpPolar)
    # we could have also created a QCPColorGradient instance and added own colors to
    # the gradient, see the documentation of QCPColorGradient for what's possible.

    # rescale the data dimension (color) such that all data points lie in the span visualized by the color gradient:
    colorMap.rescaleDataRange();

    # make sure the axis rect and color scale synchronize their bottom and top margins (so they line up):
    plot.marginGroup = QCPMarginGroup(plot)
    plot.axisRect().setMarginGroup(QCP.msBottom|QCP.msTop, plot.marginGroup)
    plot.colorScale.setMarginGroup(QCP.msBottom|QCP.msTop, plot.marginGroup)

    # rescale the key (x) and value (y) axes so the whole color map is visible:
    plot.rescaleAxes()
    return "Color Map Demo"

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
        setupColorMapDemo(self.centralWidget())



if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
