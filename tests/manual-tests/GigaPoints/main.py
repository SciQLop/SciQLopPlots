import SciQLopPlots
from SciQLopPlots import SciQLopPlot, MultiPlotsVerticalSpan,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, GraphType, PlotType, AxisType, SciQLopPlotRange, PlotsModel, InspectorView
from PySide6.QtWidgets import QMainWindow, QApplication, QScrollArea,QWidget, QVBoxLayout, QTabWidget, QDockWidget, QTreeView
from PySide6.QtGui import QPen, QColorConstants, QColor, QBrush
from PySide6.QtCore import Qt, QTimer, QObject, QThread, Signal
import sys, os
import math
import numpy as np
from datetime import datetime, timezone
from types import SimpleNamespace
import humanize

from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.inprocess import QtInProcessKernelManager
from numba import jit, prange

os.environ['QT_API'] = 'PySide6'
os.environ['QT_QPA_PLATFORM'] = 'xcb'

print(f"Using SciQLopPlots lib from: {SciQLopPlots.__file__}, {SciQLopPlots.SciQLopPlotsBindings.__file__}")


sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow

@jit(nopython=True, parallel=True)
def compute_data(size):
    """Compute a large array of random data."""
    data = np.zeros(size, dtype=np.float64)
    for i in prange(size):
        data[i] = (np.cos(i / 180.) * (np.cos(i**2 / 18.**2) * np.cos(i / 90.) * 100. +
                                        np.sin(i / 50.) * 50.) + np.random.rand() * 10.)
    return data

def generate_data(size=int(1e9)):
    """Generate a large array of random data."""
    # Simulate a large dataset
    time_vector = np.arange(size, dtype=np.float64)
    return time_vector, compute_data(size)

class GigaPoints(SciQLopMultiPlotPanel):
    def __init__(self, parent, size=1e9):
        SciQLopMultiPlotPanel.__init__(self,parent, synchronize_x=False, synchronize_time=True)
        time, data = generate_data(int(size))  # Generate 1 billion points
        (self._plot, self._graph) = self.plot(time, data, labels=[f"{humanize.scientific(size)} points"], colors=[QColorConstants.Blue])



if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.ipython_widget.close()
    w.add_tab(GigaPoints(w, size=1e9), "GigaPoints")
    w.showFullScreen()
    app.exec()
