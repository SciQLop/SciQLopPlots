"""A projection plot applies the marker and colours it is given, like every other plot."""
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import GraphMarkerShape, GraphType, SciQLopNDProjectionPlot

T = np.linspace(0, 1, 20)
COLORS = [QColor("red"), QColor("green"), QColor("blue")]


def _projection(qtbot, data):
    nd = SciQLopNDProjectionPlot()
    qtbot.addWidget(nd)
    return nd, nd.plot(data, labels=["a", "b", "c"], colors=COLORS,
                   graph_type=GraphType.ParametricCurve, marker=GraphMarkerShape.Circle)


def test_arrays_get_the_marker(qtbot):
    _nd, curves = _projection(qtbot, [T, T, 2 * T, 3 * T])
    assert curves.components()
    assert all(c.marker_shape() == GraphMarkerShape.Circle for c in curves.components())


def test_arrays_get_the_colours(qtbot):
    _nd, curves = _projection(qtbot, [T, T, 2 * T, 3 * T])
    assert curves.colors()[:3] == COLORS


def test_a_callback_gets_the_marker(qtbot):
    _nd, curves = _projection(qtbot, lambda start, stop: [T, T, 2 * T, 3 * T])
    assert curves.components()
    assert all(c.marker_shape() == GraphMarkerShape.Circle for c in curves.components())
