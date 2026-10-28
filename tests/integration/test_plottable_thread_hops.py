"""Legend and plottable style setters called off the GUI thread run on the GUI thread.

The same contract as tests/integration/test_thread_guards.py (SciQLop#147), for the drawn
objects that are not items or axes: the call returns at once, applies nothing on the calling
thread, and lands once the GUI thread processes its events.
"""
import numpy as np
import pytest
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QFont, QPen

from conftest import off_thread, process_events
from SciQLopPlots import (
    ColorGradient,
    GraphLineStyle,
    GraphMarkerShape,
    HistogramNormalization,
    HistogramStyle,
    SciQLopNDProjectionPlot,
)

X = np.linspace(0, 10, 50)
RED = QColor(200, 10, 10)


def _lines(plot):
    return plot.plot(X, np.column_stack([np.sin(X), np.sin(X) - 1, np.sin(X) + 1]),
                     labels=["a", "lo", "hi"])


def _colormap(plot):
    return plot.plot(X, np.linspace(0, 1, 20), np.random.rand(50, 20))


def _curve(plot):
    return plot.parametric_curve(X, np.column_stack([np.sin(X), np.cos(X)]))


def _projection(plot):
    nd = SciQLopNDProjectionPlot(3, plot)
    plot.projection_plot = nd  # Python owns nd despite its Qt parent: keep it for the test
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    for _ in range(50):
        if len(projection.components()) == 3:
            break
        process_events()
    return projection


def _filled(graph):
    graph.set_fill_to_value(0, 0.0)
    return graph


CASES = {
    "legend.set_alignment": (lambda p: p.legend(),
                             lambda o: o.set_alignment(Qt.AlignLeft | Qt.AlignBottom),
                             lambda o: o.alignment(), Qt.AlignLeft | Qt.AlignBottom),
    "legend.set_columns": (lambda p: p.legend(), lambda o: o.set_columns(3),
                           lambda o: o.columns(), 3),
    "legend.set_font": (lambda p: p.legend(), lambda o: o.set_font(QFont("Monospace", 23)),
                        lambda o: o.font().pointSize(), 23),
    "graph.set_labels": (_lines, lambda o: o.set_labels(["x", "y", "z"]),
                         lambda o: o.labels(), ["x", "y", "z"]),
    "graph.set_colors": (_lines, lambda o: o.set_colors([RED] * 3),
                         lambda o: o.colors(), [RED] * 3),
    "graph.set_gap_threshold": (_lines, lambda o: o.set_gap_threshold(9.0),
                                lambda o: o.gap_threshold(), 9.0),
    "graph.set_fill_to_value": (_lines, lambda o: o.set_fill_to_value(0, 0.5),
                                lambda o: o.components()[0].has_fill(), True),
    "graph.set_fill_between": (_lines, lambda o: o.set_fill_between(0, 1),
                               lambda o: o.components()[0].has_fill(), True),
    "graph.clear_fill": (lambda p: _filled(_lines(p)), lambda o: o.clear_fill(0),
                         lambda o: o.components()[0].has_fill(), False),
    "graph.set_fill_color": (lambda p: _filled(_lines(p)), lambda o: o.set_fill_color(0, RED),
                             lambda o: o.components()[0].fill_color(), RED),
    "graph.set_column_line_style": (_lines,
                                    lambda o: o.set_column_line_style(0, GraphLineStyle.NoLine),
                                    lambda o: o.components()[0].line_style(),
                                    GraphLineStyle.NoLine),
    "graph.set_column_marker_shape": (_lines,
                                      lambda o: o.set_column_marker_shape(
                                          0, GraphMarkerShape.Circle),
                                      lambda o: o.components()[0].marker_shape(),
                                      GraphMarkerShape.Circle),
    "graph.set_column_in_legend": (_lines, lambda o: o.set_column_in_legend(0, False),
                                   lambda o: o.column_in_legend(0), False),
    "graph.set_color_gradient": (_lines, lambda o: o.set_color_gradient(ColorGradient.Hot),
                                 lambda o: o.color_gradient(), ColorGradient.Hot),
    "histogram.set_bins": (lambda p: p.histogram(np.random.rand(100)),
                           lambda o: o.set_bins(7), lambda o: o.bins(), 7),
    "histogram.set_normalization": (lambda p: p.histogram(np.random.rand(100)),
                                    lambda o: o.set_normalization(HistogramNormalization.Density),
                                    lambda o: o.normalization(), HistogramNormalization.Density),
    "histogram.set_style": (lambda p: p.histogram(np.random.rand(100)),
                            lambda o: o.set_style(HistogramStyle.Step),
                            lambda o: o.style(), HistogramStyle.Step),
    "histogram.set_colors": (lambda p: p.histogram(np.random.rand(100)),
                             lambda o: o.set_colors([RED]), lambda o: o.colors(), [RED]),
    "colormap.set_contour_levels": (_colormap, lambda o: o.set_contour_levels([0.5]),
                                    lambda o: list(o.contour_levels()), [0.5]),
    "colormap.set_auto_contour_levels": (_colormap, lambda o: o.set_auto_contour_levels(4),
                                         lambda o: o.auto_contour_level_count(), 4),
    "colormap.set_contour_pen": (_colormap, lambda o: o.set_contour_pen(QPen(RED)),
                                 lambda o: o.contour_pen().color(), RED),
    "colormap.set_contour_color": (_colormap, lambda o: o.set_contour_color(RED),
                                   lambda o: o.contour_color(), RED),
    "colormap.set_contour_width": (_colormap, lambda o: o.set_contour_width(7.0),
                                   lambda o: o.contour_width(), 7.0),
    "colormap.set_contour_labels_enabled": (_colormap,
                                            lambda o: o.set_contour_labels_enabled(True),
                                            lambda o: o.contour_labels_enabled(), True),
    "curve.set_time_color_enabled": (_curve, lambda o: o.set_time_color_enabled(True),
                                     lambda o: o.time_color_enabled(), True),
    "projection.set_colors": (_projection, lambda o: o.set_colors([RED] * 3),
                              lambda o: o.colors(), [RED] * 3),
    "projection.set_time_color_enabled": (_projection, lambda o: o.set_time_color_enabled(True),
                                          lambda o: o.time_color_enabled(), True),
}


@pytest.mark.parametrize("name", sorted(CASES))
def test_setter_lands_on_gui_thread(plot, name):
    make, call, read, value = CASES[name]
    target = make(plot)
    process_events()
    before = read(target)
    assert before != value, "pick a value that differs from the default"
    errors = []

    def run():
        try:
            call(target)
        except Exception as e:  # noqa: BLE001 - the test reports it
            errors.append(e)

    off_thread(run)
    assert (errors, read(target)) == ([], before), "applied on the calling thread"
    process_events()
    assert read(target) == value


def test_graph_error_bars_land_on_gui_thread(plot):
    """No getter for error bars: a direct slot records the thread that applied them."""
    import threading
    graph = _lines(plot)
    appliers = []
    graph.components()[0].error_bars_changed.connect(
        lambda: appliers.append(threading.get_ident()), Qt.ConnectionType.DirectConnection)
    off_thread(lambda: graph.set_error_bars(0, 1, 2))
    off_thread(lambda: graph.clear_error_bars(0))
    process_events()
    assert appliers == [threading.get_ident()] * 2


@pytest.mark.parametrize("call", [
    lambda g: g.set_column_line_style(0, GraphLineStyle.NoLine),
    lambda g: g.set_column_marker_shape(0, GraphMarkerShape.Circle),
], ids=["line_style", "marker_shape"])
def test_a_column_preset_asks_for_its_replot_on_the_gui_thread(plot, call):
    graph = _lines(plot)
    process_events()
    replots = []
    graph.replot.connect(lambda: replots.append(1), Qt.DirectConnection)
    off_thread(lambda: call(graph))
    assert replots == [], "replot requested on the calling thread"
    process_events()
    assert replots != []
