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
