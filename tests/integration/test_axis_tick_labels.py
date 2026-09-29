"""Issue #120: fixed text labels on an axis (timeline lanes, categorical axes)."""
import time

import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopPlotRange
from conftest import process_events

LANES = {0.0: "BASE", -1.0: "HKM", -2.0: "LM", -3.0: "MM"}
DECADES = {1.0: "one", 10.0: "ten", 100.0: "hundred"}


def _line_plot(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    x = np.linspace(0, 10, 50)
    plot.plot(x, -(x % 4))
    plot.x_axis().set_range(SciQLopPlotRange(0, 10))
    plot.y_axis().set_range(SciQLopPlotRange(-3.5, 0.5))
    process_events()
    return plot


def _colormap_plot(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    x = np.arange(50.0)
    plot.plot(x, np.array([1.0, 2.0]), np.repeat(np.logspace(0, 2, 50), 2).reshape(50, 2))
    process_events()
    return plot


AXES = {
    "y_axis": (_line_plot, lambda plot: plot.y_axis()),
    "colour_scale": (_colormap_plot, lambda plot: plot.z_axis()),
}


@pytest.fixture(params=AXES)
def decade_axis(request, qtbot):
    """Makes (plot, axis) pairs whose axis spans 0.5 .. 200, fit for a log scale."""
    make_plot, axis_of = AXES[request.param]

    def make():
        plot = make_plot(qtbot)
        axis = axis_of(plot)
        axis.set_range(SciQLopPlotRange(0.5, 200))
        return plot, axis

    return make


def _settle(plot, timeout=5.0):
    """A colormap resamples in the background: renders are only comparable once it is done."""
    deadline = time.monotonic() + timeout
    while any(p.busy() for p in plot.plottables()):
        assert time.monotonic() < deadline, "plottables still busy"
        process_events()


def _render(plot, tmp_path, name):
    _settle(plot)
    path = str(tmp_path / f"{name}.png")
    assert plot.save_png(path, 500, 300)
    return QImage(path)


def test_labels_round_trip(qtbot):
    plot = _line_plot(qtbot)
    axis = plot.y_axis()
    emitted = []
    axis.tick_labels_changed.connect(emitted.append)
    assert axis.tick_labels() == {}

    axis.set_tick_labels(LANES)
    axis.set_tick_labels(LANES)
    assert axis.tick_labels() == LANES
    axis.clear_tick_labels()
    assert axis.tick_labels() == {}
    assert emitted == [LANES, {}]


def test_labels_replace_numbers_and_clear_restores_them(qtbot, tmp_path):
    plot = _line_plot(qtbot)
    numeric = _render(plot, tmp_path, "numeric")

    plot.y_axis().set_tick_labels(LANES)
    assert _render(plot, tmp_path, "lanes") != numeric

    plot.y_axis().clear_tick_labels()
    assert _render(plot, tmp_path, "cleared") == numeric


def test_labels_survive_switching_to_log(decade_axis, tmp_path):
    labels_first, axis = decade_axis()
    axis.set_tick_labels(DECADES)
    axis.set_log(True)

    log_first, axis = decade_axis()
    axis.set_log(True)
    plain_log = _render(log_first, tmp_path, "plain_log")
    axis.set_tick_labels(DECADES)

    labelled = _render(log_first, tmp_path, "log_first")
    assert _render(labels_first, tmp_path, "labels_first") == labelled
    assert labelled != plain_log


def test_labels_survive_switching_back_to_linear(decade_axis, tmp_path):
    round_trip, axis = decade_axis()
    axis.set_tick_labels(DECADES)
    axis.set_log(True)
    axis.set_log(False)

    linear, axis = decade_axis()
    axis.set_tick_labels(DECADES)

    assert _render(round_trip, tmp_path, "round_trip") == _render(linear, tmp_path, "linear")


def test_clearing_labels_on_a_log_axis_restores_log_ticks(decade_axis, tmp_path):
    cleared, axis = decade_axis()
    axis.set_log(True)
    axis.set_tick_labels(DECADES)
    axis.clear_tick_labels()

    plain_log, axis = decade_axis()
    axis.set_log(True)

    assert _render(cleared, tmp_path, "cleared") == _render(plain_log, tmp_path, "plain_log")


def test_time_axis_keeps_its_ticker(qtbot, tmp_path):
    plot = SciQLopTimeSeriesPlot()
    qtbot.addWidget(plot)
    plot.time_axis().set_range(SciQLopPlotRange(1.8e9, 1.8e9 + 3600))
    dates = _render(plot, tmp_path, "dates")

    plot.time_axis().set_tick_labels({1.8e9: "start"})

    assert plot.time_axis().tick_labels() == {}
    assert _render(plot, tmp_path, "after") == dates
