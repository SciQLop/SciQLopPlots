"""Panel-wide objects under SciQLopDrawnObject."""
import numpy as np
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection,
                          SciQLopPlotRange, SciQLopVerticalSpan)
from conftest import force_gc, process_events, record

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


def _plot_spans(plot):
    return plot.findChildren(SciQLopVerticalSpan)


def test_a_panel_span_reaches_a_later_plot_and_outlives_a_removed_one(panel):
    panel.plot(X, np.sin(X))
    span = MultiPlotsVerticalSpan(panel, SciQLopPlotRange(1.0, 2.0))
    span.visible = False
    panel.plot(X, np.cos(X))
    process_events()
    later = panel.plots()[1]
    assert [s.visible() for s in _plot_spans(later)] == [False]
    panel.remove_plot(panel.plots()[0])
    force_gc()
    span.visible = True
    span.color = QColor("red")
    assert [(s.visible(), s.color()) for s in _plot_spans(later)] == [(True, QColor("red"))]


def _delete_on(signal, victim):
    import shiboken6
    signal.connect(lambda *_: shiboken6.delete(victim))
    return lambda: not shiboken6.isValid(victim)


def _span_and_plot_span(panel):
    panel.plot(X, np.sin(X))
    panel.plot(X, np.cos(X))
    process_events()
    span = MultiPlotsVerticalSpan(panel, SciQLopPlotRange(1.0, 2.0))
    return span, _plot_spans(panel.plots()[0])[0]


def _line_and_plot_line(panel):
    from SciQLopPlots import SciQLopVerticalLine
    panel.plot(X, np.sin(X))
    panel.plot(X, np.cos(X))
    process_events()
    line = MultiPlotsVerticalLine(panel, 2.0)
    return line, panel.plots()[0].findChildren(SciQLopVerticalLine)[0]


SPAN_MUTATIONS = {
    "visible": ("visible_changed", lambda s: setattr(s, "visible", False)),
    "selected": ("selection_changed", lambda s: setattr(s, "selected", True)),
    "color": ("color_changed", lambda s: setattr(s, "color", QColor("red"))),
    "range": ("range_changed", lambda s: s.set_range(SciQLopPlotRange(3.0, 4.0))),
}


@pytest.mark.parametrize("name", SPAN_MUTATIONS)
def test_a_slot_deleting_the_panel_span_mid_mutation_does_not_crash(panel, name):
    signal_name, mutate = SPAN_MUTATIONS[name]
    span, plot_span = _span_and_plot_span(panel)
    deleted = _delete_on(getattr(plot_span, signal_name), span)
    mutate(span)
    process_events()
    assert deleted()


LINE_MUTATIONS = {
    "visible": ("visible_changed", lambda l: setattr(l, "visible", False)),
    "color": ("color_changed", lambda l: setattr(l, "color", QColor("red"))),
    "position": ("position_changed", lambda l: l.set_position(5.0)),
}


@pytest.mark.parametrize("name", LINE_MUTATIONS)
def test_a_slot_deleting_the_panel_line_mid_mutation_does_not_crash(panel, name):
    signal_name, mutate = LINE_MUTATIONS[name]
    line, plot_line = _line_and_plot_line(panel)
    deleted = _delete_on(getattr(plot_line, signal_name), line)
    mutate(line)
    process_events()
    assert deleted()


def test_a_slot_deleting_a_plot_span_from_its_own_range_signal_does_not_crash(panel):
    span, plot_span = _span_and_plot_span(panel)
    deleted = _delete_on(plot_span.range_changed, span)
    plot_span.set_range(SciQLopPlotRange(3.0, 4.0))
    process_events()
    assert deleted()


@pytest.mark.parametrize("border", ["lower", "upper"])
def test_a_slot_deleting_a_plot_span_from_its_border_signal_does_not_crash(panel, border):
    span, plot_span = _span_and_plot_span(panel)
    deleted = _delete_on(getattr(plot_span, f"{border}_border_selection_changed"), span)
    getattr(plot_span, f"select_{border}_border")(True)
    process_events()
    assert deleted()
