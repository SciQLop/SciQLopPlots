"""Removing a curve must not wait for its resampler (SciQLopPlots issue #109).

``SciQLopCurve`` joined its resampler thread on destruction, on the GUI thread: removing
a curve, or closing its panel, froze the GUI for as long as the running resample lasted.
Same family as SciQLop issue #137, fixed for data pipelines. The resampler thread now
frees itself once the job ends and its result goes nowhere.

The resampler cannot be parked from Python, so the tests compare against the time one
resample takes on the same data: a waiting teardown costs about that much.
"""
import time

import numpy as np
import pytest
from PySide6.QtCore import QtMsgType, qInstallMessageHandler
from PySide6.QtWidgets import QApplication

N = 5_000_000


def _pump_until(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while not predicate() and time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)
    return predicate()


@pytest.fixture(scope="module")
def data():
    x = np.arange(N, dtype=np.float64)
    return x, np.column_stack([np.sin(x * 1e-4), np.cos(x * 1e-4)])


@pytest.fixture
def qt_messages():
    messages = []

    def handler(mode, context, message):
        messages.append((mode, message))

    previous = qInstallMessageHandler(handler)
    yield messages
    qInstallMessageHandler(previous)


def _resample_seconds(curve, data):
    start = time.monotonic()
    curve.set_data(*data)
    assert _pump_until(lambda: not curve.busy(), timeout=10.0)
    return time.monotonic() - start


def _curve(plot, data):
    curve = plot.parametric_curve(*data, labels=["a", "b"])
    assert _pump_until(lambda: not curve.busy(), timeout=10.0)
    return curve


class TestRemoveWhileTheResamplerRuns:
    def test_removing_the_curve_returns_before_the_resample_ends(self, plot, data):
        curve = _curve(plot, data)
        resample = _resample_seconds(curve, data)
        if resample < 0.05:
            pytest.skip(f"a resample takes {resample:.3f}s here, too fast to tell a wait apart")
        curve.set_data(*data)
        start = time.monotonic()
        plot.remove_plottable(curve)
        assert time.monotonic() - start < resample / 4

    def test_deleting_the_plot_returns_before_the_resample_ends(self, qtbot, data):
        from shiboken6 import delete
        from SciQLopPlots import SciQLopPlot
        plot = SciQLopPlot()
        curve = _curve(plot, data)
        resample = _resample_seconds(curve, data)
        if resample < 0.05:
            pytest.skip(f"a resample takes {resample:.3f}s here, too fast to tell a wait apart")
        curve.set_data(*data)
        start = time.monotonic()
        delete(plot)
        assert time.monotonic() - start < resample / 4

    def test_the_late_result_is_dropped_quietly(self, plot, data, qt_messages):
        curve = _curve(plot, data)
        resample = _resample_seconds(curve, data)
        curve.set_data(*data)
        plot.remove_plottable(curve)
        _pump_until(lambda: False, timeout=max(0.5, 3 * resample))
        assert plot.plottables() == []
        bad = [m for mode, m in qt_messages
               if mode in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg)
               and "QThread" in m]
        assert bad == []

    def test_an_idle_curve_is_still_removed_cleanly(self, plot, data):
        curve = _curve(plot, data)
        start = time.monotonic()
        plot.remove_plottable(curve)
        assert time.monotonic() - start < 1.0
        assert plot.plottables() == []
