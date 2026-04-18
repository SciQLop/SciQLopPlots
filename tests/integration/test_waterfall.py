"""Integration tests for SciQLopWaterfallGraph."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot, SciQLopWaterfallGraph


@pytest.fixture(scope="module")
def app():
    inst = QApplication.instance()
    return inst if inst else QApplication([])


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def _make_2d(n=100, cols=3):
    x = np.linspace(0, 1, n).astype(np.float64)
    y = np.column_stack([np.sin(x * (k + 1)) for k in range(cols)]).astype(np.float64)
    return x, y


class TestWaterfallConstruction:
    def test_add_waterfall_returns_correct_type(self, plot):
        wf = plot.add_waterfall("test")
        assert isinstance(wf, SciQLopWaterfallGraph)

    def test_set_data_2d(self, plot):
        wf = plot.add_waterfall("test", labels=["c0", "c1", "c2"])
        x, y = _make_2d()
        wf.set_data(x, y)
        assert wf.line_count() == 3
