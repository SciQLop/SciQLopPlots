"""Plottables under SciQLopDrawnObject: the bugs the move fixes."""
import numpy as np
import pytest
from PySide6.QtCore import QCoreApplication, QEvent
from PySide6.QtGui import QColor, QImage, QPen

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange
from conftest import off_thread, process_events, record

X = np.linspace(0.0, 10.0, 50)
TWO = np.column_stack([np.sin(X), np.cos(X)])


def test_selecting_a_graph_selects_its_components(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    graph.set_selected(True)
    assert [c.selected() for c in graph.components()] == [True, True]
    assert graph.selected()


def test_hiding_a_timeline_signals_once(ts_plot):
    timeline = ts_plot.add_timeline()
    seen = record(timeline.visible_changed)
    timeline.set_visible(False)
    timeline.set_visible(False)
    assert (timeline.visible(), seen) == (False, [False])


def test_the_name_property_goes_through_set_name(qtbot, plot):
    """m_user_named is set: a callback graph's late components then take the name."""
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(lambda start, stop: [X, np.sin(X)])
    graph.name = "speed"
    qtbot.waitUntil(lambda: len(graph.components()) == 1, timeout=3000)
    assert graph.components()[0].name() == "speed"


def test_renaming_a_plottable_signals_once(plot):  # guard
    graph = plot.plot(X, np.sin(X), labels=["a"])
    seen = record(graph.name_changed)
    graph.name = "trace"
    graph.set_name("trace")
    assert (graph.name, seen) == ("trace", ["trace"])


def test_a_graph_colour_points_to_colors(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    with pytest.raises(NotImplementedError, match="use colors"):
        graph.color()
    with pytest.raises(NotImplementedError, match="use colors.*set_colors"):
        graph.set_color(QColor("red"))


def test_a_queued_set_on_a_graph_of_a_deleted_plot_is_dropped(qtbot):  # Review Focus 2
    import shiboken6
    plot = SciQLopPlot()
    graph = plot.plot(X, np.sin(X), labels=["a"])
    component = graph.components()[0]
    signals = [record(graph.visible_changed), record(component.visible_changed)]
    off_thread(lambda: graph.set_visible(False))
    plot.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    process_events()  # the graph was the queued call's context: Qt dropped the call with it
    assert (shiboken6.isValid(graph), signals) == (False, [[], []])


def _late_graph(plot, kind):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    if kind == "line":
        return plot.plot(lambda start, stop: [X, TWO])
    return plot.parametric_curve(lambda start, stop: [X, TWO])


@pytest.mark.parametrize("kind", ["line", "curve"])
def test_hiding_a_graph_before_its_first_data_keeps_it_hidden(qtbot, plot, kind):
    graph = _late_graph(plot, kind)
    assert graph.components() == []
    seen = record(graph.visible_changed)
    graph.set_visible(False)
    assert (graph.visible(), seen) == (False, [False])
    qtbot.waitUntil(lambda: len(graph.components()) == 2, timeout=3000)
    assert [c.visible() for c in graph.components()] == [False, False]
    assert (graph.visible(), seen) == (False, [False])


@pytest.mark.parametrize("kind", ["line", "curve"])
def test_selecting_a_graph_before_its_first_data_keeps_it_selected(qtbot, plot, kind):
    graph = _late_graph(plot, kind)
    seen = record(graph.selection_changed)
    graph.set_selected(True)
    assert (graph.selected(), seen) == (True, [True])
    qtbot.waitUntil(lambda: len(graph.components()) == 2, timeout=3000)
    assert [c.selected() for c in graph.components()] == [True, True]
    assert seen == [True]


def test_a_colour_map_colour_points_to_the_gradient(plot, sample_colormap_data):
    colormap = plot.colormap(*sample_colormap_data)
    with pytest.raises(NotImplementedError, match="use gradient.*set_gradient"):
        colormap.set_color(QColor("red"))


def test_a_timeline_colour_points_to_category_colours(ts_plot):
    with pytest.raises(NotImplementedError, match="use category_color.*set_category_colors"):
        ts_plot.add_timeline().color()


def test_a_components_colour_also_colours_its_markers(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    component.set_color(QColor("red"))
    assert component.marker_pen().color() == QColor("red")


def test_a_components_colour_recolours_a_marker_pen_it_already_has(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    component.set_marker_pen(QPen(QColor("blue")))
    component.set_color(QColor("red"))
    assert component.marker_pen().color() == QColor("red")


def _curve(plot):
    return plot.parametric_curve(X, np.sin(X), name="curve")


def _legend_row_image(qtbot, plot, tmp_path, name):
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)
    path = str(tmp_path / f"{name}.png")
    plot.save(path, 600, 400)
    return QImage(path).copy(plot.legend().row_rect(0))


def test_selecting_a_curve_component_selects_its_legend_row(qtbot, plot, tmp_path):
    component = _curve(plot).components()[0]
    before = _legend_row_image(qtbot, plot, tmp_path, "before")
    component.set_selected(True)
    assert _legend_row_image(qtbot, plot, tmp_path, "after") != before


def test_selecting_a_curve_component_missing_from_the_legend_is_harmless(plot):
    curve = _curve(plot)
    curve.set_in_legend(False)
    component = curve.components()[0]
    component.set_selected(True)
    assert component.selected()
