"""A stale fetch result used up a graph's one-time "fit on first data" (SciQLop).

The panel moves from range A to range B while A's fetch is still running. A's
result lands first, outside the view: it consumed the first-data rescale, which
found nothing visible to fit. B's result then never fitted the y axis.
"""
import threading
import time

import numpy as np
import pytest

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, SciQLopPlotRange

A = (1.0e9, 1.0e9 + 3600)
B = (1.6e9, 1.6e9 + 3600)


def _covers(plot, low, high):
    y = plot.y_axis().range()
    assert y.start() <= low and y.stop() >= high, (y.start(), y.stop())


@pytest.fixture
def panel(qtbot):
    p = SciQLopMultiPlotPanel(None, synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(p)
    return p


def test_the_first_visible_batch_fits_the_y_axis(qtbot, panel):
    fetching_a = threading.Event()
    done = []

    def series(start, stop):
        on_a = start < A[1]
        if on_a:
            fetching_a.set()
            time.sleep(0.5)
        x = np.linspace(start, stop, 100)
        y = np.full(100, 1000.0) if on_a else np.linspace(-50.0, 80.0, 100)
        done.append("A" if on_a else "B")
        return [x, y]

    panel.set_time_axis_range(SciQLopPlotRange(*A))
    plot, _ = panel.plot(series, labels=["v"], plot_type=PlotType.TimeSeries)
    qtbot.waitUntil(fetching_a.is_set, timeout=5000)
    panel.set_time_axis_range(SciQLopPlotRange(*B))
    qtbot.waitUntil(lambda: done == ["A", "B"], timeout=5000)
    # The fit waits for NeoQCP to swap the batch in: up to its 1 s cap.
    qtbot.waitUntil(lambda: _covers(plot, -50.0, 80.0), timeout=3000)


def test_auto_scale_fits_each_new_batch_not_the_one_it_replaces(qtbot, panel):
    """Same root: a rendered graph shows a new batch only after NeoQCP's swap window,
    so a rescale on data_changed used to fit the batch being replaced."""
    done = []

    def series(start, stop):
        on_a = start < A[1]
        done.append("A" if on_a else "B")
        y = np.linspace(0.0, 1.0, 100) if on_a else np.linspace(100.0, 200.0, 100)
        return [np.linspace(start, stop, 100), y]

    panel.set_time_axis_range(SciQLopPlotRange(*A))
    plot, _ = panel.plot(series, labels=["v"], plot_type=PlotType.TimeSeries)
    plot.set_auto_scale(True)
    qtbot.waitUntil(lambda: done == ["A"], timeout=5000)
    qtbot.waitUntil(lambda: _covers(plot, 0.0, 1.0), timeout=3000)
    panel.set_time_axis_range(SciQLopPlotRange(*B))
    qtbot.waitUntil(lambda: done == ["A", "B"], timeout=5000)
    qtbot.waitUntil(lambda: _covers(plot, 100.0, 200.0), timeout=3000)


def test_a_view_set_after_the_first_batch_survives_its_fit(qtbot, plot):
    """The first-data fit runs after the batch arrived (queued, then until NeoQCP shows it).
    The caller moved x in between: that view is newer than the batch. The fit used to
    snap x back to the batch's range, and the fetch for the caller's view never ran."""
    later_fetch = threading.Event()

    def series(start, stop):
        if start > 0:
            later_fetch.wait(5)  # keep the caller's batch out of the way of the first fit
        return np.linspace(start, stop, 10), np.full(10, 5.0)

    plot.y_axis().set_range(0, 1)
    g = plot.line(series)
    moved = []

    def move_once():
        if not moved:
            moved.append(True)
            plot.x_axis().set_range(5, 15)
    g.data_changed.connect(move_once)  # direct: runs before the queued fit
    plot.x_axis().set_range(0, 10)
    try:
        qtbot.waitUntil(lambda: _covers(plot, 5.0, 5.0), timeout=3000)  # the fit ran
        x = plot.x_axis().range()
        assert (x.start(), x.stop()) == (5, 15)
    finally:
        later_fetch.set()
