"""A projection graph added after the panel's time range was set must fetch at once
(SciQLop issue #141, "related gaps"). It used to wait for the next range change:
no error, `busy` false, an empty plot.

The time-series graph is the control: it already fetches when added.
"""
import numpy as np
import pytest

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, SciQLopPlotRange
from conftest import process_events

RANGE = (1.0e9, 1.0e9 + 3600)


def _series(calls=None):
    def series(start, stop):
        if calls is not None:
            calls.append((start, stop))
        return [np.linspace(start, stop, 10), np.zeros(10)]
    return series


@pytest.fixture
def panel(qtbot):
    """A panel that already shows a series, whose time range was then set: the
    situation a projection graph gets added into."""
    seen = []
    p = SciQLopMultiPlotPanel(None, synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(p)
    p.plot(_series(seen), labels=["first"], plot_type=PlotType.TimeSeries)
    qtbot.wait(200)  # the plot registers with the panel's time sync on a queued signal
    p.set_time_axis_range(SciQLopPlotRange(*RANGE))
    qtbot.waitUntil(lambda: any(c == pytest.approx(RANGE) for c in seen), timeout=5000)
    return p


def _trajectory(calls):
    def trajectory(start, stop):
        calls.append((start, stop))
        a = np.linspace(0, 2 * np.pi, 20)
        return [np.linspace(start, stop, 20), np.cos(a), np.sin(a), np.linspace(-1.0, 1.0, 20)]
    return trajectory


def test_a_time_series_graph_fetches_when_added(qtbot, panel):
    calls = []
    panel.plot(_series(calls), labels=["v"], plot_type=PlotType.TimeSeries)
    qtbot.waitUntil(lambda: any(c == pytest.approx(RANGE) for c in calls), timeout=5000)


def test_a_projection_graph_fetches_when_added(qtbot, panel):
    calls = []
    panel.plot(_trajectory(calls), labels=["x", "y", "z"], plot_type=PlotType.Projections)
    qtbot.waitUntil(lambda: any(c == pytest.approx(RANGE) for c in calls), timeout=5000)


def test_a_projection_graph_fetches_the_new_range_too(qtbot, panel):
    calls = []
    panel.plot(_trajectory(calls), labels=["x", "y", "z"], plot_type=PlotType.Projections)
    qtbot.waitUntil(lambda: bool(calls), timeout=5000)
    later = (RANGE[0] + 100, RANGE[1] + 100)
    panel.set_time_axis_range(SciQLopPlotRange(*later))
    qtbot.waitUntil(lambda: any(c == pytest.approx(later) for c in calls), timeout=5000)
