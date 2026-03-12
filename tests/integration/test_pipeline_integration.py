"""Tests for the reactive pipeline with real Qt/binding objects."""
import pytest
import numpy as np

from SciQLopPlots import (
    SciQLopPlot, SciQLopPlotRange, SciQLopVerticalSpan,
    SciQLopGraphInterface,
)
from conftest import process_events


class TestObservablePropertyAccess:

    def test_graph_has_on_data(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        assert hasattr(g, "on")
        assert hasattr(g.on, "data")

    def test_axis_has_on_range(self, plot):
        ax = plot.x_axis()
        assert hasattr(ax, "on")
        assert hasattr(ax.on, "range")

    def test_span_has_on_range(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        assert hasattr(span, "on")
        assert hasattr(span.on, "range")

    def test_span_has_on_tooltip(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        assert hasattr(span.on, "tooltip")


class TestAxisPipeline:

    def test_axis_range_propagation(self, qtbot, plot):
        """axis.on.range >> other_axis.on.range syncs ranges."""
        ax1 = plot.x_axis()
        ax2 = plot.y_axis()
        pipeline = ax1.on.range >> ax2.on.range
        ax1.set_range(SciQLopPlotRange(10.0, 20.0))
        process_events()
        got = ax2.range()
        assert abs(got.start() - 10.0) < 1e-9
        assert abs(got.stop() - 20.0) < 1e-9


class TestSpanPipeline:

    def test_span_range_to_tooltip(self, qtbot, plot):
        """span.on.range >> transform >> span.on.tooltip updates tooltip."""
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))

        def format_range(event):
            r = event.value
            return f"{r.start():.1f}-{r.stop():.1f}"

        pipeline = span.on.range >> format_range >> span.on.tooltip
        span.set_range(SciQLopPlotRange(3.0, 7.0))
        process_events()
        assert span.tool_tip() == "3.0-7.0"


class TestTerminalSink:

    def test_terminal_callback(self, qtbot, plot):
        """Pipeline with no target fires a callback."""
        ax = plot.x_axis()
        received = []

        def sink(event):
            received.append(event.value)

        pipeline = ax.on.range >> sink
        ax.set_range(SciQLopPlotRange(100.0, 200.0))
        process_events()
        assert len(received) >= 1


class TestPipelineDisconnect:

    def test_disconnect_stops_propagation(self, qtbot, plot):
        """After disconnect, changes no longer propagate."""
        ax1 = plot.x_axis()
        ax2 = plot.y_axis()
        pipeline = ax1.on.range >> ax2.on.range

        ax1.set_range(SciQLopPlotRange(10.0, 20.0))
        process_events()
        assert abs(ax2.range().start() - 10.0) < 1e-9

        pipeline.disconnect()

        ax2.set_range(SciQLopPlotRange(0.0, 1.0))
        process_events()
        ax1.set_range(SciQLopPlotRange(50.0, 60.0))
        process_events()
        got = ax2.range()
        assert abs(got.start() - 0.0) < 1e-9
