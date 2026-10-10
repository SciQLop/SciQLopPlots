"""set_color_data with a non-numeric array raises its own error on every graph type.

The buffer converter set a TypeError, then shiboken converted the gradient enum with
that error pending and raised "SystemError: bad argument to internal function" instead.
"""
import numpy as np
import pytest

from SciQLopPlots import ColorGradient, GraphType

N = 50
x = np.linspace(0, 1, N)
GRAPHS = {
    "line": lambda p: p.plot(x, np.sin(x)),
    "multi_line": lambda p: p.plot(x, np.column_stack([x, x])),
    "curve": lambda p: p.plot(x, np.sin(x), graph_type=GraphType.ParametricCurve, labels=["c"]),
}


@pytest.mark.parametrize("make", GRAPHS.values(), ids=GRAPHS.keys())
def test_non_numeric_colour_data_raises_type_error(plot, make):
    graph = make(plot)
    with pytest.raises(TypeError, match="dtype|numeric"):
        graph.set_color_data(np.array(["a"] * N), ColorGradient.Jet)


@pytest.mark.parametrize("make", GRAPHS.values(), ids=GRAPHS.keys())
def test_none_turns_colouring_off(plot, make):
    graph = make(plot)
    graph.set_color_data(np.linspace(0, 1, N), ColorGradient.Jet)
    graph.set_color_data(None, ColorGradient.Jet)
