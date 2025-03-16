from SciQLopPlots import (SciQLopPlot,
                         MultiPlotsVerticalSpan ,SciQLopMultiPlotPanel,
                         SciQLopVerticalSpan, SciQLopTimeSeriesPlot,
                         PlotType, AxisType, GraphType, SciQLopVerticalLine,
                         SciQLopHorizontalLine)
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
    from speasy.core import datetime64_to_epoch

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


def mms1_fgm_b_gse_srvy_l2(start, stop):
    try:
        v = spz.get_data(
                        spz.inventories.tree.cda.MMS.MMS1.FGM.MMS1_FGM_SRVY_L2.mms1_fgm_b_gse_srvy_l2,
                        start, stop)
        if v is None:
            return None
        v = resample(v, 1./4)
        x = datetime64_to_epoch(v.time)
        return x, v.values.astype(np.float64)
    except Exception as e:
        print(f"Error: {e}")
        return None

def speasy_spectro(product, start, stop, invert_y=False):
    try:
        v=spz.get_data(product, start, stop)
        if v is None:
            return None
        x=datetime64_to_epoch(v.time)
        if invert_y:
            return x, v.axes[1].values.astype(np.float64)[::-1].copy() ,v.values.astype(np.float64)[:,::-1].copy()
        return x, v.axes[1].values.astype(np.float64) ,v.values.astype(np.float64)
    except Exception as e:
        print(f"Error: {e}")
        return None

def mms1_edp_hmfe_dsl_brst_l2(start, stop):
    try:
        v=spz.get_data(spz.inventories.tree.cda.MMS.MMS1.ADP_SDP.MMS1_EDP_BRST_L2_HMFE.mms1_edp_hmfe_dsl_brst_l2, start, stop)
        if v is None:
            return None
        x=datetime64_to_epoch(v.time)
        return x, v.values.astype(np.float64)
    except Exception as e:
        print(f"Error: {e}")
        return None

class Spectrum:
    def __init__(self, fft_size):
        self._fft_size = fft_size

    def __call__(self, x, y):
        try:
            if x is None or y is None:
                return np.array([]), np.array([])
            y = y[:, -1]
            spec = np.zeros(self._fft_size, dtype='complex128')
            han = np.hanning(self._fft_size)
            nw = len(y)//self._fft_size
            if nw != 0:
                for i in range(nw):
                    y_w = y[i*self._fft_size:(i+1)*self._fft_size]
                    y_w = y_w-np.mean(y_w)
                    y_w = y_w*han
                    spec += (np.fft.fft(y_w)/len(y_w))**2
                spec = np.abs(np.sqrt(spec/nw))
                freq = np.fft.fftfreq(len(spec), x[1]-x[0])[1:len(spec)//2]
                spec = spec[1:len(spec)//2]
                return freq, spec
        except Exception as e:
            print(f"Error: {e}")
            return None

class MMS(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        self.plot(lambda start, stop: speasy_spectro('cda/MMS1_FPI_FAST_L2_DIS-MOMS/mms1_dis_energyspectr_omni_fast', start, stop),
                  name="mms1_dis_energyspectr_omni_fast",
                  graph_type=GraphType.ColorMap,
                  plot_type=PlotType.TimeSeries,
                  y_log_scale=True,
                  z_log_scale=True)
        _, self.graph = self.plot(mms1_fgm_b_gse_srvy_l2,
                             labels=['Bx GSE', 'By GSE', 'Bz GSE', 'Bt'],
                             colors=[QColorConstants.Red, QColorConstants.Green, QColorConstants.Blue, QColorConstants.Black],
                             plot_type=PlotType.TimeSeries)
        self._fft = Spectrum(2**9)
        p,g=self.plot(self._fft, index=1, labels=['Spectrum'], colors=[QColorConstants.Red], plot_type=PlotType.BasicXY, sync_with=self.graph)
        p.x_axis().set_log(True)
        p.x_axis().set_range(0.1, 2)
        p.y_axis().set_log(True)
        p.y_axis().set_range(1., 1e-4)
        self._g = g
        self.set_time_axis_range(datetime(2019,2,17,12,33,0,0,timezone.utc), datetime(2019,2,17,12,34,0,0,timezone.utc))

class MMS_Spectro_Only(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        self.plot(lambda start, stop: speasy_spectro('cda/MMS1_FPI_FAST_L2_DIS-MOMS/mms1_dis_energyspectr_omni_fast', start, stop),
                  name="mms1_dis_energyspectr_omni_fast",
                  graph_type=GraphType.ColorMap,
                  plot_type=PlotType.TimeSeries,
                  y_log_scale=True,
                  z_log_scale=True)
        self.plot(lambda start, stop: speasy_spectro('cda/MMS1_FPI_BRST_L2_DIS-MOMS/mms1_dis_energyspectr_omni_brst', start, stop),
                  name="mms1_dis_energyspectr_omni_brst",
                  graph_type=GraphType.ColorMap,
                  plot_type=PlotType.TimeSeries,
                  y_log_scale=True,
                  z_log_scale=True)
        self.set_time_axis_range(datetime(2019,2,17,12,33,0,0,timezone.utc), datetime(2019,2,17,12,34,0,0,timezone.utc))

class MMS_edp_hmfe(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        _, graph = self.plot(mms1_edp_hmfe_dsl_brst_l2,
                             labels=['Ex', 'Ey', 'Ez'],
                             colors=[QColorConstants.Red, QColorConstants.Green, QColorConstants.Blue],
                             plot_type=PlotType.TimeSeries)

        self.set_time_axis_range(datetime(2023,10,20,7,0,0,0,timezone.utc), datetime(2023,10,20,9,0,0,0,timezone.utc))

class Maven_swia_spectra_diff_en_fluxes(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        self.plot(lambda start, stop: speasy_spectro('cda/MVN_SWI_L2_ONBOARDSVYSPEC/spectra_diff_en_fluxes', start, stop, True),
                  name="spectra_diff_en_fluxes",
                  graph_type=GraphType.ColorMap,
                  plot_type=PlotType.TimeSeries,
                  y_log_scale=True,
                  z_log_scale=True)

        self.set_time_axis_range(datetime(2022,2,13,0,0,0,0,timezone.utc), datetime(2022,2,14,0,0,0,0,timezone.utc))

if __name__ == '__main__':
    from speasy.core.cache import _cache
    _cache._data._local = ThreadStorage()
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(MMS(w), "MMS")
    w.add_tab(MMS_Spectro_Only(w), "MMS Spectro Only")
    w.add_tab(MMS_edp_hmfe(w), "MMS EDP HMFE")
    w.add_tab(Maven_swia_spectra_diff_en_fluxes(w), "Maven SWIA Spectra Diff En Fluxes")
    w.show()
    app.exec()
