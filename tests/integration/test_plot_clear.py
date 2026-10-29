"""API review 4.6: plot.clear() removes what plot.remove() takes; axes and panel-wide objects stay."""
import numpy as np
import shiboken6
from PySide6.QtCore import QPointF

from SciQLopPlots import SciQLopPlotRange
from conftest import off_thread, process_events

X = np.linspace(0.0, 10.0, 50)
R = SciQLopPlotRange


def _fill(plot):
    line = plot.plot(X, np.sin(X), labels=["a"])
    other = plot.plot(X, np.cos(X), labels=["b"])
    group = plot.group("g", [line, other])
    owned = [line, other, group, plot.colormap(X, X, np.outer(X, X)), plot.text("t", QPointF(5, 5)),
             plot.span(R(1.0, 2.0)), plot.vertical_line(3.0)]
    return owned


def test_clear_removes_everything_remove_takes(plot):
    owned = _fill(plot)
    plot.x_axis().set_label("time")
    plot.clear()
    process_events()
    assert [shiboken6.isValid(o) for o in owned] == [False] * len(owned)
    assert (plot.plottables(), plot.groups(), plot.x_axis().label()) == ([], [], "time")


def test_clear_keeps_panel_wide_objects(panel):
    plot, _ = panel.plot(X, np.sin(X))
    span = panel.vertical_span(R(1.0, 2.0))
    plot.clear()
    process_events()
    assert shiboken6.isValid(span) and plot.plottables() == []


def test_clear_from_a_worker_lands_on_the_gui_thread(qtbot, plot):
    owned = _fill(plot)
    off_thread(plot.clear)
    qtbot.waitUntil(lambda: not any(shiboken6.isValid(o) for o in owned), timeout=2000)
