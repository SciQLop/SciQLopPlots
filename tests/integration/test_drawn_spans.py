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


SPAN_FACTORIES = {
    SciQLopVerticalSpan: lambda p: p.vertical_span(SciQLopPlotRange(0.2, 0.4)),
    SciQLopHorizontalSpan: lambda p: p.horizontal_span(SciQLopPlotRange(0.2, 0.4)),
    SciQLopRectangularSpan: lambda p: p.rectangular_span(SciQLopPlotRange(0.2, 0.4),
                                                             SciQLopPlotRange(0.2, 0.4)),
}


@pytest.mark.parametrize("cls", SPANS)
def test_the_camel_case_name_is_the_same_signal(cls):
    assert cls.selectionChanged is cls.selection_changed


@pytest.mark.parametrize("cls", SPANS)
def test_both_names_signal_once_per_change(plot, cls):
    span = SPAN_FACTORIES[cls](plot)
    seen = record(span.selectionChanged), record(span.selection_changed)
    span.set_selected(True)
    span.set_selected(False)
    assert seen == ([True, False], [True, False])


def test_clicking_a_span_selects_it_and_signals_once(qtbot, plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    span = plot.vertical_span(SciQLopPlotRange(2.0, 8.0))
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
