"""plot(..., name=) works for every graph type (#114)."""
import numpy as np
import pytest

from SciQLopPlots import GraphType, SciQLopPlotRange

X = np.linspace(0.0, 10.0, 100)


@pytest.mark.parametrize("graph_type", [GraphType.Line, GraphType.Scatter,
                                        GraphType.ParametricCurve])
def test_name_kwarg_names_the_graph(plot, graph_type):
    graph = plot.plot(X, np.sin(X), graph_type=graph_type, name="n")
    assert graph.objectName() == "n"


def test_name_kwarg_names_a_multi_column_line(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), name="n")
    assert graph.objectName() == "n"


def test_name_kwarg_names_a_callback_line(plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(lambda start, stop: [X, np.sin(X)], name="n")
    assert graph.objectName() == "n"


def test_name_kwarg_still_names_a_colormap(plot):
    y = np.linspace(0.0, 1.0, 10)
    cmap = plot.plot(X, y, np.random.rand(len(y), len(X)), name="n")
    assert cmap.objectName() == "n"
