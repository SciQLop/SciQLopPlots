"""plot.remove / panel.remove (part B)."""
import pytest
import shiboken6
from PySide6.QtCore import QPointF
from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, SciQLopPlotRange as R,
                          SciQLopVerticalLine, SciQLopVerticalSpan)
from conftest import process_events


def _panel_with_plot(panel):
    plot = panel.create_plot()
    process_events()
    return plot


def test_panel_span_marks_its_per_plot_parts(panel):
    plot = _panel_with_plot(panel)
    span = MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    line = MultiPlotsVerticalLine(panel, 1.5)
    process_events()
    parts = plot.findChildren(SciQLopVerticalSpan) + plot.findChildren(SciQLopVerticalLine)
    assert parts and all(p.property("_sciqlop_panel_part") for p in parts)


from PySide6.QtCore import QRectF
from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlot, SciQLopTextItem
from conftest import collect, off_thread, on_destroyed


ITEM_MAKERS = {
    "text": lambda p: p.text("t", QPointF(10, 10)),
    "ellipse": lambda p: p.ellipse(QRectF(0, 0, 4, 4)),
    "vertical_line": lambda p: p.vertical_line(1.0),
    "vertical_span": lambda p: p.vertical_span(R(1.0, 2.0)),
    "rectangular_span": lambda p: p.rectangular_span(R(1.0, 2.0), R(1.0, 2.0)),
    "ctor_text": lambda p: SciQLopTextItem(p, "t", QPointF(1, 1)),
    "ctor_line": lambda p: SciQLopVerticalLine(p, 1.0),
}


@pytest.mark.parametrize("kind", ITEM_MAKERS)
def test_remove_deletes_an_item_now(plot, kind):
    item = ITEM_MAKERS[kind](plot)
    gone = on_destroyed(item)
    plot.remove(item)
    assert gone == [True]
    assert not shiboken6.isValid(item)
    collect()  # a constructor-built (Python-owned) item must not be freed twice


def test_remove_deletes_a_plottable(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    n = len(plot.plottables())
    plot.remove(graph)
    assert len(plot.plottables()) == n - 1
    assert not shiboken6.isValid(graph)


def test_remove_deletes_a_group_and_a_group_drops_a_removed_member(plot, sample_data):
    x, y = sample_data
    a, b = plot.line(x, y), plot.line(x, y)
    group = plot.group("g", [a, b])
    plot.remove(a)
    assert len(group.members()) == 1
    plot.remove(group)
    assert not shiboken6.isValid(group)


def test_remove_rejects_another_plots_item(qtbot, plot):
    other = SciQLopPlot()
    other.setObjectName("other")
    qtbot.addWidget(other)
    item = other.text("t", QPointF(1, 1))
    with pytest.raises(ValueError, match="belongs to another plot.*other"):
        plot.remove(item)
    assert shiboken6.isValid(item)


def test_remove_rejects_another_plots_plottable(qtbot, plot, sample_data):
    other = SciQLopPlot()
    qtbot.addWidget(other)
    x, y = sample_data
    graph = other.line(x, y)
    with pytest.raises(ValueError, match="another plot"):
        plot.remove(graph)
    assert shiboken6.isValid(graph)


def test_remove_rejects_none(plot):
    with pytest.raises(ValueError, match="None"):
        plot.remove(None)


def test_remove_rejects_a_panel_part(panel):
    plot = _panel_with_plot(panel)
    span = MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    process_events()
    part = plot.findChildren(SciQLopVerticalSpan)[0]
    with pytest.raises(ValueError, match="panel"):
        plot.remove(part)


def test_remove_rejects_the_legend_and_a_component(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    with pytest.raises(TypeError):
        plot.remove(plot.legend())
    with pytest.raises(TypeError):
        plot.remove(graph.components()[0])


def test_remove_twice_raises_not_crashes(plot):
    item = plot.text("t", QPointF(1, 1))
    plot.remove(item)
    with pytest.raises(RuntimeError):
        plot.remove(item)


def test_remove_from_the_items_own_signal(plot):
    line = plot.vertical_line(1.0, True)
    line.position_changed.connect(lambda *_: plot.remove(line))
    line.set_position(2.0)
    process_events()
    assert not shiboken6.isValid(line)


def test_remove_a_span_from_its_own_signal(plot):
    span = plot.vertical_span(R(1.0, 2.0))
    span.range_changed.connect(lambda *_: plot.remove(span))
    span.set_range(R(3.0, 4.0))
    process_events()
    assert not shiboken6.isValid(span)


def test_remove_off_thread_lands_on_the_gui_thread(plot):
    item = plot.text("t", QPointF(1, 1))
    gone = on_destroyed(item)
    off_thread(lambda: plot.remove(item))
    assert gone == []
    process_events()
    assert gone == [True]


def test_remove_off_thread_is_dropped_if_the_item_dies_first(plot):
    item = plot.text("t", QPointF(1, 1))
    off_thread(lambda: plot.remove(item))
    shiboken6.delete(item)
    process_events()  # the queued call finds a null QPointer and does nothing


def test_remove_works_on_a_projection_pane():
    nd = SciQLopNDProjectionPlot(3)
    pane = nd.subplot(0)
    item = pane.text("t", QPointF(1, 1))
    gone = on_destroyed(item)
    pane.remove(item)
    assert gone == [True]


def test_pane_item_dies_with_the_projection_plot():
    nd = SciQLopNDProjectionPlot(3)
    item = nd.subplot(0).text("t", QPointF(1, 1))
    shiboken6.delete(nd)
    collect()
    assert not shiboken6.isValid(item)
