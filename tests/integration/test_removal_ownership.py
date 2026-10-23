"""Removals and moves after constructor parenting (widget-ownership design, change 2).

No bound removal keeps the widget today: remove_plot, removeWidget / remove_widget and
remove_panel all destroy it. SciQLopPlotContainer's remove_plot(plot, destroy) and
removeWidget(widget, destroy) unparent without destroying, but are not bound.
"""
import enum
import sys
import weakref

import pytest
import shiboken6

from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotsBindings
from shibokensupport.signature import get_signature

# Binding a removal that keeps the widget? Mark it <parent index="1" action="remove"/> (the C++
# unparents it, so Python owns it again, as with setParent(None)), list it here, and test that the
# kept widget is Python-owned afterwards.
DESTROY_FLAG_REMOVALS = set()


def _signatures(function):
    signature = get_signature(function)
    return signature if isinstance(signature, list) else [signature] if signature else []


def _wrapped_original(function):
    """The C++ method a Python wrapper (SciQLopPlots/__init__.py) delegates to, if it keeps one."""
    if hasattr(function, "__wrapped__"):
        return function.__wrapped__
    cells = [c.cell_contents for c in getattr(function, "__closure__", None) or []]
    return next((c for c in cells if _signatures(c)), None)


def _walk_bound_functions():
    """-> (functions seen with their signatures, names of Python wrappers whose original is hidden)."""
    seen, hidden = {}, set()
    for class_name, cls in vars(SciQLopPlotsBindings).items():
        if not isinstance(cls, type) or issubclass(cls, enum.Enum):
            continue
        for name, function in vars(cls).items():
            if not callable(function):
                continue
            signatures = _signatures(function) or _signatures(_wrapped_original(function))
            if signatures:
                seen[f"{class_name}.{name}"] = signatures
            elif type(function).__name__ == "function" and name != "__init__":
                hidden.add(f"{class_name}.{name}")
    return seen, hidden


# Python wrappers that keep no reference to the C++ original, so the walk cannot see their parameters.
HIDDEN_WRAPPERS = {
    "SciQLopPlot.plot", "SciQLopPlot.band", "SciQLopPlot.error_bars",
    "SciQLopTimeSeriesPlot.plot", "SciQLopNDProjectionPlot.plot",
    "SciQLopMultiPlotPanel.plot", "SciQLopMultiPlotPanel.band", "SciQLopMultiPlotPanel.error_bars",
    "SciQLopTimeline.set_intervals", "SciQLopTimeline.interval", "SciQLopTimeline.interval_at",
}


def _bound_functions_with_a_destroy_argument(seen):
    return {name for name, signatures in seen.items() if any("destroy" in s.parameters for s in signatures)}


def test_the_walk_sees_the_bindings():
    seen, hidden = _walk_bound_functions()
    parents = [n for n, sigs in seen.items() if any("parent" in s.parameters for s in sigs)]
    assert len(seen) > 1000 and len(parents) > 20
    assert "SciQLopMultiPlotPanel.remove_plot" in seen and "SciQLopMultiPlotPanel.add_plot" in seen
    assert hidden == HIDDEN_WRAPPERS


def test_every_removal_that_keeps_the_widget_is_listed():
    seen, _ = _walk_bound_functions()
    assert _bound_functions_with_a_destroy_argument(seen) == DESTROY_FLAG_REMOVALS


def test_a_broken_walk_fails_the_positive_control(monkeypatch):
    monkeypatch.setattr(sys.modules[__name__], "get_signature", lambda function: None)
    with pytest.raises(AssertionError):
        test_the_walk_sees_the_bindings()


@pytest.mark.parametrize("remove", ["remove_plot", "removeWidget"])  # remove_widget is the same function (snake alias)
def test_a_removal_deletes_a_plot_built_with_the_panel_as_parent(qtbot, panel, remove):
    plot = SciQLopPlot(panel)
    panel.add_plot(plot)
    gone = on_destroyed(plot)
    getattr(panel, remove)(plot)
    collect()
    assert (gone, shiboken6.isValid(plot)) == ([True], False)


def test_remove_panel_deletes_a_sub_panel_built_with_the_panel_as_parent(qtbot, panel):
    sub = SciQLopMultiPlotPanel(panel)
    panel.add_panel(sub)
    gone = on_destroyed(sub)
    panel.remove_panel(sub)
    collect()
    assert gone == [True]


class _TimeSyncLike(SciQLopMultiPlotPanel):
    """SciQLop's TimeSyncPanel shape: Python state, released through destroyed, not the GC."""
    released = []

    def __init__(self, parent):
        super().__init__(parent, synchronize_x=False, synchronize_time=True)
        self.state = ["kept"]
        self.destroyed.connect(lambda *_: _TimeSyncLike.released.append(True))


def test_a_parented_python_subclass_keeps_its_state_until_removed(qtbot, panel):
    _TimeSyncLike.released.clear()
    alive = weakref.ref(_TimeSyncLike(panel))
    collect()
    assert alive() is not None and alive().state == ["kept"]
    panel.add_panel(alive())
    panel.remove_panel(alive())
    collect()
    assert (_TimeSyncLike.released, alive()) == ([True], None)


def test_a_plot_moved_to_another_panel_outlives_the_first(qtbot):
    first, second = SciQLopMultiPlotPanel(), SciQLopMultiPlotPanel()
    qtbot.addWidget(second)
    plot = SciQLopPlot(first)
    first.add_plot(plot)
    second.add_plot(plot)
    gone = on_destroyed(plot)
    del plot
    shiboken6.delete(first)
    collect()
    assert (gone, len(second.plots())) == ([], 1)
