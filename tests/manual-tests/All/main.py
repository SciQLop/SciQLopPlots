from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, \
                         QCPLegend, QCPColorGradient, QCPMarginGroup, QCPAxisRect,QCPAxisTickerDateTime, MultiPlotsVerticalSpan
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
from PySide6.QtCore import Qt
import sys
import math
import numpy as np
from datetime import datetime
from types import SimpleNamespace



class SimpleGraph(QWidget):
    def __init__(self,parent):
        QWidget.__init__(self,parent)
        self.plot = SciQLopPlot(self)
        self.plot.setOpenGl(True)
        self.plot.setInteractions(QCP.iRangeDrag|QCP.iRangeZoom|QCP.iSelectPlottables)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(self.plot)
        self.graph = self.plot.addSciQLopGraph(self.plot.axisRect().axis(QCPAxis.atBottom), self.plot.axisRect().axis(QCPAxis.atLeft), ["X","Y","Z"])
        x=np.arange(3000, dtype=np.float64)
        y=np.ones((3,len(x)), dtype=np.float64)
        y[0]=np.cos(x/60.)
        y[1]=np.cos(x/600.)*1.3
        y[2]=np.cos(x/6000.)*1.7
        self.graph.setData(x,y)

        self.graph.graphAt(0).setPen(QPen(QColorConstants.Red))
        self.graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
        self.graph.graphAt(2).setPen(QPen(QColorConstants.Green))

        self.plot.xAxis.setRange(x[0],x[-1])
        self.plot.yAxis.setRange(-2,2)



class TimeSerieGraph(QWidget):
    def __init__(self,parent):
        QWidget.__init__(self,parent)
        self.plot = SciQLopPlot(self)
        self.plot.setOpenGl(True)
        self.plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom | QCP.iSelectPlottables | QCP.iSelectAxes | QCP.iSelectLegend | QCP.iSelectItems)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(self.plot)
        date_ticker = QCPAxisTickerDateTime()
        date_ticker.setDateTimeFormat("yyyy/MM/dd \nhh:mm:ss.zzz")
        date_ticker.setDateTimeSpec(Qt.UTC)
        self.plot.xAxis.setTicker(date_ticker)
        self.graph = self.plot.addSciQLopGraph(self.plot.axisRect().axis(QCPAxis.atBottom), self.plot.axisRect().axis(QCPAxis.atLeft), ["X","Y","Z"])
        x=np.arange(3000, dtype=np.float64)
        y=np.ones((3,len(x)), dtype=np.float64)
        y[0]=np.cos(x/60.)
        y[1]=np.cos(x/600.)*1.3
        y[2]=np.cos(x/6000.)*1.7
        x+=datetime.now().timestamp()
        self.graph.setData(x,y)

        self.graph.graphAt(0).setPen(QPen(QColorConstants.Red))
        self.graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
        self.graph.graphAt(2).setPen(QPen(QColorConstants.Green))
        self.plot.xAxis.setRange(x[0],x[-1])
        self.plot.yAxis.setRange(-2,2)

        middle = (x[0]+x[-1])/2
        width = (x[-1]-x[0])/2

        self._verticalSpan = MultiPlotsVerticalSpan([self.plot], QCPRange(middle-width/10, middle+width/10), QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span", parent=self)







class Tabs(QTabWidget):
    def __init__(self,parent):
        QTabWidget.__init__(self,parent)
        self.setTabPosition(QTabWidget.TabPosition.North)
        self.setTabShape(QTabWidget.TabShape.Rounded)
        self.setMovable(True)
        self.addTab(SimpleGraph(self), "Simple Graph")
        self.addTab(TimeSerieGraph(self), "Time Serie Graph")






class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.axisRects=[]
        self.graphs = []
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);

    def _setup_ui(self):
        self.tabs = Tabs(self)
        self.setCentralWidget(self.tabs)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
