"""Callback line/scatter graphs size and style their components from the data
they return, not from the number of labels supplied.

Regression context: ``SciQLopPlot::plot_impl`` used to pick the backing graph
class for a callable by ``labels.size()``. A multi-component callback called
without ``labels`` was routed to the single-line fast-path, which then *rejected*
the multi-column batch outright ("y must hold exactly one value per x sample") —
the plot rendered nothing. Routing every line/scatter callable to the data-sized
class fixes that, but the per-component styling (scatter ``NoLine``, marker,
palette colour) and auto-naming must be (re)applied once the components are
created from the first data batch, since they do not exist at ``plot()`` time.
"""
import numpy as np
import pytest

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange, GraphLineStyle


def _three_component(start, stop):
    x = np.linspace(start, stop, 100)
    return x, np.column_stack([np.sin(x), np.cos(x), np.sin(2 * x)])


def _scalar(start, stop):
    x = np.linspace(start, stop, 100)
    return x, np.sin(x)


def _trigger(plot):
    """Set an x range so the callable graphs request their first data batch."""
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))


def _name(g):
    n = g.name
    return n() if callable(n) else n


def test_multicomponent_callback_without_labels_sizes_from_data(plot, qtbot):
    g = plot.line(_three_component)  # NO labels
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    assert g.line_count() == 3


def test_scalar_callback_without_labels_is_one_line(plot, qtbot):
    g = plot.line(_scalar)  # NO labels
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=5000)
    assert g.line_count() == 1


def test_multicomponent_scatter_callback_components_have_no_line(plot, qtbot):
    g = plot.scatter(_three_component, labels=["a", "b", "c"])
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    styles = [c.line_style() for c in g.components()]
    assert all(s == GraphLineStyle.NoLine for s in styles), styles


def test_scalar_scatter_callback_without_labels_has_no_line(plot, qtbot):
    g = plot.scatter(_scalar)  # NO labels
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=5000)
    assert g.components()[0].line_style() == GraphLineStyle.NoLine


def test_two_scalar_callables_get_distinct_colors(plot, qtbot):
    g1 = plot.line(_scalar)
    g2 = plot.line(_scalar)
    _trigger(plot)
    qtbot.waitUntil(lambda: g1.line_count() == 1 and g2.line_count() == 1,
                    timeout=5000)
    c1 = g1.components()[0].color().name()
    c2 = g2.components()[0].color().name()
    assert c1 != c2, (c1, c2)


def test_unlabeled_components_autonamed_from_graph_name(plot, qtbot):
    g = plot.line(_three_component)
    g.set_name("b_gse")  # name set before data arrives (the natural flow)
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    assert g.labels() == ["b_gse[0]", "b_gse[1]", "b_gse[2]"]


def test_unlabeled_scalar_autonamed_from_graph_name(plot, qtbot):
    g = plot.line(_scalar)
    g.set_name("amplitude")
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=5000)
    assert g.labels() == ["amplitude"]


def test_explicit_labels_win_over_autonaming(plot, qtbot):
    g = plot.line(_three_component, labels=["bx", "by", "bz"])
    g.set_name("b_gse")
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    assert g.labels() == ["bx", "by", "bz"]


def test_multicomponent_scatter_without_labels_sizes_from_data(plot, qtbot):
    g = plot.scatter(_three_component)  # scatter shares the Line branch
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    assert g.line_count() == 3


def test_unnamed_multicomponent_does_not_leak_placeholder_id(plot, qtbot):
    # An unnamed graph keeps an auto-generated placeholder id ("Line0", …) used
    # internally — it must NOT bleed into the user-facing component labels.
    g = plot.line(_three_component)  # no name, no labels
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=5000)
    placeholder = _name(g)
    leaked = [lbl for lbl in g.labels() if placeholder and placeholder in lbl]
    assert leaked == [], (placeholder, g.labels())


def test_unnamed_scalar_label_is_not_placeholder_id(plot, qtbot):
    g = plot.line(_scalar)  # no name, no labels
    _trigger(plot)
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=5000)
    assert g.labels() != [_name(g)], g.labels()
