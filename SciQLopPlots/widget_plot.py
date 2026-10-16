"""Any QWidget as a row of a SciQLopMultiPlotPanel: a matplotlib canvas, a Qt Graphs view,
a Qt 3D window, ... It follows the panel's time range."""
from PySide6.QtCore import QSize
from PySide6.QtWidgets import QVBoxLayout

from .SciQLopPlotsBindings import SciQLopPlotDummyAxis, SciQLopPlotInterface


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

    def sizeHint(self):
        # The panel shares its height out by size hints. A plot's, not the widget's: a
        # QQuickWidget or a window container has none and would get no room, a matplotlib
        # canvas a large one and would crowd the plots out.
        return QSize(50, 50)

    @property
    def widget(self):
        return self._widget

    def on_time_range(self, start, stop):
        if self._on_time_range is not None:
            self._on_time_range(start, stop)

    # What the panel calls on its plots.
    def time_axis(self):
        return self._axis

    def x_axis(self):
        return self._axis

    def color_palette(self):
        return self._palette

    def set_color_palette(self, palette):
        self._palette = list(palette)
