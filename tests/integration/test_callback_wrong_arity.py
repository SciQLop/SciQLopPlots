"""A callback batch with the wrong number of arrays for its graph is reported
(fetch_failed + a Qt warning), never silently dropped."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlotRange


def _three_arrays(start, stop):
    x = np.linspace(start, stop, 20)
    return x, x - 1, x + 1


def _two_arrays(start, stop):
    x = np.linspace(start, stop, 20)
    return x, np.sin(x)


def _fetch(graph):
    graph.set_range(SciQLopPlotRange(0.0, 10.0))


def test_line_callback_returning_three_arrays_is_reported(qtbot, qtlog, plot):
    g = plot.line(_three_arrays)
    with qtbot.waitSignal(g.fetch_failed, timeout=5000) as blocker:
        _fetch(g)
    assert blocker.args[0] == "expected (x, y), got 3 arrays"
    assert any("expected (x, y), got 3 arrays" in r.message for r in qtlog.records)
    assert len(g.components()) == 0


def test_colormap_callback_returning_two_arrays_is_reported(qtbot, plot):
    g = plot.colormap(_two_arrays)
    with qtbot.waitSignal(g.fetch_failed, timeout=5000) as blocker:
        _fetch(g)
    assert blocker.args[0] == "expected (x, y, z), got 2 arrays"


def test_two_pane_projection_takes_time_and_two_dimensions(qtbot):
    proj = SciQLopNDProjectionPlot(2)
    qtbot.addWidget(proj)
    t = np.linspace(0.0, 10.0, 200)
    g = proj.add_model_curve(lambda start, stop: [t, np.cos(t), np.sin(t)], label="model")
    failures = []
    g.fetch_failed.connect(failures.append)
    _fetch(g)
    qtbot.waitUntil(lambda: proj.subplot(0).x_axis().range().start() < -0.9 or bool(failures),
                    timeout=5000)
    assert failures == []
