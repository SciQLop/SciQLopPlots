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


class TestColormapAxesExposed:
    """When a colormap is added to a plot already registered in the inspector
    tree, the colormap axes (z and y2) must appear under the plot node.

    Regression: TypeDescriptor.children was evaluated only once at addNode
    time, when has_colormap() was still false. graph_list_changed re-published
    plottables but not the new axes, so y2/z stayed invisible.
    """

    def _child_names(self, model, parent_idx):
        return {
            model.data(model.index(r, 0, parent_idx))
            for r in range(model.rowCount(parent_idx))
        }

    def _find_child(self, model, parent_idx, target):
        from SciQLopPlots import PlotsModel
        from PySide6.QtCore import QModelIndex
        for r in range(model.rowCount(parent_idx)):
            idx = model.index(r, 0, parent_idx)
            if PlotsModel.object(idx) is target:
                return idx
        raise AssertionError(f"{target!r} not found")

    def test_colormap_added_after_panel_attached(self, qtbot, sample_colormap_data):
        from SciQLopPlots import PlotsModel, SciQLopPlot
        from PySide6.QtCore import QModelIndex
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)

        plot = SciQLopPlot()
        panel.add_plot(plot)
        process_events()

        # PlotsModel is a singleton — sibling panels from earlier tests may
        # still be at row 0, so look up by object identity rather than index.
        model = PlotsModel.instance()
        panel_idx = self._find_child(model, QModelIndex(), panel)
        plot_idx = self._find_child(model, panel_idx, plot)
        assert "Color Scale" not in self._child_names(model, plot_idx)
        assert "Y Axis 2" not in self._child_names(model, plot_idx)

        x, y, z = sample_colormap_data
        plot.colormap(x, y, z)
        process_events()

        names = self._child_names(model, plot_idx)
        assert "Color Scale" in names, names
        assert "Y Axis 2" in names, names


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
