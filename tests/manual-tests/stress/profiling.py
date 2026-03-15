"""Real-time data producer with FFT and spectrogram for profiling.

Showcases:
- QThread data producer with signal-based updates
- Real-time FFT spectrum
- Rolling spectrogram colormap
"""
import sys
import os
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt, QThread, Signal

from SciQLopPlots import (
    SciQLopMultiPlotPanel, GraphType, PlotType,
)


def fft(x, y, fft_size=2**12):
    if x is None or y is None:
        return np.array([]), np.array([])
    try:
        freq = np.fft.rfftfreq(fft_size, x[1] - x[0])
        spec = np.zeros(len(freq), dtype="complex128")
        han = np.hanning(fft_size)
        nw = len(y) // fft_size
        if nw != 0:
            for i in range(nw):
                y_w = y[i * fft_size : (i + 1) * fft_size]
                y_w = y_w - np.mean(y_w)
                y_w = y_w * han
                spec += (np.fft.rfft(y_w) / len(y_w)) ** 2
            spec = np.abs(np.sqrt(spec / nw))
            return freq, spec
    except Exception as e:
        print(f"Error: {e}")


class DataProducer(QThread):
    update_signal = Signal(object, object)

    def __init__(self, parent=None, size=2**10):
        super().__init__(parent)
        self.moveToThread(self)
        gen_size = size * 16
        gen_x = np.arange(gen_size, dtype=np.float64)
        self._size = size
        self._x = np.arange(size, dtype=np.float64)
        noise = np.random.rand(gen_size) * 20.0
        self._base = (
            np.cos(gen_x / 180.0 + noise)
            * (np.cos(gen_x**2 / 18.0**2 + noise) * np.cos(gen_x / 90.0) * 100.0
               + np.sin(gen_x / 50.0) * 50.0 + noise)
        )
        self._index = 0
        self.start()

    def run(self):
        while True:
            self._index += 5
            self._index %= len(self._base) - self._size
            self.update_signal.emit(
                self._x, self._base[self._index : self._index + self._size]
            )
            self.msleep(1)


class Spectrogram:
    def __init__(self, size, fft_size, roll=True):
        self._size = size
        self._x = np.arange(size, dtype=np.float64)
        self._freq = np.fft.rfftfreq(fft_size, 1.0)
        self._z = np.zeros((size, len(self._freq)), dtype=np.float64)
        self._last_x = len(self._z) - 1
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
            return None


class RealTimePanel(SciQLopMultiPlotPanel):
    def __init__(self, parent=None, with_spectro=True):
        super().__init__(parent, synchronize_x=False, synchronize_time=True)
        self._plot, self._graph = self.plot(
            np.arange(10) * 1.0, np.arange(10) * 1.0,
            labels=["Signal"], colors=[QColorConstants.Blue],
        )
        size = 2**14
        plot, self._fft_graph = self.plot(
            lambda x, y: fft(x, y, size), index=0,
            labels=["FFT"], colors=[QColorConstants.Red],
            sync_with=self._graph,
        )
        plot.x_axis().set_log(True)
        plot.x_axis().set_range(0.01, 1)
        plot.y_axis().set_log(True)
        plot.y_axis().set_range(1.0, 1e-4)
        if with_spectro:
            self._spec = Spectrogram(500, size)
            self.plot(
                self._spec, name="Spectrogram",
                graph_type=GraphType.ColorMap,
                plot_type=PlotType.BasicXY,
                sync_with=self._fft_graph,
                y_log_scale=False, z_log_scale=True,
            )
        self._producer = DataProducer(size=size)
        self._producer.update_signal.connect(
            self._graph.set_data, Qt.QueuedConnection,
        )


app = QApplication(sys.argv)

w = QMainWindow()
w.setWindowTitle("Profiling Demo — Real-time FFT + Spectrogram")
w.setCentralWidget(RealTimePanel(w))
w.resize(1000, 700)
w.show()
sys.exit(app.exec())
