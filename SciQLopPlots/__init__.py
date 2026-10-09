from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from . import SciQLopPlotsBindings
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *
from datetime import date, datetime, timezone
import traceback

import sys

sys.modules["SciQLopPlotsBindings"] = SciQLopPlotsBindings


def _register_with_shiboken_signatures():
    """Make wrong-argument TypeErrors readable instead of a masking NameError.

    Shiboken builds the "Supported signatures:" part of an argument-mismatch
    TypeError by eval()-ing the parameter type strings (e.g.
    ``SciQLopPlotsBindings.SciQLopPlot``) in shibokensupport.signature.mapping's
    namespace. PySide6's own types are pre-registered there; ours are not, so
    every overload mismatch raised ``NameError: name 'SciQLopPlotsBindings' is
    not defined`` that masked the real TypeError.

    Two registrations are needed:
      * the binding module, so ``SciQLopPlotsBindings.<Type>`` resolves;
      * every ``<primitive-type>`` we declare in bindings.xml. These are not
        wrapped classes, so without an entry they resolve to a bare ``str``
        and shiboken's argument matcher hard-aborts the interpreter
        (``the_type.__module__`` on a str) — the caller gets ``Fatal Python
        error: libshiboken: seterror_argument did not receive a result``
        instead of a catchable TypeError. Keep ``_PRIMITIVE_TYPE_MAP`` in sync
        with the ``<primitive-type>`` list at the top of bindings.xml.

    Finally, a global ``enum class`` default renders in the generated
    signatures as ``.GraphMarkerShape.NoMarker`` (module prefix dropped → a
    leading dot the parser can't eval). That only warns — once per signature,
    and only while formatting an error — but it would bury the now-useful
    message under a wall of RuntimeWarnings, so the specific warning is muted.
    """
    import collections.abc
    import warnings

    # The whole body is best-effort: this only makes binding-misuse errors
    # readable, so any shift in shiboken's signature internals (missing module,
    # renamed/restructured namespace or type_map) must degrade to the raw
    # shiboken errors, never break ``import SciQLopPlots``.
    try:
        from shibokensupport.signature import mapping

        mapping.namespace["SciQLopPlotsBindings"] = SciQLopPlotsBindings
        mapping.type_map.update({
            "SciQLopPyBuffer": object,          # PyObject-backed buffer
            "GetDataPyCallable": collections.abc.Callable,
            "long": int,
            # Shiboken emits namespaced primitives with dots in the generated
            # signature strings (``std::size_t`` -> ``std.size_t``) but looks
            # some paths up under the original spelling, so register both.
            "std::string": str, "std.string": str,
            "std::size_t": int, "std.size_t": int,
        })
        # Scoped to the parser that emits it so we never hide a same-named
        # RuntimeWarning from unrelated code.
        warnings.filterwarnings(
            "ignore", message="pyside_type_init", category=RuntimeWarning,
            module=r"shibokensupport\.signature")
    except Exception:
        pass


_register_with_shiboken_signatures()

from . import tracing  # noqa: E402,F401  -- runtime tracer facade

__version__ = '0.50.0'

def _merge_kwargs(kwargs, **kwargs2):
    for k, v in kwargs2.items():
        if k not in kwargs and v is not None:
            kwargs[k] = v
    return kwargs

def _apply_waterfall_kwargs(result, offsets=None, normalize=True, gain=1.0):
    import numpy as np
    from .SciQLopPlotsBindings import WaterfallOffsetMode

    # Plot-level cls.waterfall returns the graph; panel-level returns (plot, graph).
    wf = result[1] if isinstance(result, tuple) else result

    if offsets is None:
        wf.set_offset_mode(WaterfallOffsetMode.Uniform)
        wf.set_uniform_spacing(1.0)
    elif isinstance(offsets, (int, float)):
        wf.set_offset_mode(WaterfallOffsetMode.Uniform)
        wf.set_uniform_spacing(float(offsets))
    else:
        arr = np.asarray(offsets, dtype=np.float64).ravel()
        wf.set_offset_mode(WaterfallOffsetMode.Custom)
        wf.set_offsets(arr.tolist())

    wf.set_normalize(bool(normalize))
    wf.set_gain(float(gain))
    return result


_WATERFALL_KWARGS = ("offsets", "normalize", "gain")


def _pop_waterfall_kwargs(kwargs):
    return {k: kwargs.pop(k) for k in _WATERFALL_KWARGS if k in kwargs}


def _reject_waterfall_kwargs(kwargs, graph_type):
    stray = [k for k in _WATERFALL_KWARGS if k in kwargs]
    if stray:
        raise TypeError(
            f"{', '.join(stray)} kwarg(s) only apply to GraphType.Waterfall, "
            f"got graph_type={graph_type!r}")


_HISTOGRAM2D_KWARGS = ("x_bins", "y_bins", "x_bins_log", "y_bins_log")


def _reject_histogram2d_kwargs(kwargs, graph_type):
    stray = [k for k in _HISTOGRAM2D_KWARGS if k in kwargs]
    if stray:
        raise TypeError(
            f"{', '.join(stray)} kwarg(s) only apply to GraphType.Histogram2D, "
            f"got graph_type={graph_type!r}")


def _patch_sciqlop_plot(cls):
    def plot_func(self, callback, graph_type=None, **kwargs):
        kwargs = {k: v for k, v in kwargs.items() if v is not None}
        if graph_type == GraphType.Waterfall:
            _reject_histogram2d_kwargs(kwargs, graph_type)
            wf_kwargs = _pop_waterfall_kwargs(kwargs)
            wf = cls.waterfall(self, callback, **kwargs)
            return _apply_waterfall_kwargs(wf, **wf_kwargs)
        if graph_type == GraphType.Histogram2D:
            _reject_waterfall_kwargs(kwargs, graph_type)
            return cls.histogram2d(self, callback, **kwargs)
        _reject_waterfall_kwargs(kwargs, graph_type)
        _reject_histogram2d_kwargs(kwargs, graph_type)
        if graph_type == GraphType.ParametricCurve:
            return cls.parametric_curve(self, callback, **kwargs)
        elif graph_type == GraphType.Line:
            return cls.line(self, callback, **kwargs)
        elif graph_type == GraphType.Scatter:
            return cls.scatter(self, callback, **kwargs)
        elif graph_type == GraphType.ColorMap:
            return cls.colormap(self, callback, **kwargs)
        raise ValueError(f"unsupported graph_type {graph_type!r} for single-arg plot()")

    def plot(self, *args, name=None, labels=None, colors=None, graph_type=None, **kwargs):
        graph_type = graph_type or GraphType.Line
        # Only colormaps and 2D histograms take name= natively (#114).
        takes_name = graph_type in (GraphType.ColorMap, GraphType.Histogram2D) or len(args) == 3
        graph = _plot(self, *args, name=name if takes_name else None, labels=labels,
                      colors=colors, graph_type=graph_type, **kwargs)
        if name is not None and not takes_name:
            graph.set_name(name)
        return graph

    def _plot(self, *args, name=None, labels=None, colors=None, graph_type=None, **kwargs):
        kwargs = _merge_kwargs(kwargs, name=name, labels=labels, colors=colors)
        if (graph_type == GraphType.ParametricCurve) and (len(args) in (1, 2, 4)) and not callable(args[0]):
            _reject_waterfall_kwargs(kwargs, graph_type)
            _reject_histogram2d_kwargs(kwargs, graph_type)
            plot_type = kwargs.pop("plot_type", None)
            if plot_type == PlotType.Projections:
                return cls.projection(self, *args, **kwargs)
            return cls.parametric_curve(self, *args, **kwargs)
        if len(args) == 1:
            return plot_func(self, *args, graph_type=graph_type, **kwargs)
        if len(args) == 2:
            if graph_type == GraphType.Waterfall:
                _reject_histogram2d_kwargs(kwargs, graph_type)
                wf_kwargs = _pop_waterfall_kwargs(kwargs)
                wf = _apply_waterfall_kwargs(cls.waterfall(self, *args, **kwargs), **wf_kwargs)
                # plot() fitted the axes before the offsets were applied.
                self.rescale_axes()
                return wf
            if graph_type == GraphType.Histogram2D:
                _reject_waterfall_kwargs(kwargs, graph_type)
                return cls.histogram2d(self, *args, **kwargs)
            _reject_waterfall_kwargs(kwargs, graph_type)
            _reject_histogram2d_kwargs(kwargs, graph_type)
            if graph_type == GraphType.Line:
                return cls.line(self, *args, **kwargs)
            if graph_type == GraphType.Scatter:
                return cls.scatter(self, *args, **kwargs)
            if graph_type == GraphType.ColorMap:
                return cls.colormap(self, *args, **kwargs)
            raise ValueError(f"unsupported graph_type {graph_type!r} for 2-arg plot()")
        if len(args) == 3:
            _reject_waterfall_kwargs(kwargs, graph_type)
            _reject_histogram2d_kwargs(kwargs, graph_type)
            return cls.colormap(self, *args, **kwargs)
        raise ValueError(f"only 1, 2 or 3 arguments are supported, got {len(args)}")

    cls.plot = plot
    return cls



SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
SciQLopMultiPlotPanel = _patch_sciqlop_plot(SciQLopMultiPlotPanel)
SciQLopNDProjectionPlot = _patch_sciqlop_plot(SciQLopNDProjectionPlot)

# --- Reactive pipeline API ---
from .properties import register_property, OnDescriptor
from .pipeline import Pipeline, PartialPipeline
from .event import Event

register_property(
    SciQLopGraphInterface, "data",
    signal_name="data_changed",
    getter_name="data",
    setter_name="set_data",
    property_type="data",
    signal_args=(),
    splat=True,  # set_data(x, y[, z])
)

from .SciQLopPlotsBindings import SciQLopWaterfallGraph

register_property(
    SciQLopWaterfallGraph, "offset_mode",
    signal_name="offset_mode_changed",
    getter_name="offset_mode", setter_name="set_offset_mode",
    property_type="enum",
)
register_property(
    SciQLopWaterfallGraph, "spacing",
    signal_name="uniform_spacing_changed",
    getter_name="uniform_spacing", setter_name="set_uniform_spacing",
    property_type="float",
)
register_property(
    SciQLopWaterfallGraph, "offsets",
    signal_name="offsets_changed",
    getter_name="offsets", setter_name="set_offsets",
    property_type="array",
)
register_property(
    SciQLopWaterfallGraph, "normalize",
    signal_name="normalize_changed",
    getter_name="normalize", setter_name="set_normalize",
    property_type="bool",
)
register_property(
    SciQLopWaterfallGraph, "gain",
    signal_name="gain_changed",
    getter_name="gain", setter_name="set_gain",
    property_type="float",
)

SciQLopWaterfallGraph.on = OnDescriptor()

register_property(
    SciQLopPlotsBindings.SciQLopPlotAxisInterface, "range",
    signal_name="range_changed",
    getter_name="range",
    setter_name="set_range",
    property_type="range",
    splat=True,  # set_range(start, stop) — unlike the span's set_range(range)
)

register_property(
    SciQLopPlotsBindings.SciQLopVerticalSpan, "range",
    signal_name="range_changed",
    getter_name="range",
    setter_name="set_range",
    property_type="range",
)

register_property(
    SciQLopPlotsBindings.SciQLopVerticalSpan, "tooltip",
    signal_name=None,
    getter_name="tool_tip",
    setter_name="set_tool_tip",
    property_type="string",
)

for _cls in (
    SciQLopGraphInterface,
    SciQLopPlotsBindings.SciQLopPlotAxisInterface,
    SciQLopPlotsBindings.SciQLopVerticalSpan,
):
    _cls.on = OnDescriptor()


# --- Accept the QPointer wrappers panel.plots() returns wherever a raw plot
#     pointer is expected (item/span constructors, panel plot-management
#     methods). shiboken cannot implicitly convert an object-type smart pointer,
#     so we dereference it here instead of forcing callers to write .data().
#
#     A Ptr is matched by isinstance against the registered smart-pointer types —
#     NEVER by probing for a .data() member: QByteArray, QModelIndex, ndarray and
#     many others expose .data() and would be silently corrupted by a duck-typed
#     check.
#
#     Tradeoff: routing a ctor through a Python wrapper bypasses shiboken's
#     tp_init, so a *wrong-args* call to a wrapped entry point yields a terser
#     "(missing signature)" TypeError instead of the "Supported signatures"
#     listing. Still a TypeError, never the old NameError; the happy path
#     (raw pointer or auto-deref'd Ptr) is unaffected.
import functools

_PTR_HANDLE_TYPES = (
    SciQLopPlotsBindings.SciQLopPlotInterfacePtr,
    SciQLopPlotsBindings.MultiPlotsVerticalSpanPtr,
)


def _deref_ptr_handle(value):
    return value.data() if isinstance(value, _PTR_HANDLE_TYPES) else value


def _accept_ptr_handle(func):
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        return func(*(_deref_ptr_handle(a) for a in args),
                    **{k: _deref_ptr_handle(v) for k, v in kwargs.items()})
    return wrapper


for _item_cls in (
    SciQLopPlotsBindings.SciQLopTextItem,
    SciQLopPlotsBindings.SciQLopPixmapItem,
    SciQLopPlotsBindings.SciQLopEllipseItem,
    SciQLopPlotsBindings.SciQLopCurvedLineItem,
    SciQLopPlotsBindings.SciQLopStraightLine,
    SciQLopPlotsBindings.SciQLopVerticalLine,
    SciQLopPlotsBindings.SciQLopHorizontalLine,
    SciQLopPlotsBindings.SciQLopVerticalSpan,
    SciQLopPlotsBindings.SciQLopHorizontalSpan,
    SciQLopPlotsBindings.SciQLopRectangularSpan,
):
    _item_cls.__init__ = _accept_ptr_handle(_item_cls.__init__)

for _method in ("add_plot", "remove_plot", "insert_plot", "move_plot", "index", "contains"):
    setattr(SciQLopMultiPlotPanel, _method,
            _accept_ptr_handle(getattr(SciQLopMultiPlotPanel, _method)))


# --- set_color_data(values, gradient): unmask the buffer error.
#
# Shiboken converts `values` before `gradient`. When the buffer conversion
# rejects the array it sets a Python error, and the enum converter then runs
# with that error already pending and reports "SystemError: bad argument to
# internal function" instead — burying the real cause. Validating the buffer
# first, in Python, keeps the useful TypeError.
def _validate_color_data(func):
    @functools.wraps(func)
    def wrapper(self, values, *args, **kwargs):
        if values is not None:
            SciQLopPlotsBindings.validate_buffer(values, "values")
        return func(self, values, *args, **kwargs)
    return wrapper


for _graph_cls in (SciQLopPlotsBindings.SciQLopSingleLineGraph,
                   SciQLopPlotsBindings.SciQLopCurve,
                   SciQLopPlotsBindings.SciQLopNDProjectionCurves):
    _graph_cls.set_color_data = _validate_color_data(_graph_cls.set_color_data)


# --- SciQLopTimeline.set_intervals(...): numpy-friendly front end for
# set_intervals_coded(), which only takes float64 buffers plus a first-seen
# name table per categorical column (lane, category).
import numpy as np


def _datetime64_seconds(a):
    ns = a.astype("datetime64[ns]")
    return np.where(np.isnat(ns), np.nan, ns.astype(np.int64) / 1e9)


def _epoch_seconds(values):
    a = np.asarray(values)
    if np.issubdtype(a.dtype, np.datetime64):
        return _datetime64_seconds(a)
    return np.ascontiguousarray(a, dtype=np.float64)


# --- Time ranges take datetime64 (any unit), datetime, date and ISO strings, all
# meaning UTC (SciQLop#150). Left to shiboken, a datetime64 went through float()
# (nanoseconds, or a TypeError for coarser units), and a datetime through QDateTime,
# which drops its zone and reads it in the machine's local time.
def _epoch_second(value):
    if isinstance(value, np.datetime64):
        return float(_datetime64_seconds(value))
    if isinstance(value, datetime):
        return (value if value.tzinfo else value.replace(tzinfo=timezone.utc)).timestamp()
    if isinstance(value, date):
        return datetime(value.year, value.month, value.day, tzinfo=timezone.utc).timestamp()
    if isinstance(value, str):
        try:
            return _epoch_second(datetime.fromisoformat(value))
        except ValueError:
            return value  # the C++ parser takes its own formats, and raises on garbage
    return value


def _is_date(value):
    return isinstance(value, (np.datetime64, date, str))


def _accept_dates_in_range(init):
    @functools.wraps(init)
    def wrapper(self, *args, **kwargs):
        if len(args) == 2 and any(map(_is_date, args)):
            seconds = [_epoch_second(a) for a in args]
            if not any(isinstance(s, str) for s in seconds):
                return init(self, *seconds, True, **kwargs)
        return init(self, *args, **kwargs)
    return wrapper


def _accept_dates(func):
    @functools.wraps(func)
    def wrapper(self, *args, **kwargs):
        return func(self, *map(_epoch_second, args), **kwargs)
    return wrapper


SciQLopPlotRange.__init__ = _accept_dates_in_range(SciQLopPlotRange.__init__)
for _cls in [c for c in vars(SciQLopPlotsBindings).values() if isinstance(c, type)]:
    for _name in ("set_range", "set_time_range", "set_x_axis_range", "set_time_axis_range"):
        if _name in _cls.__dict__:
            setattr(_cls, _name, _accept_dates(_cls.__dict__[_name]))


def _first_seen_codes(values, n):
    if values is None:
        return np.zeros(n, dtype=np.float64), [""]
    a = np.asarray(values).astype(str)
    unique, first, inverse = np.unique(a, return_index=True, return_inverse=True)
    order = np.argsort(first)
    rank = np.empty_like(order)
    rank[order] = np.arange(len(order))
    return rank[inverse].astype(np.float64), [str(u) for u in unique[order]]


def _check_intervals(start, stop, columns):
    if any(len(c) != len(start) for c in [stop, *columns] if c is not None):
        raise ValueError("start, stop, lane, category, label and ids must have the same length")
    if np.isnan(start).any() or np.isnan(stop).any():
        raise ValueError("start and stop must not contain NaN")
    if (stop < start).any():
        raise ValueError("stop must not be before start")


def _set_intervals(self, start, stop=None, lane=None, category=None, label=None, ids=None):
    start = _epoch_seconds(start)
    stop = start.copy() if stop is None else _epoch_seconds(stop)
    _check_intervals(start, stop, [lane, category, label, ids])
    lane_codes, lane_names = _first_seen_codes(lane, len(start))
    category_codes, category_names = _first_seen_codes(category, len(start))
    ids = np.arange(len(start), dtype=np.float64) if ids is None else np.ascontiguousarray(ids, dtype=np.float64)
    labels = [] if label is None else [str(s) for s in label]
    self.set_intervals_coded(start, stop, lane_codes, lane_names, category_codes, category_names,
                             labels, ids)


SciQLopTimeline.set_intervals = _set_intervals


# --- SciQLopTimeline `lanes` and editing API (`editable`, `edit_modes`, `snap_to`):
# properties over the C++ getters/setters captured below before being
# replaced.
_EDIT_MODES = {"move", "resize", "change_lane", "create", "delete"}
_edit_modes_get = SciQLopTimeline.edit_modes
_editable_get = SciQLopTimeline.editable
_lanes_get = SciQLopTimeline.lanes


def _set_edit_modes(self, modes):
    unknown = set(modes) - _EDIT_MODES
    if unknown:
        raise ValueError(f"unknown edit modes: {sorted(unknown)}; expected {sorted(_EDIT_MODES)}")
    self.set_edit_modes(sorted(modes))


def _get_snap_to(self):
    mode = self.snap_mode()
    if mode == "times":
        return list(self.snap_times())
    return {"edges": "edges", "step": self.snap_step()}.get(mode)


def _set_snap_to(self, value):
    if isinstance(value, (list, tuple, np.ndarray)):
        if len(value) == 0:
            raise ValueError("snap_to times must not be empty")
        self.set_snap_times([float(t) for t in _epoch_seconds(value).ravel()])
    elif value is None:
        self.clear_snap()
    elif value == "edges":
        self.set_snap_edges()
    elif isinstance(value, (int, float)) and not isinstance(value, bool) and value > 0:
        self.set_snap_step(float(value))
    else:
        raise ValueError("snap_to must be 'edges', a positive number of seconds, "
                         "a non-empty list of times, or None")


SciQLopTimeline.edit_modes = property(lambda self: set(_edit_modes_get(self)), _set_edit_modes)
SciQLopTimeline.editable = property(_editable_get, SciQLopTimeline.set_editable)
SciQLopTimeline.snap_to = property(_get_snap_to, _set_snap_to)
SciQLopTimeline.lanes = property(_lanes_get, SciQLopTimeline.set_lanes)


_TIMELINE_STYLES = ("wave", "bars")
_style_get = SciQLopTimeline.style


def _set_style(self, name):
    if name not in _TIMELINE_STYLES:
        raise ValueError(f"style must be one of {_TIMELINE_STYLES}, not {name!r}")
    self.set_style(name)


SciQLopTimeline.style = property(_style_get, _set_style)

_STACK_MODES = (None, "time", "category")
_stack_get = SciQLopTimeline.stack
_forbid_overlap_get = SciQLopTimeline.forbid_overlap
_category_order_get = SciQLopTimeline.category_order


def _set_stack(self, mode):
    if mode not in _STACK_MODES:
        raise ValueError(f"stack must be one of {_STACK_MODES}, not {mode!r}")
    self.set_stack(mode or "")


SciQLopTimeline.stack = property(lambda self: _stack_get(self) or None, _set_stack)
SciQLopTimeline.forbid_overlap = property(lambda self: bool(_forbid_overlap_get(self)),
                                          lambda self, v: self.set_forbid_overlap(bool(v)))
SciQLopTimeline.category_order = property(lambda self: list(_category_order_get(self)),
                                          lambda self, names: self.set_category_order(list(names)))
SciQLopTimeline.interval = lambda self, id: self.interval_info(int(id)) or None
SciQLopTimeline.interval_at = lambda self, x, y: None if (i := self._interval_at(x, y)) < 0 else i

