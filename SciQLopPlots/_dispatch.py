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
    host_keywords: Mapping
    returns: type
    consumed: frozenset = frozenset()

    def keywords_on(self, host_cls):
        return self.native | self.host_keywords.get(host_class(host_cls), frozenset())


def _on(hosts, method):
    return {h: method for h in hosts}


_PLOTS_AND_PANEL = (SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel)
_PANEL_KEYWORDS = {SciQLopMultiPlotPanel: frozenset({"plot_type", "index"})}
_NAME = frozenset({"name"})
_CURVE = frozenset({"labels", "colors", "marker"})
_SYNC = frozenset({"sync_with"})
_WATERFALL_POST = frozenset({"offsets", "normalize", "gain"})
_COLORMAP = frozenset({"y_log_scale", "z_log_scale"})
_HISTOGRAM2D = frozenset({"x_bins", "y_bins", "x_bins_log", "y_bins_log"})
_HISTOGRAM = frozenset({"bins"})
_HISTOGRAM_POST = frozenset({"range", "log_bins", "normalization", "style"})
_PROJECTION = frozenset({"labels", "colors", "marker"})


def _curve_kind(graph_type, method, four_arrays=False):
    four = (Shape("arrays", 4, frozenset({"marker"})),) if four_arrays else ()
    return Kind(graph_type,
                (Shape("arrays", 2, _CURVE), *four, Shape("callback", None, _CURVE | _SYNC)),
                _on(_PLOTS_AND_PANEL, method), _NAME, _PANEL_KEYWORDS,
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
        frozenset(),
        # parametric_curve, which backs the projection plot, also takes name
        {**_PANEL_KEYWORDS, SciQLopNDProjectionPlot: _NAME},
        SciQLopGraphInterface,
        consumed=frozenset({"plot_type"})),  # it picks this row; projection() has no plot_type
    "colormap": Kind(
        GraphType.ColorMap,
        (Shape("arrays", 3, _COLORMAP), Shape("callback", None, _COLORMAP | _SYNC)),
        _on(_PLOTS_AND_PANEL, "colormap"), _NAME, _PANEL_KEYWORDS,
        SciQLopColorMapInterface),
    "waterfall": Kind(
        GraphType.Waterfall,
        (Shape("arrays", 2, _CURVE, _WATERFALL_POST),
         Shape("callback", None, _CURVE | _SYNC, _WATERFALL_POST)),
        _on(_PLOTS_AND_PANEL, "waterfall"), _NAME, _PANEL_KEYWORDS,
        SciQLopGraphInterface),
    "histogram2d": Kind(
        GraphType.Histogram2D,
        (Shape("arrays", 2, _HISTOGRAM2D), Shape("callback", None, _HISTOGRAM2D | _SYNC)),
        _on(_PLOTS_AND_PANEL, "histogram2d"), _NAME, _PANEL_KEYWORDS,
        SciQLopColorMapInterface),
    "histogram": Kind(
        GraphType.Histogram,
        (Shape("arrays", 1, _HISTOGRAM, _HISTOGRAM_POST),
         Shape("callback", None, _HISTOGRAM | _SYNC, _HISTOGRAM_POST)),
        _on((SciQLopPlot, SciQLopMultiPlotPanel), "histogram"), _NAME, {SciQLopMultiPlotPanel: frozenset({"index"})},
        SciQLopHistogram),
}


class PlotArgumentsError(TypeError, ValueError):
    """TypeError per the table's rules; also a ValueError, which plot() raised before it."""


_TWO_ARGUMENT_HISTOGRAM = ("GraphType.Histogram takes one argument (values or a callback); "
                           "use plot.bars(edges, heights) for pre-binned data")


def host_class(cls):
    return next(h for h in HOSTS if issubclass(cls, h))


def _is_list_of_arrays(value):
    # Not a tuple: plot((x, y)) is far more often a forgotten * than a projection.
    return isinstance(value, list) and len(value) > 0 and all(np.ndim(v) >= 1 for v in value)


def reject_callback_with_arrays(args):
    if len(args) > 1 and any(callable(a) for a in args):
        raise TypeError("expected a callback or arrays, not a callback mixed with arrays")


def shape_of(args):
    reject_callback_with_arrays(args)
    if len(args) == 1 and callable(args[0]):
        return "callback", None
    if len(args) == 1 and _is_list_of_arrays(args[0]):
        return "list", None
    return "arrays", len(args)


def _warn_if_three_arrays_with(graph_type):
    # SciQLop's XY panels pass ParametricCurve with three arrays: a warning until 1.0, not an error.
    if graph_type not in (None, GraphType.ColorMap):
        warnings.warn(f"graph_type={graph_type.name} with 3 arrays (x, y, z) is deprecated: use "
                      "GraphType.ColorMap or drop graph_type (3 arrays make a colour map)",
                      DeprecationWarning, stacklevel=5)


def _is_panel(host_cls):
    return host_class(host_cls) is SciQLopMultiPlotPanel


def _reject_plot_type_off_panels(host_cls, kwargs):
    if "plot_type" in kwargs and not _is_panel(host_cls):
        raise TypeError(f"plot_type kwarg is panel-only: {host_cls.__name__} is a single plot")


def _projection_arrays(args):
    # (t, values) is a projection only with one column per dimension: (x, y) is a curve,
    # which the factory turns down with "projections take a list of arrays or a callback".
    return len(args) != 2 or np.ndim(args[1]) == 2


def _wants_projection(host_cls, args, shape, graph_type, kwargs):
    if issubclass(host_cls, SciQLopNDProjectionPlot):
        return graph_type == GraphType.ParametricCurve
    if kwargs.get("plot_type") != PlotType.Projections:
        return False
    if shape == "callback":
        return True
    return graph_type in (None, GraphType.ParametricCurve) and _projection_arrays(args)


def _kind_by_graph_type(graph_type, shape, nargs):
    if graph_type == GraphType.Histogram and shape == "arrays" and nargs == 2:
        raise TypeError(_TWO_ARGUMENT_HISTOGRAM)
    names = [n for n, k in KINDS.items() if k.graph_type == graph_type and n != "projection"]
    if not names:
        raise ValueError(f"unsupported graph_type {graph_type!r}")
    return names[0]


def _list_kind(graph_type, plot_type):
    if graph_type not in (None, GraphType.ParametricCurve):
        hint = ": pass the values as one array" if graph_type == GraphType.Histogram else ""
        raise TypeError("a list of arrays makes a projection, "
                        f"not a {getattr(graph_type, 'name', graph_type)}{hint}")
    if plot_type not in (None, PlotType.Projections):
        raise ValueError(f"a list of arrays makes a projection, it can't go on a {plot_type.name} plot")
    return "projection"


def _kind_name(host_cls, args, graph_type, kwargs):
    shape, nargs = shape_of(args)
    if shape == "list":
        return _list_kind(graph_type, kwargs.get("plot_type"))
    if shape == "arrays" and nargs == 3:
        _warn_if_three_arrays_with(graph_type)
        return "colormap"
    if _wants_projection(host_cls, args, shape, graph_type, kwargs):
        return "projection"
    return _kind_by_graph_type(graph_type or GraphType.Line, shape, nargs)


def _describe(shape):
    if shape.name == "arrays":
        return f"{shape.nargs} array{'s' if shape.nargs > 1 else ''}"
    return {"list": "a list of arrays", "callback": "a callback"}[shape.name]


def _shape_row(name, kind, shape, nargs):
    row = next((s for s in kind.shapes if s.name == shape and s.nargs == nargs), None)
    if row is None:
        got = _describe(Shape(shape, nargs, frozenset()))
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


def _foreign_keyword_message(stray, name, shape, accepted, graph_type):
    accepted = ", ".join(sorted(accepted))
    if set(stray) <= _kind_options(KINDS[name]):
        return f"{', '.join(stray)} kwarg(s) not accepted by {name} with {_describe(shape)}; accepted: {accepted}"
    owners = sorted({k.graph_type.name for n, k in KINDS.items()
                     if n != name and k.graph_type != KINDS[name].graph_type and set(stray) <= _kind_options(k)})
    if owners:
        return (f"{', '.join(stray)} kwarg(s) only apply to "
                f"{' or '.join('GraphType.' + o for o in owners)}, got graph_type={graph_type!r}")
    return f"{', '.join(stray)} kwarg(s) not accepted by {name}; accepted: {accepted}"


def _check_keywords(name, kind, shape, host_cls, graph_type, kwargs):
    allowed = COMMON | kind.keywords_on(host_cls) | shape.options | shape.post | {"name"}
    stray = sorted(set(kwargs) - allowed)
    if stray:
        raise TypeError(_foreign_keyword_message(stray, name, shape, allowed, graph_type))


def _reject_a_graph_type_of_another_type(graph_type):
    # PlotArgumentsError: test_p1_bugs still expects the ValueError plot() raised before.
    if graph_type is not None and not isinstance(graph_type, GraphType):
        raise PlotArgumentsError(f"unsupported graph_type {graph_type!r}: "
                                 "graph_type must be a GraphType")


def resolve(host_cls, args, graph_type, kwargs):
    _reject_a_graph_type_of_another_type(graph_type)
    _reject_plot_type_off_panels(host_cls, kwargs)
    shape, nargs = shape_of(args)
    name = _kind_name(host_cls, args, graph_type, kwargs)
    kind = KINDS[name]
    row = _shape_row(name, kind, shape, nargs)
    _check_host(name, kind, host_cls)
    _check_keywords(name, kind, row, host_cls, graph_type, kwargs)
    return name, kind, row
