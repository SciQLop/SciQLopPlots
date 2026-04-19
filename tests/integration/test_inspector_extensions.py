"""Tests for the InspectorExtension API.

Covers the per-graph extension triad (add/remove/list), signal emission,
lifetime via QObject parenting, Python subclassing, and delegate rendering.
"""
import numpy as np
import pytest
from PySide6.QtWidgets import QGroupBox, QLabel, QWidget

from SciQLopPlots import (
    DelegateRegistry,
    GraphType,
    InspectorExtension,
    SciQLopColorMap,
)


def _make_delegate(target, qtbot):
    d = DelegateRegistry.instance().create_delegate(target, None)
    assert d is not None
    qtbot.addWidget(d)
    return d
from conftest import process_events, force_gc


class LabelExtension(InspectorExtension):
    """Minimal extension producing a QLabel. Tracks build_widget calls."""

    def __init__(self, title="Parameters", prio=0, text="ext-body"):
        super().__init__()
        self._title = title
        self._prio = prio
        self._text = text
        self.build_calls = 0

    def section_title(self):
        return self._title

    def priority(self):
        return self._prio

    def build_widget(self, parent):
        self.build_calls += 1
        return QLabel(self._text, parent)


@pytest.fixture
def line_graph(plot, sample_data):
    x, y = sample_data
    g = plot.plot(x, y, graph_type=GraphType.Line)
    process_events()
    return g


class TestExtensionTriad:
    def test_empty_on_fresh_graph(self, line_graph):
        assert line_graph.inspector_extensions() == []

    def test_add_appears_in_list(self, line_graph):
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        exts = line_graph.inspector_extensions()
        assert len(exts) == 1
        assert exts[0] is ext

    def test_add_is_idempotent(self, line_graph):
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        line_graph.add_inspector_extension(ext)
        assert len(line_graph.inspector_extensions()) == 1

    def test_remove(self, line_graph):
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        line_graph.remove_inspector_extension(ext)
        assert line_graph.inspector_extensions() == []

    def test_remove_unknown_noop(self, line_graph):
        ext = LabelExtension()
        line_graph.remove_inspector_extension(ext)
        assert line_graph.inspector_extensions() == []

    def test_multiple_extensions_preserve_order(self, line_graph):
        a, b, c = LabelExtension("A"), LabelExtension("B"), LabelExtension("C")
        for e in (a, b, c):
            line_graph.add_inspector_extension(e)
        assert line_graph.inspector_extensions() == [a, b, c]


class TestSignal:
    def test_signal_emits_on_add(self, line_graph, qtbot):
        with qtbot.waitSignal(line_graph.inspector_extensions_changed, timeout=500):
            line_graph.add_inspector_extension(LabelExtension())

    def test_signal_emits_on_remove(self, line_graph, qtbot):
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        with qtbot.waitSignal(line_graph.inspector_extensions_changed, timeout=500):
            line_graph.remove_inspector_extension(ext)


class TestLifetime:
    def test_extension_reparented_to_graph(self, line_graph):
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        assert ext.parent() is line_graph

    def test_extension_autoremoved_when_destroyed(self, line_graph, qtbot):
        import shiboken6
        ext = LabelExtension()
        line_graph.add_inspector_extension(ext)
        with qtbot.waitSignal(line_graph.inspector_extensions_changed, timeout=500):
            shiboken6.delete(ext)
        assert line_graph.inspector_extensions() == []


class TestDelegateRendering:
    def test_graph_delegate_renders_extension(self, line_graph, qtbot):
        ext = LabelExtension(title="My Params", text="hello-body")
        line_graph.add_inspector_extension(ext)

        delegate = _make_delegate(line_graph, qtbot)
        process_events()

        groups = delegate.findChildren(QGroupBox)
        ext_groups = [g for g in groups if g.objectName().startswith("__inspector_ext__")]
        assert len(ext_groups) == 1
        assert ext_groups[0].title() == "My Params"
        assert ext.build_calls == 1
        # Body widget reachable as a descendant label.
        labels = [w for w in ext_groups[0].findChildren(QLabel) if w.text() == "hello-body"]
        assert labels, "extension body widget not found under group"

    def test_delegate_renders_nothing_without_extensions(self, line_graph, qtbot):
        delegate = _make_delegate(line_graph, qtbot)
        process_events()
        groups = [g for g in delegate.findChildren(QGroupBox)
                  if g.objectName().startswith("__inspector_ext__")]
        assert groups == []

    def test_delegate_sorts_by_priority(self, line_graph, qtbot):
        low = LabelExtension(title="Low", prio=0)
        high = LabelExtension(title="High", prio=100)
        # Insert high first — delegate must still render Low before High.
        line_graph.add_inspector_extension(high)
        line_graph.add_inspector_extension(low)

        delegate = _make_delegate(line_graph, qtbot)
        process_events()

        titles = [g.title() for g in delegate.findChildren(QGroupBox)
                  if g.objectName().startswith("__inspector_ext__")]
        assert titles == ["Low", "High"]

    def test_invalidated_rebuilds_section(self, line_graph, qtbot):
        ext = LabelExtension(title="X")
        line_graph.add_inspector_extension(ext)
        delegate = _make_delegate(line_graph, qtbot)
        process_events()
        assert ext.build_calls == 1
        ext.invalidated.emit()
        process_events()
        assert ext.build_calls == 2

    def test_add_after_delegate_built_appears(self, line_graph, qtbot):
        delegate = _make_delegate(line_graph, qtbot)
        process_events()

        ext = LabelExtension(title="LateAdd")
        line_graph.add_inspector_extension(ext)
        process_events()

        titles = [g.title() for g in delegate.findChildren(QGroupBox)
                  if g.objectName().startswith("__inspector_ext__")]
        assert titles == ["LateAdd"]

    def test_colormap_delegate_also_renders(self, qtbot, sample_colormap_data):
        from SciQLopPlots import SciQLopPlot
        plot = SciQLopPlot()
        qtbot.addWidget(plot)
        x, y, z = sample_colormap_data
        cmap = plot.plot(x, y, z, graph_type=GraphType.ColorMap)
        assert isinstance(cmap, SciQLopColorMap)

        ext = LabelExtension(title="CMapParams")
        cmap.add_inspector_extension(ext)

        delegate = _make_delegate(cmap, qtbot)
        process_events()

        titles = [g.title() for g in delegate.findChildren(QGroupBox)
                  if g.objectName().startswith("__inspector_ext__")]
        assert titles == ["CMapParams"]
