"""Panning a plot with several callable graphs: every graph reports busy while
its fetch runs, and once the fetches return, every graph shows the new range.
Pins the deferred data swap end to end (NeoQCP stages the replacement source,
the plot commits all of them in one window)."""
import threading
import time

import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlotRange


class BlockingCallable:
    def __init__(self):
        self.entered = threading.Event()
        self.release = threading.Event()
        self.calls = 0

    def __call__(self, start, stop):
        self.calls += 1
        self.entered.set()
        self.release.wait(5.0)
        x = np.linspace(start, stop, 200_000, dtype=np.float64)
        y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
        return x, y

    def unblock(self):
        self.release.set()


def _pump_until(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while not predicate() and time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)
    return predicate()


def _first_key(graph):
    data = graph.data()
    if len(data) < 2 or data[0] is None:
        return None
    x = np.asarray(data[0])
    return float(x[0]) if x.size else None


def test_pan_keeps_all_graphs_busy_then_swaps_every_graph(plot):
    providers = [BlockingCallable() for _ in range(3)]
    for p in providers:
        p.unblock()
    graphs = [plot.plot(p, labels=["a", "b"]) for p in providers]
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    assert _pump_until(lambda: all(not g.busy() and _first_key(g) == 0.0 for g in graphs),
                       timeout=10.0)

    for p in providers:
        p.release.clear()
        p.entered.clear()
    plot.x_axis().set_range(SciQLopPlotRange(100.0, 110.0))
    assert all(p.entered.wait(5.0) for p in providers)
    assert all(g.busy() for g in graphs)
    assert all(_first_key(g) == 0.0 for g in graphs)

    for p in providers:
        p.unblock()
    assert _pump_until(lambda: all(not g.busy() and _first_key(g) == 100.0 for g in graphs),
                       timeout=10.0)
