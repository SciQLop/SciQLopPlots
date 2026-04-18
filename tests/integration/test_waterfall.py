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


class TestWaterfallRawValueAt:
    def test_raw_value_at_row_major(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.ascontiguousarray(np.column_stack([x, 2 * x, 3 * x]))  # row-major
        wf.set_data(x, y)
        # At key=0.5 (idx 5), component 1 should be 2*0.5 = 1.0
        assert wf.raw_value_at(1, 0.5) == pytest.approx(1.0)

    def test_raw_value_at_column_major(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.asfortranarray(np.column_stack([x, 2 * x, 3 * x]))
        wf.set_data(x, y)
        assert wf.raw_value_at(2, 0.5) == pytest.approx(1.5)

    def test_raw_value_at_normalize_still_returns_raw(self, plot):
        wf = plot.add_waterfall("w", labels=["c0"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.sin(x * 3.14159).astype(np.float64)
        wf.set_data(x, y)
        wf.set_normalize(True)
        wf.set_gain(100.0)
        # Raw value is pre-normalize, pre-gain — should match raw sin.
        assert wf.raw_value_at(0, 0.5) == pytest.approx(np.sin(0.5 * 3.14159))

    def test_raw_value_at_out_of_range_is_finite(self, plot):
        """Binary search clamps to array bounds, so out-of-range keys map to
        nearest endpoint — a finite value."""
        wf = plot.add_waterfall("w", labels=["c0"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.ones_like(x)
        wf.set_data(x, y)
        assert wf.raw_value_at(0, 999.0) == 1.0   # clamped to last
        assert wf.raw_value_at(0, -999.0) == 1.0  # clamped to first

    def test_raw_value_at_invalid_component_is_nan(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.column_stack([x, 2 * x]).astype(np.float64)
        wf.set_data(x, y)
        import math
        assert math.isnan(wf.raw_value_at(5, 0.5))  # component out of range


from SciQLopPlots import GraphType


class TestWaterfallPlotDispatch:
    def test_plot_graph_type_waterfall(self, plot):
        x, y = _make_2d(cols=4)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, labels=["a", "b", "c", "d"])
        assert isinstance(wf, SciQLopWaterfallGraph)
        assert wf.line_count() == 4

    def test_offsets_float_sets_uniform_mode(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, offsets=2.5,
                       labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform
        assert wf.uniform_spacing() == 2.5

    def test_offsets_array_sets_custom_mode(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall,
                       offsets=[0.0, 1.5, 4.0], labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Custom
        assert list(wf.offsets()) == [0.0, 1.5, 4.0]

    def test_offsets_none_default(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform
        assert wf.uniform_spacing() == 1.0

    def test_offsets_array_wrong_len_raises(self, plot):
        x, y = _make_2d(cols=3)
        with pytest.raises(ValueError):
            plot.plot(x, y, graph_type=GraphType.Waterfall,
                      offsets=[0.0, 1.0], labels=["a", "b", "c"])

    def test_normalize_gain_kwargs(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall,
                       normalize=False, gain=3.0, labels=["a", "b", "c"])
        assert wf.normalize() is False
        assert wf.gain() == 3.0

    def test_callable_variant(self, plot):
        from SciQLopPlots import SciQLopWaterfallGraphFunction

        def cb(start, stop):
            n = 50
            x = np.linspace(start, stop, n).astype(np.float64)
            y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
            return x, y

        wf = plot.plot(cb, graph_type=GraphType.Waterfall, labels=["a", "b"])
        assert isinstance(wf, SciQLopWaterfallGraphFunction)
