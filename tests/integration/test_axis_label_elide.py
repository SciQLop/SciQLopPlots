"""An axis label longer than its axis is elided, not cut at both ends (SciQLop).

A long y label in a short stacked plot ran past the axis at both ends, into the
neighbouring plots' label areas.
"""
import numpy as np
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot

LONG = "Magnetic field vector in GSE plus Btotal (8 or 16 S/s) [nT (all flags)] " * 3


def _ink_rows(path, columns):
    img = QImage(path)
    return [y for y in range(img.height())
            if any(img.pixelColor(x, y).lightness() < 128 for x in columns)]


def test_long_y_label_stays_within_its_axis(qtbot, tmp_path):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    plot.plot(np.arange(10.0), np.arange(10.0), labels=["v"])
    plot.y_axis().set_label(LONG)
    path = str(tmp_path / "label.png")
    assert plot.save_png(path, 600, 150)

    rows = _ink_rows(path, range(0, 20))   # the rotated label's column, left of the ticks
    assert rows, "label not drawn"
    img_height = QImage(path).height()
    assert min(rows) > 2 and max(rows) < img_height - 3, (min(rows), max(rows), img_height)
    assert plot.y_axis().label() == LONG


def test_long_colour_scale_label_stays_within_its_bar(qtbot, tmp_path):
    """Same on the colour scale axis: a short spectrogram's z label from CDF metadata."""
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    x, y = np.arange(100.0), np.logspace(1, 4, 16)
    plot.plot(x, y, np.random.rand(100, 16), name="spec")
    plot.z_axis().set_label("DEF_omni [keV/(cm^2 s sr keV)] " * 3)
    path = str(tmp_path / "zlabel.png")
    assert plot.save_png(path, 600, 150)

    width = QImage(path).width()
    rows = _ink_rows(path, range(width - 20, width))   # the rotated z label, at the right edge
    assert rows, "label not drawn"
    img_height = QImage(path).height()
    assert min(rows) > 2 and max(rows) < img_height - 3, (min(rows), max(rows), img_height)
