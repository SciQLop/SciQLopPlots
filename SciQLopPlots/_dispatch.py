"""The table that drives plot() on every host (docs/plans/2026-10-22-dispatch-table-design.md)."""
import warnings
from dataclasses import dataclass
from typing import Mapping

import numpy as np

from .SciQLopPlotsBindings import (GraphType, PlotType, SciQLopColorMapInterface,
                                   SciQLopGraphInterface, SciQLopHistogram, SciQLopMultiPlotPanel,
                                   SciQLopNDProjectionPlot, SciQLopPlot, SciQLopTimeSeriesPlot)

COMMON = frozenset({"metadata"})
# Subclasses first: a time series plot is also a SciQLopPlot.
HOSTS = (SciQLopTimeSeriesPlot, SciQLopNDProjectionPlot, SciQLopMultiPlotPanel, SciQLopPlot)


@dataclass(frozen=True)
class Shape:
    name: str
    nargs: int | None
    options: frozenset
    post: frozenset = frozenset()


@dataclass(frozen=True)
class Kind:
    graph_type: object
    shapes: tuple
    factory: Mapping
    native: frozenset
    panel_keywords: frozenset
    returns: type
    consumed: frozenset = frozenset()


def _on(hosts, method):
    return {h: method for h in hosts}


_PLOTS_AND_PANEL = (SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel)
_BOTH_PANEL_KEYWORDS = frozenset({"plot_type", "index"})
_NAME = frozenset({"name"})
_CURVE = frozenset({"labels", "colors", "marker"})
_SYNC = frozenset({"sync_with"})
_WATERFALL_POST = frozenset({"offsets", "normalize", "gain"})
_COLORMAP = frozenset({"y_log_scale", "z_log_scale"})
_HISTOGRAM2D = frozenset({"x_bins", "y_bins", "x_bins_log", "y_bins_log"})
_HISTOGRAM = frozenset({"bins", "range", "log_bins", "normalization", "style"})
_PROJECTION = frozenset({"labels", "colors"})


def _curve_kind(graph_type, method, four_arrays=False):
    four = (Shape("arrays", 4, frozenset({"marker"})),) if four_arrays else ()
    return Kind(graph_type,
                (Shape("arrays", 2, _CURVE), *four, Shape("callback", None, _CURVE | _SYNC)),
                _on(_PLOTS_AND_PANEL, method), _NAME, _BOTH_PANEL_KEYWORDS,
                SciQLopGraphInterface)


KINDS = {
    "line": _curve_kind(GraphType.Line, "line"),
    "scatter": _curve_kind(GraphType.Scatter, "scatter"),
    "parametric_curve": _curve_kind(GraphType.ParametricCurve, "parametric_curve",
                                    four_arrays=True),
    "projection": Kind(
        GraphType.ParametricCurve,
        (Shape("list", None, _PROJECTION),
         Shape("arrays", 2, _PROJECTION),  # (t, values) with one column per dimension
         Shape("arrays", 4, _PROJECTION),  # (t, d0, d1, d2)
         Shape("callback", None, _PROJECTION | _SYNC)),
        {SciQLopMultiPlotPanel: "projection", SciQLopNDProjectionPlot: "parametric_curve"},
        frozenset(), frozenset({"index"}), SciQLopGraphInterface,
        consumed=frozenset({"plot_type"})),
    "colormap": Kind(
        GraphType.ColorMap,
        (Shape("arrays", 3, _COLORMAP), Shape("callback", None, _COLORMAP | _SYNC)),
        _on(_PLOTS_AND_PANEL, "colormap"), _NAME, _BOTH_PANEL_KEYWORDS,
        SciQLopColorMapInterface),
    "waterfall": Kind(
        GraphType.Waterfall,
        (Shape("arrays", 2, _CURVE, _WATERFALL_POST),
         Shape("callback", None, _CURVE | _SYNC, _WATERFALL_POST)),
        _on(_PLOTS_AND_PANEL, "waterfall"), _NAME, _BOTH_PANEL_KEYWORDS,
        SciQLopGraphInterface),
    "histogram2d": Kind(
        GraphType.Histogram2D,
        (Shape("arrays", 2, _HISTOGRAM2D), Shape("callback", None, _HISTOGRAM2D | _SYNC)),
        _on(_PLOTS_AND_PANEL, "histogram2d"), _NAME, _BOTH_PANEL_KEYWORDS,
        SciQLopColorMapInterface),
    "histogram": Kind(
        GraphType.Histogram,
        (Shape("arrays", 1, _HISTOGRAM), Shape("callback", None, _HISTOGRAM | _SYNC)),
        _on((SciQLopPlot, SciQLopMultiPlotPanel), "histogram"), _NAME, frozenset({"index"}),
        SciQLopHistogram),
}


class PlotArgumentsError(TypeError, ValueError):
    """TypeError per the table's rules; also a ValueError, which plot() raised before it."""


_TWO_ARGUMENT_HISTOGRAM = ("GraphType.Histogram takes one argument (values or a callback); "
                           "use plot.bars(edges, heights) for pre-binned data")


def host_class(cls):
    return next(h for h in HOSTS if issubclass(cls, h))


def _is_list_of_arrays(value):
    return isinstance(value, (list, tuple)) and len(value) > 0 and all(np.ndim(v) >= 1 for v in value)


def shape_of(args):
    if len(args) == 1 and callable(args[0]):
        return "callback", None
    if len(args) == 1 and _is_list_of_arrays(args[0]):
        return "list", None
    return "arrays", len(args)


def _warn_if_three_arrays_with(graph_type):
    # SciQLop's XY panels pass ParametricCurve with three arrays: a warning until 1.0, not an error.
    if graph_type not in (None, GraphType.ColorMap):
        warnings.warn(f"3 arrays (x, y, z) make a colour map, not a {graph_type.name}: "
                      "drop graph_type or pass GraphType.ColorMap", DeprecationWarning,
                      stacklevel=5)


def _wants_projection(host_cls, shape, graph_type, kwargs):
    projections = kwargs.get("plot_type") == PlotType.Projections
    if shape == "callback" and projections:
        return True
    on_projection_plot = issubclass(host_cls, SciQLopNDProjectionPlot)
    return graph_type == GraphType.ParametricCurve and (projections or on_projection_plot)


def _kind_by_graph_type(graph_type, shape, nargs):
    if graph_type == GraphType.Histogram and shape == "arrays" and nargs == 2:
        raise TypeError(_TWO_ARGUMENT_HISTOGRAM)
    names = [n for n, k in KINDS.items() if k.graph_type == graph_type and n != "projection"]
    if not names:
        raise ValueError(f"unsupported graph_type {graph_type!r}")
    return names[0]


def _list_kind(graph_type):
    if graph_type not in (None, GraphType.ParametricCurve):
        raise TypeError(f"a list of arrays makes a projection, not a {getattr(graph_type, 'name', graph_type)}")
    return "projection"


def _kind_name(host_cls, shape, nargs, graph_type, kwargs):
    if shape == "list":
        return _list_kind(graph_type)
    if shape == "arrays" and nargs == 3:
        _warn_if_three_arrays_with(graph_type)
        return "colormap"
    if _wants_projection(host_cls, shape, graph_type, kwargs):
        return "projection"
    return _kind_by_graph_type(graph_type or GraphType.Line, shape, nargs)


def kind_of(host_cls, args, graph_type, kwargs):
    """The table row a plot() call lands on, before the host and keywords are checked."""
    shape, nargs = shape_of(args)
    return _kind_name(host_cls, shape, nargs, graph_type, kwargs)


def _describe(shape):
    if shape.name == "arrays":
        return f"{shape.nargs} array{'s' if shape.nargs > 1 else ''}"
    return {"list": "a list of arrays", "callback": "a callback"}[shape.name]


def _shape_row(name, kind, shape, nargs):
    row = next((s for s in kind.shapes if s.name == shape and s.nargs == nargs), None)
    if row is None:
        got = f"{nargs} arrays" if shape == "arrays" else f"a {shape}"
        forms = " or ".join(_describe(s) for s in kind.shapes)
        raise PlotArgumentsError(
            f"{name} (GraphType.{kind.graph_type.name}) takes {forms}, got {got}; plot() takes "
            "only 1, 2 or 3 arrays, a list of arrays or a callback (or x, y, labels, colors "
            "with GraphType.ParametricCurve)")
    return row


def _check_host(name, kind, host_cls):
    if host_class(host_cls) not in kind.factory:
        raise NotImplementedError(f"{name} is not supported by {host_cls.__name__}")


def _kind_options(kind):
    return frozenset().union(*(s.options | s.post for s in kind.shapes))


def _foreign_keyword_message(stray, name, accepted, graph_type):
    owners = sorted({k.graph_type.name for k in KINDS.values() if set(stray) <= _kind_options(k)})
    if owners:
        return (f"{', '.join(stray)} kwarg(s) only apply to "
                f"{' or '.join('GraphType.' + o for o in owners)}, got graph_type={graph_type!r}")
    return f"{', '.join(stray)} kwarg(s) not accepted by {name}; accepted: {', '.join(sorted(accepted))}"


def _check_keywords(name, kind, shape, host_cls, graph_type, kwargs):
    allowed = COMMON | kind.native | kind.consumed | shape.options | shape.post | {"name"}
    if host_class(host_cls) is SciQLopMultiPlotPanel:
        allowed |= kind.panel_keywords
    stray = sorted(set(kwargs) - allowed)
    if stray:
        raise TypeError(_foreign_keyword_message(stray, name, allowed, graph_type))


def resolve(host_cls, args, graph_type, kwargs):
    shape, nargs = shape_of(args)
    name = _kind_name(host_cls, shape, nargs, graph_type, kwargs)
    kind = KINDS[name]
    row = _shape_row(name, kind, shape, nargs)
    _check_host(name, kind, host_cls)
    _check_keywords(name, kind, row, host_cls, graph_type, kwargs)
    return name, kind, row
