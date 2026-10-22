"""Dispatch errors (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot

X = np.linspace(0, 1, 10)


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
