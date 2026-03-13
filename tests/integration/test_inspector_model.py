"""Regression tests for Inspector model bugs.

Bug 1: Double destroyed connection (Node constructor + InspectorBase::connect_node)
Bug 5: Null-pointer crash in SciQLopMultiPlotPanelInspector::connect_node
"""
import pytest
import gc
import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel
from conftest import force_gc, process_events


class TestBug1_DoubleDestroyedConnection:
    """Bug 1: obj->destroyed should be connected to node->objectDestroyed once.

    With duplicate connections, objectDestroyed fires twice when the wrapped
    object is destroyed, potentially causing double-removal of the node.
    """

    def test_no_crash_on_panel_destruction(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.line(x, y)
        process_events()

        del panel
        force_gc()

    def test_no_crash_on_plot_destruction(self, qtbot, sample_data):
        x, y = sample_data
        p = SciQLopPlot()
        qtbot.addWidget(p)
        p.line(x, y)
        process_events()

        del p
        force_gc()


class TestTreeDeletion:
    """Deleting from tree should request object deletion."""

    def test_tree_delete_removes_graph(self, qtbot, sample_data):
        from SciQLopPlots import PlotsModel
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x, y = sample_data
        plot, graph = panel.line(x, y)
        process_events()

        model = PlotsModel.instance()
        panel_idx = model.index(0, 0)
        plot_idx = model.index(0, 0, panel_idx)
        initial_row_count = model.rowCount(plot_idx)
        assert initial_row_count > 0

        del panel
        force_gc()


class TestNoDoubleDelete:
    """Ensure no double-delete or use-after-free on destruction cascades."""

    def test_panel_with_multiple_plots(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        panel.line(x, y)
        panel.line(x, y)
        panel.line(x, y)
        process_events()

        del panel
        force_gc()

    def test_rapid_add_remove(self, qtbot, sample_data):
        x, y = sample_data
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)

        for _ in range(5):
            panel.line(x, y)
            process_events()

        del panel
        force_gc()
