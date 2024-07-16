from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, \
                         QCPLegend, QCPColorGradient, QCPMarginGroup, QCPAxisRect
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
from PySide6.QtCore import Qt
import sys
import math
import numpy as np
from types import SimpleNamespace


class PlotPanel(QScrollArea):
    def __init__(self,parent):
        QScrollArea.__init__(self,parent)
        self.setWidget(QWidget(self))
        self.widget().setLayout(QVBoxLayout())
        self.setVerticalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAsNeeded)
        self.setWidgetResizable(True)

    def set_plot(self, plot):
        self.widget().layout().addWidget(plot)



class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.axisRects=[]
        self.graphs = []
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);

    def _setup_ui(self):
        plot: SciQLopPlot = SciQLopPlot(self)
        plot.setOpenGl(True)
        plot.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom|QCP.iSelectPlottables)
        plot.plotLayout().clear()
        self.plot_panel = PlotPanel(self)
        self.setCentralWidget(self.plot_panel)
        self.plot_panel.set_plot(plot)
        for i in range(10):
            axis_rect = QCPAxisRect(plot)
            plot.legend = QCPLegend()
            axis_rect.insetLayout().addElement(plot.legend, Qt.AlignBottom|Qt.AlignRight)
            plot.legend.setVisible(True)
            axis_rect.setMinimumSize(200,200)
            self.axisRects.append(axis_rect)
            plot.plotLayout().addElement(len(self.axisRects)-1,0,axis_rect)
            graph = plot.addSciQLopGraph(axis_rect.axis(QCPAxis.atBottom), axis_rect.axis(QCPAxis.atLeft), ["X","Y","Z"])
            self.graphs.append(graph)
            x=np.arange(300000)*1.
            y=np.ones((3,len(x)))
            y[0]=np.cos(x/60)
            y[1]=np.cos(x/600)*1.3
            y[2]=np.cos(x/6000)*1.7
            graph.setData(x,y)

            graph.graphAt(0).setPen(QPen(QColorConstants.Red))
            graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
            graph.graphAt(2).setPen(QPen(QColorConstants.Green))

        #plot.legend.setVisible(True)

        plot.rescaleAxes()


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
