"""SciQLopPlots Gallery — tabbed showcase of all major features."""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QTabWidget, QWidget, QVBoxLayout, QPushButton, QFileDialog
from PySide6.QtGui import QColor, QColorConstants, QPen, QBrush
from PySide6.QtCore import Qt, QRectF

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopVerticalSpan,
    SciQLopPlotRange, MultiPlotsVerticalSpan, SciQLopEllipseItem,
    PlotType, GraphType, Coordinates,
    OverlayLevel, OverlaySizeMode, OverlayPosition,
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


def bimodal_scatter(n=100_000):
    rng = np.random.default_rng(42)
    x1 = rng.normal(-2, 1.0, n // 2)
    y1 = rng.normal(1, 0.6, n // 2)
    x2 = rng.normal(2, 0.8, n // 2)
    y2 = rng.normal(-1, 1.2, n // 2)
    return np.concatenate([x1, x2]), np.concatenate([y1, y2])


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


def create_contour_tab():
    panel = SciQLopMultiPlotPanel(synchronize_x=True)

    x = np.linspace(-4, 4, 200)
    y = np.linspace(-4, 4, 150)
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.sin(xx) * np.cos(yy) + 0.3 * np.sin(2 * xx + yy)

    _, cmap1 = panel.plot(x, y, z)
    cmap1.set_contour_levels([-0.8, -0.4, 0.0, 0.4, 0.8])
    cmap1.set_contour_color(QColor("white"))
    cmap1.set_contour_width(1.5)

    _, cmap2 = panel.plot(x, y, z)
    cmap2.set_auto_contour_levels(10)
    cmap2.set_contour_color(QColor("black"))
    cmap2.set_contour_labels_enabled(True)

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


def create_histogram2d_tab():
    plot = SciQLopPlot()
    x, y = bimodal_scatter(100_000)
    hist = plot.add_histogram2d("Density", 80, 80)
    hist.set_data(x, y)
    plot.x_axis().set_range(float(x.min()), float(x.max()))
    plot.y_axis().set_range(float(y.min()), float(y.max()))
    return plot


def create_waterfall_tab():
    n_traces = 12
    n_samples = 3000
    t = np.linspace(0, 15, n_samples).astype(np.float64)
    rng = np.random.default_rng(17)
    distances = np.sort(rng.uniform(0, 80, n_traces))
    y = np.zeros((n_samples, n_traces), dtype=np.float64)
    for i in range(n_traces):
        onset = distances[i] / 5.0
        pulse = np.exp(-((t - onset) / 0.3) ** 2) * np.sin(2 * np.pi * 2 * (t - onset))
        y[:, i] = pulse + 0.05 * rng.standard_normal(n_samples)

    plot = SciQLopPlot()
    plot.plot(
        t, y,
        graph_type=GraphType.Waterfall,
        offsets=distances.tolist(),
        normalize=False,
        gain=3.0,
        labels=[f"stn{i:02d}" for i in range(n_traces)],
    )
    return plot


def create_overlay_tab():
    plot = SciQLopPlot()
    x, y = butterfly_curve()
    plot.x_axis().set_range(min(x), max(x))
    plot.y_axis().set_range(min(y), max(y))
    plot.plot(
        x, y,
        labels=["butterfly"],
        colors=[QColorConstants.DarkCyan],
        graph_type=GraphType.ParametricCurve,
    )
    ov = plot.overlay()
    ov.show_message(
        "Overlay demo — Info level",
        OverlayLevel.Info,
        OverlaySizeMode.FitContent,
        OverlayPosition.Top,
    )
    ov.set_collapsible(True)
    ov.set_opacity(0.9)
    return plot


def create_stacked_tab():
    container = QWidget()
    layout = QVBoxLayout(container)
    layout.setContentsMargins(0, 0, 0, 0)

    btn = QPushButton("Toggle Span Creation Mode (Shift+Click to draw)")
    btn.setCheckable(True)

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

    btn.toggled.connect(panel.set_span_creation_enabled)
    panel.span_created.connect(
        lambda s: print(f"Span created: [{s.range.start():.1f}, {s.range.stop():.1f}]"))

    layout.addWidget(btn)
    layout.addWidget(panel, stretch=1)
    return container


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


def create_busy_indicator_tab():
    import time

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)

    def slow_signal(start, stop):
        time.sleep(1.5)
        x = np.arange(start, stop, dtype=np.float64)
        y = np.column_stack([
            np.cos(x / 6) * np.cos(x) * 100,
            np.cos(x / 60) * np.cos(x / 6) * 1300,
        ])
        return x, y

    plot, graph = panel.plot(
        slow_signal,
        labels=["slow X", "slow Y"],
        colors=[QColorConstants.Red, QColorConstants.Blue],
        plot_type=PlotType.TimeSeries,
    )

    def fast_signal(start, stop):
        x = np.arange(start, stop, dtype=np.float64)
        y = np.sin(x / 100) * np.cos(x / 10)
        return x, y

    panel.plot(
        fast_signal,
        labels=["fast"],
        colors=[QColorConstants.DarkGreen],
        plot_type=PlotType.TimeSeries,
    )

    return panel


# ── Export helper ─────────────────────────────────────────────────────────────

def with_export_button(plot_widget):
    container = QWidget()
    layout = QVBoxLayout(container)
    layout.setContentsMargins(0, 0, 0, 0)
    btn = QPushButton("Export...")
    btn.clicked.connect(lambda: _do_export(plot_widget))
    layout.addWidget(btn)
    layout.addWidget(plot_widget, stretch=1)
    return container

def _do_export(widget):
    path, _ = QFileDialog.getSaveFileName(
        widget, "Export Plot",
        "", "PDF (*.pdf);;PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)")
    if path:
        widget.save(path)


# ── Main ─────────────────────────────────────────────────────────────────────

app = QApplication(sys.argv)

window = QMainWindow()
window.setWindowTitle("SciQLopPlots Gallery")
window.resize(1000, 700)

tabs = QTabWidget()
tabs.addTab(with_export_button(create_line_tab()), "Line Graph")
tabs.addTab(with_export_button(create_colormap_tab()), "Colormap")
tabs.addTab(with_export_button(create_contour_tab()), "Contour Overlay")
tabs.addTab(with_export_button(create_histogram2d_tab()), "Histogram 2D")
tabs.addTab(with_export_button(create_waterfall_tab()), "Waterfall")
tabs.addTab(with_export_button(create_curve_tab()), "Parametric Curve")
tabs.addTab(with_export_button(create_spans_tab()), "Vertical Spans")
tabs.addTab(with_export_button(create_overlay_tab()), "Overlay")
tabs.addTab(create_stacked_tab(), "Stacked Plots")
tabs.addTab(with_export_button(create_pipeline_tab()), "Pipeline (FFT)")
tabs.addTab(with_export_button(create_busy_indicator_tab()), "Busy Indicator")

window.setCentralWidget(tabs)
window.show()

sys.exit(app.exec())
