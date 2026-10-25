"""The old span and line names during Plan A: subclasses that only fix kind and scope (5.6)."""
import numpy as np
import pytest
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection,
                          SciQLopHorizontalSpan, SciQLopPlotRange as R, SciQLopRangeItemInterface,
                          SciQLopRectangularSpan, SciQLopSpan, SciQLopStraightLine,
                          SciQLopVerticalLine, SciQLopVerticalSpan, SpanKind)
from conftest import plots_of, process_events

X = np.linspace(0.0, 10.0, 50)
OLD_PLOT_SPANS = {
    SciQLopVerticalSpan: (lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)), SpanKind.Vertical),
    SciQLopHorizontalSpan: (lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)), SpanKind.Horizontal),
    SciQLopRectangularSpan: (lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
                             SpanKind.Rectangular),
}


def _old(make, host):
    with pytest.warns(DeprecationWarning):
        return make(host)


def _with_plots(panel):
    panel.plot(X, np.sin(X))
    panel.plot(X, np.cos(X))
    process_events()
    return panel


@pytest.mark.parametrize("cls", OLD_PLOT_SPANS, ids=lambda cls: cls.__name__)
def test_an_old_plot_span_is_a_span_of_its_kind_and_no_other_class(plot, cls):
    make, kind = OLD_PLOT_SPANS[cls]
    span = _old(make, plot)
    assert isinstance(span, SciQLopSpan) and isinstance(span, cls)
    assert (span.kind(), span.scope()) == (kind, plot)
    others = (set(OLD_PLOT_SPANS) - {cls}) | {MultiPlotsVerticalSpan, SciQLopRangeItemInterface}
    assert not any(isinstance(span, other) for other in others)


def test_factory_spans_are_never_an_old_class(plot, panel):
    spans = [plot.vertical_span(R(1.0, 2.0)), plot.horizontal_span(R(1.0, 2.0)),
             panel.vertical_span(R(1.0, 2.0))]
    old = tuple(OLD_PLOT_SPANS) + (MultiPlotsVerticalSpan,)
    assert [type(span) for span in spans] == [SciQLopSpan] * 3
    assert not any(isinstance(span, old) for span in spans)


def test_an_old_panel_span_has_old_class_parts(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0)), panel)
    assert isinstance(span, SciQLopSpan) and (span.kind(), span.scope()) == (SpanKind.Vertical, panel)
    parts = [p for plot in plots_of(panel) for p in plot.findChildren(SciQLopVerticalSpan)]
    assert len(parts) == 2
    assert all(p.is_panel_part() and not isinstance(p, MultiPlotsVerticalSpan) for p in parts)


def test_the_old_panel_span_keeps_its_properties(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0), id="event"), panel)
    span.range = R(3.0, 4.0)
    span.selected = True
    span.color = QColor("red")
    span.visible = False
    span.tooltip = "tip"
    span.read_only = True
    assert (span.range, span.selected, span.color, span.visible, span.tooltip, span.read_only,
            span.id) == (R(3.0, 4.0), True, QColor("red"), False, "tip", True, "event")
    with pytest.raises(TypeError):
        span.range()


def test_the_old_panel_span_keeps_show_and_hide(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0)), panel)
    span.hide()
    assert span.visible is False
    span.show()
    assert span.visible is True


def test_the_old_panel_span_lost_its_plumbing():
    assert not any(hasattr(MultiPlotsVerticalSpan, name) for name in
                   ("addObject", "add_object", "removeObject", "replotAll", "update_plot_list"))


def test_the_collection_still_makes_old_panel_spans(panel):
    _with_plots(panel)
    collection = MultiPlotsVSpanCollection(panel)
    assert isinstance(collection.create_span(R(1.0, 2.0)).data(), MultiPlotsVerticalSpan)


def test_the_old_vertical_span_keeps_pixel_range_and_border_selection(qtbot, plot):
    plot.resize(800, 600)
    plot.show()
    qtbot.waitExposed(plot)
    plot.x_axis().set_range(R(0.0, 100.0))
    process_events()
    span = _old(lambda p: SciQLopVerticalSpan(p, R(20.0, 80.0)), plot)
    process_events()
    span.set_pixel_range(span.pixel_range)
    assert span.range()[0] == pytest.approx(20.0, abs=0.1)
    seen = []
    span.lower_border_selection_changed.connect(seen.append)
    span.select_lower_border(True)
    assert (seen, span.selected()) == ([True], True)
    for cls in (SciQLopVerticalSpan, SciQLopHorizontalSpan):
        assert all(callable(getattr(cls, name, None))
                   for name in ("select_lower_border", "select_upper_border"))


@pytest.mark.parametrize("cls", OLD_PLOT_SPANS, ids=lambda cls: cls.__name__)
def test_on_position_no_longer_resolves_on_an_old_span(plot, cls):
    span = _old(OLD_PLOT_SPANS[cls][0], plot)
    with pytest.raises(AttributeError):
        span.on.position


def test_an_old_panel_line_has_old_class_parts(panel):
    _with_plots(panel)
    line = _old(lambda p: MultiPlotsVerticalLine(p, 2.0), panel)
    assert isinstance(line, SciQLopStraightLine)
    assert (line.orientation(), line.scope()) == (Qt.Orientation.Vertical, panel)
    parts = [p for plot in plots_of(panel) for p in plot.findChildren(SciQLopVerticalLine)]
    assert len(parts) == 2
    assert all(p.is_panel_part() and not isinstance(p, MultiPlotsVerticalLine) for p in parts)


def test_the_old_panel_line_keeps_its_defaults_and_properties(panel):
    _with_plots(panel)
    line = _old(lambda p: MultiPlotsVerticalLine(p, 2.0), panel)
    assert (line.color, line.read_only) == (QColor(100, 100, 100), False)
    line.position = 4.0
    line.color = QColor("red")
    line.line_width = 3.0
    line.visible = False
    line.selected = True
    line.tooltip = "tip"
    line.read_only = True
    assert (line.position, line.color, line.line_width, line.visible, line.selected, line.tooltip,
            line.read_only) == (4.0, QColor("red"), 3.0, False, True, "tip", True)
    with pytest.raises(TypeError):
        line.position()


def test_the_old_panel_line_lost_its_plumbing():
    assert not any(hasattr(MultiPlotsVerticalLine, name) for name in
                   ("addObject", "add_object", "removeObject", "replotAll", "update_plot_list"))
