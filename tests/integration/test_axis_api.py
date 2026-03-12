"""Tests for axis manipulation and SciQLopPlotRange."""
import pytest
import math
from PySide6.QtCore import Qt

from SciQLopPlots import (
    SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopPlotRange, AxisType,
)


class TestAxisAccess:

    def test_x_axis(self, plot):
        ax = plot.x_axis()
        assert ax is not None

    def test_y_axis(self, plot):
        ax = plot.y_axis()
        assert ax is not None

    def test_x2_axis(self, plot):
        ax = plot.x2_axis()
        assert ax is not None

    def test_y2_axis(self, plot):
        ax = plot.y2_axis()
        assert ax is not None

    def test_time_axis_on_ts_plot(self, ts_plot):
        ax = ts_plot.time_axis()
        assert ax is not None

    def test_axis_by_type(self, plot):
        ax = plot.axis(AxisType.XAxis)
        assert ax is not None
        ax = plot.axis(AxisType.YAxis)
        assert ax is not None


class TestAxisRange:

    def test_set_get_range(self, plot):
        ax = plot.x_axis()
        r = SciQLopPlotRange(0.0, 10.0)
        ax.set_range(r)
        got = ax.range()
        assert abs(got.start() - 0.0) < 1e-9
        assert abs(got.stop() - 10.0) < 1e-9

    def test_set_range_double_overload(self, plot):
        ax = plot.x_axis()
        ax.set_range(5.0, 15.0)
        got = ax.range()
        assert abs(got.start() - 5.0) < 1e-9
        assert abs(got.stop() - 15.0) < 1e-9

    def test_rescale_no_crash(self, plot):
        ax = plot.x_axis()
        ax.rescale()  # no data, should not crash


class TestAxisProperties:

    def test_set_log(self, plot):
        ax = plot.y_axis()
        ax.set_log(True)
        assert ax.log() is True
        ax.set_log(False)
        assert ax.log() is False

    def test_set_label(self, plot):
        ax = plot.x_axis()
        ax.set_label("Time (s)")
        assert ax.label() == "Time (s)"

    def test_set_visible(self, plot):
        ax = plot.x_axis()
        ax.set_visible(False)
        assert ax.visible() is False
        ax.set_visible(True)
        assert ax.visible() is True

    def test_orientation(self, plot):
        ax = plot.x_axis()
        assert ax.orientation() in (Qt.Horizontal, Qt.Vertical)


class TestSciQLopPlotRange:

    def test_construction(self):
        r = SciQLopPlotRange(1.0, 5.0)
        assert r.start() == 1.0
        assert r.stop() == 5.0

    def test_auto_sorts(self):
        r = SciQLopPlotRange(10.0, 2.0)
        assert r.start() == 2.0
        assert r.stop() == 10.0

    def test_size(self):
        r = SciQLopPlotRange(2.0, 8.0)
        assert abs(r.size() - 6.0) < 1e-9

    def test_center(self):
        r = SciQLopPlotRange(2.0, 8.0)
        assert abs(r.center() - 5.0) < 1e-9

    def test_contains_value(self):
        r = SciQLopPlotRange(0.0, 10.0)
        assert r.contains(5.0)
        assert not r.contains(15.0)

    def test_contains_range(self):
        outer = SciQLopPlotRange(0.0, 10.0)
        inner = SciQLopPlotRange(2.0, 8.0)
        assert outer.contains(inner)
        assert not inner.contains(outer)

    def test_equality(self):
        a = SciQLopPlotRange(1.0, 5.0)
        b = SciQLopPlotRange(1.0, 5.0)
        assert a == b

    def test_inequality(self):
        a = SciQLopPlotRange(1.0, 5.0)
        b = SciQLopPlotRange(1.0, 6.0)
        assert a != b

    def test_getitem(self):
        r = SciQLopPlotRange(3.0, 7.0)
        assert r[0] == 3.0
        assert r[1] == 7.0

    def test_len(self):
        r = SciQLopPlotRange(3.0, 7.0)
        assert len(r) == 2

    def test_addition(self):
        r = SciQLopPlotRange(2.0, 4.0)
        shifted = r + 3.0
        assert abs(shifted.start() - 5.0) < 1e-9
        assert abs(shifted.stop() - 7.0) < 1e-9

    def test_multiplication(self):
        r = SciQLopPlotRange(0.0, 10.0)
        scaled = r * 2.0
        assert abs(scaled.size() - 20.0) < 1e-9
        assert abs(scaled.center() - 5.0) < 1e-9

    def test_is_valid(self):
        r = SciQLopPlotRange(1.0, 5.0)
        assert r.is_valid()

    def test_is_null(self):
        r = SciQLopPlotRange(float('nan'), float('nan'))
        assert r.is_null()

    def test_intersects(self):
        a = SciQLopPlotRange(0.0, 5.0)
        b = SciQLopPlotRange(3.0, 8.0)
        assert a.intersects(b)
        c = SciQLopPlotRange(6.0, 8.0)
        assert not a.intersects(c)
