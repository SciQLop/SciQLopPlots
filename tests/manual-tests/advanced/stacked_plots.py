"""Stacked time-series plots with colormap and multi-plot vertical span.

Showcases:
- SciQLopMultiPlotPanel with multiple stacked plots
- Time axis synchronization
- MultiPlotsVerticalSpan across all plots
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants, QColor
from PySide6.QtCore import Qt

from SciQLopPlots import (
    SciQLopMultiPlotPanel, MultiPlotsVerticalSpan, PlotType,
)


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
panel.setWindowTitle("Stacked Plots Demo")

for i in range(4):
    panel.plot(
        make_data,
        labels=["X", "Y", "Z"],
        colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
        plot_type=PlotType.TimeSeries,
    )

x_range = panel.plot_at(0).x_axis().range()
MultiPlotsVerticalSpan(
    panel, x_range / 10,
    QColor(100, 100, 200, 80),
    read_only=False, visible=True,
    tool_tip="Drag across all plots",
)

panel.show()
panel.resize(800, 700)
sys.exit(app.exec())
