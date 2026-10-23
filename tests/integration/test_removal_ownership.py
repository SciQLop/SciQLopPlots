"""Removals and moves after constructor parenting (widget-ownership design, change 2).

No bound removal keeps the widget today: remove_plot, removeWidget / remove_widget and
remove_panel all destroy it. SciQLopPlotContainer's remove_plot(plot, destroy) and
removeWidget(widget, destroy) unparent without destroying, but are not bound.
"""
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


def _bound_functions_with_a_destroy_argument():
    found = set()
    for class_name, cls in vars(SciQLopPlotsBindings).items():
        if not isinstance(cls, type):
            continue
        for name, function in vars(cls).items():
            try:
                signature = get_signature(function)
            except Exception:  # noqa: BLE001 - not every attribute has a signature
                continue
            signatures = signature if isinstance(signature, list) else [signature] if signature else []
            if any(s is not None and "destroy" in s.parameters for s in signatures):
                found.add(f"{class_name}.{name}")
    return found


def test_every_removal_that_keeps_the_widget_is_listed():
    assert _bound_functions_with_a_destroy_argument() == DESTROY_FLAG_REMOVALS


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
