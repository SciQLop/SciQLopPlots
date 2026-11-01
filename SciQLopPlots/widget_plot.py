"""Any QWidget as a row of a SciQLopMultiPlotPanel: a matplotlib canvas, a Qt Graphs view,
a Qt 3D window, ... It follows the panel's time range."""
import functools

import shiboken6
from PySide6.QtCore import QSize
from PySide6.QtWidgets import QVBoxLayout

from .SciQLopPlotsBindings import SciQLopPlotDummyAxis, SciQLopPlotInterface


def _alive(method):
    """Raise like a bound method on a deleted object: these methods are Python, shiboken doesn't check them."""
    @functools.wraps(method)
    def checked(self, *args, **kwargs):
        if not shiboken6.isValid(self):
            raise RuntimeError(f"Internal C++ object ({type(self).__name__}) already deleted.")
        return method(self, *args, **kwargs)
    return checked


class WidgetPlot(SciQLopPlotInterface):
    """A panel row holding ``widget``.

    The panel's time range (pans, zooms, ``set_time_axis_range``) reaches
    ``on_time_range(start, stop)``, in seconds since 1970: pass a callable, or override
    the method in a subclass. The panel's own plots still draw with SciQLopPlots; this row
    only hosts the widget, so crosshairs, margin alignment and themes skip it.
    """

    def __init__(self, widget, on_time_range=None, parent=None):
        super().__init__(parent)
        QVBoxLayout(self).addWidget(widget)
        self.layout().setContentsMargins(0, 0, 0, 0)
        self._widget = widget
        self._on_time_range = on_time_range
        self._palette = []
        # The panel sets its time range on this axis, and reads it back.
        self._axis = SciQLopPlotDummyAxis(self)
        self._axis.range_changed.connect(lambda r: self.on_time_range(r.start(), r.stop()))

    @_alive
    def sizeHint(self):
        # The panel shares its height out by size hints. A plot's, not the widget's: a
        # QQuickWidget or a window container has none and would get no room, a matplotlib
        # canvas a large one and would crowd the plots out.
        return QSize(50, 50)

    @property
    @_alive
    def widget(self):
        return self._widget

    @_alive
    def on_time_range(self, start, stop):
        if self._on_time_range is not None:
            self._on_time_range(start, stop)

    # What the panel calls on its plots.
    @_alive
    def time_axis(self):
        return self._axis

    @_alive
    def x_axis(self):
        return self._axis

    @_alive
    def color_palette(self):
        return self._palette

    @_alive
    def set_color_palette(self, palette):
        self._palette = list(palette)
