"""Moving the inspector's selection between a graph and its components.

A graph's selection is all its components. The tree is single-selection: a click on a new
node deselects the old one, and that must happen before the new one is selected.
"""
import numpy as np
import pytest
from PySide6.QtCore import QItemSelectionModel, QModelIndex

from SciQLopPlots import InspectorView, PlotsModel, PlotsTreeView, SciQLopMultiPlotPanel
from conftest import process_events

X = np.linspace(0.0, 10.0, 50)
THREE = np.column_stack([np.sin(X), np.cos(X), X])


def _row(model, parent, obj):
    return next(idx for idx in (model.index(r, 0, parent) for r in range(model.rowCount(parent)))
                if PlotsModel.object(idx) is obj)


class Inspected:
    def __init__(self, qtbot):
        self.panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(self.panel)
        self.inspector = InspectorView()
        qtbot.addWidget(self.inspector)
        self.plot, self.graph = self.panel.line(X, THREE, labels=["a", "b", "c"])
        self.other = self.plot.plot(X, np.sin(X), labels=["d"])
        process_events()
        self.tree = self.inspector.findChild(PlotsTreeView)

    def index(self, obj):
        model = PlotsModel.instance()
        plot = _row(model, _row(model, QModelIndex(), self.panel), self.plot)
        if obj in (self.graph, self.other):
            return _row(model, plot, obj)
        return _row(model, _row(model, plot, self.graph), obj)

    def click(self, obj):
        self.tree.selectionModel().setCurrentIndex(
            self.index(obj), QItemSelectionModel.SelectionFlag.ClearAndSelect)
        process_events()


def _selection(graph):
    return [c.selected() for c in graph.components()]


@pytest.fixture
def inspected(qtbot):
    return Inspected(qtbot)


def test_graph_node_then_component_node_selects_only_that_component(inspected):
    inspected.click(inspected.graph)
    inspected.click(inspected.graph.component(1))
    assert _selection(inspected.graph) == [False, True, False]


def test_component_node_then_graph_node_selects_the_whole_graph(inspected):
    inspected.click(inspected.graph.component(1))
    inspected.click(inspected.graph)
    assert _selection(inspected.graph) == [True, True, True]


def test_graph_node_then_another_graph_node_moves_the_selection(inspected):
    inspected.click(inspected.graph)
    inspected.click(inspected.other)
    assert (_selection(inspected.graph), _selection(inspected.other)) == ([False] * 3, [True])
