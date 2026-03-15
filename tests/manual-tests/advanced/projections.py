from SciQLopPlots import SciQLopPlot, \
                         MultiPlotsVerticalSpan ,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, PlotType, AxisType, GraphType, SciQLopNDProjectionPlot
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt
import sys, os
import numpy as np
from datetime import datetime, timezone
import threading
from typing import Dict, Any

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow

os.environ['QT_API'] = 'PySide6'

try:
    import speasy as spz
    from speasy.signal.resampling import resample
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


def spc_pos_gse(spacecraft, start, stop):
    try:
        v=spz.get_data(f"amda/{spacecraft}_xyz_gse", start, stop)
        if v is None:
            return None
        t=(v.time.astype(np.int64)/1e9).astype(np.float64)
        x = v.values[:,0].astype(np.float64)
        y = v.values[:,1].astype(np.float64)
        z = v.values[:,2].astype(np.float64)
        return t, x, y, z
    except Exception as e:
        print(f"Error: {e}")
        return None

class MMS(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        proj,_ = self.plot(lambda start,stop: spc_pos_gse("mms1", start, stop), labels=['mms1']*3, graph_type=GraphType.ParametricCurve,
                            plot_type=PlotType.Projections)
        for i in range(2,4):
            proj.plot(lambda start,stop: spc_pos_gse(f"mms{i+1}", start, stop), labels=[f'mms{i+1}']*3, graph_type=GraphType.ParametricCurve)
        for i in range(4):
            proj.plot(lambda start,stop: spc_pos_gse(f"c{i+1}", start, stop), labels=[f'c{i+1}']*3, graph_type=GraphType.ParametricCurve)
        self.set_time_axis_range(datetime(2019,2,17,12,33,0,0,timezone.utc), datetime(2019,2,17,12,34,0,0,timezone.utc))



if __name__ == '__main__':
    from speasy.core.cache import _cache
    _cache._data._local = ThreadStorage()
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(MMS(w), "MMS")
    w.show()
    app.exec()
