from SciQLopPlots import SciQLopPlot, \
                         MultiPlotsVerticalSpan ,SciQLopMultiPlotPanel, SciQLopVerticalSpan, \
                         SciQLopTimeSeriesPlot, PlotType, AxisType, GraphType, PlotDragNDropCallback
from PySide6.QtWidgets import QMainWindow, QApplication, QListWidget ,QWidget, QSplitter, QHBoxLayout, QTabWidget, QDockWidget
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt, QMimeData
import sys, os
import numpy as np
from datetime import datetime, timezone
import threading
from typing import Dict, Any

sys.path.append(os.path.join(os.path.dirname(__file__), ".."))
from common import MainWindow


os.environ['QT_API'] = 'PySide6'

class DNDSource(QListWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.addItems(["Cos", "Sin", "Rand"])
        self.setDragEnabled(True)

    def mimeData(self, items):
        mime_data = QMimeData()
        mime_data.setData("text/plain", (";".join([item.text() for item in items])).encode())
        return mime_data


class MyCallback(PlotDragNDropCallback):
    def __init__(self,parent, source):
        super().__init__("text/plain", True, parent)
        self.source = source

    def call(self, plot, mime_data:QMimeData):
        print(f"Called with plot {plot}, {mime_data}")
        data = mime_data.data("text/plain").data().decode()
        print(data)
        x = np.linspace(0, 2*np.pi, 100)
        if data == "Cos":
            plot.plot(x, np.cos(x), labels=["cos"], colors=[QColorConstants.DarkGreen])
        elif data == "Sin":
            plot.plot(x, np.sin(x), labels=["sin"], colors=[QColorConstants.DarkBlue])
        elif data == "Rand":
            plot.plot(x, np.random.rand(len(x)), labels=["rand"], colors=[QColorConstants.DarkRed])



class DNDTarget(SciQLopMultiPlotPanel):
    def __init__(self, parent=None, source=None):
        super().__init__(parent,synchronize_x=False, synchronize_time=True)
        self._callback = MyCallback(self, source=source)
        self.add_accepted_mime_type(self._callback)


class DNDTest(QSplitter):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAcceptDrops(True)
        self.source = DNDSource(self)
        self.addWidget(self.source)
        self.target = DNDTarget(self, source=self.source)
        self.addWidget(self.target)


if __name__ == '__main__':
    QApplication.setAttribute(Qt.AA_UseDesktopOpenGL, True)
    QApplication.setAttribute(Qt.AA_ShareOpenGLContexts, True)
    app = QApplication(sys.argv)
    w = MainWindow()
    w.add_tab(DNDTest(w), "DND Test")
    w.show()
    app.exec()
