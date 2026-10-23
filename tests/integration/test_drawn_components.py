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


def _render(plot, tmp_path):
    from PySide6.QtGui import QImage
    plot.replot(True)
    path = str(tmp_path / "legend.png")
    plot.save_png(path, plot.width(), plot.height())
    return QImage(path)


def _border_pixels(image, rect):
    """(y, colour) along the legend item's left border, where the frame's left side runs and no
    icon or text is drawn."""
    return [(y, image.pixelColor(x, y))
            for y in range(rect.top(), rect.bottom() + 1)
            for x in range(rect.left() - 2, rect.left() + 3)]


@pytest.fixture
def shown_pair(qtbot, plot, tmp_path):
    """A shown two-line group, and its legend's selection frame (colour, row bands) as NeoQCP
    draws it: the pixels that change on the item's left border when row a alone is framed."""
    import collections
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    first = graph.components()[0]
    first.set_selected(True)  # also expands the group: both renders share one layout
    image = _render(plot, tmp_path)
    rect = plot.legend().row_rect(0)
    framed = _border_pixels(image, rect)
    first.set_selected(False)
    plain = _border_pixels(_render(plot, tmp_path), rect)
    changed = [(y, c) for (y, c), (_, p) in zip(framed, plain) if c != p]
    colour = collections.Counter(c.rgb() for _, c in changed).most_common(1)[0][0]
    top, bottom = changed[0][0], changed[-1][0]  # row a's frame; NeoQCP gives rows one height
    return graph, colour, rect, [(top, bottom), (bottom, 2 * bottom - top)]


def _framed_rows(plot, tmp_path, shown_pair):
    """Which of the group's rows (a, b) the legend's selection frame reaches."""
    _, colour, rect, bands = shown_pair
    border = _border_pixels(_render(plot, tmp_path), rect)
    return tuple(any(c.rgb() == colour for y, c in border if top + 2 <= y < bottom - 2)
                 for top, bottom in bands)


def test_every_selected_component_shows_in_the_legend(plot, shown_pair, tmp_path):
    graph = shown_pair[0]
    graph.set_selected(True)
    assert _framed_rows(plot, tmp_path, shown_pair) == (True, True)


def test_deselecting_one_component_leaves_the_other_framed(plot, shown_pair, tmp_path):
    graph = shown_pair[0]
    graph.set_selected(True)
    graph.components()[1].set_selected(False)
    assert _framed_rows(plot, tmp_path, shown_pair) == (True, False)


def test_an_unselected_group_frames_no_row(plot, shown_pair, tmp_path):
    assert _framed_rows(plot, tmp_path, shown_pair) == (False, False)


def test_set_pen_off_the_gui_thread_lands_on_it_and_signals_once(plot):
    from PySide6.QtGui import QPen
    from conftest import off_thread, process_events
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.color_changed)
    off_thread(lambda: component.set_pen(QPen(QColor("red"))))
    queued = component.pen().color() != QColor("red")
    process_events()
    assert (queued, component.pen().color(), seen) == (True, QColor("red"), [QColor("red")])
