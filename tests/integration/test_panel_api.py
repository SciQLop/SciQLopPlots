"""Tests for SciQLopMultiPlotPanel: plot management, data, spans."""
import pytest
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
    SciQLopPlotRange, SciQLopGraphInterface, PlotType,
    MultiPlotsVerticalSpan,
)
from conftest import force_gc


class TestPanelPlotManagement:

    def test_add_plot(self, panel):
        plot = SciQLopPlot()
        panel.add_plot(plot)
        assert panel.size() == 1
        assert not panel.empty()

    def test_add_multiple_plots(self, panel):
        for _ in range(5):
            panel.add_plot(SciQLopPlot())
        assert panel.size() == 5

    def test_plot_at(self, panel):
        plot = SciQLopPlot()
        panel.add_plot(plot)
        retrieved = panel.plot_at(0)
        assert retrieved is not None

    def test_plots_list(self, panel):
        panel.add_plot(SciQLopPlot())
        panel.add_plot(SciQLopPlot())
        plots = panel.plots()
        assert len(plots) == 2

    def test_remove_plot(self, panel):
        plot = SciQLopPlot()
        panel.add_plot(plot)
        assert panel.size() == 1
        panel.remove_plot(plot)
        assert panel.empty()

    def test_insert_plot(self, panel):
        p1 = SciQLopPlot()
        p2 = SciQLopPlot()
        panel.add_plot(p1)
        panel.insert_plot(0, p2)
        assert panel.size() == 2

    def test_move_plot(self, panel):
        p1 = SciQLopPlot()
        p2 = SciQLopPlot()
        panel.add_plot(p1)
        panel.add_plot(p2)
        panel.move_plot(0, 1)
        assert panel.size() == 2

    def test_clear(self, panel):
        for _ in range(3):
            panel.add_plot(SciQLopPlot())
        assert panel.size() == 3
        panel.clear()
        assert panel.empty()

    def test_contains(self, panel):
        plot = SciQLopPlot()
        panel.add_plot(plot)
        assert panel.contains(plot)

    def test_create_plot(self, panel):
        plot = panel.create_plot()
        assert plot is not None
        assert panel.size() == 1

    def test_create_plot_timeseries(self, panel):
        plot = panel.create_plot(plot_type=PlotType.TimeSeries)
        assert plot is not None
        assert panel.size() == 1


class TestPanelPlotting:

    def test_line_returns_tuple(self, panel, sample_data):
        x, y = sample_data
        result = panel.line(x, y)
        assert isinstance(result, tuple)
        assert len(result) == 2
        plot_obj, graph_obj = result
        assert plot_obj is not None
        assert graph_obj is not None

    def test_colormap_returns_tuple(self, panel, sample_colormap_data):
        x, y, z = sample_colormap_data
        result = panel.colormap(x, y, z)
        assert isinstance(result, tuple)
        assert len(result) == 2

    def test_callable_line(self, panel):
        def make_data(start, stop):
            x = np.linspace(start, stop, 100)
            y = np.sin(x)
            return x, y

        result = panel.line(make_data)
        assert isinstance(result, tuple)
        assert result[0] is not None
        assert result[1] is not None

    def test_multiple_graphs_create_multiple_plots(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        panel.line(x, y)
        panel.line(x, y)
        assert panel.size() == 3


class TestPanelAxisRange:

    def test_set_x_axis_range(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        r = SciQLopPlotRange(2.0, 8.0)
        panel.set_x_axis_range(r)
        got = panel.x_axis_range()
        assert abs(got.start() - 2.0) < 1e-9
        assert abs(got.stop() - 8.0) < 1e-9

    def test_set_time_axis_range(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        r = SciQLopPlotRange(100.0, 200.0)
        panel.set_time_axis_range(r)
        got = panel.time_axis_range()
        assert abs(got.start() - 100.0) < 1e-9


class TestMultiPlotsVerticalSpan:

    def test_create(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(2.0, 5.0),
            QColor(100, 100, 200),
        )
        assert span is not None

    def test_range(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(2.0, 5.0),
        )
        r = span.range()
        assert abs(r.start() - 2.0) < 1e-9
        assert abs(r.stop() - 5.0) < 1e-9

    def test_set_range(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0),
        )
        span.set_range(SciQLopPlotRange(3.0, 7.0))
        r = span.range()
        assert abs(r.start() - 3.0) < 1e-9

    def test_visibility(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0),
        )
        assert span.is_visible()
        span.set_visible(False)
        assert not span.is_visible()

    def test_color(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        color = QColor(255, 0, 0)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0), color,
        )
        assert span.get_color().red() == 255

    def test_delete_no_crash(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0),
        )
        del span
        force_gc()
