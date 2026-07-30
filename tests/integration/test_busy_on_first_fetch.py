"""A callable graph must report busy while its range-driven fetch is running.

Reproduces the "no activity marker right after a product drop" report: dropping
a product creates a callable graph whose first fetch is triggered by the graph's
own range_changed, and that path must raise the busy flag the same way the
remote-graph path does — otherwise nothing marks the graph as fetching, neither
on creation nor on any later pan.
"""
import threading
import time

import numpy as np
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlotRange


class BlockingCallable:
    """Callable that parks in the worker thread until released."""

    def __init__(self, n_components=1):
        self.entered = threading.Event()
        self.release = threading.Event()
        self.n_components = n_components

    def __call__(self, start, stop):
        self.entered.set()
        self.release.wait(5.0)
        x = np.linspace(start, stop, 16, dtype=np.float64)
        y = np.column_stack([np.sin(x)] * self.n_components).astype(np.float64)
        return x, y if self.n_components > 1 else np.sin(x)

    def unblock(self):
        self.release.set()


def _wait_for_fetch(callable_, timeout=5.0):
    assert callable_.entered.wait(timeout), "callable was never invoked"


def _pump_until(predicate, timeout=5.0):
    """Pump the event loop until predicate holds. A fixed iteration count is not
    enough: the worker thread finishes on its own schedule, so under load the
    result can land after the last processEvents()."""
    deadline = time.monotonic() + timeout
    while not predicate() and time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)
    return predicate()


@pytest.fixture
def unblock_all():
    created = []
    yield created
    for c in created:
        c.unblock()
    _pump_until(lambda: all(not c.entered.is_set() or c.release.is_set()
                            for c in created), timeout=2.0)


class TestBusyDuringFirstFetch:
    def test_single_line_graph_busy_while_fetching(self, plot, unblock_all):
        cb = BlockingCallable()
        unblock_all.append(cb)
        graph = plot.plot(cb, labels=["sig"])
        _wait_for_fetch(cb)
        assert graph.busy() is True

    def test_multi_line_graph_busy_while_fetching(self, plot, unblock_all):
        cb = BlockingCallable(n_components=3)
        unblock_all.append(cb)
        graph = plot.plot(cb, labels=["a", "b", "c"])
        _wait_for_fetch(cb)
        assert graph.busy() is True

    def test_busy_changed_emitted_on_first_fetch(self, plot, unblock_all):
        cb = BlockingCallable()
        unblock_all.append(cb)
        graph = plot.plot(cb, labels=["sig"])
        received = []
        graph.busy_changed.connect(received.append)
        _wait_for_fetch(cb)
        cb.unblock()
        assert _pump_until(lambda: False in received), \
            "busy never cleared after the fetch completed"

    def test_busy_raised_again_on_range_change(self, plot, unblock_all):
        cb = BlockingCallable()
        unblock_all.append(cb)
        graph = plot.plot(cb, labels=["sig"])
        _wait_for_fetch(cb)
        cb.unblock()
        assert _pump_until(lambda: graph.busy() is False), \
            "busy stayed raised after the fetch completed"

        cb.release.clear()
        cb.entered.clear()
        graph.set_range(SciQLopPlotRange(100.0, 200.0))
        _wait_for_fetch(cb)
        assert graph.busy() is True
