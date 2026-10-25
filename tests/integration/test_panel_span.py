"""Panel-scope spans: parts follow the plots, user edits flow up once (API review 5.6, Plan A)."""
import numpy as np
import pytest
import shiboken6
from PySide6.QtCore import QPoint, Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import (SciQLopMultiPlotPanel, SciQLopNDProjectionPlot, SciQLopPlot,
                          SciQLopPlotRange as R, SciQLopSpan)
from conftest import canvas_of, collect, process_events, record

X = np.linspace(0.0, 10.0, 200)


def _panel(qtbot, n=2):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    for i in range(n):
        panel.plot(X, np.sin(X + i))
    panel.set_time_axis_range(R(0.0, 10.0))
    process_events()
    return panel


def _parts(panel, span):
    """The plot-scope parts `span` built, in plot order."""
    return [part for i in range(panel.size())
            for part in panel.plot_at(i).findChildren(SciQLopSpan)
            if part.is_panel_part() and part.id() == span.id()]


def test_one_plain_marked_part_per_plot(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    parts = _parts(panel, span)
    assert len(parts) == 2
    assert all(type(part) is SciQLopSpan and part.is_panel_part() for part in parts)
    assert [part.scope() for part in parts] == [panel.plot_at(0), panel.plot_at(1)]
    assert (span.scope(), span.is_panel_part()) == (panel, False)


def test_the_span_is_the_panels_child_and_outlives_its_python_reference(qtbot):
    panel = _panel(qtbot)
    panel.span(key_range=R(2.0, 8.0))
    collect()
    [span] = [child for child in panel.children() if isinstance(child, SciQLopSpan)]
    assert len(_parts(panel, span)) == 2


def test_parts_follow_plots_joining_and_leaving(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    panel.plot(X, np.cos(X))
    process_events()
    assert len(_parts(panel, span)) == 3
    panel.remove_plot(panel.plot_at(0))
    collect()
    assert len(_parts(panel, span)) == 2


def test_parts_built_later_carry_the_current_state(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    span = panel.span(key_range=R(2.0, 8.0), color=QColor("red"))
    span.set_key_range(R(3.0, 4.0))
    span.set_read_only(True)
    span.set_tool_tip("tip")
    span.set_visible(False)
    span.set_line_width(5.0)
    panel.plot(X, np.sin(X))
    process_events()
    [part] = _parts(panel, span)
    assert (part.key_range(), part.color(), part.read_only(), part.tool_tip(), part.visible(),
            part.line_width()) == (R(3.0, 4.0), QColor("red"), True, "tip", False, 5.0)


def test_a_plot_destroyed_under_the_span_is_survived(qtbot):
    """The doomed plot is built in Python: shiboken6.delete of a panel.plot() plot leaves a stale
    wrapper that crashes the panel's own destruction (a crash that predates spans)."""
    panel = _panel(qtbot)
    doomed = SciQLopPlot()
    panel.add_plot(doomed)
    process_events()
    span = panel.span(key_range=R(2.0, 8.0))
    assert len(_parts(panel, span)) == 3
    shiboken6.delete(doomed)
    collect()
    span.set_key_range(R(3.0, 4.0))
    span.set_color(QColor("red"))
    assert [(part.key_range(), part.color()) for part in _parts(panel, span)] == \
        [(R(3.0, 4.0), QColor("red"))] * 2


def test_an_edit_on_a_part_reaches_the_span_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    first.set_key_range(R(3.0, 4.0))
    assert (ranges, keys) == ([R(3.0, 4.0)], [R(3.0, 4.0)])
    assert span.key_range() == second.key_range() == R(3.0, 4.0)


def test_a_rectangular_part_edit_reaches_both_axes(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0), value_range=R(-0.5, 0.5))
    first, second = _parts(panel, span)
    values = record(span.value_range_changed)
    first.set_value_range(R(0.1, 0.2))
    assert values == [R(0.1, 0.2)]
    assert second.value_range() == R(0.1, 0.2)


def test_selecting_a_part_selects_the_span_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    seen = record(span.selection_changed)
    first.set_selected(True)
    assert (span.selected(), second.selected(), seen) == (True, True, [True])


def test_one_drag_on_a_part_emits_once_and_moves_the_other_part(qtbot):
    panel = _panel(qtbot)
    panel.resize(800, 600)
    panel.show()
    qtbot.waitExposed(panel)
    process_events()
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    panel.replot(True)
    process_events()
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    canvas = canvas_of(panel.plot_at(0))
    start = QPoint(canvas.width() // 2, canvas.height() // 2)
    end = start + QPoint(40, 0)
    QTest.mousePress(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, start)
    QTest.mouseMove(canvas, end)
    QTest.mouseRelease(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, end)
    process_events()
    assert (len(ranges), len(keys)) == (1, 1)
    assert ranges[0] != R(2.0, 8.0)
    assert span.key_range() == first.key_range() == second.key_range() == ranges[0]


def test_reordering_plots_keeps_one_part_per_plot(qtbot):
    """Review Focus 2."""
    panel = _panel(qtbot, 3)
    span = panel.span(key_range=R(2.0, 8.0))
    panel.move_plot(0, 2)
    process_events()
    assert len(_parts(panel, span)) == 3


def test_a_projection_plot_in_the_panel_gets_no_part(qtbot):
    """Review Focus 3."""
    panel = _panel(qtbot)
    projection = SciQLopNDProjectionPlot(3)
    panel.add_plot(projection)
    process_events()
    span = panel.span(key_range=R(2.0, 8.0))
    span.set_key_range(R(3.0, 4.0))
    assert [part.key_range() for part in _parts(panel, span)] == [R(3.0, 4.0)] * 2
    assert projection.findChildren(SciQLopSpan) == []


def test_a_slot_removing_the_span_during_a_part_edit(qtbot):
    """Review Focus 5: the emitting part dies with the span."""
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, _ = _parts(panel, span)
    span.range_changed.connect(lambda *_: panel.remove(span))
    first.set_key_range(R(3.0, 4.0))
    process_events()
    assert not shiboken6.isValid(span)
    assert [p for i in range(panel.size()) for p in panel.plot_at(i).findChildren(SciQLopSpan)] == []


def test_panel_remove_deletes_the_span_and_its_parts(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    parts = _parts(panel, span)
    panel.remove(span)
    assert not shiboken6.isValid(span)
    assert not any(shiboken6.isValid(part) for part in parts)


def test_plot_remove_refuses_a_part(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.plot_at(0).remove(_parts(panel, span)[0])


def test_plot_remove_refuses_a_panel_span_naming_panel_remove(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match=r"panel\.remove"):
        panel.plot_at(0).remove(span)


def test_panel_remove_refuses_a_plot_span_naming_plot_remove(qtbot):
    panel = _panel(qtbot)
    plot_span = panel.plot_at(0).span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match=r"plot\.remove"):
        panel.remove(plot_span)


def test_panel_remove_refuses_a_part(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.remove(_parts(panel, span)[0])
