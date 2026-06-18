from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from . import SciQLopPlotsBindings
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *
from datetime import datetime, timezone
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
      * our ``<primitive-type>`` declarations in the type map. These are not
        wrapped classes, so without an entry they resolve to a bare ``str``
        and shiboken's argument matcher hard-aborts the interpreter
        (``the_type.__module__`` on a str). PyObject-backed buffer → object,
        the data callable → Callable.

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
        mapping.type_map["SciQLopPyBuffer"] = object
        mapping.type_map["GetDataPyCallable"] = collections.abc.Callable
        # Scoped to the parser that emits it so we never hide a same-named
        # RuntimeWarning from unrelated code.
        warnings.filterwarnings(
            "ignore", message="pyside_type_init", category=RuntimeWarning,
            module=r"shibokensupport\.signature")
    except Exception:
        pass


_register_with_shiboken_signatures()

from . import tracing  # noqa: E402,F401  -- runtime tracer facade

__version__ = '0.28.1'

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
                wf = cls.waterfall(self, *args, **kwargs)
                return _apply_waterfall_kwargs(wf, **wf_kwargs)
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

