"""Interactive vertical spans with tooltips.

Showcases:
- SciQLopVerticalSpan creation and interaction
- Read-only vs editable spans
- Span tooltips and colors
- Reactive pipeline: span.on.range >> tooltip_updater >> span.on.tooltip
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColor, QColorConstants

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopVerticalSpan, SciQLopPlotRange, PlotType,
)


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.sin(x / 100) * np.cos(x / 10)
    return x, y


def range_to_tooltip(event):
    r = event.value
    return f"[{r.start():.1f}, {r.stop():.1f}] (size: {r.size():.1f})"


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Vertical Spans Demo")

plot, graph = panel.plot(
    make_data,
    labels=["signal"],
    colors=[QColorConstants.DarkBlue],
    plot_type=PlotType.TimeSeries,
)

colors = [
    QColor(200, 100, 100, 80),
    QColor(100, 200, 100, 80),
    QColor(100, 100, 200, 80),
]

r = plot.x_axis().range()
for i, color in enumerate(colors):
    offset = r.size() * (0.2 + i * 0.25)
    span = SciQLopVerticalSpan(
        plot,
        SciQLopPlotRange(r.start() + offset, r.start() + offset + r.size() * 0.1),
        color,
        read_only=(i == 0),
        visible=True,
        tool_tip=f"Span {i+1}" + (" (read-only)" if i == 0 else " (drag me!)"),
    )
    if i > 0:
        span.on.range >> range_to_tooltip >> span.on.tooltip

panel.show()
panel.resize(800, 500)
sys.exit(app.exec())
