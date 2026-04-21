"""Tests for the uniform style API across all drawing primitives."""
import pytest
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot,
    SciQLopPlotRange,
    SciQLopTextItem,
    SciQLopEllipseItem,
    SciQLopCurvedLineItem,
    SciQLopVerticalLine,
    SciQLopHorizontalLine,
    SciQLopVerticalSpan,
    SciQLopHorizontalSpan,
    SciQLopRectangularSpan,
)


@pytest.fixture(scope="module")
def app():
    instance = QApplication.instance()
    if instance is None:
        instance = QApplication([])
    return instance


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


class TestTextItemStyle:
    def test_color_roundtrip(self, plot):
        t = SciQLopTextItem(plot, "hello", QPointF(10, 10))
        t.set_color(QColor("red"))
        assert t.color() == QColor("red")

    def test_font_size_roundtrip(self, plot):
        t = SciQLopTextItem(plot, "hello", QPointF(10, 10))
        t.set_font_size(18.0)
        assert abs(t.font_size() - 18.0) < 0.1

    def test_font_color_roundtrip(self, plot):
        t = SciQLopTextItem(plot, "hello", QPointF(10, 10))
        t.set_font_color(QColor("blue"))
        assert t.font_color() == QColor("blue")

    def test_font_color_and_color_are_same(self, plot):
        t = SciQLopTextItem(plot, "hello", QPointF(10, 10))
        t.set_color(QColor("green"))
        assert t.font_color() == QColor("green")
        t.set_font_color(QColor("orange"))
        assert t.color() == QColor("orange")


class TestEllipseItemStyle:
    def test_color_roundtrip(self, plot):
        e = SciQLopEllipseItem(plot, QRectF(0, 0, 50, 50))
        e.set_color(QColor("green"))
        assert e.color() == QColor("green")

    def test_line_width_roundtrip(self, plot):
        e = SciQLopEllipseItem(plot, QRectF(0, 0, 50, 50))
        e.set_line_width(3.5)
        assert abs(e.line_width() - 3.5) < 0.01

    def test_fill_color_roundtrip(self, plot):
        e = SciQLopEllipseItem(plot, QRectF(0, 0, 50, 50))
        e.set_fill_color(QColor("yellow"))
        assert e.fill_color() == QColor("yellow")

    def test_line_style_roundtrip(self, plot):
        e = SciQLopEllipseItem(plot, QRectF(0, 0, 50, 50))
        e.set_line_style(Qt.PenStyle.DashLine)
        assert e.line_style() == Qt.PenStyle.DashLine

    def test_color_independent_of_fill(self, plot):
        e = SciQLopEllipseItem(plot, QRectF(0, 0, 50, 50))
        e.set_color(QColor("red"))
        e.set_fill_color(QColor("blue"))
        assert e.color() == QColor("red")
        assert e.fill_color() == QColor("blue")


class TestCurvedLineItemStyle:
    def test_color_roundtrip(self, plot):
        c = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(100, 100))
        c.set_color(QColor("red"))
        assert c.color() == QColor("red")

    def test_line_width_roundtrip(self, plot):
        c = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(100, 100))
        c.set_line_width(2.5)
        assert abs(c.line_width() - 2.5) < 0.01

    def test_line_style_roundtrip(self, plot):
        c = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(100, 100))
        c.set_line_style(Qt.PenStyle.DotLine)
        assert c.line_style() == Qt.PenStyle.DotLine

    def test_color_changes_preserve_width(self, plot):
        c = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(100, 100))
        c.set_line_width(4.0)
        c.set_color(QColor("purple"))
        assert abs(c.line_width() - 4.0) < 0.01

    def test_width_changes_preserve_color(self, plot):
        c = SciQLopCurvedLineItem(plot, QPointF(0, 0), QPointF(100, 100))
        c.set_color(QColor("cyan"))
        c.set_line_width(3.0)
        assert c.color() == QColor("cyan")


class TestStraightLineStyle:
    def test_vertical_line_color(self, plot):
        vl = SciQLopVerticalLine(plot, 5.0)
        vl.set_color(QColor("blue"))
        assert vl.color() == QColor("blue")

    def test_horizontal_line_color(self, plot):
        hl = SciQLopHorizontalLine(plot, 3.0)
        hl.set_color(QColor("red"))
        assert hl.color() == QColor("red")

    def test_line_width_roundtrip(self, plot):
        vl = SciQLopVerticalLine(plot, 5.0)
        vl.set_line_width(2.0)
        assert abs(vl.line_width() - 2.0) < 0.01

    def test_line_style_roundtrip(self, plot):
        vl = SciQLopVerticalLine(plot, 5.0)
        vl.set_line_style(Qt.PenStyle.DashDotLine)
        assert vl.line_style() == Qt.PenStyle.DashDotLine

    def test_all_pen_styles(self, plot):
        vl = SciQLopVerticalLine(plot, 5.0)
        for style in [
            Qt.PenStyle.SolidLine,
            Qt.PenStyle.DashLine,
            Qt.PenStyle.DotLine,
            Qt.PenStyle.DashDotLine,
            Qt.PenStyle.DashDotDotLine,
        ]:
            vl.set_line_style(style)
            assert vl.line_style() == style


class TestVerticalSpanStyle:
    def test_color_roundtrip(self, plot):
        vs = SciQLopVerticalSpan(plot, SciQLopPlotRange(1.0, 5.0))
        vs.set_color(QColor(200, 100, 50, 128))
        assert vs.color() == QColor(200, 100, 50, 128)

    def test_borders_color_roundtrip(self, plot):
        vs = SciQLopVerticalSpan(plot, SciQLopPlotRange(1.0, 5.0))
        vs.set_borders_color(QColor("red"))
        assert vs.borders_color() == QColor("red")

    def test_line_width_roundtrip(self, plot):
        vs = SciQLopVerticalSpan(plot, SciQLopPlotRange(1.0, 5.0))
        vs.set_line_width(4.0)
        assert abs(vs.line_width() - 4.0) < 0.01

    def test_line_style_roundtrip(self, plot):
        vs = SciQLopVerticalSpan(plot, SciQLopPlotRange(1.0, 5.0))
        vs.set_line_style(Qt.PenStyle.DashLine)
        assert vs.line_style() == Qt.PenStyle.DashLine


class TestHorizontalSpanStyle:
    def test_color_roundtrip(self, plot):
        hs = SciQLopHorizontalSpan(plot, SciQLopPlotRange(2.0, 8.0))
        hs.set_color(QColor("cyan"))
        assert hs.color() == QColor("cyan")

    def test_borders_color_roundtrip(self, plot):
        hs = SciQLopHorizontalSpan(plot, SciQLopPlotRange(2.0, 8.0))
        hs.set_borders_color(QColor("magenta"))
        assert hs.borders_color() == QColor("magenta")

    def test_line_width_roundtrip(self, plot):
        hs = SciQLopHorizontalSpan(plot, SciQLopPlotRange(2.0, 8.0))
        hs.set_line_width(2.0)
        assert abs(hs.line_width() - 2.0) < 0.01

    def test_line_style_roundtrip(self, plot):
        hs = SciQLopHorizontalSpan(plot, SciQLopPlotRange(2.0, 8.0))
        hs.set_line_style(Qt.PenStyle.DotLine)
        assert hs.line_style() == Qt.PenStyle.DotLine

    def test_range_roundtrip(self, plot):
        hs = SciQLopHorizontalSpan(plot, SciQLopPlotRange(2.0, 8.0))
        r = hs.range()
        assert abs(r.start() - 2.0) < 0.01
        assert abs(r.stop() - 8.0) < 0.01


class TestRectangularSpanStyle:
    def test_color_roundtrip(self, plot):
        rs = SciQLopRectangularSpan(
            plot, SciQLopPlotRange(0, 5), SciQLopPlotRange(0, 10)
        )
        rs.set_color(QColor("magenta"))
        assert rs.color() == QColor("magenta")

    def test_borders_color_roundtrip(self, plot):
        rs = SciQLopRectangularSpan(
            plot, SciQLopPlotRange(0, 5), SciQLopPlotRange(0, 10)
        )
        rs.set_borders_color(QColor("orange"))
        assert rs.borders_color() == QColor("orange")

    def test_line_width_roundtrip(self, plot):
        rs = SciQLopRectangularSpan(
            plot, SciQLopPlotRange(0, 5), SciQLopPlotRange(0, 10)
        )
        rs.set_line_width(3.0)
        assert abs(rs.line_width() - 3.0) < 0.01

    def test_line_style_roundtrip(self, plot):
        rs = SciQLopRectangularSpan(
            plot, SciQLopPlotRange(0, 5), SciQLopPlotRange(0, 10)
        )
        rs.set_line_style(Qt.PenStyle.DashLine)
        assert rs.line_style() == Qt.PenStyle.DashLine

    def test_key_value_range_roundtrip(self, plot):
        rs = SciQLopRectangularSpan(
            plot, SciQLopPlotRange(1, 3), SciQLopPlotRange(4, 7)
        )
        kr = rs.key_range()
        vr = rs.value_range()
        assert abs(kr.start() - 1.0) < 0.01
        assert abs(kr.stop() - 3.0) < 0.01
        assert abs(vr.start() - 4.0) < 0.01
        assert abs(vr.stop() - 7.0) < 0.01
