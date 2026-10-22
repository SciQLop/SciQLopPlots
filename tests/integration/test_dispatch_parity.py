"""The dispatch table and the C++ factory signatures stay in step.

For every kind, shape and host, the keywords the factory overload takes (read from
shiboken's signatures) must be the ones the table lets through to it.
"""
import numpy as np
import pytest
from shibokensupport.signature import get_signature

import SciQLopPlots
from SciQLopPlots import (GraphMarkerShape, GraphType, SciQLopMultiPlotPanel,
                          SciQLopNDProjectionPlot, _dispatch)


def _overloads(method):
    sig = get_signature(method)
    return sig if isinstance(sig, list) else [sig]


def _first_annotation(overload):
    return overload.parameters[list(overload.parameters)[1]].annotation


def _is_overload_for(shape, kind_name, overload):
    first = str(_first_annotation(overload))
    if shape.name == "callback":
        return "Callable" in first
    if shape.name == "list" or kind_name == "projection":
        return "Sequence[object]" in first
    return first == "<class 'object'>"


def _positional_count(shape, kind_name):
    return shape.nargs if shape.name == "arrays" and kind_name != "projection" else 1


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


def test_every_kind_shape_and_host_is_compared():
    assert len(_cases()) == 51


def _xyz(start, stop):
    t = np.linspace(0.0, 1.0, 20)
    return t, np.stack([t, 2 * t, 3 * t], axis=1)


@pytest.mark.parametrize("named", [False, True])
# The C++ plot_impl accepts marker but never applies it, so only the call is checked.
def test_projection_plot_takes_marker_and_name(qtbot, named):
    plot = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(plot)
    extra = {"name": "orbit"} if named else {}
    graph = plot.plot(_xyz, labels=["x", "y", "z"], graph_type=GraphType.ParametricCurve,
                         marker=GraphMarkerShape.Circle, **extra)
    assert len(graph.components()) == 3
    assert not named or graph.objectName() == "orbit"
