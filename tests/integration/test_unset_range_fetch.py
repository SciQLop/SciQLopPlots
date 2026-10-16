"""A plot with no time range yet must not call a data callback with NaN bounds."""
import math

import numpy as np
from SciQLopPlots import GraphType, SciQLopNDProjectionPlot, SciQLopPlot, SciQLopPlotRange


def _recording(calls, columns):
    def callback(start, stop):
        calls.append((start, stop))
        t = np.linspace(0.0, 1.0, 10)
        return [t] * columns
    return callback


def _nan_calls(calls):
    return [c for c in calls if math.isnan(c[0]) or math.isnan(c[1])]


def test_standalone_projection_never_fetches_nan(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    calls = []
    proj.plot(_recording(calls, 4), labels=["a", "b", "c"], graph_type=GraphType.ParametricCurve)
    proj.add_model_curve(_recording(calls, 4), label="model")
    qtbot.wait(400)
    assert _nan_calls(calls) == []


def test_projection_fetches_once_given_a_range(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    calls = []
    proj.plot(_recording(calls, 4), labels=["a", "b", "c"], graph_type=GraphType.ParametricCurve)
    proj.time_axis().set_range(10.0, 20.0)
    qtbot.waitUntil(lambda: (10.0, 20.0) in calls, timeout=2000)


def test_plot_given_a_nan_range_does_not_fetch_it(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    calls = []
    graph = plot.plot(_recording(calls, 2), labels=["y"])
    graph.set_range(SciQLopPlotRange(float("nan"), float("nan")))
    qtbot.wait(400)
    assert _nan_calls(calls) == []
