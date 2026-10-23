"""A slot answering an item's own signal by deleting it must not crash the setter still running."""
import pytest
import shiboken6

import numpy as np

from SciQLopPlots import (MultiPlotsVerticalSpan, SciQLopHorizontalSpan, SciQLopPlotRange, SciQLopRectangularSpan,
                          SciQLopVerticalSpan)
from conftest import process_events

R = SciQLopPlotRange


def _timeline(ts_plot):
    tl = ts_plot.add_timeline()
    tl.set_intervals([10, 60], [40, 90], lane=["A", "B"], ids=[7, 8])
    process_events()
    return tl


def _timeline_selecting_7(ts_plot):
    tl = _timeline(ts_plot)
    tl.select_ids([7])
    return tl


CASES = {
    "vspan_range": (lambda p, _: SciQLopVerticalSpan(p, R(0.2, 0.4)), "range_changed",
                    lambda s: s.set_range(R(0.5, 0.6))),
    "hspan_range": (lambda p, _: SciQLopHorizontalSpan(p, R(0.2, 0.4)), "range_changed",
                    lambda s: s.set_range(R(0.5, 0.6))),
    "rspan_key_range": (lambda p, _: SciQLopRectangularSpan(p, R(0.2, 0.4), R(0.2, 0.4)),
                        "key_range_changed", lambda s: s.set_key_range(R(0.5, 0.6))),
    "rspan_value_range": (lambda p, _: SciQLopRectangularSpan(p, R(0.2, 0.4), R(0.2, 0.4)),
                          "value_range_changed", lambda s: s.set_value_range(R(0.5, 0.6))),
    "timeline_select": (lambda _, ts: _timeline(ts), "selected_intervals_changed",
                        lambda t: t.select_ids([7])),
    "timeline_set_intervals": (lambda _, ts: _timeline(ts), "replot",
                               lambda t: t.set_intervals([20], [30], lane=["A"], ids=[9])),
    "timeline_data_drops_selection": (lambda _, ts: _timeline_selecting_7(ts),
                                      "selected_intervals_changed",
                                      lambda t: t.set_intervals([20], [30], lane=["A"], ids=[9])),
}


@pytest.mark.parametrize("name", CASES)
def test_a_slot_deleting_an_item_from_its_own_signal_does_not_crash(plot, ts_plot, name):
    make, signal, mutate = CASES[name]
    item = make(plot, ts_plot)
    getattr(item, signal).connect(lambda *_: shiboken6.delete(item))
    mutate(item)
    process_events()
    assert not shiboken6.isValid(item)


SPANS = {"vspan": SciQLopVerticalSpan, "hspan": SciQLopHorizontalSpan}


@pytest.mark.parametrize("border", ["lower", "upper"])
@pytest.mark.parametrize("kind", SPANS)
def test_a_slot_deleting_a_span_on_selection_does_not_crash_its_border_selector(plot, kind, border):
    span = SPANS[kind](plot, R(0.2, 0.4))
    span.selection_changed.connect(lambda *_: shiboken6.delete(span))
    getattr(span, f"select_{border}_border")(True)
    process_events()
    assert not shiboken6.isValid(span)


@pytest.mark.parametrize("border", ["lower", "upper"])
def test_a_slot_deleting_a_panel_span_on_selection_does_not_crash_its_border_selector(panel, border):
    x = np.linspace(0.0, 10.0, 50)
    panel.plot(x, np.sin(x))
    panel.plot(x, np.cos(x))
    process_events()
    span = MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    plot_span = panel.plots()[0].findChildren(SciQLopVerticalSpan)[0]
    span.selection_changed.connect(lambda *_: shiboken6.delete(span))
    getattr(plot_span, f"select_{border}_border")(True)
    process_events()
    assert not shiboken6.isValid(span)
