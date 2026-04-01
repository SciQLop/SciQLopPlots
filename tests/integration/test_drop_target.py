"""Reproducer tests for drag-and-drop targeting bug.

Bug: When dropping a product onto a plot inside a panel, it could end up
in the wrong plot due to:
1. widget_at() not mapping coordinates from panel space to container space
2. QCursor::pos() being unreliable on Wayland (used in ExistingPlot path)
3. Index space mismatch between index(pos) (raw widget index) and
   plot_at(index) (plot-only index)

Fix verified by testing widget_at() coordinate mapping, which is the
method now used in dropEvent to find the target plot.
"""
import pytest
import numpy as np
from PySide6.QtCore import QPointF
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlotInterface
from conftest import process_events


def _make_panel_with_plots(qtbot, n=3):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    panel.resize(400, 600)
    panel.show()
    x = np.linspace(0, 10, 50, dtype=np.float64)
    y = np.sin(x).astype(np.float64)
    for _ in range(n):
        panel.line(x, y)
    process_events()
    return panel


def _plot_center_in_panel(panel, plot_index):
    """Return the center of plot_index-th plot in panel-local coordinates."""
    plot = panel.plot_at(plot_index)
    center = plot.rect().center()
    return QPointF(plot.mapTo(panel, center))


class TestWidgetAtCoordinateMapping:
    """widget_at must return the correct widget after coordinate mapping.

    This is the method used by dropEvent to find the target plot.
    Before the fix, widget_at() did not map from panel coords to container
    coords, causing it to find the wrong plot.
    """

    def test_widget_at_returns_correct_plot(self, qtbot):
        panel = _make_panel_with_plots(qtbot, n=3)
        for i in range(3):
            pos = _plot_center_in_panel(panel, i)
            widget = panel.widget_at(pos)
            expected = panel.plot_at(i)
            assert widget == expected, (
                f"widget_at for plot {i} returned {widget}, expected {expected}"
            )

    def test_widget_at_returns_sciqlop_plot_interface(self, qtbot):
        """widget_at result must be castable to SciQLopPlotInterface."""
        panel = _make_panel_with_plots(qtbot, n=3)
        for i in range(3):
            pos = _plot_center_in_panel(panel, i)
            widget = panel.widget_at(pos)
            assert isinstance(widget, SciQLopPlotInterface), (
                f"widget_at for plot {i} returned {type(widget)}, "
                f"expected SciQLopPlotInterface"
            )

    def test_widget_at_with_many_plots(self, qtbot):
        """Verify targeting with enough plots to require scrolling."""
        panel = _make_panel_with_plots(qtbot, n=8)
        for i in range(8):
            pos = _plot_center_in_panel(panel, i)
            widget = panel.widget_at(pos)
            expected = panel.plot_at(i)
            assert widget == expected, (
                f"widget_at for plot {i} returned {widget}, expected {expected}"
            )

    def test_widget_at_after_scroll(self, qtbot):
        """After scrolling, widget_at must still find the correct plot."""
        panel = _make_panel_with_plots(qtbot, n=8)

        scroll_bar = panel.verticalScrollBar()
        scroll_bar.setValue(scroll_bar.maximum())
        process_events()

        last_idx = panel.plot_count() - 1
        pos = _plot_center_in_panel(panel, last_idx)
        widget = panel.widget_at(pos)
        expected = panel.plot_at(last_idx)
        assert widget == expected, (
            f"widget_at for last plot (idx {last_idx}) after scroll "
            f"returned {widget}, expected {expected}"
        )

    def test_index_matches_widget_at(self, qtbot):
        """index(pos) and widget_at(pos) must agree on which plot is at pos."""
        panel = _make_panel_with_plots(qtbot, n=3)
        for i in range(3):
            pos = _plot_center_in_panel(panel, i)
            idx = panel.index(pos)
            widget = panel.widget_at(pos)
            indexed_widget = panel.plot_at(idx) if idx >= 0 else None
            assert widget == indexed_widget, (
                f"At plot {i}: widget_at={widget}, "
                f"plot_at(index(pos))={indexed_widget} (index={idx})"
            )
