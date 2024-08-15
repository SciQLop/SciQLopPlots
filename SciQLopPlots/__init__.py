from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from .SciQLopPlotsBindings import GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel, SciQLopGraphInterface, AxisType
from .SciQLopPlotsBindings import *


__version__ = '0.9.0'


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
        if len(args) == 1:
            return plot_func(self, *args, name=name, labels=labels,
                             colors=colors, graph_type=graph_type,
                             **kwargs)
        if len(args) == 2:
            if graph_type == GraphType.ParametricCurve:
                return cls.parametric_curve(self, *args, labels=labels, colors=colors, **kwargs)
            if graph_type == GraphType.Line:
                return cls.line(self, *args, labels=labels, colors=colors, **kwargs)
        if len(args) == 3:
            return cls.colormap(self, *args, name=name, **kwargs)
        print(f"only 1, 2 or 3 arguments are supported, got {len(args)}")
        return None

    cls.plot = plot
    return cls


SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
SciQLopMultiPlotPanel = _patch_sciqlop_plot(SciQLopMultiPlotPanel)
