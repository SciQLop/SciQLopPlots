"""Coordinates::Pixels must be measured from the plot area, not the widget.

Regression: every Pixels-mode item mapped to QCPItemPosition::ptAbsolute, whose
origin is the top-left of the whole QCustomPlot widget -- including the axis
label / tick margins. Since QCPAbstractItem::clipToAxisRect() defaults to true,
a small pixel coordinate landed in the margin and was clipped away, so e.g.
Text(plot, "label", (10, 10), Pixels) could render nothing at all.

Pixels now maps to ptAxisRectAbsolute: pixels from the axis rect's top-left,
following the plot area across resizes and margin changes.

The plot area's geometry is not exposed to Python, so each test paints a
Data-mode probe line at the x-axis minimum -- which by definition lands on the
axis rect's left edge -- in a second colour, and measures the Pixels-mode item
relative to it.
"""
import numpy as np
import pytest
from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QImage, QPixmap

from SciQLopPlots import (
    Coordinates,
    LineTermination,
    SciQLopCurvedLineItem,
    SciQLopEllipseItem,
    SciQLopPixmapItem,
    SciQLopPlotRange,
    SciQLopStraightLine,
    SciQLopTextItem,
    SciQLopVerticalSpan,
)

# Nothing else in a default plot paints these.
SUBJECT = QColor(255, 0, 255)  # magenta - the Pixels-mode item under test
PROBE = QColor(0, 255, 255)  # cyan    - marks the axis rect's left edge

EXPORT_W, EXPORT_H = 800, 600

# Far enough inside the plot area that nothing is clipped under *either*
# interpretation, so the measurement reflects the anchor and not the clip.
OFFSET = 120.0


def _subject_columns(plot, tmp_path, name):
    """Columns painted with SUBJECT, plus the probe mask, from a rendered export."""
    path = str(tmp_path / f"{name}.png")
    assert plot.save_png(path, EXPORT_W, EXPORT_H) is True
    img = QImage(path).convertToFormat(QImage.Format_RGB32)
    assert not img.isNull()
    h, w = img.height(), img.width()
    buf = np.frombuffer(img.constBits(), dtype=np.uint8)
    arr = buf.reshape(h, img.bytesPerLine() // 4, 4)[:, :w, :]
    # Format_RGB32 is 0xffRRGGBB -> little-endian byte order is B, G, R, A.
    b, g, r = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    subject_cols = np.flatnonzero(((r == 255) & (g == 0) & (b == 255)).any(axis=0))
    probe_cols = np.flatnonzero(((r == 0) & (g == 255) & (b == 255)).any(axis=0))
    return subject_cols, probe_cols


def _subject_centre(plot, tmp_path, name):
    subject_cols, _ = _subject_columns(plot, tmp_path, name)
    assert subject_cols.size > 0, "the item under test rendered nothing"
    return (subject_cols.min() + subject_cols.max()) / 2.0


def _measure(plot, tmp_path, name):
    """Return (subject centre column, plot-area left edge) from a rendered export."""
    subject_cols, probe_cols = _subject_columns(plot, tmp_path, name)
    assert probe_cols.size > 0, "probe line did not render; cannot locate the plot area"
    assert subject_cols.size > 0, "the item under test rendered nothing"
    return (subject_cols.min() + subject_cols.max()) / 2.0, float(probe_cols.min())


def _add_probe(plot):
    """A Data-mode line at the x-axis minimum renders on the plot area's left edge.

    Needs a few pixels of width: the line is centred on the edge, so half of it
    is clipped, and a hairline is antialiased into non-pure colours.
    """
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 100.0))
    probe = SciQLopStraightLine(plot, 0.0, False, Coordinates.Data, Qt.Orientation.Vertical)
    probe.set_color(PROBE)
    probe.set_line_width(5.0)
    return probe


def _text(plot, x):
    item = SciQLopTextItem(plot, "MMMM", QPointF(x, 60.0), False, Coordinates.Pixels)
    item.set_font_size(18)
    item.set_color(SUBJECT)
    return item


def _ellipse(plot, x):
    return SciQLopEllipseItem(
        plot, QRectF(x, 60.0, 40, 40), SUBJECT, 1.0, SUBJECT, False, Coordinates.Pixels)


def _curved_line(plot, x):
    item = SciQLopCurvedLineItem(
        plot, QPointF(x, 60.0), QPointF(x + 60, 60.0),
        LineTermination.NoneTermination, LineTermination.NoneTermination,
        Coordinates.Pixels)
    item.set_color(SUBJECT)
    item.set_line_width(3.0)
    return item


def _pixmap(plot, x):
    pixmap = QPixmap(40, 40)
    pixmap.fill(SUBJECT)
    return SciQLopPixmapItem(plot, pixmap, QRectF(x, 60.0, 40, 40), False, Coordinates.Pixels)


def _straight_line(plot, x):
    item = SciQLopStraightLine(plot, x, False, Coordinates.Pixels, Qt.Orientation.Vertical)
    item.set_color(SUBJECT)
    item.set_line_width(3.0)
    return item


def _vertical_span(plot, x):
    return SciQLopVerticalSpan(
        plot, SciQLopPlotRange(x, x + 40), SUBJECT, False, True, "", Coordinates.Pixels)


# (id, factory, centre of the painted shape relative to the item's pixel origin)
ITEMS = [
    ("text", _text, 0.0),  # QCPItemText is centred on its position
    ("ellipse", _ellipse, 20.0),
    ("curved_line", _curved_line, 30.0),
    ("pixmap", _pixmap, 20.0),
    ("straight_line", _straight_line, 0.0),
    ("vertical_span", _vertical_span, 20.0),
]


@pytest.mark.parametrize("name,factory,self_centre", ITEMS, ids=[i[0] for i in ITEMS])
class TestPixelItemsAnchorToAxisRect:

    def test_offset_is_measured_from_the_plot_area(self, plot, tmp_path, name, factory,
                                                   self_centre):
        probe = _add_probe(plot)  # noqa: F841 - keep alive until rendered
        item = factory(plot, OFFSET)  # noqa: F841
        centre, area_left = _measure(plot, tmp_path, name)
        # Anchored to the widget instead, the item lands one left margin short.
        assert centre == pytest.approx(area_left + OFFSET + self_centre, abs=4.0), (
            f"{name} centre is at x={centre}; the plot area starts at x={area_left}, "
            f"so a Pixels offset of {OFFSET} should place it at "
            f"{area_left + OFFSET + self_centre}"
        )

    def test_item_follows_the_plot_area_when_margins_grow(self, plot, tmp_path, name, factory,
                                                          self_centre):
        probe = _add_probe(plot)  # noqa: F841 - keep alive until rendered
        item = factory(plot, OFFSET)  # noqa: F841
        centre_before, left_before = _measure(plot, tmp_path, f"{name}_narrow")

        # A y-axis label widens the left margin, moving the plot area right.
        plot.y_axis().set_label("A fairly long y axis label")
        centre_after, left_after = _measure(plot, tmp_path, f"{name}_wide")

        assert left_after > left_before + 10, "the left margin did not actually grow"
        assert centre_after - centre_before == pytest.approx(
            left_after - left_before, abs=4.0), (
            f"{name} did not follow the plot area: it moved "
            f"{centre_after - centre_before}px while the plot area moved "
            f"{left_after - left_before}px"
        )


class TestDataCoordinatesUnaffected:
    """Guards against a fix that makes every coordinate mode axis-rect-relative."""

    def test_data_mode_item_still_tracks_the_axis(self, plot, tmp_path):
        # No probe here: it sits at the x-axis minimum and would pan out of view.
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 100.0))
        line = SciQLopStraightLine(plot, 50.0, False, Coordinates.Data, Qt.Orientation.Vertical)
        line.set_color(SUBJECT)
        line.set_line_width(3.0)
        centre_before = _subject_centre(plot, tmp_path, "data_centre")

        # Panning must move a Data-mode item; a Pixels-mode one would not.
        plot.x_axis().set_range(SciQLopPlotRange(40.0, 140.0))
        centre_after = _subject_centre(plot, tmp_path, "data_panned")
        assert centre_after < centre_before - 10
