"""SciQLopStraightLine: one line class, vertical or horizontal, at plot and panel scope (5.6)."""
import pytest
import shiboken6
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import (Coordinates, SciQLopHorizontalLine, SciQLopPlot, SciQLopPlotRange as R,
                          SciQLopStraightLine, SciQLopVerticalLine)
from conftest import canvas_of, off_thread, plots_of, process_events, record, scoped_host

SCOPES = ["plot", "panel"]
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


def test_the_clamps_and_their_getters(host, orientation):
    line = make(host, orientation)
    assert (line.min_value(), line.max_value()) == (None, None)
    line.set_min_value(0.2)
    line.set_max_value(0.8)
    assert (line.min_value(), line.max_value()) == (0.2, 0.8)
    line.set_position(5.0)
    assert line.position() == 0.8
    line.clear_min_value()
    line.clear_max_value()
    assert (line.min_value(), line.max_value()) == (None, None)


def test_the_tool_tip_round_trips_and_signals_once(host, orientation):
    line = make(host, orientation)
    seen = record(line.tool_tip_changed)
    line.set_tool_tip("tip")
    line.set_tool_tip("tip")
    assert (line.tool_tip(), seen) == ("tip", ["tip"])


def test_on_tool_tip_has_a_signal(host):
    assert make(host, "vertical").on.tool_tip.signal is not None


def _panel(qtbot, n=2):
    panel = scoped_host("panel", qtbot)
    for _ in range(n - 2):
        panel.plot(*_xy())
    process_events()
    return panel


def _xy():
    import numpy as np
    x = np.linspace(0.0, 10.0, 200)
    return x, np.sin(x)


def _parts(panel):
    return [part for plot in plots_of(panel) for part in plot.findChildren(SciQLopStraightLine)
            if part.is_panel_part()]


def test_a_panel_line_has_one_plain_marked_part_per_plot(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    parts = _parts(panel)
    assert [type(part) for part in parts] == [SciQLopStraightLine] * 2
    assert [part.scope() for part in parts] == plots_of(panel)
    assert (line.scope(), line.is_panel_part()) == (panel, False)


def test_the_line_is_the_panels_child_and_outlives_its_python_reference(qtbot):
    from conftest import collect
    panel = _panel(qtbot)
    panel.straight_line(5.0, Qt.Orientation.Vertical)
    collect()
    assert len([c for c in panel.children() if isinstance(c, SciQLopStraightLine)]) == 1
    assert len(_parts(panel)) == 2


def test_an_edit_on_a_part_moves_the_line_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical, read_only=False)
    first, second = _parts(panel)
    seen = record(line.position_changed)
    first.set_position(3.0)
    assert (seen, line.position(), second.position()) == ([3.0], 3.0, 3.0)


def test_selecting_a_part_selects_the_line_once(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    first, second = _parts(panel)
    seen = record(line.selection_changed)
    first.set_selected(True)
    assert (line.selected(), second.selected(), seen) == (True, True, [True])


def test_the_panel_line_clamps_like_its_parts(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(3.0, Qt.Orientation.Vertical)
    line.set_min_value(2.0)
    line.set_max_value(4.0)
    line.set_position(9.0)
    assert [line.position()] + [part.position() for part in _parts(panel)] == [4.0] * 3
    assert [(part.min_value(), part.max_value()) for part in _parts(panel)] == [(2.0, 4.0)] * 2


def test_reordering_plots_keeps_one_line_part_per_plot(qtbot):
    """Review Focus 2, for lines."""
    panel = _panel(qtbot, 3)
    panel.straight_line(5.0, Qt.Orientation.Vertical)
    panel.move_plot(0, 2)
    process_events()
    assert len(_parts(panel)) == 3


def test_a_slot_removing_the_line_during_a_part_edit(qtbot):
    """Review Focus 5, for lines: the emitting part dies with the line."""
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical, read_only=False)
    first, _ = _parts(panel)
    line.position_changed.connect(lambda *_: panel.remove(line))
    first.set_position(3.0)
    process_events()
    assert not shiboken6.isValid(line)
    assert _parts(panel) == []


def test_panel_remove_deletes_the_line_and_its_parts(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    parts = _parts(panel)
    panel.remove(line)
    assert not shiboken6.isValid(line)
    assert not any(shiboken6.isValid(part) for part in parts)


def test_plot_remove_refuses_a_part_and_the_panel_line(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.plot_at(0).remove(_parts(panel)[0])
    with pytest.raises(ValueError, match=r"panel\.remove"):
        panel.plot_at(0).remove(line)


def _state_of(part):
    return (part.position(), part.orientation(), part.coordinates(), part.read_only(),
            part.tool_tip(), part.color(), part.line_width(), part.line_style(), part.visible(),
            part.selected(), part.min_value(), part.max_value())


def test_a_part_built_later_carries_the_lines_state_and_a_removed_plot_takes_its_part(qtbot):
    """Guard: pins make_part's transfer, passes before the test was written."""
    panel = _panel(qtbot)
    line = panel.straight_line(3.0, Qt.Orientation.Horizontal, color=QColor("red"),
                               read_only=False, tool_tip="tip")
    line.set_min_value(2.0)
    line.set_max_value(4.0)
    line.set_line_width(3.0)
    line.set_line_style(Qt.PenStyle.DashLine)
    line.set_visible(False)
    line.set_selected(True)
    panel.plot(*_xy())
    process_events()
    parts = _parts(panel)
    assert len(parts) == 3
    expected = (3.0, Qt.Orientation.Horizontal, Coordinates.Data, False, "tip", QColor("red"),
                3.0, Qt.PenStyle.DashLine, False, True, 2.0, 4.0)
    assert [_state_of(part) for part in parts] == [expected] * 3
    gone = parts[-1]
    panel.remove_plot(panel.plot_at(2))
    process_events()
    assert not shiboken6.isValid(gone)
    assert [_state_of(part) for part in _parts(panel)] == [expected] * 2
