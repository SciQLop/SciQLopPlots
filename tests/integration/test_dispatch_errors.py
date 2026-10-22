"""Dispatch errors (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import PlotType, SciQLopNDProjectionPlot

X = np.linspace(0, 1, 10)
Z2 = np.zeros((10, 10))


@pytest.mark.parametrize("call", [
    lambda nd: nd.line(X, X),
    lambda nd: nd.colormap(X, X, np.zeros((10, 10))),
    lambda nd: nd.histogram2d(X, X),
])
def test_projection_plot_factories_it_cannot_serve_raise(qtbot, call):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    with pytest.raises(NotImplementedError, match="SciQLopNDProjectionPlot"):
        call(nd)


def test_a_plain_plot_given_a_list_raises(qtbot, plot):
    with pytest.raises(NotImplementedError, match="SciQLopPlot"):
        plot.parametric_curve([X, X, X])


@pytest.mark.parametrize("call", [
    lambda p: p.line(X, X, plot_type=PlotType.Projections),
    lambda p: p.scatter(X, X, plot_type=PlotType.Projections),
    lambda p: p.parametric_curve(X, X, plot_type=PlotType.Projections),
    lambda p: p.waterfall(X, np.zeros((10, 4)), plot_type=PlotType.Projections),
    lambda p: p.colormap(X, X, Z2, plot_type=PlotType.Projections),
    lambda p: p.histogram2d(X, X, plot_type=PlotType.Projections),
])
def test_arrays_on_a_projections_panel_raise(qtbot, panel, call):
    with pytest.raises(ValueError, match="list of arrays or a callback"):
        call(panel)
    assert panel.plot_count() == 0
