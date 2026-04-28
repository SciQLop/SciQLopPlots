"""Hardening tests: P0/P1 crash and correctness reproducers.

Each test targets a specific issue identified in the deep review.
"""
import gc
import sys
import numpy as np
import pytest
import shiboken6
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (
    GraphType,
    InspectorExtension,
    SciQLopMultiPlotPanel,
    SciQLopPlot,
    SciQLopPlotRange,
)
from conftest import process_events, force_gc


class DummyExtension(InspectorExtension):
    def section_title(self):
        return "Dummy"

    def priority(self):
        return 0

    def build_widget(self, parent):
        return None


class TestP0_1_HolderDestructionUAF:
    """InspectorExtensionHolder destruction safety."""

    def test_panel_with_extension_destroyed_safely(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        ext = DummyExtension()
        panel.add_inspector_extension(ext)
        assert ext.parent() is panel
        shiboken6.delete(panel)
        process_events()

    def test_plot_with_extension_destroyed_safely(self, qtbot):
        plot = SciQLopPlot()
        qtbot.addWidget(plot)
        ext = DummyExtension()
        plot.add_inspector_extension(ext)
        shiboken6.delete(plot)
        process_events()

    def test_plottable_with_extension_owner_destroyed(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = np.sin(x).astype(np.float64)
        plot_and_graph = panel.plot(x, y, graph_type=GraphType.Line)
        graph = plot_and_graph[1] if isinstance(plot_and_graph, tuple) else plot_and_graph
        ext = DummyExtension()
        graph.add_inspector_extension(ext)
        shiboken6.delete(panel)
        process_events()


class TestP0_2_PlotRangeBoundsCheck:
    """SciQLopPlotRange.__getitem__/__setitem__ bounds validation."""

    def test_getitem_valid_indices(self):
        r = SciQLopPlotRange(1.0, 2.0)
        assert r[0] == 1.0
        assert r[1] == 2.0

    def test_getitem_oob_raises(self):
        r = SciQLopPlotRange(1.0, 2.0)
        with pytest.raises(IndexError):
            r[2]

    def test_getitem_large_oob_raises(self):
        r = SciQLopPlotRange(1.0, 2.0)
        with pytest.raises(IndexError):
            r[999]

    def test_getitem_negative_oob_raises(self):
        r = SciQLopPlotRange(1.0, 2.0)
        with pytest.raises(IndexError):
            r[-3]

    def test_getitem_negative_valid(self):
        r = SciQLopPlotRange(1.0, 2.0)
        assert r[-1] == 2.0
        assert r[-2] == 1.0

    def test_setitem_oob_raises(self):
        r = SciQLopPlotRange(1.0, 2.0)
        with pytest.raises(IndexError):
            r[5] = 3.0

    def test_setitem_negative_oob_raises(self):
        r = SciQLopPlotRange(1.0, 2.0)
        with pytest.raises(IndexError):
            r[-3] = 3.0

    def test_setitem_valid(self):
        r = SciQLopPlotRange(1.0, 2.0)
        r[0] = 5.0
        r[1] = 6.0
        assert r[0] == 5.0
        assert r[1] == 6.0

    def test_setitem_negative_valid(self):
        r = SciQLopPlotRange(1.0, 2.0)
        r[-1] = 9.0
        assert r[1] == 9.0


class TestP1_3_DatetimeRefLeak:
    """datetime_start/stop must not leak PyObjects."""

    def test_datetime_start_no_leak(self):
        r = SciQLopPlotRange(1000000.0, 2000000.0)
        gc.collect()
        before = sys.getrefcount(1.0)  # baseline
        for _ in range(1000):
            _ = r.datetime_start()
        gc.collect()
        after = sys.getrefcount(1.0)
        # Allow small variance but not 1000+ leaked refs
        assert after - before < 50

    def test_datetime_stop_no_leak(self):
        r = SciQLopPlotRange(1000000.0, 2000000.0)
        gc.collect()
        before = sys.getrefcount(1.0)
        for _ in range(1000):
            _ = r.datetime_stop()
        gc.collect()
        after = sys.getrefcount(1.0)
        assert after - before < 50
