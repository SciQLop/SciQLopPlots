"""Tests for ownership, destruction order, and memory safety.

These tests verify no segfault occurs during object lifecycle edge cases.
They use force_gc() to trigger garbage collection and process Qt events.
"""
import pytest
import gc
import numpy as np

from SciQLopPlots import (
    SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
    SciQLopPlotRange, SciQLopVerticalSpan,
    MultiPlotsVerticalSpan,
)
from PySide6.QtGui import QColor
from conftest import force_gc


class TestGraphOwnership:

    def test_delete_graph_ref_plot_survives(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        del g
        force_gc()
        # Plot should still be alive and functional
        assert plot.plottables() is not None

    def test_delete_plot_with_graph(self, qtbot, sample_data):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        x, y = sample_data
        g = p.line(x, y)
        del p
        force_gc()
        # g reference is now dangling — just don't crash


class TestSpanOwnership:

    def test_delete_span_plot_survives(self, plot):
        span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.0, 1.0))
        del span
        force_gc()
        assert plot is not None

    def test_delete_plot_with_span(self, qtbot):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        span = SciQLopVerticalSpan(p, SciQLopPlotRange(0.0, 1.0))
        del p
        force_gc()
        # span reference is now dangling — just don't crash


class TestPanelOwnership:

    def test_delete_panel_cleans_children(self, qtbot, sample_data):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        panel.line(x, y)
        panel.line(x, y)
        panel.line(x, y)
        del panel
        force_gc()

    def test_add_remove_plots_rapidly(self, panel):
        for _ in range(50):
            p = SciQLopPlot()
            panel.add_plot(p)
            panel.remove_plot(p)
        force_gc()
        assert panel.empty()

    def test_clear_and_repopulate(self, panel, sample_data):
        x, y = sample_data
        for _ in range(5):
            panel.line(x, y)
        assert panel.size() == 5
        panel.clear()
        assert panel.empty()
        for _ in range(3):
            panel.line(x, y)
        assert panel.size() == 3


class TestMultiSpanOwnership:

    def test_delete_multi_span(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0), QColor(100, 100, 200),
        )
        del span
        force_gc()

    def test_clear_panel_with_multi_span(self, panel, sample_data):
        x, y = sample_data
        panel.line(x, y)
        span = MultiPlotsVerticalSpan(
            panel, SciQLopPlotRange(0.0, 1.0),
        )
        panel.clear()
        force_gc()


class TestStressMemory:

    def test_mass_create_destroy_plots(self, qtbot):
        for _ in range(100):
            p = SciQLopPlot()
            del p
        force_gc()

    def test_mass_create_destroy_with_graphs(self, qtbot, sample_data):
        x, y = sample_data
        for _ in range(50):
            p = SciQLopPlot()
            p.line(x, y)
            p.line(x, y)
            del p
        force_gc()

    def test_mass_panel_lifecycle(self, qtbot, sample_data):
        x, y = sample_data
        for _ in range(20):
            panel = SciQLopMultiPlotPanel()
            panel.line(x, y)
            panel.line(x, y)
            panel.clear()
            del panel
        force_gc()

    def test_mass_spans(self, plot):
        spans = []
        for i in range(50):
            spans.append(SciQLopVerticalSpan(
                plot, SciQLopPlotRange(float(i), float(i + 1)),
            ))
        del spans
        force_gc()
