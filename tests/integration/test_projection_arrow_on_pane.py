"""Spike for SciQLop #142: can a vector be drawn as an arrow on a projection pane?

A layer would draw `Arrow(origin, vector)` as a `SciQLopCurvedLineItem` with an
arrow terminator on the pane. Two things must hold: the item works on a pane of a
projection plot (a pane is a plain SciQLopPlot inside the projection widget, not
the widget the item was written against), and the head is sized in pixels, so a
zoom changes the shaft but never squashes or blows up the head.
"""
from collections import Counter

import pytest
from PySide6.QtCore import QPointF
from PySide6.QtGui import QImage

from SciQLopPlots import (
    Coordinates,
    LineTermination,
    SciQLopCurvedLineItem,
    SciQLopNDProjectionPlot,
    SciQLopPlotRange,
)
from conftest import process_events


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


@pytest.fixture
def pane(proj):
    return proj.subplot(0)


def _ink(pane, tmp_path, name, half_range):
    pane.x_axis().set_range(SciQLopPlotRange(-half_range, half_range))
    pane.y_axis().set_range(SciQLopPlotRange(-half_range, half_range))
    process_events()
    path = tmp_path / f"{name}.png"
    assert pane.save_png(str(path), 400, 400) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    pixels = Counter(img.pixelColor(x, y).rgb()
                     for y in range(img.height()) for x in range(img.width()))
    (_bg, bg_count), = pixels.most_common(1)
    return sum(pixels.values()) - bg_count


def _vector(pane, stop_termination):
    return SciQLopCurvedLineItem(
        pane, QPointF(-5.0, -5.0), QPointF(5.0, 5.0),
        LineTermination.NoneTermination, stop_termination, Coordinates.Data)


def _head_ink(pane, tmp_path, half_range):
    plain = _vector(pane, LineTermination.NoneTermination)
    without = _ink(pane, tmp_path, f"plain_{half_range}", half_range)
    del plain
    with_head = _vector(pane, LineTermination.Arrow)
    ink = _ink(pane, tmp_path, f"arrow_{half_range}", half_range)
    del with_head
    return ink - without


def test_an_arrow_renders_on_a_projection_pane(pane, tmp_path):
    assert _head_ink(pane, tmp_path, 10.0) > 20


def test_the_head_keeps_its_pixel_size_when_zooming(pane, tmp_path):
    close = _head_ink(pane, tmp_path, 10.0)
    far = _head_ink(pane, tmp_path, 40.0)
    assert close > 0 and far > 0
    assert 0.6 < far / close < 1.7
