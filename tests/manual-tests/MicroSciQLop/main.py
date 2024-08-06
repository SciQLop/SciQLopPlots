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

os.environ['QT_API'] = 'PySide6'

try:
    import speasy as spz
except ImportError:
    print("Speasy not found")
    exit()

class GraphDataProvider(DataProviderInterface):
    def __init__(self):
        super().__init__()

    def get_data(self, start, end):
        x = np.arange(start, end, dtype=np.float64)
        print(len(x))
        y = np.cos(np.array([x/6.,x/60.,x/600.])) * np.cos(np.array([x,x/6.,x/60.])) * [[100.],[1300],[17000]]
        return x, y


class MMS(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent)
        self.graphs = []
        for _ in range(3):
            plot = SciQLopTimeSeriesPlot(None)
            self.addPlot(plot)
        self._worker = DataProviderWorker()
        p = GraphDataProvider()
        self._worker.set_data_provider(p)
        self.plotAt(0).x_axis_range_changed.connect(self._worker.set_range)
        p.new_data_2d.connect(self.graphs[0].setData)





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
        self.add_to_kernel_namespace(spz, 'spz')

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
