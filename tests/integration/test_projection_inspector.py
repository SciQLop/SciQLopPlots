"""Projection plots in the inspector: their panes are tree nodes with the plot properties,
and the projection has its own delegate (axis linking, square panes). Axis linking has a
third mode, the same scale on every axis, so a circle stays round."""
import numpy as np
import pytest
from PySide6.QtCore import QModelIndex
from PySide6.QtWidgets import QCheckBox, QComboBox

from SciQLopPlots import (DelegateRegistry, PlotsModel, PlotType, ProjectionAxisLinking,
                          SciQLopNDProjectionPlot)
from conftest import process_events

MAGNETOSPHERIC = [(0, 1), (0, 2), (1, 2)]


def _children(model, idx):
    return [PlotsModel.object(model.index(r, 0, idx)) for r in range(model.rowCount(idx))]


def _find(model, parent_idx, target):
    for r in range(model.rowCount(parent_idx)):
        idx = model.index(r, 0, parent_idx)
        if PlotsModel.object(idx) is target:
            return idx
    raise AssertionError(f"{target!r} not found")


@pytest.fixture
def projection(panel):
    proj = panel.create_plot(-1, PlotType.Projections)
    t = np.linspace(0, 1, 100)
    graph = proj.parametric_curve([t, np.cos(6 * t), np.sin(6 * t), t])
    process_events()
    return proj, graph


def _node(panel, proj):
    model = PlotsModel.instance()
    return model, _find(model, _find(model, QModelIndex(), panel), proj)


def test_the_panes_are_nodes_of_the_projection(panel, projection):
    proj, graph = projection
    model, node = _node(panel, proj)
    children = _children(model, node)
    panes = [proj.subplot(i) for i in range(proj.subplot_count())]
    assert all(any(c is p for c in children) for p in panes)
    assert any(c is graph for c in children)


def test_a_pane_is_named_after_its_dimensions(panel, projection):
    proj, _ = projection
    assert [proj.subplot(i).objectName() for i in range(3)] == ["Pane 1", "Pane 2", "Pane 3"]
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    proj.set_axis_labels(["X", "Y", "Z"])
    process_events()
    model, node = _node(panel, proj)
    names = [model.data(_find(model, node, proj.subplot(i))) for i in range(3)]
    assert names == ["X-Y", "X-Z", "Y-Z"]


def test_a_pane_shows_its_axes_not_the_trajectory_pieces(panel, projection):
    proj, graph = projection
    model, node = _node(panel, proj)
    pane = proj.subplot(0)
    children = _children(model, _find(model, node, pane))
    assert any(c is pane.x_axis() for c in children)
    assert any(c is pane.y_axis() for c in children)
    assert len(children) == 2, children    # the trajectory is the projection's, not the pane's


def test_a_pane_cannot_be_deleted_or_dragged_out(panel, projection):
    proj, _ = projection
    model, node = _node(panel, proj)
    pane_idx = _find(model, node, proj.subplot(0))
    assert model.removeRows(pane_idx.row(), 1, node) is False
    assert proj.subplot_count() == 3
    from PySide6.QtCore import Qt
    assert not (model.flags(pane_idx) & Qt.ItemIsDragEnabled)


def test_a_pane_gets_the_plot_delegate(projection, qtbot):
    proj, _ = projection
    delegate = DelegateRegistry.instance().create_delegate(proj.subplot(0), None)
    assert delegate is not None
    qtbot.addWidget(delegate)


@pytest.fixture
def delegate(projection, qtbot):
    proj, _ = projection
    d = DelegateRegistry.instance().create_delegate(proj, None)
    assert d is not None
    qtbot.addWidget(d)
    return d


def _named(delegate, kind, name):
    widget = delegate.findChild(kind, name)
    assert widget is not None, f"no {kind.__name__} named {name!r}"
    return widget


def test_the_delegate_sets_the_axis_linking(projection, delegate):
    proj, _ = projection
    combo = _named(delegate, QComboBox, "axis_linking")
    assert combo.currentText() == "Linked by dimension"
    combo.setCurrentText("Independent")
    assert proj.axis_linking() == ProjectionAxisLinking.Independent
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    assert combo.currentText() == "Same scale on all axes"


def test_the_delegate_sets_square_panes(projection, delegate):
    proj, _ = projection
    box = _named(delegate, QCheckBox, "square_panes")
    box.setChecked(True)
    assert proj.equal_aspect_ratio()
    proj.set_equal_aspect_ratio(False)
    assert not box.isChecked()


def test_same_scale_locks_square_panes_on(projection, delegate):
    proj, _ = projection
    box = _named(delegate, QCheckBox, "square_panes")
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    assert proj.equal_aspect_ratio() and box.isChecked() and not box.isEnabled()
    proj.set_equal_aspect_ratio(False)
    assert proj.equal_aspect_ratio()      # same scale needs square panes
    proj.set_axis_linking(ProjectionAxisLinking.ByDimension)
    assert box.isEnabled()


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    p.set_dimension_pairs(MAGNETOSPHERIC)
    return p


def _span(axis):
    r = axis.range()
    return r.stop() - r.start()


def test_linked_axes_maps_onto_the_linking_mode(proj):
    assert proj.axis_linking() == ProjectionAxisLinking.ByDimension and proj.linked_axes()
    proj.set_linked_axes(False)
    assert proj.axis_linking() == ProjectionAxisLinking.Independent
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    assert proj.linked_axes()


def test_independent_axes_do_not_follow_each_other(proj):
    proj.set_axis_linking(ProjectionAxisLinking.Independent)
    before = proj.subplot(1).x_axis().range()
    proj.subplot(0).x_axis().set_range(3, 4)
    after = proj.subplot(1).x_axis().range()
    assert (after.start(), after.stop()) == (before.start(), before.stop())


def test_same_scale_gives_every_axis_the_same_span(proj):
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    proj.subplot(2).y_axis().set_range(10, 12)     # Z
    axes = [a for i in range(3) for a in (proj.subplot(i).x_axis(), proj.subplot(i).y_axis())]
    assert [_span(a) for a in axes] == pytest.approx([2] * 6)
    z = proj.subplot(1).y_axis().range()           # Z on the X-Z pane still follows Z
    assert (z.start(), z.stop()) == (10, 12)


def test_same_scale_keeps_each_axis_centre(proj):
    proj.set_axis_linking(ProjectionAxisLinking.Independent)
    proj.subplot(0).x_axis().set_range(-1, 1)      # X around 0
    proj.subplot(0).y_axis().set_range(9, 11)      # Y around 10
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    proj.subplot(0).x_axis().set_range(-2, 2)
    y = proj.subplot(0).y_axis().range()
    assert y.center() == pytest.approx(10) and _span(proj.subplot(0).y_axis()) == pytest.approx(4)


def _holds(proj, dims):
    for i, (a, b) in enumerate(MAGNETOSPHERIC):
        pane = proj.subplot(i)
        for axis, values in ((pane.x_axis(), dims[a]), (pane.y_axis(), dims[b])):
            r = axis.range()
            if not (r.start() <= values.min() and values.max() <= r.stop()):
                return False
    return True


def test_same_scale_fits_the_whole_trajectory(panel, qtbot):
    proj = panel.create_plot(-1, PlotType.Projections)    # a panel's projection fits its first data
    proj.set_dimension_pairs(MAGNETOSPHERIC)
    proj.set_axis_linking(ProjectionAxisLinking.SameScale)
    t = np.linspace(0, 1, 200)
    dims = (10 * np.cos(6 * t), 2 * np.sin(6 * t), 0.5 * t)
    proj.parametric_curve([t, *dims])
    qtbot.waitUntil(lambda: _holds(proj, dims), timeout=2000)
    spans = [_span(a) for i in range(3) for a in (proj.subplot(i).x_axis(), proj.subplot(i).y_axis())]
    assert spans == pytest.approx([spans[0]] * 6)


def test_rescale_axes_fits_every_pane(proj):
    t = np.linspace(0, 1, 200)
    dims = (10 * np.cos(6 * t), 2 * np.sin(6 * t), 0.5 * t)
    g = proj.parametric_curve([t, *dims])
    for _ in range(50):
        process_events()
        if not g.busy():
            break
    process_events()
    proj.rescale_axes()
    assert _holds(proj, dims)
