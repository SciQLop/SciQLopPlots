"""Parametric butterfly curve.

Showcases:
- GraphType.ParametricCurve for non-time-series data
- Decorative items: EllipseItem
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants, QPen, QBrush
from PySide6.QtCore import Qt, QRectF

from SciQLopPlots import (
    SciQLopPlot, GraphType, SciQLopEllipseItem, Coordinates,
)


def butterfly_curve(n=5000):
    t = np.linspace(0, 12 * np.pi, n)
    x = np.sin(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    y = np.cos(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    return x, y


app = QApplication(sys.argv)

plot = SciQLopPlot()
plot.setWindowTitle("Parametric Curve Demo")

x, y = butterfly_curve()
plot.x_axis().set_range(min(x), max(x))
plot.y_axis().set_range(min(y), max(y))

plot.plot(
    x, y,
    labels=["butterfly"],
    colors=[QColorConstants.DarkMagenta],
    graph_type=GraphType.ParametricCurve,
)

# Decorative item at center
SciQLopEllipseItem(
    plot, QRectF(-0.5, -0.5, 1, 1),
    QPen(Qt.red), QBrush(QColorConstants.Transparent),
    True, Coordinates.Data,
)

plot.show()
plot.resize(600, 600)
sys.exit(app.exec())
