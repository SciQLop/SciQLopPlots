"""Tests for signal emission verification using qtbot.waitSignal."""
import pytest
import numpy as np

from SciQLopPlots import (
    SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
    SciQLopPlotRange, SciQLopVerticalSpan, SciQLopGraphInterface,
)


class TestAxisSignals:

    def test_range_changed_on_set_range(self, qtbot, plot):
        ax = plot.x_axis()
        with qtbot.waitSignal(ax.range_changed, timeout=1000):
            ax.set_range(SciQLopPlotRange(0.0, 10.0))

    def test_range_changed_with_doubles(self, qtbot, plot):
        ax = plot.y_axis()
        with qtbot.waitSignal(ax.range_changed, timeout=1000):
            ax.set_range(5.0, 15.0)


class TestSpanSignals:

    def test_range_changed_on_set_range(self, qtbot, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        with qtbot.waitSignal(span.range_changed, timeout=1000):
            span.set_range(SciQLopPlotRange(3.0, 7.0))


class TestGraphSignals:

    def test_data_changed_on_set_data(self, qtbot, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        new_y = np.cos(x).astype(np.float64)
        with qtbot.waitSignal(g.data_changed, timeout=1000):
            g.set_data(x, new_y)


class TestPanelSignals:

    def test_plot_list_changed_on_add(self, qtbot, panel):
        from SciQLopPlots import SciQLopPlot
        plot = SciQLopPlot()
        with qtbot.waitSignal(panel.plot_list_changed, timeout=1000):
            panel.add_plot(plot)

    def test_plot_list_changed_on_remove(self, qtbot, panel):
        from SciQLopPlots import SciQLopPlot
        plot = SciQLopPlot()
        panel.add_plot(plot)
        with qtbot.waitSignal(panel.plot_list_changed, timeout=1000):
            panel.remove_plot(plot)

    def test_plot_list_changed_on_clear(self, qtbot, panel):
        from SciQLopPlots import SciQLopPlot
        panel.add_plot(SciQLopPlot())
        panel.add_plot(SciQLopPlot())
        with qtbot.waitSignal(panel.plot_list_changed, timeout=1000):
            panel.clear()
