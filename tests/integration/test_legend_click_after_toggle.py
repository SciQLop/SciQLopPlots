"""A legend-row click still selects its plottable after set_in_legend(False) then (True)."""
import numpy as np
import pytest
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from conftest import process_events
from SciQLopPlots import GraphType

X = np.arange(20.0)


def _is_selected(plottable):
    selected = plottable.selected
    return selected() if callable(selected) else selected


def _histogram(plot):
    h = plot.histogram(X, name="h")
    return h, lambda: _is_selected(h)


def _line(plot):
    g = plot.plot(X, X, name="line")
    return g, lambda: g.components()[0].selected()


def _curve(plot):
    c = plot.plot(X, X, name="curve", graph_type=GraphType.ParametricCurve)
    return c, lambda: c.components()[0].selected()


def _multi_line(plot):
    g = plot.plot(X, np.column_stack([X, 2 * X]), labels=["a", "b"])
    return g, lambda: g.components()[1].selected()


def _colormap(plot):
    c = plot.colormap(X, np.arange(5.0), np.random.default_rng(0).random((20, 5)), name="cm")
    return c, lambda: _is_selected(c)


def _histogram2d(plot):
    h = plot.add_histogram2d("h2")
    h.set_data(X, X)
    return h, lambda: _is_selected(h)


def _canvas(plot):
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


def _click_at(plot, point):
    QTest.mouseClick(_canvas(plot), Qt.LeftButton, Qt.NoModifier, point)
    process_events()


def _show(qtbot, plot):
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)


def _click_legend_row(qtbot, plot, component):
    _show(qtbot, plot)
    rect = plot.legend().row_rect(0)
    assert rect.isValid()
    if component is None:
        return _click_at(plot, rect.center())
    # A multigraph is one legend group item, so row_rect cannot address its component rows.
    header_height = rect.height()
    _click_at(plot, rect.center())  # a group header click expands the component rows
    plot.replot(True)
    _click_at(plot, QPoint(rect.center().x(), rect.top() + header_height * (component + 1.5)))


@pytest.mark.parametrize("make,component", [(_histogram, None), (_line, None), (_curve, None),
                                      (_multi_line, 1),
                                      (_colormap, None), (_histogram2d, None)])
@pytest.mark.parametrize("toggle", [False, True])
def test_legend_click_selects_after_a_legend_toggle(qtbot, plot, make, component, toggle):
    owner, selected = make(plot)
    if toggle:
        owner.set_in_legend(False)
        owner.set_in_legend(True)
    assert not selected()
    _click_legend_row(qtbot, plot, component)
    assert selected()
