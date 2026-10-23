"""Spans signal selection through the base; the camelCase name is an alias."""
import pytest
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from SciQLopPlots import (SciQLopHorizontalSpan, SciQLopPlotRange, SciQLopRectangularSpan,
                          SciQLopVerticalSpan)
from conftest import process_events, record

SPANS = [SciQLopVerticalSpan, SciQLopHorizontalSpan, SciQLopRectangularSpan]


@pytest.mark.parametrize("cls", SPANS)
def test_a_span_declares_no_selection_signal_of_its_own(cls):
    meta = cls.staticMetaObject
    assert meta.indexOfSignal("selectionChanged(bool)") == -1
    assert meta.indexOfSignal("selection_changed(bool)") != -1  # the base's


def test_the_camel_case_alias_still_connects(plot):
    span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.2, 0.4))
    seen = record(span.selectionChanged)
    span.set_selected(True)
    assert seen == [True]


def test_clicking_a_span_selects_it_and_signals_once(qtbot, plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    span = SciQLopVerticalSpan(plot, SciQLopPlotRange(2.0, 8.0))
    seen = record(span.selection_changed)
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)
    process_events()
    # Clicked on the QCustomPlot canvas itself, like tests/integration/test_timeline_edit.py:15:
    # the plot's child at a point may be an axis or legend widget instead.
    canvas = next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))
    centre = QPoint(canvas.width() // 2, canvas.height() // 2)
    QTest.mouseClick(canvas, Qt.LeftButton, Qt.NoModifier, centre)
    process_events()
    assert (span.selected(), seen) == (True, [True])
