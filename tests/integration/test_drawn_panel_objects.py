"""Panel-wide objects under SciQLopDrawnObject."""
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection,
                          SciQLopPlotRange)
from conftest import record

X = np.linspace(0.0, 10.0, 50)


def test_the_catalogue_overlay_pattern_signals_once(panel):
    """SciQLop's overlay connects selection_changed and sets `span.selected = True`."""
    panel.plot(X, np.sin(X))
    collection = MultiPlotsVSpanCollection(panel)
    span = collection.create_span(SciQLopPlotRange(1.0, 2.0), id="event")
    seen = []
    span.selection_changed.connect(seen.append)
    span.selected = True
    span.selected = True
    assert (span.selected, seen) == (True, [True])


def test_panel_span_state_stays_attributes(panel):
    panel.plot(X, np.sin(X))
    span = MultiPlotsVerticalSpan(panel, SciQLopPlotRange(1.0, 2.0))
    seen = record(span.visible_changed)
    span.visible = False
    span.color = QColor("red")
    assert (span.visible, span.color, seen) == (False, QColor("red"), [False])


def test_a_panel_line_colour_signals_once(panel):
    panel.plot(X, np.sin(X))
    line = MultiPlotsVerticalLine(panel, 2.0)
    seen = record(line.color_changed)
    line.color = QColor("red")
    line.color = QColor("red")
    assert seen == [QColor("red")]
