import contextlib
import sys

from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg


@contextlib.contextmanager
def _no_pyside_features_while_importing():
    """PySide picks a feature set (true_property, snake_case) from the calling frame's module. The
    bindings' init runs in importlib's frames, which reuse the user's true_property once a
    QApplication exists. Building that set during our init (likely from the lazy PySide class
    loading in bindings.xml) asks for our method signatures before they are registered and aborts
    Python ("failed to create a new feature set"). Pinning importlib's frames to none avoids it."""
    table = getattr(sys.modules.get("__feature__"), "pyside_feature_dict", None)
    if table is None:
        yield
        return
    frames = ("importlib._bootstrap", "importlib._bootstrap_external")
    saved = {name: table.get(name) for name in frames}
    table.update(dict.fromkeys(frames, 0))
    try:
        yield
    finally:
        for name, value in saved.items():
            if value is None:
                table.pop(name, None)
            else:
                table[name] = value


with _no_pyside_features_while_importing():
    from . import SciQLopPlotsBindings  # noqa: E402
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *

# One-column lines are SciQLopLineGraphs since #113: the old names stay for isinstance checks.
SciQLopSingleLineGraph = SciQLopLineGraph
SciQLopSingleLineGraphFunction = SciQLopLineGraphFunction
from datetime import date, datetime, timezone
import math
import traceback
import warnings

import os
import tempfile

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


def _as_bound(reference, bound):
    import numpy as np
    bound = np.asarray(bound, dtype=np.float64)
    if bound.ndim != 0 and bound.shape != np.shape(reference):
        raise ValueError(f"expected a scalar or an array of x's length ({len(reference)}), got shape {bound.shape}")
    return np.broadcast_to(bound, np.shape(reference))


def _error_bounds(y, err_minus, err_plus=None):
    import numpy as np
    y = np.asarray(y, dtype=np.float64)
    em = _as_bound(y, err_minus)
    ep = em if err_plus is None else _as_bound(y, err_plus)
    return y - em, y + ep


def _band_batch(x, lower, upper):
    return x, _as_columns(_as_bound(x, lower), _as_bound(x, upper))


def _error_batch(x, y, *errors):
    return x, _as_columns(y, *_error_bounds(y, *errors))


def _wrapped_callback(callback, to_batch):
    def wrapped(start, stop):
        batch = callback(start, stop)
        if isinstance(batch, dict):
            raise TypeError("band()/error_bars() callbacks return arrays, not the {'data', 'color'} form")
        if batch is None or len(batch) == 0:
            return batch
        return to_batch(*batch)
    return wrapped


def _line_data(args, to_batch, arity):
    _dispatch.reject_callback_with_arrays(args)
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
from .properties import register_property, register_hidden, OnDescriptor
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
)

for _cls, _name, _spec in _ON_EXCEPTIONS:
    register_property(_cls, _name, **_spec)

# Deprecated methods the .on convention would otherwise pick up (set_movable makes movable a
# "property"): reading them through .on would call the deprecated method.
_ON_HIDDEN = {(SciQLopPlotsBindings.SciQLopStraightLine, "movable")}

for _cls, _name in _ON_HIDDEN:
    register_hidden(_cls, _name)

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
import enum
import functools
import inspect
import warnings

from shibokensupport.signature import get_signature


_PACKAGE_DIR = os.path.dirname(__file__)


def _warn_deprecated(message):
    """Warn at the first frame outside this package: wrappers stack, a fixed stacklevel drifts."""
    frame, level = sys._getframe(1), 2
    while frame is not None and os.path.dirname(frame.f_code.co_filename) == _PACKAGE_DIR:
        frame, level = frame.f_back, level + 1
    warnings.warn(message, DeprecationWarning, stacklevel=level)

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
    SciQLopPlotsBindings.SciQLopRectangularSpan: ("plot.span", True),
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
            _warn_deprecated(message)
        if needs_plot and plot is None:
            raise ValueError(f"{type(self).__name__}: plot cannot be None")
        return init(self, plot, *args, **kwargs)
    return wrapper


for _item_cls, (_factory, _needs_plot) in _ITEM_FACTORIES.items():
    _item_cls.__init__ = _deprecated_constructor(_item_cls, _factory, _needs_plot)

def _wraps(method):
    """functools.wraps, plus the C++ signature: inspect cannot see through a shiboken method."""
    def decorate(wrapper):
        functools.wraps(method)(wrapper)
        wrapper.__signature__ = getattr(method, "__signature__", None) or get_signature(method)
        return wrapper
    return decorate


# Line and span factories took movable in the colour's place (5.6). A bool there would silently become a
# QColor (QColor(int)), so every colour slot raises; the slot counts positional arguments after self.
def _reject_bool_colour(name, method, slot):
    @_wraps(method)
    def wrapper(self, *args, **kwargs):
        if (len(args) > slot and isinstance(args[slot], bool)) or isinstance(kwargs.get("color"), bool):
            raise TypeError(f"{name}(): a bool is not a colour; "
                            "pass read_only=... to choose whether the user can drag the line")
        return method(self, *args, **kwargs)
    return wrapper


_COLOUR_SLOTS = (
    (SciQLopPlotsBindings.SciQLopPlotInterface, "vertical_line", 1),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "horizontal_line", 1),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "straight_line", 2),
    (SciQLopPlotsBindings.SciQLopPlot, "straight_line", 2),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "vertical_line", 1),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "horizontal_line", 1),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "straight_line", 2),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "vertical_span", 1),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "horizontal_span", 1),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "rectangular_span", 2),
    (SciQLopPlotsBindings.SciQLopPlotInterface, "span", 2),
    (SciQLopPlotsBindings.SciQLopPlot, "span", 2),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "vertical_span", 1),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "horizontal_span", 1),
    (SciQLopPlotsBindings.SciQLopMultiPlotPanel, "span", 2),
)
for _cls, _name, _slot in _COLOUR_SLOTS:
    _method = getattr(_cls, _name, None)
    if _method is not None and not hasattr(_method, "__wrapped__"):
        setattr(_cls, _name, _reject_bool_colour(_name, _method, _slot))


# The plot line shorthands still accept movable=..., inverted.
def _line_shorthand(name, method):
    message = f"movable=... is deprecated: use plot.{name}(..., read_only=...)"

    @_wraps(method)
    def wrapper(self, position, *args, **kwargs):
        if "movable" in kwargs:
            _warn_deprecated(message)
            kwargs["read_only"] = not kwargs.pop("movable")
        return method(self, position, *args, **kwargs)
    return wrapper


for _name in ("vertical_line", "horizontal_line"):
    setattr(SciQLopPlotsBindings.SciQLopPlotInterface, _name,
            _line_shorthand(_name, vars(SciQLopPlotsBindings.SciQLopPlotInterface)[_name]))


def _deprecated_call(message, method):
    @_wraps(method)
    def wrapper(self, *args, **kwargs):
        _warn_deprecated(message)
        return method(self, *args, **kwargs)
    return wrapper


# (class, method) -> message. Plain methods only: never a Qt slot (a wrapper cuts its connections).
_DEPRECATED_CALLS = {
    (SciQLopPlotsBindings.SciQLopPlotInterface, "rectangular_span"):
        "rectangular_span(...) is deprecated: use plot.span(...)",
    (SciQLopPlotsBindings.SciQLopStraightLine, "movable"):
        "movable() is deprecated: use line.read_only()",
    (SciQLopPlotsBindings.SciQLopStraightLine, "set_movable"):
        "set_movable(...) is deprecated: use line.set_read_only(...)",
}
for (_cls, _name), _message in _DEPRECATED_CALLS.items():
    setattr(_cls, _name, _deprecated_call(_message, vars(_cls)[_name]))

# Review 1.10: text, pixmap and ellipse items say read_only, like spans and lines. The old setter
# goes through set_read_only, so read_only_changed still fires.
def _set_movable_through_read_only(method):
    @_wraps(method)
    def wrapper(self, movable):
        _warn_deprecated("set_movable(...) is deprecated: use item.set_read_only(...)")
        self.set_read_only(not movable)
    return wrapper


for _cls in (SciQLopPlotsBindings.SciQLopMovableItemInterface, SciQLopPlotsBindings.SciQLopBoundingRectItemInterface,
             SciQLopPlotsBindings.SciQLopPolygonItemInterface, SciQLopPlotsBindings.SciQLopTextItem,
             SciQLopPlotsBindings.SciQLopPixmapItem, SciQLopPlotsBindings.SciQLopEllipseItem):
    if "movable" in vars(_cls):
        _cls.movable = _deprecated_call("movable() is deprecated: use item.read_only()", vars(_cls)["movable"])
    if "set_movable" in vars(_cls):
        _cls.set_movable = _set_movable_through_read_only(vars(_cls)["set_movable"])
    register_hidden(_cls, "movable")


# Their factories took movable (default False) where read_only (default True) now sits: a positional
# bool would silently flip, so it raises.
def _item_factory(name, method):
    message = f"movable=... is deprecated: use plot.{name}(..., read_only=...)"

    @_wraps(method)
    def wrapper(self, *args, **kwargs):
        if any(isinstance(a, bool) for a in args):
            raise TypeError(f"{name}(): pass read_only=... by keyword (it replaced movable, its inverse)")
        if "movable" in kwargs:
            _warn_deprecated(message)
            kwargs["read_only"] = not kwargs.pop("movable")
        return method(self, *args, **kwargs)
    return wrapper


for _cls in (SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlot):
    for _name in ("text", "pixmap", "ellipse"):
        setattr(_cls, _name, _item_factory(_name, vars(_cls)[_name]))

# Review 1.7 / 1.16 / 2.8: (class, old name) -> (new name, receiver). The new name is the native
# attribute under a second name; the old one warns.
_RENAMED_CALLS = {
    (SciQLopPlotsBindings.SciQLopNDProjectionPlot, "add_reference_curve"): ("reference_curve", "projection"),
    (SciQLopPlotsBindings.SciQLopNDProjectionPlot, "add_model_curve"): ("model_curve", "projection"),
    (SciQLopPlotsBindings.SciQLopColorMapBase, "auto_contour_level_count"): ("auto_contour_levels", "colormap"),
}
for (_cls, _old), (_new, _receiver) in _RENAMED_CALLS.items():
    _native = vars(_cls)[_old]
    setattr(_cls, _new, _native)
    setattr(_cls, _old, _deprecated_call(f"{_old}(...) is deprecated: use {_receiver}.{_new}(...)", _native))

SciQLopPlot.enable_cursor = _deprecated_call(
    "enable_cursor(...) is deprecated: use plot.set_crosshair_enabled(...)", vars(SciQLopPlot)["enable_cursor"])



# x(data or callback) builds from data, add_x(...) makes an empty one fed later: add_waterfall's
# callback form duplicates waterfall(callback). Its empty form stays.
def _deprecated_waterfall_callback(method):
    @_wraps(method)
    def wrapper(self, *args, **kwargs):
        if args and callable(args[0]):
            _warn_deprecated("add_waterfall(callback, ...) is deprecated: use plot.waterfall(callback, ...)")
        return method(self, *args, **kwargs)
    return wrapper


SciQLopPlot.add_waterfall = _deprecated_waterfall_callback(vars(SciQLopPlot)["add_waterfall"])

def _to_bytes(self, format="png", width=0, height=0, scale=1.0, quality=-1):
    """The image save() writes (pdf, svg, png, jpg or bmp), as bytes, without a file of yours."""
    # simplify: goes through a temporary file, so it is exactly save()'s output for plots, panels
    # and projections; render to a QBuffer instead if export speed ever matters.
    with tempfile.TemporaryDirectory() as folder:
        path = os.path.join(folder, "export")
        self.save(path, width, height, scale, quality, format)
        with open(path, "rb") as exported:
            return exported.read()


for _cls in (SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlotsBindings.SciQLopPlotPanelInterface):
    _cls.to_bytes = _to_bytes

_FORMAT_SAVERS = ("save_pdf", "save_svg", "save_png", "save_jpg", "save_bmp")
_SAVE_OWNERS = {"plot": (SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlot, SciQLopNDProjectionPlot),
                "panel": (SciQLopPlotsBindings.SciQLopPlotPanelInterface, SciQLopMultiPlotPanel)}


def _deprecated_saver(name, method, receiver):
    message = f"{name}(...) is deprecated: use {receiver}.save(...)"

    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
        _warn_deprecated(message)
        return method(self, *args, **kwargs)
    return wrapper


for _receiver, _classes in _SAVE_OWNERS.items():
    for _cls in _classes:
        for _name in _FORMAT_SAVERS:
            if _name in vars(_cls):
                setattr(_cls, _name, _deprecated_saver(_name, vars(_cls)[_name], _receiver))


# Review 1.10: one word, tooltip. tooltip / set_tooltip / tooltip_changed are the native attributes
# under a second name; the old methods warn, the old signal cannot (a wrapped signal dies on GC).
def _bound_classes():
    return [c for _, c in inspect.getmembers(SciQLopPlotsBindings, inspect.isclass)]


for _cls in _bound_classes():
    _own = vars(_cls)
    for _old, _new in (("tool_tip", "tooltip"), ("set_tool_tip", "set_tooltip")):
        if _old in _own:
            _native = _own[_old]
            setattr(_cls, _new, _native)
            setattr(_cls, _old, _deprecated_call(f"{_old}(...) is deprecated: use item.{_new}(...)", _native))
            register_hidden(_cls, _old)
    if "tool_tip_changed" in _own:
        _cls.tooltip_changed = _own["tool_tip_changed"]


def _old_tooltip_keyword(name, receiver, method):
    message = f"tool_tip=... is deprecated: use {receiver}.{name}(..., tooltip=...)"

    @_wraps(method)
    def wrapper(self, *args, **kwargs):
        if "tool_tip" in kwargs:
            _warn_deprecated(message)
            kwargs["tooltip"] = kwargs.pop("tool_tip")
        return method(self, *args, **kwargs)
    return wrapper


# Every method with a tooltip parameter, slots excepted (a wrapped slot dies on GC). Listed rather
# than found at import: reading every signature doubled the import time. A test keeps it complete.
_TOOLTIP_KEYWORD_METHODS = {
    SciQLopPlotsBindings.SciQLopPlotInterface: ("curved_line", "ellipse", "horizontal_line", "horizontal_span",
                                                "rectangular_span", "span", "straight_line", "vertical_line",
                                                "vertical_span"),
    SciQLopPlot: ("curved_line", "ellipse", "span", "straight_line"),
    SciQLopMultiPlotPanel: ("horizontal_line", "horizontal_span", "span", "straight_line", "vertical_line",
                            "vertical_span"),
}
for _cls, _names in _TOOLTIP_KEYWORD_METHODS.items():
    _receiver = "panel" if issubclass(_cls, SciQLopPlotsBindings.SciQLopPlotPanelInterface) else "plot"
    for _name in _names:
        setattr(_cls, _name, _old_tooltip_keyword(_name, _receiver, vars(_cls)[_name]))

for _method in ("add_plot", "remove_plot", "insert_plot", "move_plot", "index", "contains", "remove"):
    setattr(SciQLopMultiPlotPanel, _method,
            _accept_ptr_handle(getattr(SciQLopMultiPlotPanel, _method)))


# --- SciQLopTimeline.set_intervals(...): numpy-friendly front end for
# set_intervals_coded(), which only takes float64 buffers plus a first-seen
# name table per categorical column (lane, category).
import numpy as np


_FINE_UNITS = ("ns", "ps", "fs", "as")


def _datetime64_seconds(a):
    """Epoch seconds, NaT as NaN. Microseconds unless the data is finer, so that years
    beyond 2262 do not wrap around in the int64 nanosecond count."""
    unit, scale = ("ns", 1e9) if np.datetime_data(a.dtype)[0] in _FINE_UNITS else ("us", 1e6)
    ticks = a.astype(f"datetime64[{unit}]")
    return np.where(np.isnat(ticks), np.nan, ticks.astype(np.int64) / scale)


def _aware(dt):
    return dt if dt.tzinfo else dt.replace(tzinfo=timezone.utc)


def _iso_seconds(text):
    try:
        return _aware(datetime.fromisoformat(text)).timestamp()
    except ValueError:
        raise ValueError(f"unparsable date string {text!r}") from None


# One entry per scalar kind. Order matters: datetime is a date, bool and int are numbers.
_SCALAR_SECONDS = (
    (QtCore.QDateTime, lambda v: v.toMSecsSinceEpoch() / 1000.0),
    (datetime, lambda v: _aware(v).timestamp()),
    (date, lambda v: datetime(v.year, v.month, v.day, tzinfo=timezone.utc).timestamp()),
    (str, _iso_seconds),
    ((int, float, np.integer, np.floating), float),
)


def _scalar_seconds(value):
    if isinstance(value, np.timedelta64):
        raise TypeError(f"expected a number or a date, not {value!r}")
    if isinstance(value, np.datetime64):
        seconds = float(_datetime64_seconds(np.asarray(value)))
        if math.isnan(seconds):
            raise ValueError("NaT is not a valid time")
        return seconds
    for kinds, convert in _SCALAR_SECONDS:
        if isinstance(value, kinds):
            return convert(value)
    if hasattr(type(value), "__float__"):  # Decimal, Fraction: time arguments always took these
        return float(value)
    raise TypeError(f"expected a number or a date, not {value!r}")


def _element_seconds(value):
    """An object array holding bare numbers is a mistake (a column that lost its dtype), not times."""
    if isinstance(value, (int, float, np.number)):
        raise TypeError(f"expected a date or a date string in an object array, not {value!r}")
    return _scalar_seconds(value)


def _array_seconds(values, allow_nat):
    a = np.asarray(values)
    if np.issubdtype(a.dtype, np.datetime64):
        seconds = _datetime64_seconds(a)
        if not allow_nat and np.isnan(seconds).any():
            raise ValueError("NaT is not a valid time")
        return seconds
    if a.dtype.kind in "OUS":
        return np.array([_element_seconds(v) for v in a.ravel()], dtype=np.float64).reshape(a.shape)
    return np.ascontiguousarray(a, dtype=np.float64)


def to_epoch_seconds(value, allow_nat=False):
    """UTC epoch seconds from any time value; the conversion every time argument goes through.

    A scalar (float, int, datetime64 of any unit, datetime, date, ISO string, QDateTime) gives
    a float; an array or sequence of them gives a float64 ndarray. A naive datetime means UTC.
    NaT raises ValueError, except in an array when `allow_nat` is set (it becomes NaN, a gap).
    """
    if isinstance(value, (np.ndarray, list, tuple)) and np.ndim(value) > 0:
        return _array_seconds(value, allow_nat)
    return _scalar_seconds(value[()] if isinstance(value, np.ndarray) else value)


_TIME_INPUTS = (np.datetime64, date, str, QtCore.QDateTime)


def _is_time_input(value):
    return isinstance(value[()] if isinstance(value, np.ndarray) and value.ndim == 0 else value, _TIME_INPUTS)


def _accept_dates_in_range(init):
    @functools.wraps(init)
    def wrapper(self, *args, **kwargs):
        if len(args) not in (2, 3) or all(type(a) in (float, int) for a in args[:2]):
            return init(self, *args, **kwargs)
        if len(args) == 2:
            kwargs.setdefault("is_time_range", any(_is_time_input(a) for a in args))
        return init(self, *(to_epoch_seconds(a) for a in args[:2]), *args[2:], **kwargs)
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
    start = to_epoch_seconds(start, allow_nat=True)
    stop = start.copy() if stop is None else to_epoch_seconds(stop, allow_nat=True)
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
_edit_modes_get = SciQLopTimeline.edit_modes
_editable_get = SciQLopTimeline.editable
_lanes_get = SciQLopTimeline.lanes


# Timeline state is a member of an enum; the C++ side still takes these names. Each table maps
# member -> C++ name; `aliases` are further old values that still work, deprecated.
_STYLE_NAMES = {TimelineStyle.Wave: "wave", TimelineStyle.Bars: "bars"}
_STACK_NAMES = {TimelineStack.Off: "", TimelineStack.Time: "time", TimelineStack.Category: "category"}
_EDIT_MODE_NAMES = {TimelineEditMode.Move: "move", TimelineEditMode.Resize: "resize",
                    TimelineEditMode.ChangeLane: "change_lane", TimelineEditMode.Create: "create",
                    TimelineEditMode.Delete: "delete"}


def _member_of(enum, names, prop, value, aliases={}):
    if isinstance(value, enum):
        return value
    legacy = {name: member for member, name in names.items() if name} | aliases
    try:
        member = legacy[value]
    except (KeyError, TypeError):
        raise ValueError(f"{prop} must be a {enum.__name__} ({', '.join(m.name for m in enum)}), "
                         f"not {value!r}") from None
    _warn_deprecated(f"{prop}={value!r} is deprecated: use {enum.__name__}.{member.name}")
    return member


def _member_named(names, name):
    return {n: m for m, n in names.items()}[name]


def _set_edit_modes(self, modes):
    members = set()
    for m in modes:  # a loop, not a comprehension: on 3.11 that has its own frame (stacklevel)
        members.add(_member_of(TimelineEditMode, _EDIT_MODE_NAMES, "edit_modes", m))
    self.set_edit_modes(sorted(_EDIT_MODE_NAMES[m] for m in members))


def _get_snap_to(self):
    mode = self.snap_mode()
    if mode == "times":
        return list(self.snap_times())
    return {"edges": "edges", "step": self.snap_step()}.get(mode)


def _set_snap_to(self, value):
    if isinstance(value, (list, tuple, np.ndarray)):
        if len(value) == 0:
            raise ValueError("snap_to times must not be empty")
        self.set_snap_times([float(t) for t in to_epoch_seconds(value).ravel()])
    elif value is None:
        self.clear_snap()
    elif value == "edges":
        self.set_snap_edges()
    elif isinstance(value, (int, float)) and not isinstance(value, bool) and value > 0:
        self.set_snap_step(float(value))
    else:
        raise ValueError("snap_to must be 'edges', a positive number of seconds, "
                         "a non-empty list of times, or None")


SciQLopTimeline.edit_modes = property(
    lambda self: {_member_named(_EDIT_MODE_NAMES, n) for n in _edit_modes_get(self)},
    _set_edit_modes)
SciQLopTimeline.editable = property(_editable_get, SciQLopTimeline.set_editable)
SciQLopTimeline.snap_to = property(_get_snap_to, _set_snap_to)
SciQLopTimeline.lanes = property(_lanes_get, SciQLopTimeline.set_lanes)


_style_get = SciQLopTimeline.style
_stack_get = SciQLopTimeline.stack


def _set_style(self, value):
    self.set_style(_STYLE_NAMES[_member_of(TimelineStyle, _STYLE_NAMES, "style", value)])


def _set_stack(self, value):
    member = _member_of(TimelineStack, _STACK_NAMES, "stack", value, aliases={None: TimelineStack.Off})
    self.set_stack(_STACK_NAMES[member])


SciQLopTimeline.style = property(lambda self: _member_named(_STYLE_NAMES, _style_get(self)),
                                 _set_style)
SciQLopTimeline.stack = property(lambda self: _member_named(_STACK_NAMES, _stack_get(self)),
                                 _set_stack)
_forbid_overlap_get = SciQLopTimeline.forbid_overlap
_category_order_get = SciQLopTimeline.category_order


SciQLopTimeline.forbid_overlap = property(lambda self: bool(_forbid_overlap_get(self)),
                                          lambda self, v: self.set_forbid_overlap(bool(v)))
SciQLopTimeline.category_order = property(lambda self: list(_category_order_get(self)),
                                          lambda self, names: self.set_category_order(list(names)))
SciQLopTimeline.interval = lambda self, id: self.interval_info(int(id)) or None
SciQLopTimeline.interval_at = lambda self, x, y: None if (i := self._interval_at(x, y)) < 0 else i


_HISTOGRAM2D_NORMALIZATION_INTS = {0: Histogram2DNormalization.Count, 1: Histogram2DNormalization.PerColumn}


def _enum_normalization(method):
    # The old HistogramNormalization members are accepted by their numeric value: Count == 0 and
    # Density == 1 line up with Histogram2DNormalization.Count and .PerColumn.
    @_wraps(method)
    def wrapper(self, value):
        if not isinstance(value, Histogram2DNormalization):
            if isinstance(value, bool) or not isinstance(value, int) \
                    or value not in _HISTOGRAM2D_NORMALIZATION_INTS:
                raise ValueError("normalization must be Histogram2DNormalization.Count or "
                                 f"Histogram2DNormalization.PerColumn, not {value!r}")
            given = f"{type(value).__name__}.{value.name}" if isinstance(value, enum.Enum) else int(value)
            value = _HISTOGRAM2D_NORMALIZATION_INTS[value]
            _warn_deprecated(f"set_normalization({given}) is deprecated: use Histogram2DNormalization.{value.name}")
        return method(self, value)
    return wrapper


_set_normalization_member = SciQLopHistogram2D.set_normalization


# normalization_changed still carries an int: a pipeline's own value must not warn as deprecated.
@_wraps(_set_normalization_member)
def _set_normalization_from_signal(self, value):
    return _set_normalization_member(self, _HISTOGRAM2D_NORMALIZATION_INTS.get(value, value))


SciQLopHistogram2D._set_normalization_from_signal = _set_normalization_from_signal
register_property(SciQLopHistogram2D, "normalization", signal_name="normalization_changed",
                  getter_name="normalization", setter_name="_set_normalization_from_signal",
                  property_type="value")
# A plain method, never a Qt slot (a wrapper cuts connections).
SciQLopHistogram2D.set_normalization = _enum_normalization(SciQLopHistogram2D.set_normalization)




# snake_case names for the camelCase ones (API review 2026-10-20, 1.5 and 1.6). The same attribute
# under a second name, never a wrapper: a Python wrapper on a Qt slot breaks its connections.
# The camelCase names stay, deprecated.
# Review 1.12: one spelling for the time range on time-series plots and panels.
SciQLopTimeSeriesPlot.time_range_changed = SciQLopTimeSeriesPlot.time_axis_range_changed
SciQLopMultiPlotPanel.time_range = SciQLopMultiPlotPanel.time_axis_range
SciQLopMultiPlotPanel.set_time_range = SciQLopMultiPlotPanel.set_time_axis_range

_SNAKE_CASE_ALIASES = {
    SciQLopMultiPlotPanel: {"set_selected": "setSelected", "add_widget": "addWidget",
                            "remove_widget": "removeWidget", "index_of": "indexOf",
                            "selection_changed": "selectionChanged"},
    SciQLopPlotsBindings.SciQLopMultiPlotObject: {"add_object": "addObject",
                                                  "remove_object": "removeObject",
                                                  "replot_all": "replotAll",
                                                  "update_plot_list": "updatePlotList"},
    SciQLopPlotsBindings.PlotsModel: {"add_node": "addNode"},
    SciQLopPlotsBindings.PlotsModelNode: {"set_name": "setName", "name_changed": "nameChanged"},
}
for _cls, _aliases in _SNAKE_CASE_ALIASES.items():
    for _new, _old in _aliases.items():
        setattr(_cls, _new, getattr(_cls, _old))

# Old names kept as aliases of the native ones (docs/plans/2026-10-22-drawn-object-base-design.md),
# deprecated. The same attribute under a second name, never a wrapper (see above); a warning on
# connect would need a proxy object.
_DEPRECATED_ALIASES = {
    SciQLopPlotsBindings.SciQLopSpan: {"selectionChanged": "selection_changed"},
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
    own = {"tracing", "WidgetPlot", "to_epoch_seconds",
           "Pipeline", "PartialPipeline", "Event", "register_property", "OnDescriptor"}
    return sorted(bound | own)


__all__ = _public_names()
