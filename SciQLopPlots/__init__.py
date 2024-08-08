from PySide6 import QtCore, QtGui, QtWidgets, QtOpenGL, QtPrintSupport, QtSvg
from .SciQLopPlotsBindings import *
from .SciQLopPlotsBindings import _QCustomPlot as QCustomPlot

__version__ = '0.8.1'


# def _patch_sciqlop_plot(cls):

#     class CallableWrapper(SimpleCallable):
#         def __init__(self, callable):
#             super().__init__()
#             self.callable = callable

#         def get_data(self, *args, **kwargs):
#             return self.callable(*args, **kwargs)

#     def plot(*args, **kwargs):
#         if callable(args[1]):
#             w = CallableWrapper(args[1])
#             return cls._plot(args[0], w, **kwargs)
#         else:
#             return cls._plot(*args, **kwargs)

#     cls._plot = cls.plot
#     cls.plot = plot
#     return cls


# SciQLopPlot = _patch_sciqlop_plot(SciQLopPlot)
# SciQLopTimeSeriesPlot = _patch_sciqlop_plot(SciQLopTimeSeriesPlot)
