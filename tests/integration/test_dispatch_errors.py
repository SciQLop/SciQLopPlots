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


PROJ = PlotType.Projections
LISTS = "list of arrays or a callback"
CMAP = "projections can't hold a colour map"
HIST2D = "projections can't hold a 2D histogram"


@pytest.mark.parametrize("call, message", [
    (lambda p: p.line(X, X, plot_type=PROJ), LISTS),
    (lambda p: p.scatter(X, X, plot_type=PROJ), LISTS),
    (lambda p: p.parametric_curve(X, X, plot_type=PROJ), LISTS),
    (lambda p: p.waterfall(X, np.zeros((10, 4)), plot_type=PROJ), LISTS),
    (lambda p: p.colormap(X, X, Z2, plot_type=PROJ), CMAP),
    (lambda p: p.colormap(lambda a, b: (X, X, Z2), plot_type=PROJ), CMAP),
    (lambda p: p.histogram2d(X, X, plot_type=PROJ), HIST2D),
    (lambda p: p.histogram2d(lambda a, b: (X, X), plot_type=PROJ), HIST2D),
])
def test_unsupported_kinds_on_a_projections_panel_raise(qtbot, panel, call, message):
    with pytest.raises(ValueError, match=message):
        call(panel)
    assert panel.plot_count() == 0


def test_a_failing_histogram2d_leaves_no_empty_plot(qtbot, panel):
    with pytest.raises(Exception):
        panel.histogram2d(X, X[:5])
    assert panel.plot_count() == 0


def test_a_callable_line_on_a_projections_panel_still_builds_a_projection_plot(qtbot, panel):
    plot, _ = panel.line(lambda a, b: (X, X), plot_type=PROJ)
    assert isinstance(plot, SciQLopNDProjectionPlot)
    assert panel.plot_count() == 1
