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


from SciQLopPlots import WaterfallOffsetMode


class TestWaterfallKnobs:
    def test_default_offset_mode_is_uniform(self, plot):
        wf = plot.add_waterfall("w")
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform

    def test_set_offset_mode_switches(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_offset_mode(WaterfallOffsetMode.Custom)
        assert wf.offset_mode() == WaterfallOffsetMode.Custom

    def test_set_uniform_spacing(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_uniform_spacing(2.5)
        assert wf.uniform_spacing() == 2.5

    def test_set_offsets_wrong_len_raises(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x, y = _make_2d(cols=3)
        wf.set_data(x, y)
        with pytest.raises(ValueError):
            wf.set_offsets([0.0, 1.0])  # too short

    def test_set_offsets_correct_len(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x, y = _make_2d(cols=3)
        wf.set_data(x, y)
        wf.set_offsets([0.0, 1.0, 2.5])
        assert list(wf.offsets()) == [0.0, 1.0, 2.5]

    def test_set_normalize(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_normalize(False)
        assert wf.normalize() is False

    def test_set_gain(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_gain(2.0)
        assert wf.gain() == 2.0

    def test_set_gain_negative_ok(self, plot):
        """Negative gain flips polarity — valid, not an error."""
        wf = plot.add_waterfall("w")
        wf.set_gain(-1.0)
        assert wf.gain() == -1.0
