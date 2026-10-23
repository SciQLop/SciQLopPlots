"""Rule 2: an aggregate reads its parts and signals only when that reading flips."""
import numpy as np

from conftest import process_events, record

X = np.linspace(0.0, 10.0, 50)
TWO = np.column_stack([np.sin(X), np.cos(X)])


def _two(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    return graph, graph.components()


def test_hiding_components_one_by_one_hides_the_graph_once(plot):
    graph, (a, b) = _two(plot)
    seen = record(graph.visible_changed)
    a.set_visible(False)
    assert seen == []  # b still shows
    b.set_visible(False)
    assert seen == [False]


def test_selecting_components_one_by_one_selects_the_graph_once(plot):
    graph, (a, b) = _two(plot)
    seen = record(graph.selection_changed)
    a.set_selected(True)  # what a click on its legend row does
    b.set_selected(True)
    assert seen == [True]


def test_selecting_a_partly_selected_graph_selects_both_silently(plot):  # guard after Task 2
    graph, (a, b) = _two(plot)
    a.set_selected(True)
    seen = record(graph.selection_changed)
    graph.set_selected(True)
    assert (a.selected(), b.selected(), seen) == (True, True, [])


def test_a_removed_component_reseeds_the_graph(plot):
    graph, (a, _) = _two(plot)
    a.set_visible(False)
    graph.set_data(X, np.sin(X)[:, None])  # one column: the second component goes
    process_events()
    assert len(graph.components()) == 1 and not graph.visible()
    seen = record(graph.visible_changed)
    graph.components()[0].set_visible(True)
    assert seen == [True]


def test_hiding_every_pane_curve_hides_the_projection_once(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(projection.components()) == 3, timeout=3000)
    seen = record(projection.visible_changed)
    for component in projection.components():
        component.set_visible(False)
    assert seen == [False]
