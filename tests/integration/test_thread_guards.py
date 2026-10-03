"""Plot-item, axis and panel mutators called off the GUI thread run on the GUI thread.

SciQLop#147: kernel-thread code deleted catalog spans while the GUI thread was in a
replot that used them (SIGSEGV in setupPaintBuffers). Mutators now re-post themselves
to the object's thread (queued, never blocking) and return. Calls that must hand an
object back (item constructors, create_span, create_plot) refuse off-thread instead.
"""
import threading

import pytest
from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QFont, QPen, QPixmap

from conftest import process_events
from SciQLopPlots import (
    LineTermination,
    MultiPlotsVerticalLine,
    MultiPlotsVerticalSpan,
    MultiPlotsVSpanCollection,
    SciQLopCurvedLineItem,
    SciQLopEllipseItem,
    SciQLopHorizontalLine,
    SciQLopHorizontalSpan,
    SciQLopPixmapItem,
    SciQLopPlot,
    SciQLopPlotRange,
    SciQLopRectangularSpan,
    SciQLopTextItem,
    SciQLopVerticalLine,
    SciQLopVerticalSpan,
)


def _in_thread(fn):
    """Runs fn on a plain Python thread; returns what it returned or raised."""
    out = {}

    def run():
        try:
            out["value"] = fn()
        except Exception as e:  # noqa: BLE001 - the test inspects it
            out["error"] = e

    t = threading.Thread(target=run)
    t.start()
    t.join(timeout=10)
    assert not t.is_alive(), "the off-thread call blocked"
    return out


def _get(obj, prop):
    """Reads prop whether the bindings expose it as a getter or as a Python property."""
    value = getattr(obj, prop)
    return value() if callable(value) else value


def _settle():
    for _ in range(5):
        process_events()


R = SciQLopPlotRange
RED = QColor(200, 10, 10)

_SPAN_SETTERS = [
    ("color", RED),
    ("borders_color", RED),
    ("line_width", 7.0),
    ("line_style", Qt.PenStyle.DashLine),
    ("selected", True),
    ("read_only", True),
    ("tool_tip", "off-thread"),
    ("visible", False),
]

ITEMS = {
    "vspan": lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)),
    "hspan": lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)),
    "rspan": lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
    "pixmap": lambda p: SciQLopPixmapItem(p, QPixmap(4, 4), QRectF(1, 1, 2, 2)),
    "ellipse": lambda p: SciQLopEllipseItem(p, QRectF(1, 1, 2, 2)),
    "text": lambda p: SciQLopTextItem(p, "hello", QPointF(1, 1)),
    "curve": lambda p: SciQLopCurvedLineItem(p, QPointF(1, 1), QPointF(2, 2)),
    "vline": lambda p: SciQLopVerticalLine(p, 3.0),
    "hline": lambda p: SciQLopHorizontalLine(p, 3.0),
}

CASES = (
    [("vspan", "range", R(5.0, 6.0))]
    + [("vspan", n, v) for n, v in _SPAN_SETTERS]
    + [("hspan", "range", R(5.0, 6.0))]
    + [("hspan", n, v) for n, v in _SPAN_SETTERS]
    + [("rspan", "key_range", R(5.0, 6.0)), ("rspan", "value_range", R(5.0, 6.0))]
    + [("rspan", n, v) for n, v in _SPAN_SETTERS]
    + [
        ("pixmap", "visible", False),
        ("ellipse", "visible", False),
        ("ellipse", "position", QPointF(1.5, 1.5)),
        ("ellipse", "color", RED),
        ("ellipse", "line_width", 7.0),
        ("ellipse", "line_style", Qt.PenStyle.DashLine),
        ("ellipse", "pen", QPen(RED)),
        ("ellipse", "brush", QBrush(RED)),
        ("ellipse", "tool_tip", "off-thread"),
        ("text", "visible", False),
        ("text", "position", QPointF(1.5, 1.5)),
        ("text", "text", "changed"),
        ("text", "color", RED),
        ("text", "font_size", 31.0),
        ("text", "font_color", RED),
        ("text", "font", QFont("Monospace", 23)),
        ("curve", "visible", False),
        ("curve", "start_position", QPointF(0.5, 0.5)),
        ("curve", "stop_position", QPointF(2.5, 2.5)),
        ("curve", "start_dir_position", QPointF(0.7, 0.2)),
        ("curve", "stop_dir_position", QPointF(2.7, 2.2)),
        ("curve", "start_termination", LineTermination.Arrow),
        ("curve", "stop_termination", LineTermination.Circle),
        ("curve", "color", RED),
        ("curve", "line_width", 7.0),
        ("curve", "line_style", Qt.PenStyle.DashLine),
        ("vline", "position", 4.5),
        ("vline", "color", RED),
        ("vline", "line_width", 7.0),
        ("vline", "line_style", Qt.PenStyle.DashLine),
        ("vline", "visible", False),
        ("vline", "movable", True),
        ("vline", "tool_tip", "off-thread"),
        ("hline", "position", 4.5),
    ]
)


@pytest.mark.parametrize("kind,prop,value", CASES, ids=[f"{k}.set_{p}" for k, p, _ in CASES])
def test_item_setter_lands_on_gui_thread(plot, kind, prop, value):
    item = ITEMS[kind](plot)
    _settle()
    before = _get(item, prop)
    assert before != value, "pick a value that differs from the default"

    out = _in_thread(lambda: getattr(item, "set_" + prop)(value))
    assert "error" not in out, out.get("error")
    assert _get(item, prop) == before, "applied on the calling thread"

    _settle()
    assert _get(item, prop) == value


AXIS_CASES = [
    ("range", R(3.0, 4.0)),
    ("log", True),
    ("label", "off-thread"),
    ("visible", False),
    ("tick_labels_visible", False),
    ("tick_labels", {1.0: "one", 2.0: "two"}),
    ("label_color", RED),
    ("tick_label_color", RED),
    ("label_font", QFont("Monospace", 23)),
    ("tick_label_font", QFont("Monospace", 23)),
    ("selected", True),
    ("autoscale_percentile_low", 5.0),
    ("autoscale_percentile_high", 95.0),
]


@pytest.mark.parametrize("prop,value", AXIS_CASES, ids=[f"set_{p}" for p, _ in AXIS_CASES])
def test_axis_setter_lands_on_gui_thread(plot, prop, value):
    axis = plot.y_axis()
    before = _get(axis, prop)
    assert before != value

    out = _in_thread(lambda: getattr(axis, "set_" + prop)(value))
    assert "error" not in out, out.get("error")
    assert _get(axis, prop) == before, "applied on the calling thread"

    _settle()
    assert _get(axis, prop) == value


def test_axis_clear_tick_labels_lands_on_gui_thread(plot):
    axis = plot.y_axis()
    axis.set_tick_labels({1.0: "one"})
    _in_thread(axis.clear_tick_labels)
    assert axis.tick_labels() == {1.0: "one"}
    _settle()
    assert axis.tick_labels() == {}


def test_straight_line_min_max_land_on_gui_thread(plot):
    line = SciQLopVerticalLine(plot, 3.0)
    _settle()
    _in_thread(lambda: line.set_min_value(1.0))
    _in_thread(lambda: line.set_max_value(2.0))
    line.set_position(5.0)  # on the GUI thread: not clamped yet
    assert _get(line, "position") == 5.0
    _settle()
    line.set_position(5.0)
    assert _get(line, "position") == 2.0


@pytest.fixture
def vspans(panel):
    panel.create_plot()
    panel.set_time_axis_range(R(0.0, 10.0))
    _settle()
    return MultiPlotsVSpanCollection(panel)


MULTI_SPAN_CASES = [
    ("range", R(5.0, 6.0)),
    ("color", RED),
    ("selected", True),
    ("read_only", True),
    ("tool_tip", "off-thread"),
    ("visible", False),
]


@pytest.mark.parametrize("prop,value", MULTI_SPAN_CASES, ids=[f"set_{p}" for p, _ in MULTI_SPAN_CASES])
def test_multi_plot_span_setter_lands_on_gui_thread(vspans, prop, value):
    span = vspans.create_span(R(1.0, 2.0))
    _settle()
    before = _get(span, prop)
    assert before != value
    _in_thread(lambda: getattr(span, "set_" + prop)(value))
    assert _get(span, prop) == before, "applied on the calling thread"
    _settle()
    assert _get(span, prop) == value


MULTI_LINE_CASES = [
    ("position", 5.0),
    ("color", RED),
    ("line_width", 7.0),
    ("read_only", True),
    ("tool_tip", "off-thread"),
    ("visible", False),
]


@pytest.mark.parametrize("prop,value", MULTI_LINE_CASES, ids=[f"set_{p}" for p, _ in MULTI_LINE_CASES])
def test_multi_plot_line_setter_lands_on_gui_thread(panel, prop, value):
    panel.create_plot()
    vline = MultiPlotsVerticalLine(panel, 3.0)
    _settle()
    before = _get(vline, prop)
    assert before != value
    _in_thread(lambda: getattr(vline, "set_" + prop)(value))
    assert _get(vline, prop) == before, "applied on the calling thread"
    _settle()
    assert _get(vline, prop) == value


def test_multi_plot_line_constructor_refuses_off_thread(panel):
    panel.create_plot()
    out = _in_thread(lambda: MultiPlotsVerticalLine(panel, 3.0))
    assert isinstance(out.get("error"), RuntimeError)


def test_delete_span_lands_on_gui_thread(vspans):
    span = vspans.create_span(R(1.0, 2.0), id="a")
    _settle()
    _in_thread(lambda: vspans.delete_span("a"))
    assert vspans.span("a"), "deleted on the calling thread"
    _settle()
    assert not vspans.span("a")


def test_delete_span_object_lands_on_gui_thread(vspans):
    span = vspans.create_span(R(1.0, 2.0))
    _settle()
    _in_thread(lambda: vspans.delete_span(span))
    assert len(vspans.spans()) == 1
    _settle()
    assert len(vspans.spans()) == 0


def test_create_span_refuses_off_thread(vspans):
    out = _in_thread(lambda: vspans.create_span(R(1.0, 2.0)))
    assert isinstance(out.get("error"), RuntimeError)
    _settle()
    assert vspans.spans() == []


@pytest.mark.parametrize("kind", sorted(ITEMS))
def test_item_constructor_refuses_off_thread(plot, kind):
    out = _in_thread(lambda: ITEMS[kind](plot))
    assert isinstance(out.get("error"), RuntimeError)


def test_multi_plot_span_constructor_refuses_off_thread(panel):
    panel.create_plot()
    out = _in_thread(lambda: MultiPlotsVerticalSpan(panel, R(1.0, 2.0)))
    assert isinstance(out.get("error"), RuntimeError)


def test_create_plot_refuses_off_thread(panel):
    out = _in_thread(panel.create_plot)
    assert isinstance(out.get("error"), RuntimeError)
    _settle()
    assert len(panel.plots()) == 0


def test_add_plot_lands_on_gui_thread(panel):
    plot = SciQLopPlot()
    _in_thread(lambda: panel.add_plot(plot))
    assert len(panel.plots()) == 0, "added on the calling thread"
    _settle()
    assert len(panel.plots()) == 1


def test_insert_and_remove_plot_land_on_gui_thread(panel):
    first = panel.create_plot()
    plot = SciQLopPlot()
    _in_thread(lambda: panel.insert_plot(0, plot))
    _settle()
    assert panel.plot_at(0) is plot
    _in_thread(lambda: panel.remove_plot(first))
    assert len(panel.plots()) == 2, "removed on the calling thread"
    _settle()
    assert len(panel.plots()) == 1


def test_move_plot_lands_on_gui_thread(panel):
    a, b = panel.create_plot(), panel.create_plot()
    _in_thread(lambda: panel.move_plot(0, 1))
    assert panel.plot_at(0) is a
    _settle()
    assert panel.plot_at(0) is b


def test_clear_lands_on_gui_thread(panel):
    panel.create_plot()
    _in_thread(panel.clear)
    assert len(panel.plots()) == 1
    _settle()
    assert len(panel.plots()) == 0


def test_add_and_remove_panel_land_on_gui_thread(panel):
    from SciQLopPlots import SciQLopMultiPlotPanel

    sub = SciQLopMultiPlotPanel()
    _in_thread(lambda: panel.add_panel(sub))
    assert sub.parentWidget() is None, "added on the calling thread"
    _settle()
    assert sub.parentWidget() is not None
    assert panel.size() == 1
    _in_thread(lambda: panel.remove_panel(sub))
    assert panel.size() == 1, "removed on the calling thread"
    _settle()
    assert panel.size() == 0


def test_remove_plottable_lands_on_gui_thread(plot, sample_data):
    x, y = sample_data
    graph = plot.plot(x, y)
    _settle()
    n = len(plot.plottables())
    _in_thread(lambda: plot.remove_plottable(graph))
    assert len(plot.plottables()) == n, "removed on the calling thread"
    _settle()
    assert len(plot.plottables()) == n - 1


def test_queued_call_on_deleted_item_is_dropped(plot):
    span = SciQLopVerticalSpan(plot, R(1.0, 2.0))
    _settle()
    _in_thread(lambda: span.set_range(R(5.0, 6.0)))
    span.deleteLater()
    _settle()  # deletion and the queued call both run; must not crash
