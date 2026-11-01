"""A C++ argument error reaches Python with the class that says what kind of mistake it was."""
import numpy as np
import pytest

import SciQLopPlots as S


@pytest.fixture
def plot(qtbot):
    p = S.SciQLopPlot()
    qtbot.addWidget(p)
    return p


def test_invalid_argument_is_value_error(plot):
    # set_intervals_coded has no hand-written catch: shiboken's own exception handling translates it.
    one = lambda v, dtype=np.float64: np.array([v], dtype=dtype)
    timeline = plot.add_timeline()
    with pytest.raises(ValueError, match="float64"):
        timeline.set_intervals_coded(one(20.0, np.float32), one(30.0), one(0.0), ["A"], one(0.0), ["x"], ["l"],
                                     one(7.0))


def test_unsupported_call_still_not_implemented(plot):
    cmap = plot.colormap(np.arange(3.0), np.arange(2.0), np.zeros((3, 2)))
    with pytest.raises(NotImplementedError):
        cmap.set_data(np.arange(3.0), np.arange(3.0))

