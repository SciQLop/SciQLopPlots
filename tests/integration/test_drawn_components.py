"""Components under SciQLopDrawnObject."""
import numpy as np
from PySide6.QtGui import QColor

from conftest import record

X = np.linspace(0.0, 10.0, 50)


def test_renaming_a_component_signals_once_and_renames_its_legend_row(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.name_changed)
    component.set_name("b")
    component.set_name("b")
    assert (seen, plot.legend().row_labels()) == (["b"], ["b"])


def test_a_component_colour_change_signals_once(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.color_changed)
    component.set_color(QColor("red"))
    component.set_color(QColor("red"))
    assert (component.color(), seen) == (QColor("red"), [QColor("red")])


def test_set_colors_signals_each_component(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    seen = [record(c.color_changed) for c in graph.components()]
    graph.set_colors([QColor("red"), QColor("blue")])
    assert seen == [[QColor("red")], [QColor("blue")]]


def test_clearing_a_default_named_component_clears_its_legend_row(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]))
    component = graph.components()[0]
    component.set_name("")
    assert (component.name(), plot.legend().row_labels()) == ("", [" … Component 1"])


def test_a_default_named_component_reports_the_name_neoqcp_draws(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]))
    assert [c.name() for c in graph.components()] == ["Component 0", "Component 1"]


def test_a_labelled_component_reports_its_label(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    assert [c.name() for c in graph.components()] == ["a", "b"]


def test_selecting_a_sibling_component_leaves_the_first_selected_as_neoqcp_does(plot):
    # QCPMultiGraph::setComponentSelection writes one component's selection and touches no other.
    first, second = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"]).components()
    seen = record(first.selection_changed)
    first.set_selected(True)
    second.set_selected(True)
    assert ([first.selected(), second.selected()], seen) == ([True, True], [True])
