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

        plot.legend.setVisible(True)

        plot.rescaleAxes()
        plot.setOpenGl(True)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
