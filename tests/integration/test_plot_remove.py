"""plot.remove / panel.remove (part B)."""
import pytest
import shiboken6
from PySide6.QtCore import QPointF
from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, SciQLopPlotRange as R,
                          SciQLopVerticalLine, SciQLopVerticalSpan)
from conftest import process_events


def _panel_with_plot(panel):
    plot = panel.create_plot()
    process_events()
    return plot


def test_panel_span_marks_its_per_plot_parts(panel):
    plot = _panel_with_plot(panel)
    span = MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    line = MultiPlotsVerticalLine(panel, 1.5)
    process_events()
    parts = plot.findChildren(SciQLopVerticalSpan) + plot.findChildren(SciQLopVerticalLine)
    assert parts and all(p.property("_sciqlop_panel_part") for p in parts)
