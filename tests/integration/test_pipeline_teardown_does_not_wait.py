"""Destroying a graph must not wait for its data callback (SciQLop issue #137).

``DataProviderWorker`` joined its worker thread when the pipeline was destroyed, on the
GUI thread. ``quit()`` only takes effect once the worker's event loop gets control back,
and it is inside the Python callback: closing a panel froze the GUI for as long as a
long HTTP read lasted. The worker thread now frees itself once the callback returns and
the late result goes nowhere.
"""
import threading
import time

import numpy as np
import pytest
from PySide6.QtCore import QtMsgType, qInstallMessageHandler
from PySide6.QtWidgets import QApplication


class ParkedCallable:
    """Parks in the worker thread until released; the bound keeps a failing test
    from hanging the suite (it then shows as a slow destroy)."""

    def __init__(self, hold=5.0):
        self.entered = threading.Event()
        self.release = threading.Event()
        self.finished = threading.Event()
        self.hold = hold
        self.calls = 0

    def __call__(self, start, stop):
        self.calls += 1
        self.entered.set()
        self.release.wait(self.hold)
        x = np.linspace(start, stop, 16, dtype=np.float64)
        self.finished.set()
        return x, np.sin(x)


def _pump_until(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while not predicate() and time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)
    return predicate()


@pytest.fixture
def parked():
    callables = []

    def make(**kwargs):
        cb = ParkedCallable(**kwargs)
        callables.append(cb)
        return cb

    yield make
    for cb in callables:
        cb.release.set()
    _pump_until(lambda: all(cb.finished.is_set() or not cb.entered.is_set() for cb in callables),
                timeout=3.0)


@pytest.fixture
def qt_messages():
    messages = []

    def handler(mode, context, message):
        messages.append((mode, message))

    previous = qInstallMessageHandler(handler)
    yield messages
    qInstallMessageHandler(previous)


def _destroy_seconds(plot, graph):
    start = time.monotonic()
    plot.remove_plottable(graph)
    return time.monotonic() - start


class TestDestroyWhileTheCallbackRuns:
    def test_removing_the_graph_returns_at_once(self, plot, parked):
        cb = parked()
        graph = plot.plot(cb, labels=["sig"])
        assert cb.entered.wait(5.0), "the callback never started"
        assert _destroy_seconds(plot, graph) < 1.0

    def test_deleting_the_plot_returns_at_once(self, qtbot, parked):
        from shiboken6 import delete
        from SciQLopPlots import SciQLopPlot
        plot = SciQLopPlot()
        cb = parked()
        plot.plot(cb, labels=["sig"])
        assert cb.entered.wait(5.0), "the callback never started"
        start = time.monotonic()
        delete(plot)
        assert time.monotonic() - start < 1.0

    def test_the_callback_runs_to_completion_and_nothing_is_delivered(
            self, plot, parked, qt_messages):
        cb = parked()
        graph = plot.plot(cb, labels=["sig"])
        assert cb.entered.wait(5.0), "the callback never started"
        plot.remove_plottable(graph)
        cb.release.set()
        assert _pump_until(cb.finished.is_set), "the callback was cut short"
        _pump_until(lambda: False, timeout=0.3)
        assert cb.calls == 1, "the late result triggered another fetch"
        bad = [m for mode, m in qt_messages
               if mode in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg)
               and "QThread" in m]
        assert bad == []

    def test_a_graph_that_is_not_fetching_is_still_removed_cleanly(self, plot, parked):
        cb = parked(hold=0.0)
        graph = plot.plot(cb, labels=["sig"])
        assert cb.finished.wait(5.0)
        _pump_until(lambda: not graph.busy(), timeout=3.0)
        assert _destroy_seconds(plot, graph) < 1.0
