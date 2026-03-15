"""Histogram 2D: scatter data binned into a density colormap.

Showcases:
- SciQLopHistogram2D for 2D density estimation
- Binning configuration (key_bins, value_bins)
- Bimodal gaussian scatter data
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot


def bimodal_scatter(n=100_000):
    rng = np.random.default_rng(42)
    x1 = rng.normal(-2, 1.0, n // 2)
    y1 = rng.normal(1, 0.6, n // 2)
    x2 = rng.normal(2, 0.8, n // 2)
    y2 = rng.normal(-1, 1.2, n // 2)
    return np.concatenate([x1, x2]), np.concatenate([y1, y2])


app = QApplication(sys.argv)

plot = SciQLopPlot()
plot.setWindowTitle("Histogram 2D Demo")

x, y = bimodal_scatter(100_000)

hist = plot.add_histogram2d("Density", 80, 80)
hist.set_data(x, y)

plot.x_axis().set_range(float(x.min()), float(x.max()))
plot.y_axis().set_range(float(y.min()), float(y.max()))

plot.show()
plot.resize(800, 600)
sys.exit(app.exec())
