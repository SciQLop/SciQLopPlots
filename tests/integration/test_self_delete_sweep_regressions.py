"""One case per root cause the self-delete sweep (tests/sweeps/self_delete) found: a slot deletes the
object from a signal its setter emits while the setter still runs."""
import numpy as np
import pytest
import shiboken6
from PySide6.QtGui import QColor, QPen

from SciQLopPlots import ColorGradient, Coordinates, GraphType, SciQLopPlotRange
from conftest import process_events

X = np.linspace(0.0, 10.0, 50)
LINES = np.column_stack([np.sin(X), np.cos(X), np.sin(2 * X)])
SCALARS = np.linspace(1.0, 2.0, len(X))


@pytest.fixture
def shown(plot):
    plot.resize(600, 400)
    plot.show()
    process_events()
    return plot


def _pump(n=20):
    for _ in range(n):
        process_events()


def _delete_once(target):
    fired = []

    def slot(*_):
        if not fired and shiboken6.isValid(target):
            fired.append(1)
            shiboken6.delete(target)
    return slot, fired


def _lines(p):
    return p.plot(X, LINES, labels=["a", "b", "c"])


def _waterfall(p):
    return p.plot(X, LINES, labels=["a", "b", "c"], graph_type=GraphType.Waterfall)


def _big_waterfall(p):
    """Enough points to resample, so a re-layout runs the async pipeline and flips busy."""
    x = np.linspace(0.0, 10.0, 60_000)
    return p.plot(x, np.column_stack([np.sin(x), np.cos(x)]), labels=["a", "b"],
                  graph_type=GraphType.Waterfall)


def _curve(p):
    return p.parametric_curve(np.cos(X), np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])


# (builder, signal, setter): the slot on the built object's signal deletes it.
GRAPH_CASES = {
    "busy:line_set_busy": (_lines, "busy_changed", lambda g: g.set_busy(True)),
    "busy:histogram_set_busy": (lambda p: p.histogram(np.sin(X)), "busy_changed", lambda g: g.set_busy(True)),
    "busy:histogram2d_set_data": (lambda p: p.histogram2d(X, np.sin(X)), "busy_changed",
                                  lambda g: g.set_data(X, np.cos(X))),
    "busy:curve_set_busy": (_curve, "busy_changed", lambda g: g.set_busy(True)),
    "busy:waterfall_set_gain": (_big_waterfall, "busy_changed", lambda g: g.set_gain(g.gain() + 1.0)),
    "busy:histogram2d_set_bins": (lambda p: p.histogram2d(X, np.sin(X)), "busy_changed",
                                  lambda g: g.set_bins(7, 7)),
    "busy:histogram2d_set_x_bins_log": (lambda p: p.histogram2d(X + 1.0, np.sin(X)), "busy_changed",
                                        lambda g: g.set_x_bins_log(True)),
    "histogram:request_rescale": (lambda p: p.histogram(np.sin(X)), "request_rescale", lambda g: None),
    "histogram:bins_changed": (lambda p: p.histogram(np.sin(X)), "bins_changed", lambda g: None),
    "histogram:set_values": (lambda p: p.histogram(np.sin(X)), "request_rescale",
                             lambda g: g.set_values(np.cos(X))),
    "data:line_replot": (_lines, "replot", lambda g: g.set_data(X, LINES)),
    "data:line_data_changed": (_lines, "data_changed", lambda g: g.set_data(X, LINES)),
    "data:line_data_and_color": (_lines, "data_changed", lambda g: g.set_data_and_color([X, LINES], SCALARS)),
    "data:waterfall_replot": (_waterfall, "replot", lambda g: g.set_data(X, LINES)),
    "data:curve_replot": (_curve, "replot",
                          lambda g: g.set_data(np.cos(X), np.column_stack([np.sin(X), np.cos(X)]))),
    "color:gradient": (_lines, "replot", lambda g: g.set_color_gradient(ColorGradient.Hot)),
    "color:data": (_lines, "replot", lambda g: g.set_color_data(SCALARS, ColorGradient.Hot)),
    "color:line_set_colors": (_lines, "replot", lambda g: g.set_colors([QColor("red")] * 3)),
    "color:curve_set_colors": (_curve, "replot", lambda g: g.set_colors([QColor("red")] * 2)),
    "span:set_coordinates": (lambda p: p.vertical_span(SciQLopPlotRange(1.0, 2.0)), "range_changed",
                             lambda s: s.set_coordinates(Coordinates.Pixels)),
}


@pytest.mark.parametrize("name", GRAPH_CASES)
def test_a_slot_deleting_the_object_while_its_setter_runs_does_not_crash(qtbot, shown, name):
    make, signal, mutate = GRAPH_CASES[name]
    obj = make(shown)
    if name.startswith("busy:"):
        qtbot.waitUntil(lambda: not obj.busy(), timeout=5000)
    elif not name.startswith("histogram:"):
        _pump()
    slot, fired = _delete_once(obj)
    getattr(obj, signal).connect(slot)
    mutate(obj)
    qtbot.waitUntil(lambda: bool(fired), timeout=5000)  # a histogram bins on a worker thread
    _pump()
    assert not shiboken6.isValid(obj)


COMPONENT_SETTERS = {
    "set_pen": lambda c: c.set_pen(QPen(QColor("red"), 3)),
    "set_color": lambda c: c.set_color(QColor("red")),
}


@pytest.mark.parametrize("target", ["component", "graph"])
@pytest.mark.parametrize("setter", COMPONENT_SETTERS)
@pytest.mark.parametrize("make", [_lines, _waterfall, _curve], ids=["line", "waterfall", "curve"])
def test_a_slot_deleting_a_component_or_its_graph_on_replot_does_not_crash(shown, make, setter, target):
    graph = make(shown)
    _pump()
    component = graph.components()[0]
    victim = component if target == "component" else graph
    slot, fired = _delete_once(victim)
    component.replot.connect(slot)
    COMPONENT_SETTERS[setter](component)
    _pump()
    assert fired
    assert not shiboken6.isValid(victim)


def test_a_slot_deleting_a_projection_while_it_sets_coloured_data_does_not_crash(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.show()
    proj = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(proj.components()) > 0 and not proj.busy(), timeout=5000)
    slot, fired = _delete_once(proj)
    proj.busy_changed.connect(slot)
    proj.set_data_and_color([X, X, np.sin(X), np.cos(X)], SCALARS)
    _pump()
    assert fired
    assert not shiboken6.isValid(proj)


def _projection(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.show()
    proj = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(proj.components()) > 0 and not proj.busy(), timeout=5000)
    return nd, proj


def _pane_curve(nd):
    (curve,) = nd.subplot(0).plottables()
    return curve


PROJECTION_COLOR_SETTERS = {
    "set_color_gradient": lambda p: p.set_color_gradient(ColorGradient.Hot),
    "set_color_data": lambda p: p.set_color_data(SCALARS, ColorGradient.Hot),
}


@pytest.mark.parametrize("setter", PROJECTION_COLOR_SETTERS)
def test_a_slot_deleting_a_projection_from_a_pane_curve_replot_does_not_crash(qtbot, setter):
    nd, proj = _projection(qtbot)
    slot, fired = _delete_once(proj)
    _pane_curve(nd).replot.connect(slot)
    PROJECTION_COLOR_SETTERS[setter](proj)
    _pump()
    assert fired
    assert not shiboken6.isValid(proj)


# Found by opencode (round 3): deletions the sweep's (setter, signal) pairs didn't reach.
def test_a_slot_deleting_the_plot_from_its_x_axis_range_signal_does_not_crash(qapp):
    from SciQLopPlots import SciQLopPlot
    plot = SciQLopPlot()  # not a fixture: the test deletes it
    plot.show()
    plot.line(X, np.sin(X))
    _pump()
    slot, fired = _delete_once(plot)
    plot.x_axis_range_changed.connect(slot)
    plot.x_axis().set_range(SciQLopPlotRange(2.0, 5.0))
    _pump()
    assert fired and not shiboken6.isValid(plot)


def test_a_slot_deleting_the_panel_from_plot_added_does_not_crash(qapp):
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel()
    panel.show()
    _pump()
    slot, fired = _delete_once(panel)
    panel.plot_added.connect(slot)
    try:
        panel.plot(X, np.sin(X))
    except RuntimeError:
        pass  # the panel is gone before the call can return its plot: refusing is fine
    _pump()
    assert fired and not shiboken6.isValid(panel)



# Found by opencode's branch review (Task 10): two emits in a row, the panel deleted by the first slot.
@pytest.mark.parametrize("signal, act", [
    ("plot_removed", lambda panel: panel.remove_plot(panel.plot_at(0))),
    ("plot_list_changed", lambda panel: panel.move_plot(0, 1)),
], ids=["remove_plot", "move_plot"])
def test_a_slot_deleting_the_panel_between_two_emits_does_not_crash(qapp, signal, act):
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel()  # not a fixture: the test deletes it
    panel.show()
    panel.plot(X, np.sin(X))
    panel.plot(X, np.cos(X))
    _pump()
    slot, fired = _delete_once(panel)
    getattr(panel, signal).connect(slot)
    act(panel)
    _pump()
    assert fired and not shiboken6.isValid(panel)


# M3 (final review): a plot_added slot deleting the new plot itself, not the panel.
def test_a_slot_deleting_the_new_plot_from_plot_added_does_not_crash(qapp):
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel()
    panel.show()
    deleted = []

    def slot(plot):
        if not deleted:
            deleted.append(1)
            shiboken6.delete(plot)
    panel.plot_added.connect(slot)
    try:
        panel.plot(X, np.sin(X))
    except RuntimeError:
        pass  # the plot is gone before the call can return it: refusing is fine
    _pump()
    assert deleted and shiboken6.isValid(panel)
    shiboken6.delete(panel)
