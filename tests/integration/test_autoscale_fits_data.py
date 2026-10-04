"""Autoscale fits the data: its minimum (not 0), and every component of a
multi-component graph (not just one of them)."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopPlotRange, GraphType

x = np.linspace(0, 10, 1000)
y = 150 + 50 * np.sin(x)  # [100, 200]
# Neither the first nor the last component alone spans the union [0, 200].
y3 = np.column_stack([y, y - 100, y - 50])


def _callback(values):
    def get_data(start, stop):
        return x.copy(), values.copy()
    return get_data


GRAPHS = {
    "line": (lambda p: p.plot(x, y), (100, 200)),
    "multi_line": (lambda p: p.plot(x, y3), (0, 200)),
    "scatter": (lambda p: p.plot(x, y, graph_type=GraphType.Scatter), (100, 200)),
    "curve": (lambda p: p.plot(x, y, graph_type=GraphType.ParametricCurve), (100, 200)),
    "callback_line": (lambda p: p.plot(_callback(y)), (100, 200)),
    "callback_multi_line": (lambda p: p.plot(_callback(y3), labels=["a", "b", "c"]), (0, 200)),
    "callback_colored_line": (
        lambda p: p.plot(lambda start, stop: {"data": [x.copy(), y.copy()], "color": x.copy()}),
        (100, 200)),
    "callback_curve": (
        lambda p: p.plot(_callback(y), graph_type=GraphType.ParametricCurve), (100, 200)),
}

RESCALES = {
    "y_axis": lambda p: p.y_axis().rescale(),
    "rescale_axes": lambda p: p.rescale_axes(),
    "hovered_or_selected": lambda p: p.rescale_hovered_or_selected_axes(),
}


def _y_after_rescale(qtbot, p, make, rescale):
    p.resize(800, 400)
    p.show()
    qtbot.waitExposed(p)
    make(p)
    p.x_axis().set_range(SciQLopPlotRange(0, 10))
    qtbot.wait(200)

    def fitted():
        p.y_axis().set_range(SciQLopPlotRange(-1000, 1000))
        rescale(p)
        assert p.y_axis().range().stop() < 300

    qtbot.waitUntil(fitted, timeout=3000)
    return p.y_axis().range()


@pytest.mark.parametrize("how", RESCALES)
@pytest.mark.parametrize("kind", GRAPHS)
@pytest.mark.parametrize("plot_fixture", ["plot", "ts_plot"])
def test_rescale_fits_all_data(qtbot, request, plot_fixture, kind, how):
    p = request.getfixturevalue(plot_fixture)
    make, (lo, hi) = GRAPHS[kind]
    r = _y_after_rescale(qtbot, p, make, RESCALES[how])
    pad = (hi - lo) * p.y_axis().autoscale_margin()
    assert r.start() == pytest.approx(lo - pad, abs=5)
    assert r.stop() == pytest.approx(hi + pad, abs=5)
