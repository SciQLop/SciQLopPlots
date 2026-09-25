"""A coloured batch reaching a graph that cannot draw a colour axis.

Only line graphs and projections used to listen to new_data_colored: anything else
dropped the whole batch without a word (and a remote graph stayed busy). Now curves
take the colour too, and the other graphs keep the data, drop the colour and warn.
"""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import GraphType, SciQLopPlotRange
from conftest import process_events

N = 50


def _xy():
    x = np.linspace(10.0, 20.0, N)
    return [x, np.sin(x)]


def _xyz():
    x = np.linspace(10.0, 20.0, 20)
    y = np.linspace(0.0, 5.0, 10)
    return [x, y, np.random.rand(len(y), len(x))]


def _channel(plot, factory):
    graph = factory(plot)
    QApplication.processEvents()
    return graph, graph.remote_channel()


class TestCurve:
    def test_a_callback_curve_takes_a_coloured_batch(self, qtbot, plot):
        def coloured(start, stop):
            t = np.linspace(start, stop, N)
            return {"data": [np.cos(t), np.sin(t)], "color": np.linspace(1.0, 2.0, N)}

        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        curve = plot.plot(coloured, labels=["c"], graph_type=GraphType.ParametricCurve)
        qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
        assert curve.time_color_enabled()

    def test_a_remote_curve_takes_a_coloured_batch_and_clears_busy(self, qtbot, plot):
        curve, channel = _channel(plot, lambda p: p.add_remote_curve(["c"]))
        channel.data_requested.connect(
            lambda _r: channel.set_data_colored(_xy(), np.linspace(1.0, 2.0, N)))
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
        qtbot.waitUntil(lambda: curve.busy() is False, timeout=5000)


REMOTE_WITHOUT_COLOUR = [
    pytest.param(lambda p: p.add_remote_waterfall(["w"]), _xy, id="waterfall"),
    pytest.param(lambda p: p.add_remote_color_map("cm"), _xyz, id="colormap"),
    pytest.param(lambda p: p.add_remote_histogram2d("h"), _xy, id="histogram2d"),
]


@pytest.mark.parametrize("factory,make_data", REMOTE_WITHOUT_COLOUR)
def test_a_graph_without_colour_keeps_the_data_and_warns(qtbot, qtlog, plot, factory, make_data):
    graph, channel = _channel(plot, factory)
    data = make_data()
    channel.data_requested.connect(
        lambda _r: channel.set_data_colored(data, np.linspace(1.0, 2.0, len(data[0]))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: graph.busy() is False, timeout=5000)
    process_events()
    assert len(graph.data()) > 0 and len(np.asarray(graph.data()[0])) > 0
    assert any("colour" in r.message for r in qtlog.records)


def test_a_callback_colormap_keeps_the_data_of_a_coloured_batch(qtbot, qtlog, plot):
    def coloured(start, stop):
        data = _xyz()
        return {"data": data, "color": np.linspace(1.0, 2.0, len(data[0]))}

    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    cmap = plot.plot(coloured, graph_type=GraphType.ColorMap)
    qtbot.waitUntil(lambda: len(cmap.data()) > 0 and len(np.asarray(cmap.data()[0])) > 0,
                    timeout=5000)
    assert any("colour" in r.message for r in qtlog.records)
