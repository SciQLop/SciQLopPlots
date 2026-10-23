"""Rule 2: an aggregate reads its parts and signals only when that reading flips."""
import numpy as np
import pytest
from PySide6.QtGui import QColor
from SciQLopPlots import ColorGradient

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


FIVE = np.column_stack([X] * 5)


def _curve(plot, columns):
    return plot.parametric_curve(X, np.column_stack([np.sin(X)] * columns))


def test_shrinking_past_hidden_leading_columns_is_safe_for_a_line_graph(plot):
    graph = plot.plot(X, FIVE, labels=list("abcde"))
    for component in graph.components()[:-1]:
        component.set_visible(False)
    graph.set_data(X, np.sin(X)[:, None])
    process_events()
    assert len(graph.components()) == 1 and not graph.visible()


def test_shrinking_past_hidden_leading_columns_is_safe_for_a_curve(plot, qtbot):
    curve = _curve(plot, 5)
    for component in curve.components()[:-1]:
        component.set_visible(False)
    curve.set_data(X, np.sin(X)[:, None])  # the curve's data lands asynchronously
    qtbot.waitUntil(lambda: len(curve.components()) == 1, timeout=3000)
    assert not curve.visible()


def test_a_curve_follows_its_components(plot):
    curve = _curve(plot, 2)
    seen = record(curve.visible_changed)
    for component in curve.components():
        component.set_visible(False)
    assert seen == [False]


def test_removing_the_only_selected_component_is_silent(plot):
    graph, (a, b) = _two(plot)
    b.set_selected(True)
    seen = record(graph.selection_changed)
    graph.set_data(X, np.sin(X)[:, None])
    process_events()
    assert seen == []
    graph.components()[0].set_selected(True)
    assert seen == [True]


def test_graph_setters_emit_once_even_with_component_signals(plot):
    graph, _ = _two(plot)
    visible, selected = record(graph.visible_changed), record(graph.selection_changed)
    graph.set_visible(False)
    graph.set_selected(True)
    assert (visible, selected) == ([False], [True])



def _states_when_signalled(signal, parts, state):
    """What each part reads while a slot on `signal` runs: an aggregate signals once applied."""
    seen = []
    signal.connect(lambda value: seen.append((value, [getattr(p, state)() for p in parts()])))
    return seen


def test_showing_an_all_hidden_graph_signals_after_every_component_shows(plot):
    graph, _ = _two(plot)
    graph.set_visible(False)
    seen = _states_when_signalled(graph.visible_changed, graph.components, "visible")
    graph.set_visible(True)
    assert seen == [(True, [True, True])]


def test_selecting_an_unselected_graph_signals_after_every_component_is_selected(plot):
    graph, _ = _two(plot)
    seen = _states_when_signalled(graph.selection_changed, graph.components, "selected")
    graph.set_selected(True)
    assert seen == [(True, [True, True])]


def test_hiding_a_graph_still_signals_once_every_component_is_hidden(plot):  # guard
    graph, _ = _two(plot)
    seen = _states_when_signalled(graph.visible_changed, graph.components, "visible")
    graph.set_visible(False)
    assert seen == [(False, [False, False])]


def test_showing_an_all_hidden_projection_signals_after_every_pane_shows(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(projection.components()) == 3, timeout=3000)
    projection.set_visible(False)
    seen = _states_when_signalled(projection.visible_changed, projection.components, "visible")
    projection.set_visible(True)
    assert seen == [(True, [True, True, True])]


def test_deleting_the_only_selected_component_is_silent_like_a_shrinking_set_data(plot):
    import shiboken6
    graph, (a, b) = _two(plot)
    b.set_selected(True)
    seen = record(graph.selection_changed)
    shiboken6.delete(b)
    process_events()
    assert (len(graph.components()), seen) == (1, [])
    graph.components()[0].set_selected(True)
    assert seen == [True]


def test_a_graph_whose_last_selected_component_is_deleted_is_not_selected(plot):
    import shiboken6
    graph = plot.plot(X, np.sin(X), labels=["a"])
    graph.components()[0].set_selected(True)
    seen = record(graph.selection_changed)
    shiboken6.delete(graph.components()[0])
    process_events()
    assert (graph.components(), graph.selected(), seen) == ([], False, [])


def test_a_slot_deleting_the_other_component_after_the_flip_keeps_the_signal(plot):
    import shiboken6
    graph, (a, b) = _two(plot)
    seen = record(graph.visible_changed)
    a.visible_changed.connect(lambda _: shiboken6.delete(b))
    graph.set_visible(False)
    assert (len(graph.components()), graph.visible(), seen) == (1, False, [False])


def _three_pane_projection(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(projection.components()) == 3, timeout=3000)
    return nd, projection


def test_a_slot_deleting_another_pane_curve_mid_hide_keeps_the_projection_signal(qtbot):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    first, second, _ = (component.parent() for component in projection.components())
    seen = record(projection.visible_changed)
    first.visible_changed.connect(lambda _: shiboken6.delete(second))
    projection.set_visible(False)  # the first curve hides, its slot deletes the second
    assert (len(projection.components()), projection.visible(), seen) == (2, False, [False])


def _first_pane_curve(projection):
    return projection.components()[0].parent()


def test_a_slot_deleting_the_projection_mid_hide_does_not_crash(qtbot):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    _first_pane_curve(projection).visible_changed.connect(lambda _: shiboken6.delete(projection))
    projection.set_visible(False)  # _set_visible went on to update the colour scale of a dead self
    process_events()
    assert not shiboken6.isValid(projection)


def test_a_slot_deleting_the_projection_mid_select_does_not_crash(qtbot):  # guard
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    _first_pane_curve(projection).selection_changed.connect(lambda _: shiboken6.delete(projection))
    projection.set_selected(True)
    process_events()
    assert not shiboken6.isValid(projection)


def test_a_slot_deleting_the_projection_mid_rename_does_not_crash(qtbot):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    projection.components()[0].name_changed.connect(lambda _: shiboken6.delete(projection))
    projection.name = "renamed"  # the base emitted name_changed on a dead self after _named
    process_events()
    assert not shiboken6.isValid(projection)


ND_COLOUR_OPERATIONS = {
    "colors": lambda p: p.set_colors([QColor("red")] * 3),
    "time_gradient": lambda p: p.set_time_color_gradient(QColor("red"), QColor("blue")),
    "color_data": lambda p: p.set_color_data(np.linspace(0, 1, 50), ColorGradient.Viridis),
}


@pytest.mark.parametrize("name", ND_COLOUR_OPERATIONS)
def test_a_slot_deleting_the_projection_mid_colouring_does_not_crash(qtbot, name):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    first = _first_pane_curve(projection)
    first.data_changed.connect(lambda *_: shiboken6.delete(projection))
    ND_COLOUR_OPERATIONS[name](projection)
    process_events()
    assert not shiboken6.isValid(projection)


GRAPH_KINDS = {
    "line_graph": lambda plot: plot.plot(X, TWO, labels=["a", "b"]),
    "curve": lambda plot: plot.parametric_curve(X, TWO, labels=["a", "b"]),
}

PART_SETTERS = {
    "visible": ("visible_changed", lambda g: g.set_visible(False)),
    "colors": ("color_changed", lambda g: g.set_colors([QColor("red"), QColor("blue")])),
    "labels": ("name_changed", lambda g: g.set_labels(["c", "d"])),
}


@pytest.mark.parametrize("setter", PART_SETTERS)
@pytest.mark.parametrize("kind", GRAPH_KINDS)
def test_a_slot_deleting_the_graph_while_it_sets_its_parts_does_not_crash(plot, kind, setter):
    import shiboken6
    graph = GRAPH_KINDS[kind](plot)
    signal, apply = PART_SETTERS[setter]
    getattr(graph.components()[0], signal).connect(lambda *_: shiboken6.delete(graph))
    apply(graph)  # the setter went on with the parts list, or the colour scale, of a dead self
    process_events()
    assert not shiboken6.isValid(graph)
