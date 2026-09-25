"""A remote graph's busy flag always ends, also when no data comes back.

Busy goes on when a request goes out and used to go off only when data of the right
shape arrived. A worker error, an empty answer, or a range the provider already had
left the graph faded for good.
"""
import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlotRange
from conftest import process_events


def _remote_line(plot):
    graph = plot.add_remote_line_graph(["B"])
    QApplication.processEvents()
    return graph, graph.remote_channel()


def test_request_done_ends_a_request_without_data(qtbot, plot):
    graph, channel = _remote_line(plot)
    requests = []
    channel.data_requested.connect(lambda r: requests.append(r))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert graph.busy() is True
    channel.request_done()
    qtbot.waitUntil(lambda: graph.busy() is False, timeout=2000)


def test_an_empty_answer_ends_the_request(qtbot, plot):
    graph, channel = _remote_line(plot)
    channel.data_requested.connect(lambda _r: channel.set_data([]))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: graph.busy() is False, timeout=2000)


def test_coming_back_to_the_current_range_ends_the_request(qtbot, plot):
    graph, channel = _remote_line(plot)
    x = np.linspace(10.0, 20.0, 10)
    requests = []

    def answer(r):
        requests.append((r.start(), r.stop()))
        channel.set_data(x, x)

    channel.data_requested.connect(answer)
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: graph.busy() is False and len(requests) > 0, timeout=2000)
    # Both within the provider's coalescing window: it sees 10..20 again, which it has.
    plot.x_axis().set_range(SciQLopPlotRange(30.0, 40.0))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.wait(200)
    process_events()
    assert graph.busy() is False
