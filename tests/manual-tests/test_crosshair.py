"""Manual test: crosshair cursor with cross-plot sync.

Run: python tests/manual-tests/test_crosshair.py
Expected: 3 stacked plots with synchronized crosshair.
- Hover over any plot -> vertical dashed line appears on ALL plots
- Tooltip shows values for all curves at the crosshair X position
- Moving mouse off a plot hides all crosshairs
- Selection still works (clicking curves)
- Pan/zoom still works (dragging)
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow
from PySide6.QtGui import QColorConstants

from SciQLopPlots import SciQLopMultiPlotPanel, PlotType, GraphType


def multicomp_signal(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


def sine_signal(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.sin(x / 100) * np.cos(x / 10)
    return x, y


def damped_signal(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.exp(-x / 3000) * np.sin(x / 20)
    return x, y


app = QApplication(sys.argv)
win = QMainWindow()
win.setWindowTitle("Crosshair Cursor Test")
panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
win.setCentralWidget(panel)

panel.plot(
    multicomp_signal,
    labels=["X", "Y", "Z"],
    colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
    plot_type=PlotType.TimeSeries,
)

panel.plot(
    sine_signal,
    labels=["sin*cos"],
    plot_type=PlotType.TimeSeries,
)

panel.plot(
    damped_signal,
    labels=["damped"],
    plot_type=PlotType.TimeSeries,
)

win.resize(900, 700)
win.show()
sys.exit(app.exec())
