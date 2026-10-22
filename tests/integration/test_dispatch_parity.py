"""The dispatch table and the C++ factory signatures stay in step.

For every kind, shape and host, the keywords the factory overload takes (read from
shiboken's signatures) must be the ones the table lets through to it.
"""
import inspect

import numpy as np
import pytest
from shibokensupport.signature import get_signature

import SciQLopPlots
from SciQLopPlots import (GraphMarkerShape, GraphType, SciQLopMultiPlotPanel,
                          SciQLopNDProjectionPlot, SciQLopPlot, SciQLopTimeSeriesPlot, _dispatch)


def _overloads(method):
    sig = get_signature(method)
    return sig if isinstance(sig, list) else [sig]


def _first_annotation(overload):
    return overload.parameters[list(overload.parameters)[1]].annotation


def _called_shape(shape, kind_name):
    # plot() turns a projection's arrays into the one list its factory takes (_projection_buffers).
    return "list" if kind_name == "projection" and shape.name == "arrays" else shape.name


_FIRST_ANNOTATION = {"callback": lambda a: "Callable" in a,
                     "list": lambda a: "Sequence[object]" in a,
                     "arrays": lambda a: a == "<class 'object'>"}


def _is_overload_for(shape, kind_name, overload):
    return _FIRST_ANNOTATION[_called_shape(shape, kind_name)](str(_first_annotation(overload)))


def _positional_count(shape, kind_name):
    return shape.nargs if _called_shape(shape, kind_name) == "arrays" else 1


def _factory_method(kind_name, host, method_name):
    if kind_name == "histogram":
        return SciQLopPlots._cpp_histogram[host]
    return getattr(host, method_name)


def _cpp_keywords(kind_name, shape, method):
    matching = [o for o in _overloads(method) if _is_overload_for(shape, kind_name, o)]
    assert len(matching) == 1, f"{len(matching)} overloads match the {shape.name} shape"
    names = list(matching[0].parameters)[1:]
    return set(names[_positional_count(shape, kind_name):])


def _table_keywords(kind_name, kind, shape, host):
    return set(_dispatch.COMMON | kind.keywords_on(host) | shape.options) - kind.consumed


def _cases():
    return [(n, s, h, m) for n, k in _dispatch.KINDS.items()
            for s in k.shapes for h, m in k.factory.items()]


def _mismatch(kind_name, shape, host, method_name):
    kind = _dispatch.KINDS[kind_name]
    cpp = _cpp_keywords(kind_name, shape, _factory_method(kind_name, host, method_name))
    table = _table_keywords(kind_name, kind, shape, host)
    if cpp != table:
        return (f"{kind_name}/{shape.name}{shape.nargs or ''} on {host.__name__}.{method_name}: "
                f"only in C++ {sorted(cpp - table)}, only in the table {sorted(table - cpp)}")


@pytest.mark.filterwarnings("ignore:pyside_type_init:RuntimeWarning")
def test_table_keywords_match_the_factory_signatures():
    mismatches = [m for m in (_mismatch(*case) for case in _cases()) if m]
    assert mismatches == []


_PLOTS_AND_PANEL = (SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel)
EXPECTED_CASES = {
    "line": (("arrays2", "callback"), _PLOTS_AND_PANEL, "line"),
    "scatter": (("arrays2", "callback"), _PLOTS_AND_PANEL, "scatter"),
    "parametric_curve": (("arrays2", "arrays4", "callback"), _PLOTS_AND_PANEL, "parametric_curve"),
    "projection": (("list", "arrays2", "arrays4", "callback"),
                   {SciQLopMultiPlotPanel: "projection", SciQLopNDProjectionPlot: "parametric_curve"}),
    "colormap": (("arrays3", "callback"), _PLOTS_AND_PANEL, "colormap"),
    "waterfall": (("arrays2", "callback"), _PLOTS_AND_PANEL, "waterfall"),
    "histogram2d": (("arrays2", "callback"), _PLOTS_AND_PANEL, "histogram2d"),
    "histogram": (("arrays1", "callback"), (SciQLopPlot, SciQLopMultiPlotPanel), "histogram"),
}


def _expected_cases():
    def hosts(entry):
        return entry[1] if isinstance(entry[1], dict) else {h: entry[2] for h in entry[1]}
    return {(kind, shape, host, method) for kind, entry in EXPECTED_CASES.items()
            for shape in entry[0] for host, method in hosts(entry).items()}


def test_every_kind_shape_and_host_is_compared():
    cases = {(n, f"{s.name}{s.nargs or ''}", h, m) for n, s, h, m in _cases()}
    assert cases == _expected_cases()


def test_projection_arrays_reach_the_factory_as_one_list():
    t = np.arange(3.0)
    assert isinstance(SciQLopPlots._projection_buffers((t, t, t, t)), list)
    assert len(SciQLopPlots._projection_buffers((t, np.zeros((3, 3))))) == 4


def _named_keywords(function):
    params = list(inspect.signature(function).parameters.values())[2:]  # self, values
    return {p.name for p in params if p.kind is not p.VAR_KEYWORD}


@pytest.mark.filterwarnings("ignore:pyside_type_init:RuntimeWarning")
@pytest.mark.parametrize("host", [SciQLopPlot, SciQLopMultiPlotPanel])
def test_the_python_histogram_takes_the_cpp_keywords_and_the_post_ones(host):
    """plot() calls the Python histogram wrapper, not the C++ factory."""
    kind = _dispatch.KINDS["histogram"]
    shape = kind.shapes[0]
    cpp = _cpp_keywords("histogram", shape, SciQLopPlots._cpp_histogram[host])
    named = _named_keywords(host.histogram)
    assert shape.post <= named
    assert named - shape.post <= cpp
    assert inspect.signature(host.histogram).parameters.keys() >= {"kwargs"}


def _xyz(start, stop):
    t = np.linspace(0.0, 1.0, 20)
    return t, np.stack([t, 2 * t, 3 * t], axis=1)


@pytest.mark.parametrize("named", [False, True])
def test_projection_plot_takes_marker_and_name(qtbot, named):
    plot = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(plot)
    extra = {"name": "orbit"} if named else {}
    graph = plot.plot(_xyz, labels=["x", "y", "z"], graph_type=GraphType.ParametricCurve,
                         marker=GraphMarkerShape.Circle, **extra)
    assert len(graph.components()) == 3
    assert {c.marker_shape() for c in graph.components()} == {GraphMarkerShape.Circle}
    assert not named or graph.objectName() == "orbit"
