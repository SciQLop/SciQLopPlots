"""API review 2.12: 2-D histograms get the colour map's contour API (it lives on the colour-map base)."""
import numpy as np
from PySide6.QtGui import QColor, QPen

from conftest import record

RNG = np.random.default_rng(0)


def _histogram(plot):
    return plot.histogram2d(RNG.normal(size=2000), RNG.normal(size=2000))


def test_levels_and_auto_levels(plot):
    hist = _histogram(plot)
    seen = []
    hist.contour_levels_changed.connect(lambda: seen.append(True))
    hist.set_contour_levels([2.0, 5.0])
    assert list(hist.contour_levels()) == [2.0, 5.0]
    hist.set_auto_contour_levels(4)
    assert (hist.auto_contour_levels(), list(hist.contour_levels())) == (4, [])
    assert len(seen) == 2


def test_pen_colour_width_and_labels(plot):
    hist = _histogram(plot)
    pens = record(hist.contour_pen_changed)
    labels = record(hist.contour_labels_enabled_changed)
    hist.set_contour_pen(QPen(QColor("red"), 2.0))
    hist.set_contour_color(QColor("blue"))
    hist.set_contour_width(3.0)
    hist.set_contour_labels_enabled(True)
    assert (hist.contour_color(), hist.contour_width(), hist.contour_labels_enabled()) == (
        QColor("blue"), 3.0, True)
    assert len(pens) == 3 and labels == [True]


def test_colour_maps_keep_their_contours(plot):
    x = np.linspace(0.0, 1.0, 10)
    cmap = plot.colormap(x, x, np.outer(x, x))
    cmap.set_contour_levels([0.5])
    cmap.set_contour_labels_enabled(True)
    assert (list(cmap.contour_levels()), cmap.contour_labels_enabled()) == ([0.5], True)
