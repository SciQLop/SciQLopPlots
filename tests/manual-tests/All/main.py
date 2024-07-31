from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, \
                         QCPLegend, QCPColorGradient, QCPMarginGroup, QCPAxisRect,QCPAxisTickerDateTime, \
                         MultiPlotsVerticalSpan, QCPAxisTickerLog,SciQLopMultiPlotPanel, SciQLopVerticalSpan
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
from PySide6.QtCore import Qt
import sys, os
import math
import numpy as np
from datetime import datetime
from types import SimpleNamespace

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager

os.environ['QT_API'] = 'PySide6'

def make_plot(parent, time_axis=False):
    plot = SciQLopPlot(parent)
    plot.setInteractions(QCP.iRangeDrag | QCP.iRangeZoom | QCP.iSelectPlottables | QCP.iSelectAxes | QCP.iSelectLegend | QCP.iSelectItems)
    if time_axis:
        date_ticker = QCPAxisTickerDateTime()
        date_ticker.setDateTimeFormat("yyyy/MM/dd \nhh:mm:ss.zzz")
        date_ticker.setDateTimeSpec(Qt.UTC)
        plot.xAxis.setTicker(date_ticker)
    plot.enable_legend(True)
    plot.minimize_margins()
    return plot

def add_graph(plot, time_axis=False, offset=0.):
    graph = plot.addSciQLopGraph(plot.axisRect().axis(QCPAxis.atBottom), plot.axisRect().axis(QCPAxis.atLeft), ["X","Y","Z"])
    x=np.arange(3000, dtype=np.float64)
    y=np.cos(np.array([x/6.,x/60.,x/600.])) * np.cos(np.array([x,x/6.,x/60.])) * [[100.],[1300],[17000]]
    y+=offset
    if time_axis:
        x+=datetime.now().timestamp()
    graph.setData(x,y)

    graph.graphAt(0).setPen(QPen(QColorConstants.Red))
    graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
    graph.graphAt(2).setPen(QPen(QColorConstants.Green))

    plot.xAxis.setRange(x[0],x[-1])
    plot.yAxis.setRange(1.2*np.min(y),1.2*np.max(y))
    return graph

def butterfly():
    t = np.linspace(0, 12*np.pi, 5000)
    x = np.sin(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    y = np.cos(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    return x, y

def add_curve(plot):
    curve = plot.addSciQLopCurve(plot.axisRect().axis(QCPAxis.atBottom), plot.axisRect().axis(QCPAxis.atLeft), ["butterfly"])
    curve.setData(*butterfly())
    return curve

def add_colormap(plot, time_axis=False):
    x_axis = plot.axisRect().axis(QCPAxis.atBottom)
    y_axis = plot.axisRect().axis(QCPAxis.atRight)
    colormap = plot.addSciQLopColorMap(x_axis, y_axis, "Cmap")
    colormap.set_auto_scale_y(True)
    colormap.colorMap().setLayer(plot.layer("background"))
    color_scale = QCPColorScale(plot)
    plot.plotLayout().addElement(0, 1, color_scale)

    color_scale.setDataScaleType(QCPAxis.stLogarithmic)
    color_scale.axis().setTicker(QCPAxisTickerLog())
    color_scale.setType(QCPAxis.atRight)

    colormap.colorMap().setColorScale(color_scale)
    colormap.colorMap().setInterpolate(False)
    colormap.colorMap().setDataScaleType(QCPAxis.stLogarithmic)

    y_axis.setScaleType(QCPAxis.stLogarithmic)
    y_axis.setTicker(QCPAxisTickerLog())
    y_axis.setVisible(True)

    scale = QCPColorGradient(QCPColorGradient.gpJet)
    scale.setNanHandling(QCPColorGradient.nhTransparent)
    colormap.colorMap().setGradient(scale)
    colormap.colorMap().addToLegend()

    x=np.arange(3000, dtype=np.float64)
    y=np.logspace(1, 4, 64)
    if time_axis:
        x+=datetime.now().timestamp()
    z = np.random.rand(3000,64)+np.cos(np.arange(3000*64, dtype=np.float64)/6000.).reshape(3000,64)
    colormap.setData(x,y,z)

    return color_scale, colormap


class SimpleGraph(QWidget):
    def __init__(self,parent):
        QWidget.__init__(self,parent)
        self.setMouseTracking(True)
        self.plot = make_plot(self)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(self.plot)
        self.graph = add_graph(self.plot, time_axis=False, offset=2e4)

class SimpleCurve(QWidget):
    def __init__(self,parent):
        QWidget.__init__(self,parent)
        self.setMouseTracking(True)
        self.plot = make_plot(self)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(self.plot)
        self.curve = add_curve(self.plot)

class TimeSerieGraph(QWidget):
    def __init__(self,parent):
        QWidget.__init__(self,parent)
        self.setMouseTracking(True)
        self.plot = make_plot(self, time_axis=True)
        self.setLayout(QVBoxLayout())
        self.layout().addWidget(self.plot)

        self.graph = add_graph(self.plot, time_axis=True)

        x_range = self.plot.xAxis.range()

        middle = x_range.center()
        width = x_range.size()

        self._verticalSpan = SciQLopVerticalSpan(self.plot, QCPRange(middle-width/10, middle+width/10), QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")

        self._ro_verticalSpan = SciQLopVerticalSpan(self.plot, QCPRange(middle+width/20, middle+width/10), QColor(200, 100, 100, 100), read_only=True, visible=True, tool_tip="Vertical Span")



class StackedPlots(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent)
        self.setMouseTracking(True)
        self.graphs = []
        for _ in range(3):
            plot = make_plot(None, time_axis=True)
            self.addPlot(plot)
            self.graphs.append(add_graph(plot, time_axis=True))

        self.color_scale, self.colormap = add_colormap(self.plots()[-1], time_axis=True)

        x_range = self.plotAt(0).xAxis.range()

        middle = x_range.center()
        width = x_range.size()

        self._verticalSpan = MultiPlotsVerticalSpan(self, QCPRange(middle-width/10, middle+width/10), QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")




def fix_name(name):
    return name.replace(" ", "_").replace(":", "_").replace("/", "_")

class Tabs(QTabWidget):
    def __init__(self,parent):
        QTabWidget.__init__(self,parent)
        self.setMouseTracking(True)
        self.setTabPosition(QTabWidget.TabPosition.North)
        self.setTabShape(QTabWidget.TabShape.Rounded)
        self.setMovable(True)
        self._objects_to_export ={}
        self.add_tab(SimpleGraph(self), "Simple Graph")
        self.add_tab(SimpleCurve(self), "Simple Curve")
        self.add_tab(TimeSerieGraph(self), "Time Serie Graph")
        self.add_tab(StackedPlots(self), "Stacked Plots")

    def add_tab(self, widget, title):
        self.addTab(widget, title)
        self._objects_to_export[fix_name(title)] = widget

    def export_objects(self):
        return self._objects_to_export






class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setMouseTracking(True)
        self.axisRects=[]
        self.graphs = []
        self._setup_kernel()
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390);
        self.setWindowTitle("SciQLopPlots Test")
        self.add_to_kernel_namespace(self, 'main_window')
        for name, widget in self.tabs.export_objects().items():
            self.add_to_kernel_namespace(widget, name)
        self.add_to_kernel_namespace(make_plot, 'make_plot')
        self.add_to_kernel_namespace(add_graph, 'add_graph')
        self.add_to_kernel_namespace(add_curve, 'add_curve')
        self.add_to_kernel_namespace(add_colormap, 'add_colormap')
        self.add_to_kernel_namespace(butterfly, 'butterfly')


    def _setup_kernel(self):
        self.kernel_manager = QtInProcessKernelManager()
        self.kernel_manager.start_kernel(show_banner=False)
        self.kernel = self.kernel_manager.kernel
        self.kernel.gui = 'qt'

        self.kernel_client = self.kernel_manager.client()
        self.kernel_client.start_channels()

    def add_to_kernel_namespace(self, obj, name):
        self.kernel.shell.push({name: obj})

    def _setup_ui(self):
        self.tabs = Tabs(self)
        self.setCentralWidget(self.tabs)

        self.ipython_widget = RichJupyterWidget(self)
        dock = QDockWidget("IPython Console", self)
        dock.setWidget(self.ipython_widget)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock)
        self.ipython_widget.kernel_manager = self.kernel_manager
        self.ipython_widget.kernel_client = self.kernel_client


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
