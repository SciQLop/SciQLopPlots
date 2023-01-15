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
        self.graph = plot.addSciQLopColorMap(plot.xAxis, plot.yAxis, 'MyColorMap')
        nx = 2000
        ny = 2000
        data = np.empty((nx,ny))
        x_vect= (np.arange(nx)*8./nx -4.).reshape((-1,1))
        y_vect= (np.arange(ny)*8./ny -4.).reshape((-1,1))

        data[:,:] = np.cos(np.sqrt((x_vect+2)**2 @ y_vect.T**2))
        self.graph.setData(x_vect, y_vect, data)

        plot.legend.setVisible(True)

        plot.colorScale = QCPColorScale(plot)
        plot.plotLayout().addElement(0, 1, plot.colorScale); # add it to the right of the main axis rect
        plot.colorScale.setType(QCPAxis.atRight) # scale shall be vertical bar with tick/axis labels right (actually atRight is already the default)

        self.graph.colorMap().setColorScale(plot.colorScale) # associate the color map with the color scale
        plot.colorScale.axis().setLabel("Magnetic Field Strength");

        # set the color gradient of the color map to one of the presets:
        self.graph.colorMap().setGradient(QCPColorGradient.GradientPreset.gpPolar)
        # we could have also created a QCPColorGradient instance and added own colors to
        # the gradient, see the documentation of QCPColorGradient for what's possible.

        # rescale the data dimension (color) such that all data points lie in the span visualized by the color gradient:
        self.graph.colorMap().rescaleDataRange();

        # make sure the axis rect and color scale synchronize their bottom and top margins (so they line up):
        plot.marginGroup = QCPMarginGroup(plot)
        plot.axisRect().setMarginGroup(QCP.msBottom|QCP.msTop, plot.marginGroup)
        plot.colorScale.setMarginGroup(QCP.msBottom|QCP.msTop, plot.marginGroup)

        # rescale the key (x) and value (y) axes so the whole color map is visible:
        plot.rescaleAxes()

        plot.setOpenGl(True)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
