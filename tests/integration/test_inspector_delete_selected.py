"""Issue #116: deleting a graph component that is current in the inspector.

Removing the row makes the tree move its current index to the parent graph.
That selects the graph, which asks each of its components if it is selected,
including the one being destroyed.
"""
from PySide6.QtCore import QCoreApplication, QEvent, QItemSelectionModel, QModelIndex

from SciQLopPlots import SciQLopMultiPlotPanel, InspectorView, PlotsModel, PlotsTreeView
from conftest import force_gc, process_events


def _find_row(model, parent_idx, obj):
    for r in range(model.rowCount(parent_idx)):
        idx = model.index(r, 0, parent_idx)
        if PlotsModel.object(idx) is obj:
            return idx
    raise AssertionError(f"{obj} not found in the inspector model")


def _flush_deferred_deletes():
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)


def _make_component_current(qtbot, sample_data):
    x, y = sample_data
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    inspector = InspectorView()
    qtbot.addWidget(inspector)
    plot, graph = panel.line(x, y)
    process_events()

    model = PlotsModel.instance()
    panel_idx = _find_row(model, QModelIndex(), panel)
    plot_idx = _find_row(model, panel_idx, plot)
    graph_idx = _find_row(model, plot_idx, graph)
    component = graph.component(0)
    component_idx = _find_row(model, graph_idx, component)

    tree = inspector.findChild(PlotsTreeView)
    tree.selectionModel().setCurrentIndex(
        component_idx, QItemSelectionModel.SelectionFlag.ClearAndSelect)
    process_events()
    return panel, inspector, graph, component


def test_delete_current_component_does_not_crash(qtbot, sample_data):
    panel, inspector, graph, component = _make_component_current(qtbot, sample_data)

    component.deleteLater()
    _flush_deferred_deletes()

    assert len(graph.components()) == 0
    del panel, inspector
    force_gc()


def test_graph_selected_after_component_deleted(qtbot, sample_data):
    panel, inspector, graph, component = _make_component_current(qtbot, sample_data)

    component.deleteLater()
    _flush_deferred_deletes()

    assert graph.selected() is False
    del panel, inspector
    force_gc()


def test_delete_graph_with_current_component_does_not_crash(qtbot, sample_data):
    panel, inspector, graph, component = _make_component_current(qtbot, sample_data)

    graph.deleteLater()
    _flush_deferred_deletes()

    del panel, inspector
    force_gc()


def test_delete_panel_with_current_component_does_not_crash(qtbot, sample_data):
    panel, inspector, graph, component = _make_component_current(qtbot, sample_data)

    panel.deleteLater()
    _flush_deferred_deletes()

    del inspector
    force_gc()
