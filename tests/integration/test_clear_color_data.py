"""API review 3.8: clear_color_data() / has_color_data() on line graphs and curves."""
import numpy as np
import pytest

from SciQLopPlots import ColorGradient, GraphType

X = np.linspace(0.0, 10.0, 50)
MAKERS = {
    "line": lambda p: p.plot(X, np.sin(X)),
    "curve": lambda p: p.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve),
}


@pytest.mark.parametrize("kind", MAKERS)
def test_clear_color_data_keeps_the_gradient(plot, kind):
    graph = MAKERS[kind](plot)
    graph.set_color_data(X, ColorGradient.Viridis)
    assert graph.has_color_data()
    graph.clear_color_data()
    assert not graph.has_color_data()
    assert graph.color_gradient() == ColorGradient.Viridis


def test_an_uncoloured_graph_has_no_color_data(plot):
    assert not MAKERS["line"](plot).has_color_data()
