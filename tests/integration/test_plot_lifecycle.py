"""Tests that plot objects can be created and destroyed without crashing."""
import pytest
import gc
from conftest import force_gc, process_events

from SciQLopPlots import (
    SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel,
    SciQLopPlotRange,
)


class TestPlotCreation:

    def test_create_basic_plot(self, qtbot):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        assert p is not None

    def test_create_time_series_plot(self, qtbot):
        p = SciQLopTimeSeriesPlot()
        qtbot.addWidget(p)
        assert p is not None
        assert p.time_axis() is not None

    def test_create_multi_plot_panel(self, qtbot):
        p = SciQLopMultiPlotPanel()
        qtbot.addWidget(p)
        assert p is not None
        assert p.empty()
        assert p.size() == 0

    def test_create_panel_with_kwargs(self, qtbot):
        from PySide6.QtCore import Qt
        p = SciQLopMultiPlotPanel(
            synchronize_x=True,
            synchronize_time=True,
            orientation=Qt.Vertical,
        )
        qtbot.addWidget(p)
        assert p is not None


class TestPlotDestruction:

    def test_delete_empty_plot(self, qtbot):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        del p
        force_gc()

    def test_delete_plot_with_graph(self, qtbot, sample_data):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        x, y = sample_data
        g = p.line(x, y)
        assert g is not None
        del g
        del p
        force_gc()

    def test_delete_panel_with_plots(self, qtbot, sample_data):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        panel.line(x, y)
        panel.line(x, y)
        assert panel.size() == 2
        del panel
        force_gc()

    def test_clear_panel(self, qtbot, sample_data):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        panel.line(x, y)
        panel.line(x, y)
        assert panel.size() == 2
        panel.clear()
        assert panel.empty()

    def test_remove_plot_from_panel(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        plot = SciQLopPlot()
        panel.add_plot(plot)
        assert panel.size() == 1
        panel.remove_plot(plot)
        assert panel.empty()


class TestStressLifecycle:

    def test_rapid_create_destroy(self, qtbot):
        """Create and destroy many plots rapidly."""
        for _ in range(50):
            p = SciQLopPlot()
            del p
        force_gc()

    def test_rapid_panel_create_destroy(self, qtbot, sample_data):
        """Create panels with plots, destroy them rapidly."""
        x, y = sample_data
        for _ in range(20):
            panel = SciQLopMultiPlotPanel()
            panel.line(x, y)
            panel.line(x, y)
            del panel
        force_gc()
