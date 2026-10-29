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
    # The real check: only a fit of the caller's batch can fit y. A refit of x would land on
    # (20, 30) as well, so the x check cannot tell it from x being left alone.
    qtbot.waitUntil(lambda: _covers(plot.y_axis(), 5.0), timeout=3000)
    assert _range(plot.x_axis()) == (20, 30)


def test_colour_map_first_fit_keeps_a_moved_x(qtbot, plot, held):
    def series(start, stop):
        if start > 0:
            held.wait(5)
        return np.linspace(start, stop, 10), np.arange(4.0), np.full((10, 4), 5.0)

    plot.z_axis().set_range(0, 1)
    cmap = plot.colormap(series)
    move, moved = _move_once(plot.x_axis(), (5, 15))
    cmap.data_changed.connect(move)
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: _covers(plot.z_axis(), 5.0), timeout=3000)  # the fit ran
    assert moved
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


def _same_scale_projection(qtbot):
    proj = SciQLopNDProjectionPlot(2)
    qtbot.addWidget(proj)
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    t = np.linspace(0, 1, 100)
    data = [np.cos(6 * t), np.sin(6 * t)] * 2
    return proj, proj.parametric_curve(data), data


def _after_every_pane_batch(proj, slot, graph=0):
    """Calls `slot` once every pane's curve announced its batch: a move made between two
    panes' batches is older than the later one, so it is not the case under test."""
    curves = [proj.subplot(i).plottables()[graph] for i in range(proj.subplot_count())]
    landed = set()

    def on_batch(i):
        landed.add(i)
        if len(landed) == len(curves):
            slot()
    for i, curve in enumerate(curves):
        curve.data_changed.connect(lambda *_, i=i: on_batch(i))


def _fitted(pane):
    return all(_covers(axis, v) for axis in (pane.x_axis(), pane.y_axis()) for v in (-0.9, 0.9))


def test_same_scale_projection_keeps_a_moved_x(qtbot):
    """A pane of a SameScale projection fits only with all the others: with an axis
    moved, it must fit nothing rather than everything."""
    proj, g, _ = _same_scale_projection(qtbot)
    pane = proj.subplot(0)
    move, moved = _move_once(pane.x_axis(), (5, 15))
    _after_every_pane_batch(proj, move)
    qtbot.waitUntil(lambda: bool(moved), timeout=3000)
    _drain()
    assert _range(pane.x_axis()) == (5, 15)


@pytest.mark.parametrize("axis", ["x_axis", "y_axis"])
def test_same_scale_pane_that_skipped_its_fit_fits_the_next_batch(qtbot, axis):
    """The move keeps the batch in view, yet the pane fitted nothing: the first fit must
    not be used up. A value axis (y) moving counts too: the projection re-spans every axis."""
    proj, g, data = _same_scale_projection(qtbot)
    pane = proj.subplot(0)
    move, moved = _move_once(getattr(pane, axis)(), (-0.2, 0.2))
    _after_every_pane_batch(proj, move)
    qtbot.waitUntil(lambda: bool(moved), timeout=3000)
    _drain()
    assert not _fitted(pane)
    g.set_data(data)
    qtbot.waitUntil(lambda: _fitted(pane), timeout=3000)


def test_same_scale_panes_fitted_together_do_not_fit_again(qtbot):
    """The first pane's fit fits every pane and moves the others' axes. That is no caller
    move: their own fits are served by it, and a later batch must not refit a caller's zoom."""
    proj, g, data = _same_scale_projection(qtbot)
    pane = proj.subplot(0)
    qtbot.waitUntil(lambda: _fitted(pane), timeout=3000)
    _drain()
    pane.x_axis().set_range(-0.2, 0.2)
    zoomed = _range(pane.x_axis())
    landed = []
    _after_every_pane_batch(proj, lambda: landed.append(True))
    g.set_data(data)
    qtbot.waitUntil(lambda: bool(landed), timeout=3000)
    _drain()
    assert _range(pane.x_axis()) == zoomed


@pytest.mark.parametrize("announced, run", [
    (ProjectionAxisLinking.Independent, ProjectionAxisLinking.SameScale),
    (ProjectionAxisLinking.SameScale, ProjectionAxisLinking.Independent),
])
def test_a_first_fit_survives_a_linking_change_before_it_runs(qtbot, announced, run):
    """A pane counts its own fits, a SameScale projection counts its panes' fits together.
    A first fit announced under one linking and run under the other must still fit."""
    proj, _, _ = _same_scale_projection(qtbot)
    qtbot.waitUntil(lambda: _fitted(proj.subplot(0)), timeout=3000)
    _drain()
    proj.set_axis_linking(announced)
    t = np.linspace(0, 1, 100)
    proj.parametric_curve([10 + np.cos(6 * t), 10 + np.sin(6 * t)] * 2)
    toggled = []
    _after_every_pane_batch(proj, lambda: toggled or (toggled.append(True),
                                                      proj.set_axis_linking(run)), graph=1)
    qtbot.waitUntil(lambda: bool(toggled), timeout=3000)
    _drain()
    for i in range(proj.subplot_count()):
        pane = proj.subplot(i)
        assert all(_covers(axis, v) for axis in (pane.x_axis(), pane.y_axis())
                   for v in (9.1, 10.9)), (i, _range(pane.x_axis()), _range(pane.y_axis()))


def test_set_data_from_a_worker_fits_with_ranges_read_on_the_gui_thread(qtbot, plot):
    # The axes belong to the GUI thread: the fit's snapshot of them must not be taken on the worker.
    plot.set_auto_scale(True)
    graph = plot.plot(np.empty(0), np.empty(0))  # the worker's batch is the first data
    worker = threading.Thread(target=graph.set_data, args=(np.linspace(1000, 2000, 10), np.zeros(10)))
    worker.start()
    worker.join()
    qtbot.waitUntil(lambda: _covers(plot.x_axis(), 1500), timeout=5000)
