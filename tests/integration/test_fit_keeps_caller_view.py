"""A fit announced by a batch runs later: queued, then until NeoQCP shows the batch.

A caller who moved a key axis in that window chose a view newer than the batch. The fit used
to snap the axis back to the batch's range, which also superseded the caller's own fetch.

Each test moves the axis from a direct slot on the graph's ``data_changed``: it runs inside
``set_data``, after the plot read the key ranges and before the queued fit.
"""
import threading

import numpy as np
import pytest
from PySide6.QtCore import QCoreApplication

from SciQLopPlots import ProjectionAxisLinking, SciQLopNDProjectionPlot


def _range(axis):
    r = axis.range()
    return r.start(), r.stop()


def _covers(axis, value):
    r = axis.range()
    return r.start() <= value <= r.stop()


def _drain():
    """Runs the queued fits without letting a worker's fetch land (no sleep)."""
    for _ in range(20):
        QCoreApplication.processEvents()


def _move_once(axis, window, when=lambda: True):
    moved = []

    def move():
        if not moved and when():
            moved.append(True)
            axis.set_range(*window)
    return move, moved


@pytest.fixture
def held():
    """Holds the fetches of the caller's view, so only the first batch can be fitted."""
    event = threading.Event()
    yield event
    event.set()


def test_x_moved_after_the_first_batch_survives_its_fit(qtbot, plot, held):
    def series(start, stop):
        if start > 0:
            held.wait(5)
        return np.linspace(start, stop, 10), np.full(10, 5.0)

    plot.y_axis().set_range(0, 1)
    g = plot.line(series)
    move, moved = _move_once(plot.x_axis(), (5, 15))
    g.data_changed.connect(move)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: _covers(plot.y_axis(), 5.0), timeout=3000)  # the fit ran
    assert moved
    assert _range(plot.x_axis()) == (5, 15)


def test_x2_moved_after_the_first_batch_survives_its_fit(qtbot, plot):
    def series(start, stop):
        return np.linspace(start, stop, 10), np.full(10, 5.0)

    plot.y_axis().set_range(0, 1)
    g = plot.line(series)
    g.set_x_axis(plot.x2_axis())
    move, moved = _move_once(plot.x2_axis(), (5, 15))
    g.data_changed.connect(move)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: _covers(plot.y_axis(), 5.0), timeout=3000)  # the fit ran
    assert moved
    assert _range(plot.x2_axis()) == (5, 15)


def test_a_batch_moved_off_view_leaves_the_first_fit_to_the_next_one(qtbot, plot):
    """The fit finds none of the batch in view, so y has nothing to fit: the caller's own
    batch must still get the first-data fit."""
    def series(start, stop):
        return np.linspace(start, stop, 10), np.full(10, 5.0)

    plot.y_axis().set_range(0, 1)
    g = plot.line(series)
    move, moved = _move_once(plot.x_axis(), (20, 30))
    g.data_changed.connect(move)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: _covers(plot.y_axis(), 5.0), timeout=3000)
    assert _range(plot.x_axis()) == (20, 30)


def test_colour_map_first_fit_keeps_a_moved_x(qtbot, plot, held):
    def series(start, stop):
        if start > 0:
            held.wait(5)
        return np.linspace(start, stop, 10), np.arange(4.0), np.full((10, 4), 5.0)

    cmap = plot.colormap(series)
    move, moved = _move_once(plot.x_axis(), (5, 15))
    cmap.data_changed.connect(move)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: bool(moved), timeout=3000)
    _drain()
    assert _range(plot.x_axis()) == (5, 15)


def test_colour_map_auto_scale_keeps_a_moved_x(qtbot, plot, held):
    def series(start, stop):
        if start >= 25:
            held.wait(5)
        return np.linspace(start, stop, 10), np.arange(4.0), np.full((10, 4), 5.0)

    cmap = plot.colormap(series)
    plot.set_auto_scale(True)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: len(cmap.data()) > 0 and np.asarray(cmap.data()[0])[0] == 0, timeout=3000)
    _drain()
    move, moved = _move_once(plot.x_axis(), (25, 35),
                             when=lambda: np.asarray(cmap.data()[0])[0] == 20)
    cmap.data_changed.connect(move)
    plot.x_axis().set_range(20, 30)
    qtbot.waitUntil(lambda: bool(moved), timeout=3000)
    _drain()
    assert _range(plot.x_axis()) == (25, 35)


def test_same_scale_projection_keeps_a_moved_x(qtbot):
    """A pane of a SameScale projection fits only with all the others: with a key axis
    moved, it must fit nothing rather than everything."""
    proj = SciQLopNDProjectionPlot(2)
    qtbot.addWidget(proj)
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    pane = proj.subplot(0)
    t = np.linspace(0, 1, 100)
    g = proj.parametric_curve([np.cos(6 * t), np.sin(6 * t)] * 2)
    panes_with_data = []
    move, moved = _move_once(pane.x_axis(), (5, 15), when=lambda: len(panes_with_data) == 2)
    g.data_changed.connect(lambda: panes_with_data.append(True))
    g.data_changed.connect(move)  # after every pane's batch: a move inside the batch is older
    qtbot.waitUntil(lambda: bool(moved), timeout=3000)
    _drain()
    assert _range(pane.x_axis()) == (5, 15)
