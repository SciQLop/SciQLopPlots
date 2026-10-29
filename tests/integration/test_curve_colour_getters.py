"""API review 2.5: the curve's two colourings (by time, by value) both read back."""
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import GraphType

X = np.linspace(0.0, 10.0, 50)


def _curve(plot):
    return plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve)


def test_time_values_and_gradient_read_back(plot):
    curve = _curve(plot)
    assert curve.time_color_gradient() == [QColor(0, 0, 255), QColor(255, 0, 0)]
    curve.set_time_values(X)
    curve.set_time_color_gradient(QColor("green"), QColor("black"))
    assert list(curve.time_values()) == list(X)
    assert curve.time_color_gradient() == [QColor("green"), QColor("black")]


def test_color_values_read_back(plot):
    curve = _curve(plot)
    assert list(curve.color_values()) == []
    curve.set_color_values(np.cos(X))
    assert list(curve.color_values()) == list(np.cos(X))
