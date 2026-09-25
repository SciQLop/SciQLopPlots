"""A coloured refresh of another length keeps the colour scale shown.

set_data used to drop the old colours (other length), which hid the scale, and the
new colours showed it again: two graph_list_changed per pan, each one republishing
the inspector.
"""
import numpy as np

from SciQLopPlots import GraphType, SciQLopPlotRange
from conftest import process_events


def _coloured(start, stop):
    n = int(20 * (stop - start)) + 7  # every range has its own length
    t = np.linspace(start, stop, n)
    return {"data": [t, np.column_stack([np.sin(t), np.cos(t)])],
            "color": np.linspace(1.0, 2.0, n)}


def test_a_coloured_refresh_does_not_hide_and_show_the_scale(qtbot, plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(_coloured, labels=["a", "b"], graph_type=GraphType.Line)
    qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
    process_events()

    changes = []
    plot.graph_list_changed.connect(lambda: changes.append(1))
    plot.x_axis().set_range(SciQLopPlotRange(2.0, 15.0))
    qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
    qtbot.wait(100)
    process_events()
    assert len(np.asarray(graph.color_data())) == len(_coloured(2.0, 15.0)["color"])
    assert changes == []
    assert plot.z_axis().visible()
