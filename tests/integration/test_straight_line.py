"""SciQLopStraightLine: one line class, vertical or horizontal, at plot and panel scope (5.6)."""
import pytest
import shiboken6
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import (Coordinates, SciQLopHorizontalLine, SciQLopPlot, SciQLopPlotRange as R,
                          SciQLopStraightLine, SciQLopVerticalLine)
from conftest import canvas_of, off_thread, plots_of, process_events, record, scoped_host

SCOPES = ["plot"]
ORIENTATIONS = {"vertical": (Qt.Orientation.Vertical, 5.0),
                "horizontal": (Qt.Orientation.Horizontal, 0.5)}


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


@pytest.fixture(params=list(ORIENTATIONS))
def orientation(request):
    return request.param


def make(host, orientation, **kwargs):
    orient, position = ORIENTATIONS[orientation]
    return host.straight_line(position, orient, **kwargs)


def test_the_factory_returns_the_plain_class(host, orientation):
    line = make(host, orientation)
    assert type(line) is SciQLopStraightLine
    assert (line.orientation(), line.position()) == ORIENTATIONS[orientation]


def test_the_defaults(host, orientation):
    line = make(host, orientation)
    assert (line.color(), line.read_only(), line.visible(), line.selected(), line.tool_tip()) == \
        (QColor(0, 0, 0), True, True, False, "")
    assert line.coordinates() == Coordinates.Data
    assert (line.scope(), line.is_panel_part()) == (host, False)


def test_the_factory_arguments(host, orientation):
    line = make(host, orientation, color=QColor("red"), read_only=False, visible=False,
                tool_tip="tip")
    assert (line.color(), line.read_only(), line.visible(), line.tool_tip()) == \
        (QColor("red"), False, False, "tip")


SIGNALLED = {"position": (0.25, "position_changed"), "read_only": (False, "read_only_changed")}


@pytest.mark.parametrize("name", SIGNALLED)
def test_signalled_accessors_round_trip_and_emit_once(host, orientation, name):
    value, signal = SIGNALLED[name]
    line = make(host, orientation)
    seen = record(getattr(line, signal))
    getattr(line, f"set_{name}")(value)
    getattr(line, f"set_{name}")(value)
    assert (getattr(line, name)(), seen) == (value, [value])


STYLE = {"line_width": 3.0, "line_style": Qt.PenStyle.DashLine}


@pytest.mark.parametrize("name", STYLE)
def test_the_style_round_trips(host, orientation, name):
    line = make(host, orientation)
    getattr(line, f"set_{name}")(STYLE[name])
    assert getattr(line, name)() == STYLE[name]


def test_switching_coordinates_keeps_the_line_in_place(shown_host, orientation):
    line = make(shown_host, orientation)
    process_events()
    data = line.position()
    coordinates, positions = record(line.coordinates_changed), record(line.position_changed)
    line.set_coordinates(Coordinates.Pixels)
    process_events()
    pixels = line.position()
    canvas = canvas_of(plots_of(shown_host)[0])
    assert 0 < pixels < (canvas.width() if orientation == "vertical" else canvas.height())
    assert (coordinates, positions) == ([Coordinates.Pixels], [pixels])
    line.set_coordinates(Coordinates.Data)
    process_events()
    assert line.position() == pytest.approx(data, abs=0.05)


def test_a_pixels_line_stays_put_on_zoom(shown_host, orientation):
    line = make(shown_host, orientation)
    process_events()
    line.set_coordinates(Coordinates.Pixels)
    before = line.position()
    for plot in plots_of(shown_host):
        plot.y_axis().set_range(R(-5.0, 5.0))
    if isinstance(shown_host, SciQLopPlot):
        shown_host.x_axis().set_range(R(0.0, 100.0))
    else:
        shown_host.set_time_axis_range(R(0.0, 100.0))
    process_events()
    assert line.position() == before


def test_the_delete_key_asks_once(shown_host, orientation):
    line = make(shown_host, orientation)
    asked = []
    line.delete_requested.connect(lambda: asked.append(True))
    line.set_selected(True)
    process_events()
    QTest.keyClick(canvas_of(plots_of(shown_host)[0]), Qt.Key.Key_Delete)
    process_events()
    assert asked == [True]


def test_remove_deletes_it(host, orientation):
    line = make(host, orientation)
    host.remove(line)
    assert not shiboken6.isValid(line)


def test_the_factory_raises_off_thread(host):
    out = {}

    def call():
        try:
            host.straight_line(1.0, Qt.Orientation.Vertical)
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


@pytest.mark.parametrize("name", ["position", "read_only", "coordinates"])
def test_on_resolves_with_a_signal(host, name):
    assert getattr(make(host, "vertical").on, name).signal is not None


def test_the_old_line_classes_keep_position_as_a_property(plot):
    """Guard: SciQLop reads line.position on the old classes."""
    with pytest.warns(DeprecationWarning):
        vertical = SciQLopVerticalLine(plot, 1.0)
    with pytest.warns(DeprecationWarning):
        horizontal = SciQLopHorizontalLine(plot, 0.5)
    vertical.position = 2.0
    assert (vertical.position, horizontal.position) == (2.0, 0.5)


def clamped_pixels_line(shown_host):
    line = shown_host.straight_line(5.0, Qt.Orientation.Vertical)
    line.set_min_value(1.0)
    line.set_max_value(5.0)
    process_events()
    line.set_coordinates(Coordinates.Pixels)
    return line


def test_the_data_clamps_do_not_apply_in_pixels(shown_host):
    line = clamped_pixels_line(shown_host)
    line.set_position(400.0)
    assert line.position() == pytest.approx(400.0)


def test_the_clamps_apply_again_back_in_data(shown_host):
    line = clamped_pixels_line(shown_host)
    line.set_coordinates(Coordinates.Data)
    line.set_position(50.0)
    assert line.position() == 5.0
