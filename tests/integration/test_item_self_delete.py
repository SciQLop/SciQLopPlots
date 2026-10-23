"""A slot answering an item's own signal by deleting it must not crash the setter still running."""
import pytest
import shiboken6

from SciQLopPlots import (SciQLopHorizontalSpan, SciQLopPlotRange, SciQLopRectangularSpan,
                          SciQLopVerticalSpan)
from conftest import process_events

R = SciQLopPlotRange


def _timeline(ts_plot):
    tl = ts_plot.add_timeline()
    tl.set_intervals([10, 60], [40, 90], lane=["A", "B"], ids=[7, 8])
    process_events()
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
}


@pytest.mark.parametrize("name", CASES)
def test_a_slot_deleting_an_item_from_its_own_signal_does_not_crash(plot, ts_plot, name):
    make, signal, mutate = CASES[name]
    item = make(plot, ts_plot)
    getattr(item, signal).connect(lambda *_: shiboken6.delete(item))
    mutate(item)
    process_events()
    assert not shiboken6.isValid(item)
