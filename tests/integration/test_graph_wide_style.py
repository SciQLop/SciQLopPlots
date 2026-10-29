"""API review 4.9: one line width / marker for a whole line graph, now and for later components."""
import numpy as np

from SciQLopPlots import GraphMarkerShape, SciQLopPlotRange

X = np.linspace(0.0, 10.0, 50)
TWO = np.column_stack([np.sin(X), np.cos(X)])


def test_applies_to_every_component(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    graph.set_line_width(3.0)
    graph.set_marker_shape(GraphMarkerShape.Circle)
    graph.set_marker_size(7.0)
    assert [(c.line_width(), c.marker_shape(), c.marker_size()) for c in graph.components()] == \
        [(3.0, GraphMarkerShape.Circle, 7.0)] * 2
    assert (graph.line_width(), graph.marker_shape(), graph.marker_size()) == (3.0, GraphMarkerShape.Circle, 7.0)


def test_applies_to_components_that_arrive_later(qtbot, plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(lambda start, stop: [X, TWO])
    graph.set_line_width(4.0)
    qtbot.waitUntil(lambda: len(graph.components()) == 2, timeout=3000)
    assert [c.line_width() for c in graph.components()] == [4.0, 4.0]


def test_a_column_or_component_setting_still_wins(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    graph.set_column_marker_shape(1, GraphMarkerShape.Square)
    graph.set_marker_shape(GraphMarkerShape.Circle)
    graph.components()[0].set_line_width(9.0)
    graph.set_line_width(2.0)
    graph.components()[0].set_line_width(9.0)
    assert [c.marker_shape() for c in graph.components()] == [GraphMarkerShape.Circle, GraphMarkerShape.Square]
    assert [c.line_width() for c in graph.components()] == [9.0, 2.0]
