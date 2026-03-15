"""Custom drag-and-drop callback for creating plots from a list widget.

Showcases:
- PlotDragNDropCallback for custom mime type handling
- Dynamic plot creation on drop
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QListWidget, QSplitter
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt, QMimeData

from SciQLopPlots import SciQLopMultiPlotPanel, PlotDragNDropCallback


class DNDSource(QListWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.addItems(["Cos", "Sin", "Rand"])
        self.setDragEnabled(True)

    def mimeData(self, items):
        mime_data = QMimeData()
        mime_data.setData("text/plain", (";".join([item.text() for item in items])).encode())
        return mime_data


class PlotOnDropCallback(PlotDragNDropCallback):
    def __init__(self, parent):
        super().__init__("text/plain", True, parent)

    def call(self, plot, mime_data: QMimeData):
        data = mime_data.data("text/plain").data().decode()
        x = np.linspace(0, 2 * np.pi, 100)
        generators = {
            "Cos": lambda: np.cos(x),
            "Sin": lambda: np.sin(x),
            "Rand": lambda: np.random.rand(len(x)),
        }
        if data in generators:
            plot.plot(x, generators[data](), labels=[data.lower()], colors=[QColorConstants.DarkGreen])


app = QApplication(sys.argv)

splitter = QSplitter()
splitter.setWindowTitle("Drag & Drop Demo")

source = DNDSource(splitter)
splitter.addWidget(source)

panel = SciQLopMultiPlotPanel(splitter, synchronize_x=False, synchronize_time=True)
callback = PlotOnDropCallback(panel)
panel.add_accepted_mime_type(callback)
splitter.addWidget(panel)

splitter.show()
splitter.resize(800, 500)
sys.exit(app.exec())
