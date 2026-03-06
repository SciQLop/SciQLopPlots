"""Manual test demonstrating the reactive pipeline API.

Shows:
1. Transform pipeline: span.on.range >> compute_stats >> span.on.tooltip
2. Terminal sink: span.on.range >> print_callback
3. Data pipeline: graph1.on.data >> fft >> graph2.on.data
"""
import sys
import os
import numpy as np

from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopVerticalSpan,
    SciQLopPlotRange, GraphType,
)


def make_data(lower, upper):
    n = max(int((upper - lower) * 100), 64)
    x = np.linspace(lower, upper, n)
    # Two frequencies so the FFT is interesting
    y = np.sin(2 * np.pi * 5 * x) + 0.5 * np.sin(2 * np.pi * 13 * x)
    return x, y


def compute_stats(event):
    r = event.value
    return f"Range: [{r.start():.1f}, {r.stop():.1f}], Size: {r.stop() - r.start():.1f}"


def compute_fft(event):
    """Transform: take graph data, return FFT magnitude spectrum."""
    data = event.value  # QList<PyBuffer> = [x, y]
    if data is None or len(data) < 2:
        return None
    x = np.array(data[0])
    y = np.array(data[1])
    if len(x) < 4:
        return None
    dt = (x[-1] - x[0]) / (len(x) - 1)
    if dt <= 0:
        return None
    freqs = np.fft.rfftfreq(len(y), d=dt)
    magnitude = np.abs(np.fft.rfft(y)) / len(y)
    return [freqs, magnitude]


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel()
panel.setWindowTitle("Pipeline API Test")

# --- Plot 1: signal with two frequencies ---
plot1, graph1 = panel.plot(make_data, labels=["signal"])

# Add a vertical span on plot 1
span = SciQLopVerticalSpan(
    plot1, SciQLopPlotRange(2.0, 4.0),
    QColor(100, 100, 200, 80), read_only=False,
    visible=True, tool_tip="Drag me!"
)

# Pipeline: update span tooltip when it moves
span.on.range >> compute_stats >> span.on.tooltip

# Terminal sink: log range changes
span.on.range >> (lambda event: print(f"Span moved to: {event.value.start():.1f} - {event.value.stop():.1f}"))

# --- Plot 2: FFT of plot 1's data ---
x_init = np.linspace(0, 10, 512)
y_init = np.zeros(len(np.fft.rfftfreq(len(x_init))))
_, fft_graph = panel.plot(np.fft.rfftfreq(len(x_init), d=0.01), y_init, labels=["FFT magnitude"])

# Data pipeline: when graph1 data changes, compute FFT and show in graph2
graph1.on.data >> compute_fft >> fft_graph.on.data

panel.show()
panel.resize(800, 600)

sys.exit(app.exec())
