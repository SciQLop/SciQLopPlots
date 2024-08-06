from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, \
                         QCPLegend, QCPColorGradient, QCPMarginGroup, QCPAxisRect,QCPAxisTickerDateTime, \
                         MultiPlotsVerticalSpan, QCPAxisTickerLog,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, DataProviderInterface, DataProviderWorker
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

try:
    from numba import njit, prange
    NUMBA_AVAILABLE = True
except ImportError:
    NUMBA_AVAILABLE = False

os.environ['QT_API'] = 'PySide6'

NPOINTS = 30000

def make_plot(parent, time_axis=False):
    if time_axis:
        plot = SciQLopTimeSeriesPlot(parent)
    else:
        plot = SciQLopPlot(parent)
    plot.enable_legend(True)
    plot.minimize_margins()
    return plot

def add_graph(plot, time_axis=False, offset=0.):
    graph = plot.addSciQLopGraph(["X","Y","Z"])
    x=np.arange(NPOINTS, dtype=np.float64)
    y=np.cos(np.array([x/6.,x/60.,x/600.])) * np.cos(np.array([x,x/6.,x/60.])) * [[100.],[1300],[17000]]
    y+=offset
    if time_axis:
        x+=datetime.now().timestamp()
    graph.set_data(x,y)

    graph.graphAt(0).setPen(QPen(QColorConstants.Red))
    graph.graphAt(1).setPen(QPen(QColorConstants.Blue))
    graph.graphAt(2).setPen(QPen(QColorConstants.Green))

    plot.x_axis().set_range(x[0],x[-1])
    plot.y_axis().set_range(1.2*np.min(y),1.2*np.max(y))
    return graph

def butterfly():
    t = np.linspace(0, 12*np.pi, 5000)
    x = np.sin(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    y = np.cos(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    return x, y

def add_curve(plot):
    curve = plot.addSciQLopCurve(["butterfly"])
    curve.set_data(*butterfly())
    return curve

def add_colormap(plot, time_axis=False):
    colormap = plot.addSciQLopColorMap("Cmap", y_log_scale=True, z_log_scale=True)

    x=np.arange(NPOINTS, dtype=np.float64)
    y=np.logspace(1, 4, 64)
    if time_axis:
        x+=datetime.now().timestamp()
    z = np.random.rand(NPOINTS,64)+np.cos(np.arange(NPOINTS*64, dtype=np.float64)/6000.).reshape(NPOINTS,64)
    colormap.set_data(x,y,z)
    return colormap


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

        x_range = self.plot.x_axis().range()

        middle =( x_range[0] + x_range[1]) / 2
        width = x_range[1] - x_range[0]

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

        self.cmap = add_colormap(self.plots()[-1], time_axis=True)

        x_range = self.plotAt(0).x_axis().range()

        middle = (x_range[0] + x_range[1]) / 2
        width = x_range[1] - x_range[0]

        self._verticalSpan = MultiPlotsVerticalSpan(self, QCPRange(middle-width/10, middle+width/10), QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")

if NUMBA_AVAILABLE:
    @njit(parallel=True)
    def _make_data(x,y):
        for i in prange(len(x)):
            y[0,i] = math.cos(x[i]*(1./6.)) * math.cos(x[i]) * 100.
            y[1,i] = math.cos(x[i]*(1./60.)) * math.cos(x[i]*(1./6.)) * 1300.
            y[2,i] = math.cos(x[i]*(1./600.)) * math.cos(x[i]*(1./60.)) * 17000.
        return x, y

    class GraphDataProvider(DataProviderInterface):
        def __init__(self):
            super().__init__()


        def get_data(self, start, end):
            x = np.arange(start, end, dtype=np.float64)
            y = np.empty((3, len(x)), dtype=np.float64)
            return _make_data(x, y)


else:

    class GraphDataProvider(DataProviderInterface):
        def __init__(self):
            super().__init__()

        def get_data(self, start, end):
            x = np.arange(start, end, dtype=np.float64)
            y = np.empty((3, len(x)), dtype=np.float64)
            y[0,:] = np.cos(x*(1./6.)) * np.cos(x) * 100.
            y[1,:] = np.cos(x*(1./60.)) * np.cos(x*(1./6.)) * 1300.
            y[2,:] = np.cos(x*(1./600.)) * np.cos(x*(1./60.)) * 17000.
            return x, y



class DataProducers(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent)
        self.setMouseTracking(True)
        self.graphs = []
        self._workers = []
        self._providers = []
        for _ in range(3):
            plot = make_plot(None, time_axis=True)
            self.addPlot(plot)
            self.graphs.append(add_graph(plot, time_axis=True))
            self._providers.append(GraphDataProvider())
            self._workers.append(DataProviderWorker())
            self._workers[-1].set_data_provider(self._providers[-1])
            self.plots()[-1].x_axis_range_changed.connect(self._workers[-1].set_range)
            self._providers[-1].new_data_2d.connect(self.graphs[-1].set_data)



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
        self.add_tab(DataProducers(self), "DataProducers")

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
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
