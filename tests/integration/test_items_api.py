"""Tests for overlay items: spans, lines, text, shapes."""
import pytest
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtCore import QRectF, QPointF

from SciQLopPlots import (
    SciQLopPlot, SciQLopVerticalSpan, SciQLopPlotRange,
    SciQLopVerticalLine, SciQLopHorizontalLine,
    SciQLopTextItem, SciQLopEllipseItem, SciQLopPixmapItem,
    SciQLopCurvedLineItem, Coordinates,
)
from conftest import force_gc


class TestVerticalSpan:

    def test_create(self, plot):
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(2.0, 5.0),
            QColor(100, 100, 200, 80),
        )
        assert span is not None

    def test_range_roundtrip(self, plot):
        r = SciQLopPlotRange(2.0, 5.0)
        span = SciQLopVerticalSpan(plot, r)
        got = span.range()
        assert abs(got.start() - 2.0) < 1e-9
        assert abs(got.stop() - 5.0) < 1e-9

    def test_set_range(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        span.set_range(SciQLopPlotRange(3.0, 7.0))
        got = span.range()
        assert abs(got.start() - 3.0) < 1e-9
        assert abs(got.stop() - 7.0) < 1e-9

    def test_color_roundtrip(self, plot):
        color = QColor(255, 0, 0, 128)
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0), color)
        got = span.color()
        assert got.red() == 255
        assert got.green() == 0

    def test_set_color(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        span.set_color(QColor("green"))
        assert span.color().green() > 0

    def test_visibility(self, plot):
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(0.0, 1.0),
            visible=True,
        )
        assert span.visible() is True
        span.set_visible(False)
        assert span.visible() is False

    def test_read_only(self, plot):
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(0.0, 1.0),
            read_only=True,
        )
        assert span.read_only() is True
        span.set_read_only(False)
        assert span.read_only() is False

    def test_tool_tip(self, plot):
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(0.0, 1.0),
            tool_tip="hello",
        )
        assert span.tool_tip() == "hello"
        span.set_tool_tip("world")
        assert span.tool_tip() == "world"

    def test_delete_span_no_crash(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        del span
        force_gc()

    def test_delete_plot_with_span_no_crash(self, qtbot):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        span = SciQLopVerticalSpan(p, SciQLopPlotRange(0.0, 1.0))
        del p
        force_gc()


class TestStraightLines:

    def test_create_vertical_line(self, plot):
        line = SciQLopVerticalLine(plot, 5.0)
        assert line is not None
        assert abs(line.position - 5.0) < 1e-9

    def test_create_horizontal_line(self, plot):
        line = SciQLopHorizontalLine(plot, 3.0)
        assert line is not None
        assert abs(line.position - 3.0) < 1e-9

    def test_set_position(self, plot):
        line = SciQLopVerticalLine(plot, 5.0)
        line.set_position(8.0)
        assert abs(line.position - 8.0) < 1e-9

    def test_set_color(self, plot):
        line = SciQLopVerticalLine(plot, 5.0)
        line.set_color(QColor("red"))
        assert line.color().red() == 255

    def test_set_line_width(self, plot):
        line = SciQLopVerticalLine(plot, 5.0)
        line.set_line_width(3.0)
        assert abs(line.line_width() - 3.0) < 1e-9

    def test_movable_line(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, movable=True)
        assert line is not None

    def test_delete_line_no_crash(self, plot):
        line = SciQLopVerticalLine(plot, 5.0)
        del line
        force_gc()


class TestTextItem:

    def test_create(self, plot):
        text = SciQLopTextItem(plot, "hello", QPointF(10, 20))
        assert text is not None

    def test_text_roundtrip(self, plot):
        text = SciQLopTextItem(plot, "hello", QPointF(10, 20))
        assert text.text() == "hello"

    def test_set_text(self, plot):
        text = SciQLopTextItem(plot, "hello", QPointF(10, 20))
        text.set_text("world")
        assert text.text() == "world"

    def test_position(self, plot):
        pos = QPointF(10, 20)
        text = SciQLopTextItem(plot, "hello", pos)
        got = text.position()
        assert abs(got.x() - 10.0) < 1e-9
        assert abs(got.y() - 20.0) < 1e-9

    def test_set_position(self, plot):
        text = SciQLopTextItem(plot, "hello", QPointF(0, 0))
        text.set_position(QPointF(50, 60))
        got = text.position()
        assert abs(got.x() - 50.0) < 1e-9

    def test_delete_no_crash(self, plot):
        text = SciQLopTextItem(plot, "hello", QPointF(0, 0))
        del text
        force_gc()


class TestEllipseItem:

    def test_create(self, plot):
        ellipse = SciQLopEllipseItem(plot, QRectF(0, 0, 10, 10))
        assert ellipse is not None

    def test_bounding_rect(self, plot):
        rect = QRectF(0, 0, 10, 10)
        ellipse = SciQLopEllipseItem(plot, rect)
        got = ellipse.bounding_rectangle()
        assert abs(got.width() - 10.0) < 1e-9

    def test_set_position(self, plot):
        ellipse = SciQLopEllipseItem(plot, QRectF(0, 0, 10, 10))
        ellipse.set_position(QPointF(5, 5))
        got = ellipse.position()
        assert abs(got.x() - 5.0) < 1e-9

    def test_delete_no_crash(self, plot):
        ellipse = SciQLopEllipseItem(plot, QRectF(0, 0, 10, 10))
        del ellipse
        force_gc()


class TestPixmapItem:

    def test_create(self, plot):
        pixmap = QPixmap(10, 10)
        item = SciQLopPixmapItem(plot, pixmap, QRectF(0, 0, 10, 10))
        assert item is not None

    def test_delete_no_crash(self, plot):
        pixmap = QPixmap(10, 10)
        item = SciQLopPixmapItem(plot, pixmap, QRectF(0, 0, 10, 10))
        del item
        force_gc()


class TestCurvedLineItem:

    def test_create(self, plot):
        item = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(10, 10))
        assert item is not None

    def test_positions(self, plot):
        start = QPointF(1, 2)
        stop = QPointF(8, 9)
        item = SciQLopCurvedLineItem(plot, start, stop)
        got_start = item.start_position()
        got_stop = item.stop_position()
        assert abs(got_start.x() - 1.0) < 1e-9
        assert abs(got_stop.x() - 8.0) < 1e-9

    def test_delete_no_crash(self, plot):
        item = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(10, 10))
        del item
        force_gc()
