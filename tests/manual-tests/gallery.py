"""SciQLopPlots Gallery — tabbed showcase of all major features."""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget
from PySide6.QtGui import QColor, QColorConstants, QPen, QBrush
from PySide6.QtCore import Qt, QRectF

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopVerticalSpan,
    SciQLopPlotRange, MultiPlotsVerticalSpan, SciQLopEllipseItem,
    PlotType, GraphType, Coordinates,
)


# ── Data generators ──────────────────────────────────────────────────────────

def multicomponent_signal(start, stop):
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


def two_freq_signal(start, stop):
    n = max(int((stop - start) * 100), 64)
    x = np.linspace(start, stop, n)
    y = np.sin(2 * np.pi * 5 * x) + 0.5 * np.sin(2 * np.pi * 13 * x)
    return x, y


def butterfly_curve(n=5000):
    t = np.linspace(0, 12 * np.pi, n)
    factor = np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5
    return np.sin(t) * factor, np.cos(t) * factor


def uniform_colormap(nx=200, ny=200):
    x = np.linspace(-4, 4, nx)
    y = np.linspace(-4, 4, ny)
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.cos(np.sqrt((xx + 2) ** 2 + yy ** 2))
    return x, y, z


# ── Tab builders ─────────────────────────────────────────────────────────────

def create_line_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.plot(
        multicomponent_signal,
        labels=["X", "Y", "Z"],
        colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
        plot_type=PlotType.TimeSeries,
    )
    return panel


def create_colormap_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=True)
    panel.plot(*uniform_colormap(200, 200))
    panel.plot(*uniform_colormap(500, 300))
    return panel


def create_curve_tab():
    plot = SciQLopPlot()
    x, y = butterfly_curve()
    plot.x_axis().set_range(min(x), max(x))
    plot.y_axis().set_range(min(y), max(y))
    plot.plot(
        x, y,
        labels=["butterfly"],
        colors=[QColorConstants.DarkMagenta],
        graph_type=GraphType.ParametricCurve,
    )
    SciQLopEllipseItem(
        plot, QRectF(-0.5, -0.5, 1, 1),
        QPen(Qt.red), QBrush(QColorConstants.Transparent),
        True, Coordinates.Data,
    )
    return plot


def create_spans_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    plot, _ = panel.plot(
        sine_signal,
        labels=["signal"],
        colors=[QColorConstants.DarkBlue],
        plot_type=PlotType.TimeSeries,
    )
    r = plot.x_axis().range()
    span_colors = [
        QColor(200, 100, 100, 80),
        QColor(100, 200, 100, 80),
        QColor(100, 100, 200, 80),
    ]
    for i, color in enumerate(span_colors):
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
            span.on.range >> (
                lambda e: f"[{e.value.start():.1f}, {e.value.stop():.1f}] (size: {e.value.size():.1f})"
            ) >> span.on.tooltip
    return panel


def create_stacked_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    for _ in range(4):
        panel.plot(
            multicomponent_signal,
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
    return panel


def create_pipeline_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    plot1, graph1 = panel.plot(two_freq_signal, labels=["signal"])

    SciQLopVerticalSpan(
        plot1, SciQLopPlotRange(2.0, 4.0),
        QColor(100, 100, 200, 80), read_only=False,
        visible=True, tool_tip="Drag me!",
    )

    x_init = np.linspace(0, 10, 512)
    y_init = np.zeros(len(np.fft.rfftfreq(len(x_init))))
    _, fft_graph = panel.plot(
        np.fft.rfftfreq(len(x_init), d=0.01), y_init, labels=["FFT magnitude"],
    )

    def compute_fft(event):
        data = event.value
        if data is None or len(data) < 2:
            return None
        x, y = np.array(data[0]), np.array(data[1])
        if len(x) < 4:
            return None
        dt = (x[-1] - x[0]) / (len(x) - 1)
        if dt <= 0:
            return None
        freqs = np.fft.rfftfreq(len(y), d=dt)
        magnitude = np.abs(np.fft.rfft(y)) / len(y)
        return [freqs, magnitude]

    graph1.on.data >> compute_fft >> fft_graph.on.data
    return panel


# ── Main ─────────────────────────────────────────────────────────────────────

app = QApplication(sys.argv)

window = QMainWindow()
window.setWindowTitle("SciQLopPlots Gallery")
window.resize(1000, 700)

tabs = QTabWidget()
tabs.addTab(create_line_tab(), "Line Graph")
tabs.addTab(create_colormap_tab(), "Colormap")
tabs.addTab(create_curve_tab(), "Parametric Curve")
tabs.addTab(create_spans_tab(), "Vertical Spans")
tabs.addTab(create_stacked_tab(), "Stacked Plots")
tabs.addTab(create_pipeline_tab(), "Pipeline (FFT)")

window.setCentralWidget(tabs)
window.show()

sys.exit(app.exec())
