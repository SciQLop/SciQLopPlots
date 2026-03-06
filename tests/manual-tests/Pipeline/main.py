"""Manual test demonstrating the reactive pipeline API.

Shows:
1. Direct axis binding: axis1.on.range >> axis2.on.range
2. Transform pipeline: span.on.range >> compute_stats >> span.on.tooltip
3. Terminal sink: span.on.range >> print_callback
"""
import sys
import os
import numpy as np

#os.environ['QT_QPA_PLATFORM'] = os.environ.get('QT_QPA_PLATFORM', 'offscreen')

from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopVerticalSpan,
    SciQLopPlotRange, GraphType,
)


def make_data(lower, upper):
    x = np.linspace(lower, upper, 1000)
    y = np.sin(x) + 0.1 * np.random.randn(1000)
    return x, y


def compute_stats(event):
    r = event.value
    return f"Range: [{r.start():.1f}, {r.stop():.1f}], Size: {r.stop() - r.start():.1f}"


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel()
panel.setWindowTitle("Pipeline API Test")

# Create a plot with data
graph = panel.plot(make_data, labels=["sin(x)"])

# Add a vertical span
plot = panel.plot_at(0)
span = SciQLopVerticalSpan(
    plot, SciQLopPlotRange(2.0, 4.0),
    QColor(100, 100, 200, 80), read_only=False,
    visible=True, tool_tip="Drag me!"
)

# Pipeline: update span tooltip when it moves
span.on.range >> compute_stats >> span.on.tooltip

# Terminal sink: log range changes
span.on.range >> (lambda event: print(f"Span moved to: {event.value.start():.1f} - {event.value.stop():.1f}"))

panel.show()
panel.resize(800, 600)

#QTimer.singleShot(1000, app.quit)
sys.exit(app.exec())
