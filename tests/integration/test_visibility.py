"""Hide/show visibility from the inspector delegates and component API."""
import numpy as np
import pytest
from PySide6.QtWidgets import QCheckBox

from SciQLopPlots import DelegateRegistry
from conftest import process_events


def _make_delegate(target, qtbot):
    d = DelegateRegistry.instance().create_delegate(target, None)
    assert d is not None
    qtbot.addWidget(d)
    return d


def _visible_checkbox(delegate):
    boxes = [c for c in delegate.findChildren(QCheckBox) if c.text() == "Visible"]
    assert len(boxes) == 1, f"expected one 'Visible' checkbox, got {len(boxes)}"
    return boxes[0]


class TestComponentDelegateVisible:
    def test_checkbox_reflects_initial_state(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        assert _visible_checkbox(d).isChecked() is True

    def test_checkbox_toggles_component(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        box = _visible_checkbox(d)
        box.setChecked(False)
        process_events()
        assert comp.visible() is False
        box.setChecked(True)
        process_events()
        assert comp.visible() is True

    def test_external_change_updates_checkbox(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        box = _visible_checkbox(d)
        comp.set_visible(False)
        process_events()
        assert box.isChecked() is False


class TestColorMapDelegateVisible:
    def test_checkbox_toggles_colormap(self, plot, sample_colormap_data, qtbot):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        d = _make_delegate(cmap, qtbot)
        box = _visible_checkbox(d)
        assert box.isChecked() is True
        box.setChecked(False)
        process_events()
        assert cmap.visible() is False

    def test_external_change_updates_checkbox(self, plot, sample_colormap_data, qtbot):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        d = _make_delegate(cmap, qtbot)
        box = _visible_checkbox(d)
        cmap.set_visible(False)
        process_events()
        assert box.isChecked() is False


class TestVisibilityContract:
    def test_component_round_trip(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        assert comp.visible() is True
        comp.set_visible(False)
        assert comp.visible() is False
        comp.set_visible(True)
        assert comp.visible() is True

    def test_component_emits_signal(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        seen = []
        comp.visible_changed.connect(lambda v: seen.append(v))
        comp.set_visible(False)
        assert seen == [False]

    def test_multicomponent_independent(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        g = plot.line(x, y, labels=["a", "b", "c"])
        process_events()
        comps = g.components()
        assert len(comps) == 3
        comps[1].set_visible(False)
        assert comps[0].visible() is True
        assert comps[1].visible() is False
        assert comps[2].visible() is True

    def test_colormap_round_trip(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        assert cmap.visible() is True
        cmap.set_visible(False)
        assert cmap.visible() is False
        cmap.set_visible(True)
        assert cmap.visible() is True
