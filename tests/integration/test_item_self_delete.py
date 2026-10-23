"""A slot answering an item's own signal by deleting it must not crash the setter still running."""
import pytest
import shiboken6

import numpy as np
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from SciQLopPlots import (MultiPlotsVerticalSpan, SciQLopHorizontalSpan, SciQLopPlotRange, SciQLopRectangularSpan,
                          SciQLopVerticalSpan, WaterfallOffsetMode)
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


def _category_timeline(ts_plot):
    tl = ts_plot.add_timeline()
    tl.stack = "category"
    tl.set_intervals([10, 20, 60], [40, 50, 90], lane=["A", "A", "B"], category=["x", "y", "x"],
                     ids=[7, 8, 9])
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
    "timeline_set_intervals": (lambda _, ts: _timeline(ts), "replot",
                               lambda t: t.set_intervals([20], [30], lane=["A"], ids=[9])),
    "timeline_data_drops_selection": (lambda _, ts: _timeline_selecting_7(ts),
                                      "selected_intervals_changed",
                                      lambda t: t.set_intervals([20], [30], lane=["A"], ids=[9])),
    "timeline_category_order": (lambda _, ts: _category_timeline(ts), "replot",
                                lambda t: t.set_category_order(["y", "x"])),
    "timeline_new_category_lanes": (lambda _, ts: _category_timeline(ts), "lanes_changed",
                                    lambda t: t.set_intervals([10, 20], [40, 50], lane=["A", "A"],
                                                              category=["x", "z"], ids=[7, 8])),
    "timeline_new_lane": (lambda _, ts: _category_timeline(ts), "lanes_changed",
                          lambda t: t.set_intervals([10], [40], lane=["C"], ids=[7])),
    "timeline_unstack": (lambda _, ts: _category_timeline(ts), "lanes_changed",
                         lambda t: setattr(t, "stack", None)),
}


@pytest.mark.parametrize("name", CASES)
def test_a_slot_deleting_an_item_from_its_own_signal_does_not_crash(plot, ts_plot, name):
    make, signal, mutate = CASES[name]
    item = make(plot, ts_plot)
    getattr(item, signal).connect(lambda *_: shiboken6.delete(item))
    mutate(item)
    process_events()
    assert not shiboken6.isValid(item)


@pytest.mark.parametrize("nested", [
    lambda tl: setattr(tl, "stack", None),
    lambda tl: tl.set_category_order(["y", "x"]),
], ids=["set_stack", "set_category_order"])
def test_a_slot_deleting_the_timeline_after_a_nested_repack_does_not_crash(ts_plot, nested):
    tl = _category_timeline(ts_plot)
    tl.stack = "time"
    tl.select_ids([7])
    tl.selected_intervals_changed.connect(lambda *_: nested(tl))
    tl.lanes_changed.connect(lambda: shiboken6.delete(tl))
    tl.set_intervals([10, 20, 30], [40, 50, 60], lane=["A", "A", "A"], category=["x", "y", "x"],
                     ids=[8, 9, 10])
    process_events()
    assert not shiboken6.isValid(tl)


WATERFALL_SETTERS = {
    "set_offset_mode": lambda w: w.set_offset_mode(WaterfallOffsetMode.Custom),
    "set_uniform_spacing": lambda w: w.set_uniform_spacing(w.uniform_spacing() + 1.0),
    "set_offsets": lambda w: w.set_offsets([1.0, 2.0]),
    "set_normalize": lambda w: w.set_normalize(not w.normalize()),
    "set_gain": lambda w: w.set_gain(w.gain() + 1.0),
}


@pytest.mark.parametrize("setter", WATERFALL_SETTERS)
def test_a_slot_deleting_the_waterfall_from_its_change_signal_does_not_crash(plot, setter):
    wf = plot.add_waterfall("w", labels=["a", "b"])
    wf.set_data(np.linspace(0.0, 1.0, 10), np.random.rand(10, 2))
    process_events()
    getattr(wf, setter.removeprefix("set_") + "_changed").connect(lambda *_: shiboken6.delete(wf))
    WATERFALL_SETTERS[setter](wf)
    process_events()
    assert not shiboken6.isValid(wf)


def test_a_slot_deleting_the_curve_while_it_sets_its_line_width_does_not_crash(plot):
    t = np.linspace(0.0, 6.0, 50)
    curve = plot.parametric_curve(np.cos(t), np.column_stack([np.sin(t), np.cos(t)]),
                                  labels=["a", "b"])
    process_events()
    curve.replot.connect(lambda: shiboken6.delete(curve))
    curve.set_line_width(3.0)
    process_events()
    assert not shiboken6.isValid(curve)


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


def _shown_timeline(ts_plot):
    ts_plot.resize(400, 300)
    ts_plot.show()
    process_events()
    tl = _timeline(ts_plot)
    ts_plot.x_axis().set_range(R(0, 100))
    process_events()
    return tl


def _pixel(tl, key, lane):
    q = tl.pixel_of(key, lane)
    return QPoint(round(q.x()), round(q.y()))


def _click(canvas, tl):
    QTest.mouseClick(canvas, Qt.LeftButton, Qt.NoModifier, _pixel(tl, 25, "A"))


def _shift_drag(canvas, tl):
    end = _pixel(tl, 95, "B")
    QTest.mousePress(canvas, Qt.LeftButton, Qt.ShiftModifier, _pixel(tl, 25, "A"))
    QTest.mouseMove(canvas, end)
    QTest.mouseRelease(canvas, Qt.LeftButton, Qt.ShiftModifier, end)


@pytest.mark.parametrize("gesture", [_click, _shift_drag], ids=["click", "shift_drag"])
def test_a_slot_deleting_the_timeline_on_a_selection_gesture_does_not_crash(ts_plot, gesture):
    tl = _shown_timeline(ts_plot)
    canvas = next(w for w in ts_plot.findChildren(QWidget) if w.inherits("QCustomPlot"))
    fired = []
    tl.selected_intervals_changed.connect(lambda *_: (fired.append(1), shiboken6.delete(tl)))
    gesture(canvas, tl)
    process_events()
    assert fired
    assert not shiboken6.isValid(tl)
