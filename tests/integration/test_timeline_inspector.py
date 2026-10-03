"""Timelines in the inspector: a tree node and a properties delegate."""
from PySide6.QtCore import QModelIndex
from PySide6.QtWidgets import QCheckBox, QComboBox, QDoubleSpinBox, QSpinBox

from SciQLopPlots import DelegateRegistry, PlotsModel
from conftest import process_events


def _find_child(model, parent_idx, target):
    for r in range(model.rowCount(parent_idx)):
        idx = model.index(r, 0, parent_idx)
        if PlotsModel.object(idx) is target:
            return idx
    raise AssertionError(f"{target!r} not found")


def _timeline(panel):
    plot, tl = panel.add_timeline()
    tl.set_intervals([0, 10], [5, 15], lane=["A", "B"])
    process_events()
    return plot, tl


def test_timeline_has_a_named_tree_node(panel):
    plot, tl = _timeline(panel)
    model = PlotsModel.instance()
    plot_idx = _find_child(model, _find_child(model, QModelIndex(), panel), plot)
    node = _find_child(model, plot_idx, tl)
    assert model.data(node) == "timeline"


def test_a_named_timeline_shows_its_name(panel):
    plot, tl = _timeline(panel)
    tl.set_name("modes")
    process_events()
    model = PlotsModel.instance()
    plot_idx = _find_child(model, _find_child(model, QModelIndex(), panel), plot)
    assert model.data(_find_child(model, plot_idx, tl)) == "modes"


def _delegate(tl, qtbot):
    delegate = DelegateRegistry.instance().create_delegate(tl, None)
    assert delegate is not None
    qtbot.addWidget(delegate)
    return delegate


def _named(delegate, kind, name):
    widget = delegate.findChild(kind, name)
    assert widget is not None, f"no {kind.__name__} named {name!r}"
    return widget


def test_delegate_sets_the_style(panel, qtbot):
    _, tl = _timeline(panel)
    delegate = _delegate(tl, qtbot)
    combo = _named(delegate, QComboBox, "style")
    assert combo.currentText() == "wave"
    combo.setCurrentText("bars")
    assert tl.style == "bars"


def test_delegate_sets_the_lane_height(panel, qtbot):
    _, tl = _timeline(panel)
    delegate = _delegate(tl, qtbot)  # keep alive: it owns the spin box
    spin = _named(delegate, QSpinBox, "lane_height")
    assert spin.value() == tl.lane_height()
    spin.setValue(30)
    assert tl.lane_height() == 30


def test_delegate_sets_editing(panel, qtbot):
    _, tl = _timeline(panel)
    delegate = _delegate(tl, qtbot)
    _named(delegate, QCheckBox, "editable").setChecked(True)
    assert tl.editable
    _named(delegate, QCheckBox, "edit_mode_delete").setChecked(True)
    assert "delete" in tl.edit_modes


def test_delegate_sets_the_snap(panel, qtbot):
    _, tl = _timeline(panel)
    delegate = _delegate(tl, qtbot)
    combo = _named(delegate, QComboBox, "snap")
    combo.setCurrentText("edges")
    assert tl.snap_to == "edges"
    combo.setCurrentText("step")
    _named(delegate, QDoubleSpinBox, "snap_step").setValue(60)
    assert tl.snap_to == 60
    combo.setCurrentText("none")
    assert tl.snap_to is None


def test_delegate_sets_the_overlap_mode(panel, qtbot):
    _, tl = _timeline(panel)
    delegate = _delegate(tl, qtbot)
    combo = _named(delegate, QComboBox, "overlap")
    assert combo.currentText() == "draw"
    combo.setCurrentText("forbid")
    assert tl.overlap == "forbid"
