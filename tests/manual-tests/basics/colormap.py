"""Colormap examples: uniform grid at different resolutions.

Showcases:
- panel.plot(x, y, z) for colormaps
- Multiple colormaps stacked in a panel
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopMultiPlotPanel


def uniform_colormap(nx=200, ny=200):
    x = np.arange(nx) * 8.0 / nx - 4.0
    y = np.arange(ny) * 8.0 / ny - 4.0
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.cos(np.sqrt((xx + 2) ** 2 + yy ** 2))
    return x, y, z


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=True)
panel.setWindowTitle("Colormap Demo")

# Uniform grid
panel.plot(*uniform_colormap(200, 200))

# Higher resolution
panel.plot(*uniform_colormap(500, 300))

panel.show()
panel.resize(800, 600)
sys.exit(app.exec())
