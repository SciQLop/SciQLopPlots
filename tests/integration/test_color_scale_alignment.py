"""The colour bar spans the same height as the data area (SciQLop screenshot).

The bar ignored the plot's bottom margin: it ran down past the x axis, alongside
the tick labels, and the last time label slid under it.
"""
import numpy as np
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot
from conftest import process_events


def _coloured_rows(img, x):
    return [y for y in range(img.height()) if img.pixelColor(x, y).saturation() > 100]


def test_colour_bar_ends_where_the_data_area_ends(qtbot, tmp_path):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    cmap = plot.plot(np.arange(100.0), np.arange(16.0), np.random.rand(100, 16), name="spec")
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    process_events()
    path = str(tmp_path / "bar.png")
    assert plot.save_png(path, 600, 300)
    img = QImage(path)

    data_rows = _coloured_rows(img, 250)
    bar_x = next(x for x in range(img.width() - 1, 0, -1) if _coloured_rows(img, x))
    bar_rows = _coloured_rows(img, bar_x - 3)
    assert abs(max(bar_rows) - max(data_rows)) <= 2, (max(bar_rows), max(data_rows))
    assert abs(min(bar_rows) - min(data_rows)) <= 2, (min(bar_rows), min(data_rows))
