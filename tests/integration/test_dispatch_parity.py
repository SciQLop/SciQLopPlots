"""The dispatch table and the C++ factory signatures stay in step.

For every kind, shape and host, the keywords the factory overload takes (read from
shiboken's signatures) must be the ones the table lets through to it.
"""
import pytest
from shibokensupport.signature import get_signature

import SciQLopPlots
from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopNDProjectionPlot, _dispatch

# SciQLopNDProjectionPlot has no projection(): the table routes it to parametric_curve, whose
# C++ overloads also take marker and name. The table keeps them off the projection row on purpose.
HOST_EXTRAS = {("projection", SciQLopNDProjectionPlot): {"marker", "name"}}


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
    keywords = _dispatch.COMMON | kind.native | shape.options
    if _dispatch.host_class(host) is SciQLopMultiPlotPanel:
        keywords |= kind.panel_keywords - kind.consumed
    return set(keywords) | HOST_EXTRAS.get((kind_name, _dispatch.host_class(host)), set())


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
