"""SciQLopSpan: one span class for every kind, at plot and panel scope (API review 5.6, Plan A)."""
import pytest
import shiboken6
from PySide6.QtCore import QPoint, Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import Coordinates, SciQLopPlot, SciQLopPlotRange as R, SciQLopSpan, SpanKind
from conftest import canvas_of, off_thread, plots_of, process_events, record, scoped_host

SCOPES = ["plot"]
KINDS = {
    "vertical": (SpanKind.Vertical, dict(key_range=R(2.0, 8.0))),
    "horizontal": (SpanKind.Horizontal, dict(value_range=R(-0.5, 0.5))),
    "rectangular": (SpanKind.Rectangular, dict(key_range=R(2.0, 8.0), value_range=R(-0.5, 0.5))),
}
ONE_AXIS = {"vertical": "key", "horizontal": "value"}
OTHER = {"key": "value", "value": "key"}
GREY = QColor(100, 100, 100, 100)


@pytest.fixture(params=SCOPES)
def host(request, qtbot):
    return scoped_host(request.param, qtbot)


@pytest.fixture
def shown_host(host, qtbot):
    host.resize(800, 600)
    host.show()
    qtbot.waitExposed(host)
    process_events()
    return host


@pytest.fixture(params=list(KINDS))
def kind(request):
    return request.param


def make(host, kind, **kwargs):
    return host.span(**KINDS[kind][1], **kwargs)


def bounded(kind):
    return [axis for axis in ("key", "value") if f"{axis}_range" in KINDS[kind][1]]


def _close(a, b, tol):
    return abs(a[0] - b[0]) <= tol and abs(a[1] - b[1]) <= tol


def test_the_factory_returns_the_plain_class(host, kind):
    span = make(host, kind)
    assert type(span) is SciQLopSpan
    assert span.kind() == KINDS[kind][0]


def test_the_class_has_no_python_constructor(plot, panel):
    """The full C++ argument list, so only a removed constructor can raise here."""
    for host in (plot, panel):
        with pytest.raises(NotImplementedError, match="cannot be instantiated"):
            SciQLopSpan(host, SpanKind.Vertical, R(1.0, 2.0), R(1.0, 2.0), QColor("red"), False,
                        True, "tip", Coordinates.Data, "id")


def test_the_defaults(host, kind):
    span = make(host, kind)
    assert span.color() == GREY
    assert (span.read_only(), span.visible(), span.selected(), span.tool_tip()) == (False, True, False, "")
    assert span.coordinates() == Coordinates.Data
    assert span.id() and span.id() != make(host, kind).id()


def test_the_factory_arguments(host, kind):
    span = make(host, kind, color=QColor("red"), read_only=True, visible=False, tool_tip="tip",
                id="event-1")
    assert (span.color(), span.read_only(), span.visible(), span.tool_tip(), span.id()) == \
        (QColor("red"), True, False, "tip", "event-1")


def test_scope_and_part_flag(host, kind):
    span = make(host, kind)
    assert span.scope() is host
    assert span.is_panel_part() is False


def test_each_bounded_axis_reads_its_range_and_the_other_reads_none(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        assert getattr(span, f"{axis}_range")() == KINDS[kind][1].get(f"{axis}_range")


def test_setting_an_unbounded_axis_raises(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        if axis not in bounded(kind):
            with pytest.raises(ValueError, match=f"{axis} axis"):
                getattr(span, f"set_{axis}_range")(R(0.0, 1.0))


def test_a_bounded_axis_round_trips(host, kind):
    span = make(host, kind)
    for axis in bounded(kind):
        getattr(span, f"set_{axis}_range")(R(0.1, 0.3))
        assert getattr(span, f"{axis}_range")() == R(0.1, 0.3)


@pytest.mark.parametrize("kind", ["vertical", "horizontal"])
def test_range_is_the_bounded_axis(host, kind):
    span = make(host, kind)
    axis = ONE_AXIS[kind]
    assert span.range() == getattr(span, f"{axis}_range")()
    span.set_range(R(0.2, 0.4))
    assert getattr(span, f"{axis}_range")() == R(0.2, 0.4)


def test_range_on_a_rectangular_span_raises(host):
    span = make(host, "rectangular")
    for call in (span.range, lambda: span.set_range(R(0.0, 1.0))):
        with pytest.raises(ValueError, match="rectangular"):
            call()


def test_a_span_without_a_range_raises(host):
    with pytest.raises(ValueError, match="key_range, value_range or both"):
        host.span()
    with pytest.raises(ValueError, match="key_range, value_range or both"):
        host.span(key_range=None, value_range=None)


def test_an_explicit_none_is_an_unbounded_axis(host):
    span = host.span(key_range=None, value_range=R(-0.5, 0.5))
    assert (span.kind(), span.key_range(), span.value_range()) == \
        (SpanKind.Horizontal, None, R(-0.5, 0.5))


def test_a_wrong_range_type_is_a_type_error(host):
    with pytest.raises(TypeError):
        host.span("2.0..8.0")


@pytest.mark.parametrize("kind", ["vertical", "horizontal"])
def test_one_set_range_emits_once_on_each_signal(host, kind):
    span = make(host, kind)
    axis = ONE_AXIS[kind]
    ranges = record(span.range_changed)
    mine = record(getattr(span, f"{axis}_range_changed"))
    others = record(getattr(span, f"{OTHER[axis]}_range_changed"))
    span.set_range(R(0.2, 0.4))
    span.set_range(R(0.2, 0.4))
    assert (ranges, mine, others) == ([R(0.2, 0.4)], [R(0.2, 0.4)], [])


def test_a_rectangular_span_never_emits_range_changed(host):
    span = make(host, "rectangular")
    ranges, keys, values = record(span.range_changed), record(span.key_range_changed), \
        record(span.value_range_changed)
    span.set_key_range(R(0.2, 0.4))
    span.set_value_range(R(0.1, 0.3))
    assert (ranges, keys, values) == ([], [R(0.2, 0.4)], [R(0.1, 0.3)])


SIGNALLED = {"read_only": (True, "read_only_changed"), "tool_tip": ("tip", "tool_tip_changed")}


@pytest.mark.parametrize("name", SIGNALLED)
def test_signalled_accessors_round_trip_and_emit_once(host, kind, name):
    value, signal = SIGNALLED[name]
    span = make(host, kind)
    seen = record(getattr(span, signal))
    getattr(span, f"set_{name}")(value)
    getattr(span, f"set_{name}")(value)
    assert (getattr(span, name)(), seen) == (value, [value])


def test_remove_deletes_it(host, kind):
    span = make(host, kind)
    host.remove(span)
    assert not shiboken6.isValid(span)


def test_the_factory_raises_off_thread(host):
    out = {}

    def call():
        try:
            host.span(key_range=R(1.0, 2.0))
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


def test_a_set_off_thread_lands_on_the_gui_thread(host, kind):
    span = make(host, kind)
    axis = bounded(kind)[0]
    seen = record(getattr(span, f"{axis}_range_changed"))
    off_thread(lambda: getattr(span, f"set_{axis}_range")(R(0.1, 0.2)))
    assert seen == []
    process_events()
    assert seen == [R(0.1, 0.2)]


def test_an_unbounded_axis_set_off_thread_raises_in_the_caller(host):
    """Review Focus 4: the kind check runs before the call is queued to the GUI thread."""
    span = make(host, "vertical")
    out = {}

    def call():
        try:
            span.set_value_range(R(0.0, 1.0))
        except ValueError as e:
            out["error"] = e
    off_thread(call)
    process_events()
    assert "value axis" in str(out.get("error", ""))


@pytest.mark.parametrize("name", ["read_only", "tool_tip", "range", "key_range", "value_range"])
def test_on_resolves_with_a_signal(host, name):
    assert getattr(make(host, "vertical").on, name).signal is not None


def test_on_range_raises_on_use_for_a_rectangular_span(host):
    prop = make(host, "rectangular").on.range
    with pytest.raises(ValueError, match="rectangular"):
        prop.value
    with pytest.raises(ValueError, match="rectangular"):
        prop.value = R(0.0, 1.0)


def test_on_reads_none_and_refuses_writes_on_an_unbounded_axis(host):
    span = make(host, "vertical")
    assert span.on.value_range.value is None
    with pytest.raises(ValueError, match="value axis"):
        span.on.value_range.value = R(0.0, 1.0)


def test_on_position_is_absent(host, kind):
    span = make(host, kind)
    assert not hasattr(span, "position")
    with pytest.raises(AttributeError):
        span.on.position


def test_the_borders_default_to_the_lighter_colour(host, kind):
    assert make(host, kind).borders_color() == GREY.lighter()


def test_pixel_accessors_follow_the_bounded_axes(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        if axis not in bounded(kind):
            with pytest.raises(ValueError, match=f"{axis} axis"):
                getattr(span, f"pixel_{axis}_range")()
    if kind == "rectangular":
        with pytest.raises(ValueError, match="rectangular"):
            span.pixel_range()


def test_on_coordinates_has_a_signal(host):
    assert make(host, "vertical").on.coordinates.signal is not None


def test_clicking_a_span_selects_it_and_signals_once(shown_host):
    span = make(shown_host, "vertical")
    seen = record(span.selection_changed)
    canvas = canvas_of(plots_of(shown_host)[0])
    QTest.mouseClick(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier,
                     QPoint(canvas.width() // 2, canvas.height() // 2))
    process_events()
    assert (span.selected(), seen) == (True, [True])


STYLE = {"borders_color": QColor("red"), "line_width": 5.0, "line_style": Qt.PenStyle.DashLine}


@pytest.mark.parametrize("name", STYLE)
def test_the_border_style_round_trips(host, kind, name):
    span = make(host, kind)
    getattr(span, f"set_{name}")(STYLE[name])
    assert getattr(span, name)() == STYLE[name]


def test_switching_coordinates_keeps_the_span_in_place(shown_host, kind):
    span = make(shown_host, kind)
    process_events()
    data = {axis: getattr(span, f"{axis}_range")() for axis in bounded(kind)}
    pixels = {axis: getattr(span, f"pixel_{axis}_range")() for axis in bounded(kind)}
    seen = record(span.coordinates_changed)
    span.set_coordinates(Coordinates.Pixels)
    process_events()
    assert seen == [Coordinates.Pixels]
    for axis in bounded(kind):
        assert _close(getattr(span, f"{axis}_range")(), pixels[axis], 1.0)
        assert _close(getattr(span, f"pixel_{axis}_range")(), pixels[axis], 1.0)
    span.set_coordinates(Coordinates.Data)
    process_events()
    for axis in bounded(kind):
        assert _close(getattr(span, f"{axis}_range")(), data[axis], 0.05)


def test_switching_coordinates_signals_the_new_numbers(shown_host):
    span = make(shown_host, "vertical")
    process_events()
    pixels = span.pixel_range()
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    span.set_coordinates(Coordinates.Pixels)
    assert len(ranges) == 1 and len(keys) == 1 and _close(ranges[0], pixels, 1.0)


def test_a_span_made_in_pixels_reads_and_writes_pixels(shown_host, kind):
    span = make(shown_host, kind, coordinates=Coordinates.Pixels)
    process_events()
    assert span.coordinates() == Coordinates.Pixels
    for axis in bounded(kind):
        assert getattr(span, f"{axis}_range")() == KINDS[kind][1][f"{axis}_range"]
        assert _close(getattr(span, f"pixel_{axis}_range")(), (2.0, 8.0) if axis == "key" else (-0.5, 0.5), 1.0)
        getattr(span, f"set_{axis}_range")(R(20.0, 60.0))
        process_events()
        assert _close(getattr(span, f"pixel_{axis}_range")(), (20.0, 60.0), 1.0)


def test_a_pixels_span_stays_put_on_zoom(shown_host, kind):
    span = make(shown_host, kind)
    process_events()
    span.set_coordinates(Coordinates.Pixels)
    before = {axis: getattr(span, f"pixel_{axis}_range")() for axis in bounded(kind)}
    for plot in plots_of(shown_host):
        plot.y_axis().set_range(R(-5.0, 5.0))
    if isinstance(shown_host, SciQLopPlot):
        shown_host.x_axis().set_range(R(0.0, 100.0))
    else:
        shown_host.set_time_axis_range(R(0.0, 100.0))
    process_events()
    for axis in bounded(kind):
        assert _close(getattr(span, f"pixel_{axis}_range")(), before[axis], 1.0)


def test_the_delete_key_asks_once(shown_host, kind):
    span = make(shown_host, kind)
    asked = []
    span.delete_requested.connect(lambda: asked.append(True))
    span.set_selected(True)
    process_events()
    QTest.keyClick(canvas_of(plots_of(shown_host)[0]), Qt.Key.Key_Delete)
    process_events()
    assert asked == [True]
