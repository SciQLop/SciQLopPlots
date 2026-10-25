from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from . import SciQLopPlotsBindings
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *

# One-column lines are SciQLopLineGraphs since #113: the old names stay for isinstance checks.
SciQLopSingleLineGraph = SciQLopLineGraph
SciQLopSingleLineGraphFunction = SciQLopLineGraphFunction
from datetime import date, datetime, timezone
import traceback
import warnings

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

    Shiboken also writes a few of our types under names the parser cannot
    resolve: a ``QPointer<T>`` inside a container keeps its C++ name instead
    of the ``<smart-pointer-type>`` instantiation, and a ``<property>`` type is
    left unqualified. Each would raise a "pyside_type_init ... UNRECOGNIZED"
    RuntimeWarning, so they are mapped too. Header defaults must stay
    parseable: a plain literal (``"ColorMap"``, not ``QStringLiteral``),
    ``Type()``, a qualified enum value, or ``QColor::fromRgb(...)`` (resolved
    through PySide6.QtGui); never ``QColor(r, g, b)``. Enum defaults must not
    start with ``::``: ``= ::GraphMarkerShape::NoMarker`` rendered as an
    unresolvable ``.GraphMarkerShape.NoMarker``.
    """
    import collections.abc

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
            "std.nullopt": None, "std::nullopt": None,
            # Shiboken emits namespaced primitives with dots in the generated
            # signature strings (``std::size_t`` -> ``std.size_t``) but looks
            # some paths up under the original spelling, so register both.
            "std::string": str, "std.string": str,
            "std::size_t": int, "std.size_t": int,
            "QPointer[SciQLopPlotsBindings.SciQLopPlotInterface]":
                SciQLopPlotsBindings.SciQLopPlotInterfacePtr,
            "QPointer[SciQLopPlotsBindings.MultiPlotsVerticalSpan]":
                SciQLopPlotsBindings.MultiPlotsVerticalSpanPtr,
            "SciQLopPlotRange": SciQLopPlotsBindings.SciQLopPlotRange,
        })
    except Exception:
        pass


_register_with_shiboken_signatures()

from . import tracing  # noqa: E402,F401  -- runtime tracer facade
from . import _dispatch  # noqa: E402

__version__ = '0.52.0'

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


def _projection_buffers(args):
    """The one list the projection binding takes: [t, d0, d1, ...].

    Accepts that list itself, (t, d0, d1, ...) or (t, values) with one column per dimension.
    """
    import numpy as np
    if len(args) == 1:
        return args[0]
    if len(args) == 2 and np.ndim(args[1]) == 2:
        t, values = args
        values = np.asarray(values)
        return [t, *(np.ascontiguousarray(values[:, i]) for i in range(values.shape[1]))]
    return list(args)


def _apply_waterfall_post(shape, host, result, post):
    _apply_waterfall_kwargs(result, **post)
    if shape.name == "arrays":
        # plot() fitted the axes before the offsets were applied. On a panel, refit
        # only the new plot: a panel-wide rescale would undo the other plots' zoom.
        (result[0] if isinstance(result, tuple) else host).rescale_axes()


# histogram's post options are not here: plot() calls the Python histogram, which applies them.
_POST = {"waterfall": _apply_waterfall_post}


def _pop_post(name, shape, kwargs):
    return {k: kwargs.pop(k) for k in shape.post if k in kwargs} if name in _POST else {}


def _apply_post(name, shape, host, result, post):
    if name in _POST:
        _POST[name](shape, host, result, post)


def plot(self, *args, graph_type=None, **kwargs):
    kwargs = {k: v for k, v in kwargs.items() if v is not None}
    name, kind, shape = _dispatch.resolve(type(self), args, graph_type, kwargs)
    post = _pop_post(name, shape, kwargs)
    label = kwargs.pop("name", None) if "name" not in kind.keywords_on(type(self)) else None
    for consumed in kind.consumed:
        kwargs.pop(consumed, None)
    factory = getattr(self, kind.factory[_dispatch.host_class(type(self))])
    args = (_projection_buffers(args),) if name == "projection" else args
    result = factory(*args, **kwargs)
    _apply_post(name, shape, self, result, post)
    if label is not None:
        _graph_of(result).set_name(label)
    return result


def _patch_sciqlop_plot(cls):
    cls.plot = plot
    return cls



def _graph_of(result):
    return result[1] if isinstance(result, tuple) else result


def _as_columns(*columns):
    import numpy as np
    return np.column_stack([np.asarray(c, dtype=np.float64) for c in columns])


def _error_bounds(y, err_minus, err_plus=None):
    import numpy as np
    y = np.asarray(y, dtype=np.float64)

    def as_error(e):
        e = np.asarray(e, dtype=np.float64)
        if e.ndim != 0 and e.shape != y.shape:
            raise ValueError(f"errors must be a scalar or have y's length ({len(y)}), got shape {e.shape}")
        return e
    em = as_error(err_minus)
    ep = em if err_plus is None else as_error(err_plus)
    return y - em, y + ep


def _band_batch(x, lower, upper):
    return x, _as_columns(lower, upper)


def _error_batch(x, y, *errors):
    return x, _as_columns(y, *_error_bounds(y, *errors))


def _wrapped_callback(callback, to_batch):
    def wrapped(start, stop):
        batch = callback(start, stop)
        if batch is None or len(batch) == 0:
            return batch
        if isinstance(batch, dict):
            raise TypeError("band()/error_bars() callbacks return arrays, not the {'data', 'color'} form")
        return to_batch(*batch)
    return wrapped


def _line_data(args, to_batch, arity):
    if len(args) == 1 and callable(args[0]):
        return (_wrapped_callback(args[0], to_batch),)
    if len(args) in arity:
        return to_batch(*args)
    raise TypeError(f"expected a callback or {' or '.join(map(str, arity))} arrays, got {len(args)} arguments")


def _named(graph, name):
    if name is not None:
        graph.set_name(name)


def _hide_columns(graph, columns, keep_in_legend=()):
    for c in columns:
        graph.set_column_line_style(c, GraphLineStyle.NoLine)
        if c not in keep_in_legend:
            graph.set_column_in_legend(c, False)


def _band(self, *args, name=None, color=None, **kwargs):
    data = _line_data(args, _band_batch, (3,))
    colors = [color, color] if color is not None else []
    result = self.line(*data, labels=[name or "band", "upper"], colors=colors, **kwargs)
    graph = _graph_of(result)
    _named(graph, name)
    _hide_columns(graph, (0, 1), keep_in_legend=(0,))
    graph.set_fill_between(0, 1)
    return result


def _error_bars(self, *args, name=None, color=None, cap_width=6.0, **kwargs):
    data = _line_data(args, _error_batch, (3, 4))
    colors = [color] * 3 if color is not None else []
    result = self.line(*data, labels=[name or "y", "lower", "upper"], colors=colors, **kwargs)
    graph = _graph_of(result)
    _named(graph, name)
    graph.set_column_marker_shape(0, GraphMarkerShape.Circle)
    _hide_columns(graph, (0, 1, 2), keep_in_legend=(0,))
    graph.set_error_bars(0, 1, 2, cap_width)
    return result


SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
SciQLopMultiPlotPanel = _patch_sciqlop_plot(SciQLopMultiPlotPanel)
SciQLopNDProjectionPlot = _patch_sciqlop_plot(SciQLopNDProjectionPlot)

def _configure_histogram(hist, range, log_bins, normalization, style):
    # Log first: the range check depends on it.
    if log_bins:
        hist.set_log_bins(True)
    if range is not None:
        hist.set_bin_range(*range)
    hist.set_normalization(normalization)
    if style is not None:
        hist.set_style(style)


def _as_values(values_or_callback):
    import numpy as np
    if isinstance(values_or_callback, (list, tuple)):
        return np.asarray(values_or_callback, dtype=np.float64)
    return values_or_callback


def _histogram_calling(cpp_histogram):
    def histogram(self, values_or_callback, bins=64, range=None, log_bins=False,
                  normalization=HistogramNormalization.Count, style=None, name=None, **kwargs):
        if name is not None:
            kwargs["name"] = name
        result = cpp_histogram(self, _as_values(values_or_callback), bins=bins, **kwargs)
        _configure_histogram(_graph_of(result), range, log_bins, normalization, style)
        return result
    histogram.__doc__ = cpp_histogram.__doc__
    return histogram


# Not SciQLopTimeSeriesPlot: x is time there, so its C++ histogram()/bars() raise.
_HISTOGRAM_HOSTS = (SciQLopPlot, SciQLopMultiPlotPanel)
_cpp_histogram = {_cls: _cls.histogram for _cls in _HISTOGRAM_HOSTS}

for _cls in _HISTOGRAM_HOSTS:
    _cls.band = _band
    _cls.error_bars = _error_bars
    _cls.histogram = _histogram_calling(_cpp_histogram[_cls])


# --- Reactive pipeline API ---
from .properties import register_property, OnDescriptor
from .pipeline import Pipeline, PartialPipeline
from .event import Event
from .widget_plot import WidgetPlot

# Names the x / set_x / x_changed convention cannot derive (see properties.py).
_ON_EXCEPTIONS = (
    (SciQLopPlotsBindings.SciQLopGraphInterface, "data", dict(
        signal_name="data_changed", getter_name="data", setter_name="set_data",
        property_type="data", signal_args=(), splat=True)),
    (SciQLopPlotsBindings.SciQLopPlotAxisInterface, "range", dict(
        signal_name="range_changed", getter_name="range", setter_name="set_range",
        property_type="range", splat=True)),
    (SciQLopPlotsBindings.SciQLopDrawnObject, "selected", dict(
        signal_name="selection_changed", getter_name="selected", setter_name="set_selected",
        property_type="value")),
    (SciQLopPlotsBindings.SciQLopWaterfallGraph, "spacing", dict(
        signal_name="uniform_spacing_changed", getter_name="uniform_spacing",
        setter_name="set_uniform_spacing", property_type="value")),
    (SciQLopPlotsBindings.SciQLopVerticalSpan, "tooltip", dict(
        signal_name=None, getter_name="tool_tip", setter_name="set_tool_tip",
        property_type="value")),
    (SciQLopPlotsBindings.SciQLopSpan, "tooltip", dict(
        signal_name="tool_tip_changed", getter_name="tool_tip", setter_name="set_tool_tip",
        property_type="value")),
    (SciQLopPlotsBindings.SciQLopStraightLine, "tooltip", dict(
        signal_name="tool_tip_changed", getter_name="tool_tip", setter_name="set_tool_tip",
        property_type="value")),
)

for _cls, _name, _spec in _ON_EXCEPTIONS:
    register_property(_cls, _name, **_spec)

for _cls in (
    SciQLopPlotsBindings.SciQLopDrawnObject,
    SciQLopPlotsBindings.SciQLopPlotAxisInterface,
    SciQLopPlotsBindings.SciQLopPlotInterface,
    SciQLopPlotsBindings.SciQLopPlotPanelInterface,
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
#
#     The same wrapper warns that the item constructors are deprecated in favour
#     of the plot and panel factories.
import functools
import warnings

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


# class -> (replacement factory, whether the first argument is a plot that must not be None)
_ITEM_FACTORIES = {
    SciQLopPlotsBindings.SciQLopTextItem: ("plot.text", True),
    SciQLopPlotsBindings.SciQLopPixmapItem: ("plot.pixmap", True),
    SciQLopPlotsBindings.SciQLopEllipseItem: ("plot.ellipse", True),
    SciQLopPlotsBindings.SciQLopCurvedLineItem: ("plot.curved_line", True),
    SciQLopPlotsBindings.SciQLopStraightLine: ("plot.straight_line", True),
    SciQLopPlotsBindings.SciQLopVerticalLine: ("plot.vertical_line", True),
    SciQLopPlotsBindings.SciQLopHorizontalLine: ("plot.horizontal_line", True),
    SciQLopPlotsBindings.SciQLopVerticalSpan: ("plot.vertical_span", True),
    SciQLopPlotsBindings.SciQLopHorizontalSpan: ("plot.horizontal_span", True),
    SciQLopPlotsBindings.SciQLopRectangularSpan: ("plot.rectangular_span", True),
    SciQLopPlotsBindings.MultiPlotsVerticalSpan: ("panel.vertical_span", False),
    SciQLopPlotsBindings.MultiPlotsVerticalLine: ("panel.vertical_line", False),
}


def _deprecated_constructor(cls, factory, needs_plot):
    # Shiboken maps every C++ exception from a constructor to RuntimeError; a null plot is a
    # caller's mistake, so it is a ValueError, raised here before the C++ guard would be.
    # Panel objects take a panel, which has no such check.
    init = _accept_ptr_handle(cls.__init__)
    owner = factory.split(".")[0]
    message = f"{cls.__name__}({owner}, ...) is deprecated: use {factory}(...)"

    @functools.wraps(init)
    def wrapper(self, plot=None, *args, **kwargs):
        if type(self) is cls:  # the guard silences the warning entirely for a Python subclass of an item class
            warnings.warn(message, DeprecationWarning, stacklevel=2)
        if needs_plot and plot is None:
            raise ValueError(f"{type(self).__name__}: plot cannot be None")
        return init(self, plot, *args, **kwargs)
    return wrapper


for _item_cls, (_factory, _needs_plot) in _ITEM_FACTORIES.items():
    _item_cls.__init__ = _deprecated_constructor(_item_cls, _factory, _needs_plot)

# The plot line shorthands took movable second (5.6); color and read_only replace it. A bool there
# would silently become a QColor (QColor(int)), so it raises; movable= still works, inverted.
def _line_shorthand(name, method):
    message = f"movable=... is deprecated: use plot.{name}(..., read_only=...)"

    @functools.wraps(method)
    def wrapper(self, position, *args, **kwargs):
        if args and isinstance(args[0], bool):
            raise TypeError(f"{name}(position, color, ...): the second argument is the colour; "
                            "pass read_only=... to choose whether the user can drag the line")
        if "movable" in kwargs:
            warnings.warn(message, DeprecationWarning, stacklevel=2)
            kwargs["read_only"] = not kwargs.pop("movable")
        return method(self, position, *args, **kwargs)
    return wrapper


for _name in ("vertical_line", "horizontal_line"):
    setattr(SciQLopPlotsBindings.SciQLopPlotInterface, _name,
            _line_shorthand(_name, vars(SciQLopPlotsBindings.SciQLopPlotInterface)[_name]))


def _deprecated_call(message, method):
    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
        warnings.warn(message, DeprecationWarning, stacklevel=2)
        return method(self, *args, **kwargs)
    return wrapper


# (class, method) -> message. Plain methods only: never a Qt slot (a wrapper cuts its connections).
_DEPRECATED_CALLS = {
    (SciQLopPlotsBindings.SciQLopPlotInterface, "rectangular_span"):
        "rectangular_span(...) is deprecated: use plot.span(...)",
}
for (_cls, _name), _message in _DEPRECATED_CALLS.items():
    setattr(_cls, _name, _deprecated_call(_message, vars(_cls)[_name]))

_FORMAT_SAVERS = ("save_pdf", "save_svg", "save_png", "save_jpg", "save_bmp")
_SAVE_OWNERS = {"plot": (SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlot, SciQLopNDProjectionPlot),
                "panel": (SciQLopPlotsBindings.SciQLopPlotPanelInterface, SciQLopMultiPlotPanel)}


def _deprecated_saver(name, method, receiver):
    message = f"{name}(...) is deprecated: use {receiver}.save(...)"

    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
        warnings.warn(message, DeprecationWarning, stacklevel=2)
        return method(self, *args, **kwargs)
    return wrapper


for _receiver, _classes in _SAVE_OWNERS.items():
    for _cls in _classes:
        for _name in _FORMAT_SAVERS:
            if _name in vars(_cls):
                setattr(_cls, _name, _deprecated_saver(_name, vars(_cls)[_name], _receiver))

for _method in ("add_plot", "remove_plot", "insert_plot", "move_plot", "index", "contains", "remove"):
    setattr(SciQLopMultiPlotPanel, _method,
            _accept_ptr_handle(getattr(SciQLopMultiPlotPanel, _method)))


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
    if isinstance(value, QtCore.QDateTime):
        return value.toMSecsSinceEpoch() / 1000.0
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
    return isinstance(value, (np.datetime64, date, str, QtCore.QDateTime))


def _accept_dates_in_range(init):
    @functools.wraps(init)
    def wrapper(self, *args, **kwargs):
        if len(args) == 2 and any(map(_is_date, args)):
            seconds = [_epoch_second(a) for a in args]
            if not any(isinstance(s, str) for s in seconds):
                return init(self, *seconds, True, **kwargs)
        return init(self, *args, **kwargs)
    return wrapper


SciQLopPlotRange.__init__ = _accept_dates_in_range(SciQLopPlotRange.__init__)


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




# snake_case names for the camelCase ones (API review 2026-10-20, 1.5 and 1.6). The same attribute
# under a second name, never a wrapper: a Python wrapper on a Qt slot breaks its connections.
# The camelCase names stay, deprecated.
_SNAKE_CASE_ALIASES = {
    SciQLopMultiPlotPanel: {"set_selected": "setSelected", "add_widget": "addWidget",
                            "remove_widget": "removeWidget", "index_of": "indexOf",
                            "selection_changed": "selectionChanged"},
    SciQLopPlotsBindings.SciQLopMultiPlotObject: {"add_object": "addObject",
                                                  "remove_object": "removeObject",
                                                  "replot_all": "replotAll",
                                                  "update_plot_list": "updatePlotList"},
}
for _cls, _aliases in _SNAKE_CASE_ALIASES.items():
    for _new, _old in _aliases.items():
        setattr(_cls, _new, getattr(_cls, _old))

# Old names kept as aliases of the native ones (docs/plans/2026-10-22-drawn-object-base-design.md),
# deprecated. The same attribute under a second name, never a wrapper (see above); a warning on
# connect would need a proxy object.
_DEPRECATED_ALIASES = {
    SciQLopPlotsBindings.SciQLopSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopVerticalSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopHorizontalSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopRectangularSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopPlotLegendInterface: {"is_visible": "visible",
                                                      "visibility_changed": "visible_changed"},
}
for _cls, _aliases in _DEPRECATED_ALIASES.items():
    for _alias, _native in _aliases.items():
        setattr(_cls, _alias, getattr(_cls, _native))


def _public_names():
    # `from .SciQLopPlotsBindings import *` and this module's own imports (numpy, datetime, the Qt
    # modules) would all leak into a star import: list what the package offers instead.
    import types
    bound = {n for n in dir(SciQLopPlotsBindings)
             if not n.startswith("_") and not isinstance(getattr(SciQLopPlotsBindings, n), types.ModuleType)}
    # The SciQLopSingleLineGraph* aliases stay importable by name, but are no longer advertised.
    own = {"tracing", "WidgetPlot",
           "Pipeline", "PartialPipeline", "Event", "register_property", "OnDescriptor"}
    return sorted(bound | own)


__all__ = _public_names()
