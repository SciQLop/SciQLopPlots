from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from . import SciQLopPlotsBindings
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *
from datetime import datetime, timezone
from dateutil.parser import parse as dateparse
import traceback

import sys

sys.modules["SciQLopPlotsBindings"] = SciQLopPlotsBindings

__version__ = '0.21.0'

def _merge_kwargs(kwargs, **kwargs2):
    for k, v in kwargs2.items():
        if k not in kwargs and v is not None:
            kwargs[k] = v
    return kwargs

def _apply_waterfall_kwargs(wf, offsets=None, normalize=True, gain=1.0):
    import numpy as np
    from .SciQLopPlotsBindings import WaterfallOffsetMode

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
    return wf


def _patch_sciqlop_plot(cls):
    def plot_func(self, callback, graph_type=None,
                  offsets=None, normalize=True, gain=1.0, **kwargs):
        kwargs = {k: v for k, v in kwargs.items() if v is not None}
        if graph_type == GraphType.ParametricCurve:
            return cls.parametric_curve(self, callback, **kwargs)
        elif graph_type == GraphType.Line:
            return cls.line(self, callback, **kwargs)
        elif graph_type == GraphType.Scatter:
            return cls.scatter(self, callback, **kwargs)
        elif graph_type == GraphType.ColorMap:
            return cls.colormap(self, callback, **kwargs)
        elif graph_type == GraphType.Waterfall:
            wf = cls.waterfall(self, callback, **kwargs)
            return _apply_waterfall_kwargs(wf, offsets=offsets,
                                           normalize=normalize, gain=gain)
        raise ValueError(f"unsupported graph_type {graph_type!r} for single-arg plot()")

    def plot(self, *args, name=None, labels=None, colors=None, graph_type=None,
             offsets=None, normalize=True, gain=1.0, **kwargs):
        graph_type = graph_type or GraphType.Line
        kwargs = _merge_kwargs(kwargs, name=name, labels=labels, colors=colors)
        if (graph_type == GraphType.ParametricCurve) and (len(args) in (1, 2, 4)) and not callable(args[0]):
            return cls.parametric_curve(self, *args, **kwargs)
        if len(args) == 1:
            return plot_func(self, *args, graph_type=graph_type,
                             offsets=offsets, normalize=normalize, gain=gain, **kwargs)
        if len(args) == 2:
            if graph_type == GraphType.Line:
                return cls.line(self, *args, **kwargs)
            if graph_type == GraphType.Scatter:
                return cls.scatter(self, *args, **kwargs)
            if graph_type == GraphType.ColorMap:
                return cls.colormap(self, *args, **kwargs)
            if graph_type == GraphType.Waterfall:
                wf = cls.waterfall(self, *args, **kwargs)
                return _apply_waterfall_kwargs(wf, offsets=offsets,
                                               normalize=normalize, gain=gain)
            raise ValueError(f"unsupported graph_type {graph_type!r} for 2-arg plot()")
        if len(args) == 3:
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

