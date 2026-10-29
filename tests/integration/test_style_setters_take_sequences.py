"""API review 1.13: the per-point style setters take any 1-D sequence: array, list or tuple."""
import numpy as np
import pytest

from SciQLopPlots import GraphType

X = np.linspace(0.0, 10.0, 50)
SHAPES = {"array": lambda v: np.asarray(v), "int array": lambda v: np.asarray(v).astype(int),
          "list": list, "tuple": tuple}


@pytest.mark.parametrize("shape", SHAPES)
def test_curve_time_and_colour_values(plot, shape):
    curve = plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve)
    curve.set_time_values(SHAPES[shape](range(50)))
    curve.set_color_values(SHAPES[shape](range(50)))


@pytest.mark.parametrize("shape", SHAPES)
def test_waterfall_offsets(plot, shape):
    waterfall = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), graph_type=GraphType.Waterfall)
    waterfall.set_offsets(SHAPES[shape]([0, 1]))
    assert list(waterfall.offsets()) == [0.0, 1.0]
