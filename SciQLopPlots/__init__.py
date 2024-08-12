from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from .SciQLopPlotsBindings import GraphType, SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel
from .SciQLopPlotsBindings import *


__version__ = '0.8.1'


def _patch_sciqlop_plot(cls):

    def plot(self, *args, name=None, labels=None, colors=None, graph_type=None, **kwargs):
        graph_type = graph_type or GraphType.Line
        if len(args) == 1:
            if graph_type == GraphType.ParametricCurve:
                return cls.parametric_curve(self, *args,labels=labels,colors=colors, **kwargs)
            elif graph_type == GraphType.Line:
                return cls.line(self, *args,labels=labels,colors=colors, **kwargs)
            elif graph_type == GraphType.Colormap:
                return cls.colormap(self, *args,name=name,**kwargs)
        elif len(args) == 2:
            if graph_type == GraphType.ParametricCurve:
                return cls.parametric_curve(self, *args,labels=labels,colors=colors, **kwargs)
            elif graph_type == GraphType.Line:
                return cls.line(self, *args,labels=labels,colors=colors, **kwargs)
        elif len(args) == 3:
            return cls.colormap(self, *args,name=name,**kwargs)
        else:
            print(f"only 1, 2 or 3 arguments are supported, got {len(args)}")
            return

    cls.plot = plot
    return cls


SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
SciQLopMultiPlotPanel = _patch_sciqlop_plot(SciQLopMultiPlotPanel)
