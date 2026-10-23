"""Components under SciQLopDrawnObject."""
import numpy as np
import pytest
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


def test_a_pen_colour_change_signals_color_changed_once(plot):
    from PySide6.QtGui import QPen
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.color_changed)
    component.set_pen(QPen(QColor("red")))
    component.set_pen(QPen(QColor("red"), 3))  # same colour: nothing to say
    component.set_line_width(2)
    assert (component.color(), seen) == (QColor("red"), [QColor("red")])


def test_an_ellipse_pen_and_a_text_font_colour_signal_color_changed(plot):
    from PySide6.QtCore import QPointF, QRectF
    from PySide6.QtGui import QPen
    from SciQLopPlots import SciQLopEllipseItem, SciQLopTextItem
    ellipse = SciQLopEllipseItem(plot, QRectF(10, 10, 20, 20))
    text = SciQLopTextItem(plot, "t", QPointF(10, 10))
    seen = record(ellipse.color_changed), record(text.color_changed)
    ellipse.set_pen(QPen(QColor("red")))
    text.set_font_color(QColor("blue"))
    assert seen == ([QColor("red")], [QColor("blue")])


SELECTION_PEN = (0, 0, 255)  # QCPLegend's default selectedIconBorderPen


def _is_selection_pen(colour):
    return (colour.red(), colour.green(), colour.blue()) == SELECTION_PEN


def _framed_rows(plot, tmp_path):
    """Which of a two-line group's rows (a, b) the legend's selection frame reaches."""
    from PySide6.QtGui import QImage
    plot.replot(True)
    path = str(tmp_path / "legend.png")
    plot.save_png(path, plot.width(), plot.height())
    image = QImage(path)
    item = plot.legend().row_rect(0)
    row = item.height() // 3  # header, a, b

    def framed(top, bottom):
        return any(_is_selection_pen(image.pixelColor(x, y))
                   for y in range(top + 2, bottom - 2) for x in range(item.left() - 2, item.left() + 3))

    return framed(item.top() + row, item.top() + 2 * row), framed(item.top() + 2 * row, item.bottom())


@pytest.fixture
def shown_pair(qtbot, plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    return graph


def test_every_selected_component_shows_in_the_legend(plot, shown_pair, tmp_path):
    shown_pair.set_selected(True)
    assert _framed_rows(plot, tmp_path) == (True, True)


def test_deselecting_one_component_leaves_the_other_framed(plot, shown_pair, tmp_path):
    shown_pair.set_selected(True)
    shown_pair.components()[1].set_selected(False)
    assert _framed_rows(plot, tmp_path) == (True, False)


def test_set_pen_off_the_gui_thread_lands_on_it_and_signals_once(plot):
    from PySide6.QtGui import QPen
    from conftest import off_thread, process_events
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.color_changed)
    off_thread(lambda: component.set_pen(QPen(QColor("red"))))
    queued = component.pen().color() != QColor("red")
    process_events()
    assert (queued, component.pen().color(), seen) == (True, QColor("red"), [QColor("red")])
