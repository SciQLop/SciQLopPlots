"""Overlay items are owned by the Python object that created them.

Dropping the last Python reference must take the drawing off the plot -- these
items are created from Python, so Python owns them.

Regression: only SciQLopStraightLine and the spans honoured that. Text, Ellipse,
CurvedLine and Pixmap had `= default` (or no) destructors, so collecting the
wrapper left the underlying QCP item painted on the plot forever, unreachable
from Python -- a leak with no way to undo it:

    item = SciQLopTextItem(plot, "hello", QPointF(10, 10))
    del item          # wrapper gone, text still on screen, nothing can remove it

All item wrappers now detach their QCP item in their destructor, so `del`,
`deleteLater()` and going out of scope all remove the item.
"""
import gc

import numpy as np
import pytest
from PySide6.QtCore import QCoreApplication, QEvent, QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QImage, QPixmap
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (
    Coordinates,
    LineTermination,
    SciQLopCurvedLineItem,
    SciQLopEllipseItem,
    SciQLopHorizontalLine,
    SciQLopHorizontalSpan,
    SciQLopPixmapItem,
    SciQLopPlotRange,
    SciQLopRectangularSpan,
    SciQLopStraightLine,
    SciQLopTextItem,
    SciQLopVerticalLine,
    SciQLopVerticalSpan,
)

MARKER = QColor(255, 0, 255)


def _painted_pixels(plot, tmp_path, name):
    path = str(tmp_path / f"{name}.png")
    assert plot.save_png(path, 800, 600) is True
    img = QImage(path).convertToFormat(QImage.Format_RGB32)
    h, w = img.height(), img.width()
    buf = np.frombuffer(img.constBits(), dtype=np.uint8)
    arr = buf.reshape(h, img.bytesPerLine() // 4, 4)[:, :w, :]
    b, g, r = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    return int(((r == 255) & (g == 0) & (b == 255)).sum())


def _flush_deferred_delete():
    """processEvents() alone does not dispatch DeferredDelete."""
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    QApplication.processEvents()


def _text(plot):
    item = SciQLopTextItem(plot, "MMMM", QPointF(120.0, 60.0), False, Coordinates.Pixels)
    item.set_font_size(20)
    item.set_color(MARKER)
    return item


def _ellipse(plot):
    return SciQLopEllipseItem(
        plot, QRectF(100, 60, 40, 40), MARKER, 1.0, MARKER, False, Coordinates.Pixels)


def _curved_line(plot):
    item = SciQLopCurvedLineItem(
        plot, QPointF(100.0, 60.0), QPointF(180.0, 60.0),
        LineTermination.NoneTermination, LineTermination.NoneTermination, Coordinates.Pixels)
    item.set_color(MARKER)
    item.set_line_width(4.0)
    return item


def _pixmap(plot):
    pixmap = QPixmap(40, 40)
    pixmap.fill(MARKER)
    return SciQLopPixmapItem(plot, pixmap, QRectF(100, 60, 40, 40), False, Coordinates.Pixels)


def _straight_line(plot):
    item = SciQLopStraightLine(plot, 120.0, False, Coordinates.Pixels, Qt.Orientation.Vertical)
    item.set_color(MARKER)
    item.set_line_width(4.0)
    return item


def _vertical_line(plot):
    item = SciQLopVerticalLine(plot, 50.0, False)
    item.set_color(MARKER)
    item.set_line_width(4.0)
    return item


def _horizontal_line(plot):
    item = SciQLopHorizontalLine(plot, 50.0, False)
    item.set_color(MARKER)
    item.set_line_width(4.0)
    return item


def _vertical_span(plot):
    return SciQLopVerticalSpan(
        plot, SciQLopPlotRange(100, 160), MARKER, False, True, "", Coordinates.Pixels)


def _horizontal_span(plot):
    return SciQLopHorizontalSpan(plot, SciQLopPlotRange(20, 60), MARKER, False, True, "")


def _rectangular_span(plot):
    return SciQLopRectangularSpan(
        plot, SciQLopPlotRange(20, 60), SciQLopPlotRange(20, 60), MARKER, False, True, "")


ITEMS = [
    ("text", _text),
    ("ellipse", _ellipse),
    ("curved_line", _curved_line),
    ("pixmap", _pixmap),
    ("straight_line", _straight_line),
    ("vertical_line", _vertical_line),
    ("horizontal_line", _horizontal_line),
    ("vertical_span", _vertical_span),
    ("horizontal_span", _horizontal_span),
    ("rectangular_span", _rectangular_span),
]


@pytest.fixture
def xy_plot(plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 100.0))
    plot.y_axis().set_range(SciQLopPlotRange(0.0, 100.0))
    return plot


@pytest.mark.parametrize("name,factory", ITEMS, ids=[i[0] for i in ITEMS])
class TestItemIsRemovedWithItsPythonReference:

    def test_dropping_the_reference_removes_the_item(self, xy_plot, tmp_path, name, factory):
        item = factory(xy_plot)
        assert _painted_pixels(xy_plot, tmp_path, f"{name}_before") > 0, (
            f"{name} did not render in the first place"
        )

        del item
        gc.collect()
        QApplication.processEvents()
        gc.collect()

        assert _painted_pixels(xy_plot, tmp_path, f"{name}_after") == 0, (
            f"{name} is still painted after its last Python reference went away: "
            f"the wrapper was collected but left its QCP item on the plot"
        )

    def test_delete_later_removes_the_item(self, xy_plot, tmp_path, name, factory):
        item = factory(xy_plot)
        assert _painted_pixels(xy_plot, tmp_path, f"{name}_del_before") > 0

        item.deleteLater()
        _flush_deferred_delete()
        gc.collect()
        _flush_deferred_delete()

        assert _painted_pixels(xy_plot, tmp_path, f"{name}_del_after") == 0, (
            f"{name} is still painted after deleteLater()"
        )

    def test_item_kept_in_a_reference_stays(self, xy_plot, tmp_path, name, factory):
        """The other half: holding the item must keep it, across collections."""
        item = factory(xy_plot)
        before = _painted_pixels(xy_plot, tmp_path, f"{name}_keep_before")
        assert before > 0

        gc.collect()
        QApplication.processEvents()
        gc.collect()

        assert _painted_pixels(xy_plot, tmp_path, f"{name}_keep_after") == before, (
            f"{name} disappeared while still referenced"
        )
        assert item is not None


# SciQLopStraightLine derives straight from QObject rather than
# SciQLopItemInterface, so it never had set_visible/visible to begin with --
# that is a missing feature, not a broken inherited one.
VISIBILITY_ITEMS = [i for i in ITEMS
                    if i[0] not in ("straight_line", "vertical_line", "horizontal_line")]


@pytest.mark.parametrize("name,factory", VISIBILITY_ITEMS,
                         ids=[i[0] for i in VISIBILITY_ITEMS])
class TestItemVisibility:
    """set_visible() is declared on SciQLopItemInterface, so it must do something.

    Text/Ellipse/CurvedLine/Pixmap inherited it without overriding, so calling it
    printed "Abstract method called" and the item stayed on screen.
    """

    def test_set_visible_toggles_the_item(self, xy_plot, tmp_path, name, factory):
        item = factory(xy_plot)
        assert _painted_pixels(xy_plot, tmp_path, f"{name}_vis_before") > 0

        item.set_visible(False)
        QApplication.processEvents()
        assert _painted_pixels(xy_plot, tmp_path, f"{name}_vis_hidden") == 0, (
            f"{name} is still painted after set_visible(False)"
        )
        assert item.visible() is False

        item.set_visible(True)
        QApplication.processEvents()
        assert _painted_pixels(xy_plot, tmp_path, f"{name}_vis_shown") > 0
        assert item.visible() is True
