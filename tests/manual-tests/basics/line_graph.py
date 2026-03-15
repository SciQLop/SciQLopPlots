"""Basic line graph with 3 components and lazy data loading.

Showcases:
- panel.plot() with callable data producer
- Multi-component line graph (X, Y, Z)
- Automatic resampling on zoom/pan
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants
from SciQLopPlots import SciQLopMultiPlotPanel, PlotType


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Line Graph Demo")

panel.plot(
    make_data,
    labels=["X", "Y", "Z"],
    colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
    plot_type=PlotType.TimeSeries,
)

panel.show()
panel.resize(800, 500)
sys.exit(app.exec())
