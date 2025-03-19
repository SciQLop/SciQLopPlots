from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from . import SciQLopPlotsBindings
from .SciQLopPlotsBindings import (GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
                                    SciQLopGraphInterface, AxisType, SciQLopPlotRange, SciQLopNDProjectionPlot)
from .SciQLopPlotsBindings import *
from datetime import datetime, timezone
from dateutil.parser import parse as dateparse

import sys

sys.modules["SciQLopPlotsBindings"] = SciQLopPlotsBindings

__version__ = '0.12.0'

def _merge_kwargs(kwargs, **kwargs2):
    for k, v in kwargs2.items():
        if k not in kwargs and v is not None:
            kwargs[k] = v
    return kwargs

def _patch_sciqlop_plot(cls):
    def plot_func(self, callback, graph_type=None, **kwargs):
        kwargs = {k: v for k, v in kwargs.items() if v is not None}
        res = None
        if graph_type == GraphType.ParametricCurve:
            res = cls.parametric_curve(self, callback, **kwargs)
        if graph_type == GraphType.Line:
            res = cls.line(self, callback, **kwargs)
        if graph_type == GraphType.ColorMap:
            res = cls.colormap(self, callback, **kwargs)
        return res

    def plot(self, *args, name=None, labels=None, colors=None, graph_type=None, **kwargs):
        graph_type = graph_type or GraphType.Line
        kwargs = _merge_kwargs(kwargs, name=name, labels=labels, colors=colors)
        if graph_type == GraphType.ParametricCurve and len(args) in (1, 2, 4):
            return cls.parametric_curve(self, *args, **kwargs)
        if len(args) == 1:
            return plot_func(self, *args, graph_type=graph_type, **kwargs)
        if len(args) == 2:
            if graph_type == GraphType.Line:
                return cls.line(self, *args, **kwargs)
        if len(args) == 3:
            return cls.colormap(self, *args, **kwargs)

        print(f"only 1, 2 or 3 arguments are supported, got {len(args)}")
        return None

    cls.plot = plot
    return cls



SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
SciQLopMultiPlotPanel = _patch_sciqlop_plot(SciQLopMultiPlotPanel)
SciQLopNDProjectionPlot = _patch_sciqlop_plot(SciQLopNDProjectionPlot)

