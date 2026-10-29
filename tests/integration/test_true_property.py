"""PySide's true_property switch turns our Q_PROPERTY declarations into Python properties.

The switch is per module: this one turns it on, the library's own code keeps seeing methods.
"""
import PySide6  # noqa: F401  (installs __feature__)
from __feature__ import true_property  # noqa: F401

import numpy as np
import pytest
from PySide6.QtCore import QPointF

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotRange
from conftest import record

X = np.linspace(0.0, 10.0, 20)


@pytest.fixture
def plot(qtbot):
    p = SciQLopPlot()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def panel(qtbot):
    p = SciQLopMultiPlotPanel()
    qtbot.addWidget(p)
    return p


DRAWN = {
    "graph": lambda p: p.plot(X, np.sin(X)),
    "colour map": lambda p: p.colormap(X, X, np.outer(X, X)),
    "text item": lambda p: p.text("t", QPointF(10, 10)),
    "span": lambda p: p.span(SciQLopPlotRange(1.0, 2.0)),
    "line": lambda p: p.vertical_line(1.0),
}


@pytest.mark.parametrize("kind", DRAWN)
def test_drawn_objects_have_properties(plot, kind):
    item = DRAWN[kind](plot)
    seen = record(item.name_changed)
    item.name = "renamed"
    item.visible = False
    item.selected = True
    assert (item.name, item.visible, item.selected, seen) == ("renamed", False, True, ["renamed"])


@pytest.mark.parametrize("make", [lambda f: f.plot, lambda f: f.panel], ids=["plot", "panel"])
def test_plots_and_panels_have_a_name_property(request, make):
    host = make(type("F", (), {"plot": request.getfixturevalue("plot"),
                               "panel": request.getfixturevalue("panel")}))
    host.name = "main"
    assert host.name == "main"


def test_the_setters_stay_for_connect(plot):
    graph = plot.plot(X, np.sin(X))
    other = plot.plot(X, np.cos(X))
    graph.visible_changed.connect(other.set_visible)
    graph.visible = False
    assert other.visible is False


def test_the_library_still_works_under_the_switch(panel):
    plot, graph = panel.plot(X, np.sin(X))
    graph.on.name >> plot.on.name
    graph.name = "piped"
    assert plot.name == "piped"
