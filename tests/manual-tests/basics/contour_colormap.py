"""Contour overlay on colormaps.

Showcases:
- Explicit contour levels with custom pen color
- Auto contour levels with labels
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColor
from SciQLopPlots import SciQLopMultiPlotPanel


def wave_field(nx=200, ny=150):
    x = np.linspace(-4, 4, nx)
    y = np.linspace(-4, 4, ny)
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.sin(xx) * np.cos(yy) + 0.3 * np.sin(2 * xx + yy)
    return x, y, z


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=True)
panel.setWindowTitle("Contour Overlay Demo")

x, y, z = wave_field()

# Explicit contour levels, white lines
_, cmap1 = panel.plot(x, y, z)
cmap1.set_contour_levels([-0.8, -0.4, 0.0, 0.4, 0.8])
cmap1.set_contour_color(QColor("white"))
cmap1.set_contour_width(1.5)

# Auto contour levels with labels
_, cmap2 = panel.plot(x, y, z)
cmap2.set_auto_contour_levels(10)
cmap2.set_contour_color(QColor("black"))
cmap2.set_contour_labels_enabled(True)

panel.show()
panel.resize(800, 700)
sys.exit(app.exec())
