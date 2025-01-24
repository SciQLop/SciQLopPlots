from SciQLopPlots import SciQLopPlot, MultiPlotsVerticalSpan,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, GraphType, PlotType, AxisType, SciQLopPlotRange, PlotsModel, InspectorView
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget, QTreeView
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
from PySide6.QtCore import Qt, QTimer, QObject, QThread, Signal
import sys, os
import math
import numpy as np
from datetime import datetime
from types import SimpleNamespace
import humanize

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager

os.environ['QT_API'] = 'PySide6'

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow



def fft(x, y, fft_size=2**12):
    if x is None or y is None:
        return np.array([]), np.array([])
    try:
        freq = np.fft.rfftfreq(fft_size, x[1]-x[0])
        spec = np.zeros(len(freq), dtype='complex128')
        han = np.hanning(fft_size)
        nw = len(y)//fft_size
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
        print(f"line: {sys.exc_info()[-1].tb_lineno}")


class RealTime(SciQLopMultiPlotPanel):
    class DataProducer(QThread):
        update_signal = Signal(object, object)
        def __init__(self,parent=None, size=2**10):
            super().__init__(parent)
            self.moveToThread(self)
            self._gen_size = size * 16
            gen_x = np.arange(self._gen_size, dtype=np.float64)
            self._size = size
            self._x = np.arange(size, dtype=np.float64)
            noise = np.random.rand(self._gen_size)*20.
            self._base = np.cos(gen_x/180.+noise)  * (np.cos(gen_x**2/18.**2+noise) * np.cos(gen_x/90.) * 100. + np.sin(gen_x/50.) * 50. +noise)
            self._index = 0
            self.start()

        def run(self):
            while True:
                self._index += 5
                self._index %= (len(self._base)-self._size)
                self.update_signal.emit(self._x, self._base[self._index:self._index+self._size])
                self.msleep(1)

    class Spectrogram:
        def __init__(self, size, fft_size, roll=True):
            sample_rate = 1.
            self._size = size
            self._x = np.arange(size, dtype=np.float64)
            self._freq = np.fft.rfftfreq(fft_size, sample_rate)
            self._z = np.zeros((size,len(self._freq)), dtype=np.float64)
            self._last_x = len(self._z)-1
            self._roll = roll

        def __call__(self, x, y, *args):
            try:
                self._z[self._last_x] = y
                if self._roll:
                    self._z = np.roll(self._z, -1, axis=0)
                else:
                    self._last_x = (self._last_x + 1) % self._size
                return self._x, self._freq, self._z
            except Exception as e:
                print(f"Error: {e}")
                print(f"line: {sys.exc_info()[-1].tb_lineno}")
                return None

    def _update_data(self, x, y):
        self._graph.set_data(x, y)

    def __init__(self, parent):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        (self._plot,self._graph) = self.plot(np.arange(10)*1., np.arange(10)*1.,labels=["Audio"], colors=[QColorConstants.Blue])
        size = 2**10
        plot, self._fft_graph = self.plot(lambda x,y:fft(x,y,size),index=0,labels=["FFT"], colors=[QColorConstants.Red], sync_with=self._graph)
        plot.x_axis().set_log(True)
        plot.x_axis().set_range(0.01, 1)
        plot.y_axis().set_log(True)
        plot.y_axis().set_range(1., 1e-4)
        self._spec = RealTime.Spectrogram(500,size)
        plot, _ = self.plot(self._spec, name="Spectrogram",graph_type=GraphType.ColorMap, plot_type=PlotType.BasicXY, sync_with=self._fft_graph,y_log_scale=False,z_log_scale=True)
        self._data_producer = RealTime.DataProducer(size=size)

        self._data_producer.update_signal.connect(self._update_data, Qt.QueuedConnection)



if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(RealTime(w), "RealTime")
    w.show()
    app.exec()
