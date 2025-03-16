from SciQLopPlots import (SciQLopPlot, MultiPlotsVerticalSpan,SciQLopMultiPlotPanel, SciQLopVerticalSpan,
                         SciQLopTimeSeriesPlot, GraphType, PlotType, AxisType, SciQLopPlotRange, PlotsModel,
                         InspectorView, SciQLopPixmapItem, SciQLopEllipseItem,
                         Coordinates)
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget, QTreeView
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush, QPixmap
from PySide6.QtCore import Qt, QTimer, QObject, QThread, Signal, QRectF
import sys, os
import math
import numpy as np
from datetime import datetime
from types import SimpleNamespace
import humanize

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager

try:
    from numba import njit, prange
    NUMBA_AVAILABLE = True
except ImportError:
    NUMBA_AVAILABLE = False

os.environ['QT_API'] = 'PySide6'

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow


NPOINTS = 300000

def make_plot(parent, time_axis=False):
    if time_axis:
        plot = SciQLopTimeSeriesPlot(parent)
        plot.x_axis().set_range(datetime.now().timestamp(), datetime.now().timestamp()+NPOINTS)
    else:
        plot = SciQLopPlot(parent)
        plot.x_axis().set_range(SciQLopPlotRange(0, NPOINTS))
    plot.enable_legend(True)
    plot.minimize_margins()
    return plot

def add_graph(plot, time_axis=False, offset=0.):
    r = plot.x_axis().range()
    x=np.arange(start=r.start(), stop=r.stop(), dtype=np.float64)
    y=np.cos(np.array([x/6.,x/60.,x/600.])) * np.cos(np.array([x,x/6.,x/60.])) * [[100.],[1300],[17000]]
    y+=offset
    y=np.array(y.T, copy=True)
    return plot.plot(x,y, labels=["X","Y","Z"], colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green])

def butterfly():
    t = np.linspace(0, 12*np.pi, 5000)
    x = np.sin(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    y = np.cos(t)*(np.exp(np.cos(t)) - 2*np.cos(4*t) - np.sin(t/12)**5)
    return x, y

def add_curve(plot):
    x, y = butterfly()
    plot.x_axis().set_range(min(x), max(x))
    plot.y_axis().set_range(min(y), max(y))
    return plot.plot(x,y, labels=["butterfly"], colors=[QColorConstants.Red], graph_type=GraphType.ParametricCurve)

def add_colormap(plot, time_axis=False):
    x=np.arange(NPOINTS, dtype=np.float64)
    y=np.logspace(1, 4, 64)
    if time_axis:
        x+=datetime.now().timestamp()

    v_mod = np.tile(np.cos(np.arange(64)*6.28/(64*4)),NPOINTS).reshape(NPOINTS,64)
    noise = np.random.rand(NPOINTS*64).reshape(NPOINTS,64)
    h_mod = np.cos(np.arange(NPOINTS*64, dtype=np.float64)*6.28/(NPOINTS/3.1)).reshape(NPOINTS,64)
    sig = np.cos(np.arange(NPOINTS*64, dtype=np.float64)*6.28/(NPOINTS*10)).reshape(NPOINTS,64)
    z = (v_mod * sig * h_mod + noise)
    z = (z*sig)/(z + 1)
    return plot.plot(x,y,z, name = "Cmap", y_log_scale=True, z_log_scale=True)


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
        self.pix = SciQLopPixmapItem(self.plot, QPixmap("://icons/SciQLop.png"), QRectF(-1, -1, 2, 2), True, Coordinates.Data)
        self.ellipse = SciQLopEllipseItem(self.plot, QRectF(-1, -1, 2, 2), QPen(Qt.red), QBrush(Qt.green), True, Coordinates.Data)
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

        self._verticalSpan = SciQLopVerticalSpan(self.plot, x_range/10, QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")

        self._ro_verticalSpan = SciQLopVerticalSpan(self.plot, x_range/20 + 100, QColor(200, 100, 100, 100), read_only=True, visible=True, tool_tip="Vertical Span")



class StackedPlots(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent)
        self.setMouseTracking(True)
        self.graphs = []
        for _ in range(5):
            plot = make_plot(None, time_axis=True)
            self.add_plot(plot)
            self.graphs.append(add_graph(plot, time_axis=True))

        self.cmap = add_colormap(self.plots()[-1], time_axis=True)

        x_range = self.plot_at(0).x_axis().range()

        self._verticalSpan = MultiPlotsVerticalSpan(self, x_range/10, QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="Vertical Span")

if NUMBA_AVAILABLE:
    @njit(parallel=True)
    def _make_data(x,y):
        for i in prange(len(x)):
            y[i,0] = (math.cos(x[i]*(1./3.)) * math.cos(x[i]) +  np.random.rand() )* 100.
            y[i,1] = (math.cos(x[i]*(1./3.)) * math.cos(x[i]*(1./2.)) +  np.random.rand()) * 1300.
            y[i,2] = (math.cos(x[i]*(1./30.)) * math.cos(x[i]*(1./3.)) + math.cos(x[i]*(1./30.)) +  np.random.rand()) * 17000.
        return x, y

    def make_data( start, end):
        x = np.arange(start, end, dtype=np.float64)
        y = np.empty((len(x),3), dtype=np.float64)
        if len(x) > 100000:
            print(f"Using Numba to generate {humanize.metric(len(x))} points")
        return _make_data(x, y)


else:

    def make_data( start, end):
        x = np.arange(start, end, dtype=np.float64)
        y = np.empty((len(x), 3), dtype=np.float64)
        y[:,0] = np.cos(x*(1./3.)) * np.cos(x) * 100.
        y[:,1] = np.cos(x*(1./30.)) * np.cos(x*(1./3.)) * 1300.
        y[:,2] = np.cos(x*(1./300.)) * np.cos(x*(1./30.)) * 17000.
        return x, y



class DataProducers(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        for _ in range(3):
            self.plot(make_data,labels=["X","Y","Z"], colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green], plot_type=PlotType.TimeSeries)
        plot = self.create_plot(-1, PlotType.TimeSeries)
        plot.plot(make_data,labels=["X","Y","Z"], colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green], sync_with=plot.y_axis())


def fft(x, y, fft_size=2**12):
    if x is None or y is None:
        return np.array([]), np.array([])
    y = y[:, -1]
    freq = np.fft.rfftfreq(fft_size, x[1]-x[0])
    spec = np.zeros(len(freq), dtype='complex128')
    han = np.hanning(fft_size)
    nw = len(y)//fft_size
    try:
        if nw != 0:
            for i in range(nw):
                y_w = y[i*fft_size:(i+1)*fft_size]
                y_w = y_w-np.mean(y_w)
                y_w = y_w*han
                spec += (np.fft.rfft(y_w)/len(y_w))**2
            spec = np.abs(np.sqrt(spec/nw))
            return freq, spec
    except Exception as e:
        print(f"Error: {e}")

class TsAndFFT(SciQLopMultiPlotPanel):
    def __init__(self,parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        _, self.graph = self.plot(make_data,labels=["X","Y","Z"], colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green], plot_type=PlotType.TimeSeries)
        plot, _ = self.plot(fft,index=0,labels=["FFT"], colors=[QColorConstants.Red], sync_with=self.graph)
        plot.x_axis().set_log(True)
        plot.x_axis().set_range(0.01, 1)
        plot.y_axis().set_log(True)
        plot.y_axis().set_range(1., 1e-4)



class RealTimeAudio(SciQLopMultiPlotPanel):
    class AudioDataProducer(QThread):
        update_signal = Signal(object, object)
        def __init__(self,parent=None, size=2**10, sample_rate=48000):
            super().__init__(parent)
            self.moveToThread(self)
            try:
                import sounddevice as sd
                self._sd = sd
                sd.default.samplerate = sample_rate
                sd.default.channels = 1
                #sd.default.device = 'pipewire'
                self._size = size
                self.x = np.arange(self._size, dtype=np.float64)
                self.index = 0
                self.start()
            except ImportError:
                print("Sounddevice not found")

        def audio_callback(self, indata, frames, time, status):
            if status:
                print(status)
            if len(indata) == len(self.x):
                self.update_signal.emit(self.x, indata.astype(np.float64))


        def run(self):
            stream = self._sd.InputStream(callback=self.audio_callback, blocksize=self._size)
            with stream:
                while True:
                    self.sleep(1)

    class AudioSpectrogram:
        def __init__(self, size, fft_size, sample_rate, roll=True):
            self._size = size
            self._x = np.arange(size, dtype=np.float64)
            self._y = np.fft.rfftfreq(fft_size, 1./sample_rate)
            self._cutof = np.where(self._y>20000)[0][0]
            self._y = self._y[:self._cutof]
            self._z = np.zeros((size,len(self._y)), dtype=np.float64)
            self._last_x = len(self._z)-1
            self._roll = roll

        def __call__(self, x, y, *args):
            try:
                self._z[self._last_x] = y[:self._cutof]
                if self._roll:
                    self._z = np.roll(self._z, -1, axis=0)
                else:
                    self._last_x = (self._last_x + 1) % self._size
                return self._x, self._y, self._z
            except Exception as e:
                print(f"Error: {e}")
                return None

    def _update_data(self, x, y):
        self._graph.set_data(x, y)

    def __init__(self, parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        (self._plot,self._graph) = self.plot(np.arange(10)*1., np.arange(10)*1.,labels=["Audio"], colors=[QColorConstants.Blue])
        size = 2**12
        sample_rate = 192000
        plot, self._fft_graph = self.plot(lambda x,y:fft(x,y,size),index=0,labels=["FFT"], colors=[QColorConstants.Red], sync_with=self._graph)
        plot.x_axis().set_log(True)
        plot.x_axis().set_range(0.01, 1)
        plot.y_axis().set_log(True)
        plot.y_axis().set_range(1., 1e-4)
        self._audio_spec = RealTimeAudio.AudioSpectrogram(300,size,sample_rate)
        plot, _ = self.plot(self._audio_spec, name="Spectrogram",graph_type=GraphType.ColorMap, plot_type=PlotType.BasicXY, sync_with=self._fft_graph,y_log_scale=False,z_log_scale=True)
        self._data_producer = RealTimeAudio.AudioDataProducer(size=size, sample_rate=sample_rate)

        self._data_producer.update_signal.connect(self._update_data)



if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(SimpleGraph(w), "Simple Graph")
    w.add_tab(SimpleCurve(w), "Simple Curve")
    w.add_tab(TimeSerieGraph(w), "Time Serie Graph")
    w.add_tab(StackedPlots(w), "Stacked Plots")
    w.add_tab(DataProducers(w), "DataProducers")
    w.add_tab(TsAndFFT(w), "TsAndFFT")
    w.add_tab(RealTimeAudio(w), "RealTimeAudio")
    w.show()
    app.exec()
