"""`name` was a Python property until 0.53: `x.name = "s"` then hid the method and renamed nothing."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot

X = np.linspace(0.0, 10.0, 20)


def _graph(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    graph = plot.plot(X, X)
    graph.host = plot  # keeps the plot, and so the graph, alive
    return graph


def _plot(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    return plot


def _panel(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    return panel


@pytest.mark.parametrize("build", [_graph, _plot, _panel], ids=["graph", "plot", "panel"])
def test_assigning_name_warns_at_the_caller_and_renames(qtbot, build):
    obj = build(qtbot)
    with pytest.warns(DeprecationWarning, match=r"x\.name = \.\.\. is deprecated: use x\.set_name") as seen:
        obj.name = "renamed"
    assert obj.name() == "renamed" and seen[0].filename == __file__


def test_other_attributes_still_land_on_the_object(qtbot):
    plot = _plot(qtbot)
    plot.my_tag = 3
    assert plot.my_tag == 3


def test_a_subclass_with_its_own_name_keeps_it(qtbot):
    class Named(SciQLopPlot):
        name = "fixed"

    plot = Named()
    qtbot.addWidget(plot)
    plot.name = "mine"
    assert plot.name == "mine"
