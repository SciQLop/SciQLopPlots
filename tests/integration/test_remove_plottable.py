"""Tests for SciQLopPlotInterface.remove_plottable()."""
import pytest
import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel
from conftest import force_gc, process_events


class TestRemovePlottableByRef:

    def test_remove_line_graph(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        g = p.line(x, y)
        assert len(p.plottables()) == 1

        p.remove_plottable(g)
        assert len(p.plottables()) == 0

    def test_remove_one_of_many(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        g1 = p.line(x, y, labels=["a"])
        g2 = p.line(x, y, labels=["b"])
        g3 = p.line(x, y, labels=["c"])
        assert len(p.plottables()) == 3

        p.remove_plottable(g2)
        remaining = p.plottables()
        assert len(remaining) == 2
        assert g1 in remaining
        assert g3 in remaining

    def test_remove_none_is_safe(self, qtbot):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        p.remove_plottable(None)
        assert len(p.plottables()) == 0


class TestRemovePlottableByIndex:

    def test_remove_first(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        p.line(x, y, labels=["a"])
        g2 = p.line(x, y, labels=["b"])
        assert len(p.plottables()) == 2

        p.remove_plottable(0)
        remaining = p.plottables()
        assert len(remaining) == 1
        assert remaining[0] is g2


class TestRemovePlottableByName:

    def test_remove_by_object_name(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        g = p.line(x, y)
        g.name = "my_graph"
        assert len(p.plottables()) == 1

        p.remove_plottable("my_graph")
        assert len(p.plottables()) == 0

    def test_remove_nonexistent_name_is_safe(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        p.line(x, y)
        assert len(p.plottables()) == 1

        p.remove_plottable("does_not_exist")
        assert len(p.plottables()) == 1


class TestRemovePlottableSignal:

    def test_graph_list_changed_emitted(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        g = p.line(x, y)

        with qtbot.waitSignal(p.graph_list_changed, timeout=1000):
            p.remove_plottable(g)


class TestRemovePlottableFromPanel:

    def test_remove_from_panel_plot(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        plot, graph = panel.line(x, y)
        assert len(plot.plottables()) == 1

        plot.remove_plottable(graph)
        assert len(plot.plottables()) == 0
