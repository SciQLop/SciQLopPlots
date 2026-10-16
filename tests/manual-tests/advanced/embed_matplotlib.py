"""A matplotlib figure as a row of a SciQLopPlots panel.

Showcases:
- WidgetPlot: any QWidget in a panel, following the panel's time range
- A histogram of the values in view, redrawn after every pan or zoom
"""
import sys

import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, WidgetPlot

try:
    from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg
    from matplotlib.figure import Figure
except ImportError:
    sys.exit("This example needs matplotlib: pip install matplotlib")

t = 1.7e9 + np.linspace(0, 6 * 3600, 200_000)           # six hours, seconds since 1970
bx = np.sin(t / 900) * 20 + np.random.default_rng(0).normal(0, 3, t.size)

app = QApplication(sys.argv)
panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("matplotlib in a panel")
panel.set_time_axis_range(t[0], t[-1])
panel.plot(t, bx, labels=["Bx"], plot_type=PlotType.TimeSeries)

figure = Figure(figsize=(6, 2), layout="constrained")
axes = figure.add_subplot()
canvas = FigureCanvasQTAgg(figure)


def histogram_of_view(start, stop):
    in_view = bx[(t >= start) & (t <= stop)]
    axes.clear()
    axes.hist(in_view, bins=60, color="tab:blue")
    axes.set_xlabel("Bx (nT)")
    axes.set_ylabel(f"{in_view.size} samples")
    canvas.draw_idle()


panel.add_plot(WidgetPlot(canvas, on_time_range=histogram_of_view))
panel.resize(900, 600)
panel.show()
sys.exit(app.exec())
