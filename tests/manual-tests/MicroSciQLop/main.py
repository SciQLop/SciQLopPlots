from SciQLopPlots import SciQLopPlot, QCP, QCPColorMap, QCPRange, QCPColorScale, QCPAxis, \
                         MultiPlotsVerticalSpan ,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, DataOrder, PlotType
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt
import sys, os
import numpy as np
from datetime import datetime, timezone
import threading
from typing import Dict, Any

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager

os.environ['QT_API'] = 'PySide6'

try:
    import speasy as spz
    def _current_thread_id():
        return threading.get_native_id()


    class ThreadStorage:
        _storage: Dict[int, Dict[str, Any]] = {}

        def __init__(self):
            self._storage = {}

        def __getattr__(self, item):
            return self._storage.get(_current_thread_id(), {}).get(item, None)

        def __setattr__(self, key, value):
            tid = _current_thread_id()
            storage = self._storage
            if tid not in storage:
                storage[tid] = {}
            storage[tid][key] = value

    from speasy.core.cache import _cache
    _cache._data._local = ThreadStorage()

except ImportError:
    print("Speasy not found")
    exit()


def spz_get_data(start, stop):
    try:
        v=spz.get_data(spz.inventories.tree.cda.MMS.MMS1.FGM.MMS1_FGM_SRVY_L2.mms1_fgm_b_gse_srvy_l2, start, stop)
        if v is None:
            return None
        x=(v.time.astype(np.int64)/1e9).astype(np.float64)
        return x, v.values.astype(np.float64)
    except Exception as e:
        print(f"Error: {e}")
        return None


class MMS(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent)
        self.graphs = []
        for _ in range(3):
            self.plot(spz_get_data,
                        labels=['Bx GSE', 'By GSE', 'Bz GSE', 'Bt'],
                        colors=[QColorConstants.Red, QColorConstants.Green, QColorConstants.Blue, QColorConstants.Black],
                        data_order=DataOrder.ColumnMajor,
                        plot_type=PlotType.TimeSeries)
        self.set_x_axis_range(
                datetime(2019,2,17,12,33,0,0,timezone.utc).timestamp(),
                datetime(2019,2,17,12,34,0,0,timezone.utc).timestamp())


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
        self.add_tab(MMS(self), "MMS")


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
    from speasy.core.cache import _cache
    _cache._data._local = ThreadStorage()
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    app.exec()
