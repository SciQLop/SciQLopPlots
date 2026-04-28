"""Tests for StraightLine position_changed signal and VSpan pixel/coordinates."""
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (
    Coordinates,
    SciQLopPlot,
    SciQLopPlotRange,
    SciQLopVerticalLine,
    SciQLopHorizontalLine,
    SciQLopVerticalSpan,
)
from conftest import process_events


class TestStraightLineSignal:

    def test_position_changed_on_set(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(10.0)
        process_events()
        assert len(received) == 1
        assert received[0] == pytest.approx(10.0)

    def test_position_changed_not_emitted_when_unchanged(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(5.0)
        process_events()
        assert received == [], (
            "set_position must not re-emit when value is unchanged "
            "(prevents two-way binding loops)"
        )

    def test_horizontal_line_signal(self, plot):
        line = SciQLopHorizontalLine(plot, 3.0, True)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(7.0)
        process_events()
        assert len(received) == 1
        assert received[0] == pytest.approx(7.0)

    def test_position_property_roundtrip(self, plot):
        from SciQLopPlots import SciQLopStraightLine
        import PySide6.QtCore
        line = SciQLopStraightLine(plot, 5.0, False, Coordinates.Data, PySide6.QtCore.Qt.Orientation.Vertical)
        assert line.position == pytest.approx(5.0)
        line.position = 12.0
        assert line.position == pytest.approx(12.0)


class TestStraightLineMinMax:

    def test_min_clamps_set_position(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(2.0)
        line.set_position(1.0)
        assert line.position == pytest.approx(2.0)

    def test_max_clamps_set_position(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_max_value(10.0)
        line.set_position(15.0)
        assert line.position == pytest.approx(10.0)

    def test_within_range_not_clamped(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(0.0)
        line.set_max_value(10.0)
        line.set_position(7.0)
        assert line.position == pytest.approx(7.0)

    def test_clear_min_removes_limit(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(3.0)
        line.set_position(1.0)
        assert line.position == pytest.approx(3.0)
        line.clear_min_value()
        line.set_position(1.0)
        assert line.position == pytest.approx(1.0)

    def test_clear_max_removes_limit(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_max_value(8.0)
        line.set_position(20.0)
        assert line.position == pytest.approx(8.0)
        line.clear_max_value()
        line.set_position(20.0)
        assert line.position == pytest.approx(20.0)

    def test_signal_emits_clamped_value(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(2.0)
        line.set_max_value(10.0)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(-5.0)
        process_events()
        assert received[-1] == pytest.approx(2.0)
        line.set_position(99.0)
        process_events()
        assert received[-1] == pytest.approx(10.0)

    def test_horizontal_line_min_max(self, plot):
        line = SciQLopHorizontalLine(plot, 5.0, True)
        line.set_min_value(1.0)
        line.set_max_value(9.0)
        line.set_position(0.0)
        assert line.position == pytest.approx(1.0)
        line.set_position(100.0)
        assert line.position == pytest.approx(9.0)


class TestVSpanPixelRange:

    def test_pixel_range_returns_valid(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        plot.x_axis().set_range(SciQLopPlotRange(0, 100))
        process_events()
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(20, 80))
        process_events()
        pr = span.pixel_range
        assert pr[0] != pr[1]

    def test_pixel_range_set_roundtrip(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        plot.x_axis().set_range(SciQLopPlotRange(0, 100))
        process_events()
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(20, 80))
        process_events()
        original_range = span.range()
        pr = span.pixel_range
        span.set_pixel_range(pr)
        process_events()
        restored = span.range()
        assert restored[0] == pytest.approx(original_range[0], abs=0.1)
        assert restored[1] == pytest.approx(original_range[1], abs=0.1)


class TestVSpanCoordinates:

    def test_default_is_data(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(10, 20))
        assert span.coordinates() == Coordinates.Data

    def test_construct_in_pixel_mode(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(100, 300),
            coordinates=Coordinates.Pixels)
        process_events()
        assert span.coordinates() == Coordinates.Pixels

    def test_pixel_span_stays_fixed_on_zoom(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(200, 400),
            coordinates=Coordinates.Pixels)
        process_events()
        r1 = span.range()
        plot.x_axis().set_range(SciQLopPlotRange(0, 1000))
        process_events()
        r2 = span.range()
        assert r1[0] == pytest.approx(r2[0], abs=0.01)
        assert r1[1] == pytest.approx(r2[1], abs=0.01)

    def test_switch_data_to_pixels(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        plot.x_axis().set_range(SciQLopPlotRange(0, 100))
        process_events()
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(20, 80))
        process_events()
        pr_before = span.pixel_range
        span.set_coordinates(Coordinates.Pixels)
        process_events()
        assert span.coordinates() == Coordinates.Pixels
        r_after = span.range()
        assert r_after[0] == pytest.approx(pr_before[0], abs=1.0)
        assert r_after[1] == pytest.approx(pr_before[1], abs=1.0)

    def test_switch_pixels_to_data(self, plot):
        plot.resize(800, 600)
        plot.show()
        process_events()
        plot.x_axis().set_range(SciQLopPlotRange(0, 100))
        process_events()
        span = SciQLopVerticalSpan(
            plot, SciQLopPlotRange(200, 400),
            coordinates=Coordinates.Pixels)
        process_events()
        span.set_coordinates(Coordinates.Data)
        process_events()
        assert span.coordinates() == Coordinates.Data
        r = span.range()
        assert r[0] != r[1]
