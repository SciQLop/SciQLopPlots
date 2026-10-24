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
    span = panel.vertical_span(R(1.0, 2.0))
    line = panel.vertical_line(1.5)
    process_events()
    parts = plot.findChildren(SciQLopVerticalSpan) + plot.findChildren(SciQLopVerticalLine)
    assert parts and all(p.property("_sciqlop_panel_part") for p in parts)


from PySide6.QtCore import QRectF
from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlot, SciQLopTextItem
from conftest import collect, off_thread, on_destroyed


def _constructed(cls, *args):
    def make(plot):
        with pytest.warns(DeprecationWarning):
            return cls(plot, *args)
    return make


ITEM_MAKERS = {
    "text": lambda p: p.text("t", QPointF(10, 10)),
    "ellipse": lambda p: p.ellipse(QRectF(0, 0, 4, 4)),
    "vertical_line": lambda p: p.vertical_line(1.0),
    "vertical_span": lambda p: p.vertical_span(R(1.0, 2.0)),
    "rectangular_span": lambda p: p.rectangular_span(R(1.0, 2.0), R(1.0, 2.0)),
    "ctor_text": _constructed(SciQLopTextItem, "t", QPointF(1, 1)),
    "ctor_line": _constructed(SciQLopVerticalLine, 1.0),
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
    span = panel.vertical_span(R(1.0, 2.0))
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


def _off_thread_errors(call):
    errors = []

    def run():
        try:
            call()
        except Exception as e:
            errors.append(e)

    off_thread(run)
    return errors


def test_remove_off_thread_is_dropped_if_the_item_dies_first(plot):
    item = plot.text("t", QPointF(1, 1))
    other = plot.text("u", QPointF(2, 2))
    item_gone, other_gone = on_destroyed(item), on_destroyed(other)
    assert _off_thread_errors(lambda: plot.remove(item)) == []
    shiboken6.delete(item)
    assert _off_thread_errors(lambda: plot.remove(other)) == []
    process_events()  # both queued calls run, in order: the first finds a null QPointer
    assert item_gone == [True]
    assert other_gone == [True]
    assert not shiboken6.isValid(other)


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


def test_panel_factories_return_panel_owned_objects(panel):
    plot = _panel_with_plot(panel)
    panel.vertical_span(R(1.0, 2.0), tool_tip="s", id="a")
    panel.vertical_line(1.5)
    import gc; gc.collect(); process_events()
    spans = panel.findChildren(MultiPlotsVerticalSpan)
    lines = panel.findChildren(MultiPlotsVerticalLine)
    assert len(spans) == 1 and len(lines) == 1
    assert spans[0].parent() is panel and lines[0].parent() is panel
    assert shiboken6.isValid(spans[0]) and shiboken6.isValid(lines[0])
    assert len(plot.findChildren(SciQLopVerticalSpan)) == 1
    assert len(plot.findChildren(SciQLopVerticalLine)) == 1


def test_panel_remove_deletes_a_span_and_its_parts(panel):
    plot = _panel_with_plot(panel)
    span = panel.vertical_span(R(1.0, 2.0))
    process_events()
    panel.remove(span)
    assert not shiboken6.isValid(span)
    assert plot.findChildren(SciQLopVerticalSpan) == []


def test_panel_remove_deletes_a_line(panel):
    _panel_with_plot(panel)
    line = panel.vertical_line(1.5)
    panel.remove(line)
    assert not shiboken6.isValid(line)


def test_panel_remove_removes_a_plot(panel):
    plot = _panel_with_plot(panel)
    panel.remove(plot)
    collect()
    assert len(panel.plots()) == 0


def test_panel_remove_accepts_handles(panel):
    from SciQLopPlots import MultiPlotsVSpanCollection
    _panel_with_plot(panel)
    collection = MultiPlotsVSpanCollection(panel)
    handle = collection.create_span(R(1.0, 2.0))
    panel.remove(handle)
    panel.remove(panel.plots()[0])
    collect()
    assert len(panel.plots()) == 0


def test_panel_remove_rejects_foreign_and_none(qtbot, panel):
    from SciQLopPlots import SciQLopMultiPlotPanel
    other = SciQLopMultiPlotPanel()
    qtbot.addWidget(other)
    foreign = other.vertical_span(R(1.0, 2.0))
    with pytest.raises(ValueError):
        panel.remove(foreign)
    with pytest.raises(ValueError):
        panel.remove(other.create_plot())
    with pytest.raises(ValueError):
        panel.remove(None)


def test_part_built_for_a_later_plot_is_refused(panel):
    from SciQLopPlots import SciQLopPlot
    _panel_with_plot(panel)
    panel.vertical_span(R(1.0, 2.0))
    late = SciQLopPlot()
    panel.add_plot(late)
    process_events()
    part = late.findChildren(SciQLopVerticalSpan)[0]
    with pytest.raises(ValueError, match="panel"):
        late.remove(part)


def test_remove_plottable_warns_and_raises_for_foreign(qtbot, plot, sample_data):
    other = SciQLopPlot()
    qtbot.addWidget(other)
    x, y = sample_data
    graph = other.line(x, y)
    with pytest.warns(DeprecationWarning, match=r"remove_plottable\(\.\.\.\) is deprecated: use plot\.remove"):
        with pytest.raises(ValueError):
            plot.remove_plottable(graph)
    assert shiboken6.isValid(graph)


@pytest.mark.parametrize("arg", [None, 99, "nope"])
def test_remove_plottable_noops_stay(plot, arg):
    with pytest.warns(DeprecationWarning):
        plot.remove_plottable(arg)


def test_remove_group_warns(plot, sample_data):
    x, y = sample_data
    group = plot.group("g", [plot.line(x, y)])
    with pytest.warns(DeprecationWarning, match=r"remove_group\(\.\.\.\) is deprecated"):
        plot.remove_group(group)
    assert not shiboken6.isValid(group)


def test_deprecation_names_the_callers_line(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    with pytest.warns(DeprecationWarning) as record:
        plot.remove_plottable(graph)
    assert record[0].filename == __file__
