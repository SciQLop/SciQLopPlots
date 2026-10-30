"""plot() is driven by one table (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

import SciQLopPlots
from SciQLopPlots import (GraphMarkerShape, GraphType, HistogramNormalization, PlotType,
                          SciQLopHistogram, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopTimeSeriesPlot)

X = np.linspace(0, 1, 10)


def test_a_list_makes_a_projection_without_graph_type(qtbot, panel):
    p, _ = panel.plot([X, X, X, X], plot_type=PlotType.Projections)
    assert isinstance(p, SciQLopNDProjectionPlot)


def test_a_list_of_arrays_with_another_graph_type_raises(qtbot, panel):
    with pytest.raises(TypeError, match="projection"):
        panel.plot([X, X, X], graph_type=GraphType.Waterfall)


def test_a_flat_list_of_values_is_not_a_projection(qtbot, plot):
    h = plot.plot([1.0, 2.0, 3.0], graph_type=GraphType.Histogram, bins=3)
    assert isinstance(h, SciQLopHistogram)
    assert h.bins() == 3


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


def test_histogram_on_a_panel_rejects_plot_type(qtbot, panel):
    with pytest.raises(TypeError, match="plot_type"):
        panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, plot_type=PlotType.BasicXY)


def _cb(a, b):
    return [X, X, X, X]


@pytest.mark.parametrize("call", [
    lambda h: h.plot(_cb, plot_type=PlotType.Projections),
    lambda h: h.plot(X, X, graph_type=GraphType.ParametricCurve, plot_type=PlotType.Projections),
    lambda h: h.plot([X] * 4, plot_type=PlotType.TimeSeries),
    lambda h: h.plot([X] * 4, plot_type=PlotType.Projections),
])
@pytest.mark.parametrize("host", ["plot", "ts_plot", "nd"])
def test_plot_type_off_a_panel_is_a_type_error(qtbot, host, call):
    with pytest.raises(TypeError, match="plot_type"):
        call(_fresh(qtbot, host))


def test_a_list_with_a_non_projection_plot_type_on_a_panel_is_a_value_error(qtbot, panel):
    with pytest.raises(ValueError, match="TimeSeries"):
        panel.plot([X] * 4, plot_type=PlotType.TimeSeries)
    assert panel.plot_count() == 0


def test_four_arrays_make_a_projection_like_a_list(qtbot, panel):
    p, g = panel.plot(X, X, X, X, plot_type=PlotType.Projections)
    assert isinstance(p, SciQLopNDProjectionPlot)
    assert len(g.components()) == 3


def test_a_keyword_of_another_shape_of_the_same_kind_names_the_shape(qtbot, plot):
    with pytest.raises(TypeError, match="labels kwarg.s. not accepted by parametric_curve with 4 arrays"):
        plot.plot(X, X, ["a"], ["#f00"], graph_type=GraphType.ParametricCurve, labels=["b"])


def test_a_list_given_to_a_histogram_says_to_pass_one_array(qtbot, plot):
    with pytest.raises(TypeError, match="one array"):
        plot.plot([X], graph_type=GraphType.Histogram)


def test_the_three_array_warning_points_at_the_caller(qtbot, plot):
    with pytest.warns(DeprecationWarning) as record:
        plot.plot(X, X, np.zeros((10, 10)), graph_type=GraphType.ParametricCurve)
    assert record[0].filename == __file__


def test_histogram_settings_are_applied_by_the_python_wrapper(qtbot, plot):
    from SciQLopPlots._dispatch import KINDS
    assert all("normalization" in s.post for s in KINDS["histogram"].shapes)
    h = plot.plot(np.arange(1.0, 100.0), graph_type=GraphType.Histogram, log_bins=True,
                  normalization=HistogramNormalization.Density)
    assert h.log_bins() and h.normalization() == HistogramNormalization.Density


# guards
def test_two_arrays_as_histogram_points_to_bars(qtbot, plot):
    with pytest.raises(TypeError, match="plot.bars"):
        plot.plot(X, X, graph_type=GraphType.Histogram)


def test_foreign_keyword_names_its_owner(qtbot, plot):
    with pytest.raises(TypeError, match="Histogram2D"):
        plot.plot(X, X, x_bins=10)


def test_nones_are_dropped(qtbot, plot):
    plot.plot(X, X, labels=None, colors=None, name=None)


def test_a_misspelled_keyword_raises_even_when_none(qtbot, plot):
    """Nones were dropped before the keywords were checked: `lables=None` passed silently."""
    with pytest.raises(TypeError, match="lables"):
        plot.plot(X, X, lables=None)


def test_plot_type_none_on_a_plot_means_not_given(qtbot, plot):
    plot.plot(X, X, plot_type=None)


def test_four_array_curve_still_works(qtbot, plot):
    g = plot.plot(X, X, ["a"], ["#f00"], graph_type=GraphType.ParametricCurve)
    assert g.components()[0].name() == "a"


def test_histogram_post_options_on_a_panel(qtbot, panel):
    _, h = panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, bins=5, range=(0.0, 10.0))
    assert h.bins() == 5


def _markers(graph):
    return {c.marker_shape() for c in graph.components()}


def test_projection_plot_scene_calls(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.plot([X, X, X, X], graph_type=GraphType.ParametricCurve)
    model = nd.plot(lambda a, b: [X, X, X, X], name="model", labels=["m"] * 3,
                    graph_type=GraphType.ParametricCurve, marker=GraphMarkerShape.Circle)
    assert _markers(model) == {GraphMarkerShape.Circle}


@pytest.mark.parametrize("data", [[X, X, X, X], lambda a, b: [X, X, X, X]],
                         ids=["list", "callback"])
def test_marker_lands_on_a_panel_projection(qtbot, panel, data):
    _, g = panel.plot(data, plot_type=PlotType.Projections, marker=GraphMarkerShape.Circle)
    assert _markers(g) == {GraphMarkerShape.Circle}


def test_the_panel_projection_factory_takes_marker(qtbot, panel):
    _, g = panel.projection([X, X, X, X], marker=GraphMarkerShape.Circle)
    assert _markers(g) == {GraphMarkerShape.Circle}


@pytest.mark.parametrize("args", [(X, X), (X, X, np.zeros((10, 10)))])
def test_a_graph_type_that_is_not_a_graph_type_is_a_type_error(qtbot, plot, args):
    with pytest.raises(TypeError, match="graph_type must be a GraphType"):
        plot.plot(*args, graph_type="Line")


def test_a_tuple_of_arrays_is_not_a_projection(qtbot, ts_plot):
    with pytest.raises(TypeError, match="got 1 array;"):
        ts_plot.plot((X, X))


@pytest.mark.parametrize("host", ["plot", "ts_plot"])
def test_a_list_with_parametric_curve_on_a_single_plot_is_not_supported(qtbot, request, host):
    target = request.getfixturevalue(host)
    with pytest.raises(NotImplementedError, match=f"projection is not supported by {type(target).__name__}"):
        target.plot([X, X, X], graph_type=GraphType.ParametricCurve)


def test_histogram_settings_are_applied_once(qtbot, plot, monkeypatch):
    calls = []
    configure = SciQLopPlots._configure_histogram
    monkeypatch.setattr(SciQLopPlots, "_configure_histogram",
                        lambda *a: calls.append(a) or configure(*a))
    plot.plot(np.arange(1.0, 100.0), graph_type=GraphType.Histogram, log_bins=True)
    assert len(calls) == 1


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


# graph_type=ParametricCurve means "projection" on a projection plot and "parametric_curve"
# elsewhere, so these calls belong to the other row; test_projection_plot_scene_calls and the
# projection tests cover them.
LANDS_ON_ANOTHER_ROW = {
    *(("nd", "parametric_curve", s) for s in (("arrays", 2), ("arrays", 4), ("callback", None))),
    *((h, "projection", s) for h in ("plot", "ts_plot")
      for s in (("arrays", 2), ("arrays", 4), ("callback", None))),
}


def _direct_factory(kind, target):
    from SciQLopPlots._dispatch import host_class
    own = kind.factory.get(host_class(type(target)))
    # A host missing from the row is walked through any factory name it has, to check the refusal.
    return getattr(target, own) if own else next(
        getattr(target, m) for m in kind.factory.values() if hasattr(target, m))


def _direct_args(name, args):
    # What plot() hands the factory: a projection always gets one list.
    return (SciQLopPlots._projection_buffers(args),) if name == "projection" else args


@pytest.mark.parametrize("host", list(HOSTS))
def test_direct_factory_walk(qtbot, host):
    from SciQLopPlots._dispatch import KINDS, host_class
    for name, kind in KINDS.items():
        for shape in kind.shapes:
            if (host, name, (shape.name, shape.nargs)) in LANDS_ON_ANOTHER_ROW:
                continue
            target = _fresh(qtbot, host)
            factory = _direct_factory(kind, target)
            args = _direct_args(name, _data(name, shape))
            if host_class(HOSTS[host]) not in kind.factory:
                with pytest.raises(NotImplementedError, match=HOSTS[host].__name__):
                    factory(*args)
                continue
            result = factory(*args)
            made = result[1] if host == "panel" else result
            assert isinstance(made, kind.returns), (host, name, shape)


@pytest.mark.parametrize("host", list(HOSTS))
def test_dispatch_walk(qtbot, host):
    from SciQLopPlots._dispatch import KINDS, host_class
    walked = set()
    for name, kind in KINDS.items():
        for shape in kind.shapes:
            args = _data(name, shape)
            kw = {"plot_type": PlotType.Projections} if (host == "panel" and name == "projection") else {}
            if (host, name, (shape.name, shape.nargs)) in LANDS_ON_ANOTHER_ROW:
                continue
            walked.add((name, shape.name, shape.nargs))
            if host_class(HOSTS[host]) not in kind.factory:
                with pytest.raises(NotImplementedError, match=HOSTS[host].__name__):
                    _fresh(qtbot, host).plot(*args, graph_type=kind.graph_type, **kw)
                continue
            target = _fresh(qtbot, host)  # a plot holds one colour map
            result = target.plot(*args, graph_type=kind.graph_type, **kw)
            made = result[1] if host == "panel" else result
            assert isinstance(made, kind.returns), (host, name, shape)
    every = sum(len(k.shapes) for k in KINDS.values())
    assert len(walked) == every - sum(1 for h, *_ in LANDS_ON_ANOTHER_ROW if h == host)
