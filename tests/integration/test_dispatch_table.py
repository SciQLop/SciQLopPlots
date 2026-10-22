"""plot() is driven by one table (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (GraphType, PlotType, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopTimeSeriesPlot)

X = np.linspace(0, 1, 10)


def test_a_list_makes_a_projection_without_graph_type(qtbot, panel):
    p, _ = panel.plot([X, X, X, X], plot_type=PlotType.Projections)
    assert isinstance(p, SciQLopNDProjectionPlot)


def test_a_list_of_arrays_with_another_graph_type_raises(qtbot, panel):
    with pytest.raises(TypeError, match="projection"):
        panel.plot([X, X, X], graph_type=GraphType.Waterfall)


def test_a_flat_list_of_values_is_not_a_projection(qtbot, plot):
    h = plot.plot([1.0, 2.0, 3.0], graph_type=GraphType.Histogram)
    assert h is not None


def test_plot_type_on_a_plot_is_a_type_error(qtbot, plot):
    with pytest.raises(TypeError, match="plot_type"):
        plot.plot(X, X, plot_type=PlotType.TimeSeries)


def test_plot_type_is_not_dropped_on_the_curve_path(qtbot, panel):
    p, _ = panel.plot(X, X, graph_type=GraphType.ParametricCurve, plot_type=PlotType.TimeSeries)
    assert isinstance(p, SciQLopTimeSeriesPlot)


def test_unsupported_kind_names_the_host_from_the_table(qtbot, ts_plot):
    with pytest.raises(NotImplementedError, match="histogram is not supported by SciQLopTimeSeriesPlot"):
        ts_plot.plot(X, graph_type=GraphType.Histogram)


def test_name_on_a_projections_panel_call(qtbot, panel):
    _, g = panel.plot([X, X, X, X], plot_type=PlotType.Projections,
                      graph_type=GraphType.ParametricCurve, name="scene")
    assert g.objectName() == "scene"


def test_histogram_keyword_not_valid_for_plot_type_on_a_panel(qtbot, panel):
    with pytest.raises(TypeError, match="plot_type"):
        panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, plot_type=PlotType.BasicXY)


# guards
def test_two_arrays_as_histogram_points_to_bars(qtbot, plot):
    with pytest.raises(TypeError, match="plot.bars"):
        plot.plot(X, X, graph_type=GraphType.Histogram)


def test_foreign_keyword_names_its_owner(qtbot, plot):
    with pytest.raises(TypeError, match="Histogram2D"):
        plot.plot(X, X, x_bins=10)


def test_nones_are_dropped(qtbot, plot):
    plot.plot(X, X, labels=None, colors=None, name=None)


def test_four_array_curve_still_works(qtbot, plot):
    g = plot.plot(X, X, ["a"], ["#f00"], graph_type=GraphType.ParametricCurve)
    assert g.components()[0].name() == "a"


def test_histogram_post_options_on_a_panel(qtbot, panel):
    _, h = panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, bins=5, range=(0.0, 10.0))
    assert h.bins() == 5


def test_projection_plot_scene_calls(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.plot([X, X, X, X], graph_type=GraphType.ParametricCurve)
    nd.plot(lambda a, b: [X, X, X, X], name="model", labels=["m"] * 3,
            graph_type=GraphType.ParametricCurve)


HOSTS = {"plot": SciQLopPlot, "ts_plot": SciQLopTimeSeriesPlot,
         "panel": SciQLopMultiPlotPanel, "nd": SciQLopNDProjectionPlot}


def _data(kind_name, shape):
    """Arguments the kind's factory accepts for this shape."""
    if shape.name == "list":
        return ([X, X, X, X],)
    if shape.name == "callback":
        batch = {"colormap": (X, X, np.zeros((10, 10))), "projection": [X, X, X, X],
                 "histogram": [X], "waterfall": (X, np.zeros((10, 4)))}.get(kind_name, (X, X))
        return (lambda a, b, batch=batch: batch,)
    if kind_name == "projection":
        return {2: (X, np.zeros((10, 3))), 4: (X, X, X, X)}[shape.nargs]
    return {1: (X,), 2: (X, np.zeros((10, 4)) if kind_name == "waterfall" else X),
            3: (X, X, np.zeros((10, 10))), 4: (X, X, ["a"], ["#f00"])}[shape.nargs]


def _fresh(qtbot, host):
    made = SciQLopNDProjectionPlot(3) if host == "nd" else HOSTS[host]()
    qtbot.addWidget(made)
    return made


@pytest.mark.parametrize("host", list(HOSTS))
def test_dispatch_walk(qtbot, host):
    from SciQLopPlots._dispatch import KINDS, host_class, kind_of
    for name, kind in KINDS.items():
        for shape in kind.shapes:
            args = _data(name, shape)
            kw = {"plot_type": PlotType.Projections} if (host == "panel" and name == "projection") else {}
            if kind_of(HOSTS[host], args, kind.graph_type, kw) != name:
                continue  # on this host the call is another row's (curves on a projection plot)
            if host_class(HOSTS[host]) not in kind.factory:
                with pytest.raises(NotImplementedError, match=HOSTS[host].__name__):
                    _fresh(qtbot, host).plot(*args, graph_type=kind.graph_type, **kw)
                continue
            target = _fresh(qtbot, host)  # a plot holds one colour map
            result = target.plot(*args, graph_type=kind.graph_type, **kw)
            made = result[1] if host == "panel" else result
            assert isinstance(made, kind.returns), (host, name, shape)
