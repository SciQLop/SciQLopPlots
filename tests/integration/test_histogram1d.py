"""1-D histogram (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (HistogramNormalization, HistogramStyle, SciQLopHistogram,
                          SciQLopPlot)


class TestHistogramClass:
    def test_bins_must_be_positive(self, plot):
        h = plot.histogram(np.random.default_rng(0).normal(size=100))
        with pytest.raises(ValueError):
            h.set_bins(0)

    def test_bad_range_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_bin_range(5.0, 1.0)
        h.set_log_bins(True)
        with pytest.raises(ValueError):
            h.set_bin_range(0.0, 10.0)

    def test_log_bins_set_the_x_axis_to_log(self, plot):
        h = plot.histogram(np.arange(1.0, 100.0))
        h.set_log_bins(True)
        assert plot.x_axis().log()

    def test_integer_values_are_binned(self, qtbot, plot):
        h = plot.histogram(np.array([1, 1, 2, 3], dtype=np.int32))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
        assert sum(h.bin_heights()[0]) == 4


def _wait_result(qtbot, h):
    qtbot.waitUntil(lambda: len(h.bin_heights()) > 0, timeout=5000)


class TestHistogramBehaviour:
    def test_factory_returns_a_histogram(self, plot):
        assert isinstance(plot.histogram(np.arange(10.0)), SciQLopHistogram)

    def test_edges_have_one_more_entry_than_heights(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=10)
        _wait_result(qtbot, h)
        assert len(h.bin_edges()) == 11
        assert len(h.bin_heights()[0]) == 10

    def test_one_column_defaults_to_bars(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        _wait_result(qtbot, h)
        assert h.style() == HistogramStyle.Bars

    def test_several_columns_default_to_step(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_data(np.arange(10.0), np.random.default_rng(0).normal(size=(10, 2)))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert h.style() == HistogramStyle.Step

    def test_explicit_style_wins(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_style(HistogramStyle.Bars)
        h.set_data(np.arange(10.0), np.random.default_rng(0).normal(size=(10, 2)))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert h.style() == HistogramStyle.Bars

    def test_column_major_columns_are_binned_apart(self, qtbot, plot):
        h = plot.histogram(np.arange(10.0))
        y = np.asfortranarray(np.column_stack([np.zeros(10), np.ones(10) * 5.0]))
        h.set_data(np.arange(10.0), y)
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 2, timeout=5000)
        assert [sum(c) for c in h.bin_heights()] == [10, 10]
        assert h.bin_heights()[0][0] == 10

    def test_binned_bars(self, plot):
        h = plot.histogram(np.arange(10.0))
        h.set_binned(np.array([0.0, 1.0, 2.0]), np.array([3.0, 4.0]))
        assert h.bin_edges() == [0.0, 1.0, 2.0]
        assert h.bin_heights() == [[3.0, 4.0]]

    def test_binned_size_mismatch_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_binned(np.array([0.0, 1.0, 2.0]), np.array([3.0, 4.0, 5.0]))

    def test_probability_sums_to_one(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=8)
        _wait_result(qtbot, h)
        h.set_normalization(HistogramNormalization.Probability)
        qtbot.waitUntil(lambda: abs(sum(h.bin_heights()[0]) - 1.0) < 1e-9, timeout=5000)

    def test_fixed_range_sets_the_edges(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=4)
        h.set_bin_range(0.0, 200.0)
        qtbot.waitUntil(lambda: h.bin_edges()[-1:] == [200.0], timeout=5000)
        assert h.bin_edges() == [0.0, 50.0, 100.0, 150.0, 200.0]

    def test_bins_changed_when_edges_move(self, qtbot, plot):
        h = plot.histogram(np.arange(100.0), bins=4)
        _wait_result(qtbot, h)
        with qtbot.waitSignal(h.bins_changed, timeout=5000):
            h.set_bins(8)

    def test_first_result_fits_the_x_axis(self, qtbot, plot):
        h = plot.histogram(np.arange(1000.0, 2000.0))
        _wait_result(qtbot, h)
        qtbot.waitUntil(lambda: plot.x_axis().range().start() >= 900.0, timeout=5000)
        r = plot.x_axis().range()
        assert r.start() <= h.bin_edges()[0] and r.stop() >= h.bin_edges()[-1]

    def test_a_histogram_can_leave_the_legend(self, plot):
        h = plot.histogram(np.arange(10.0), name="counts")
        assert plot.legend().row_labels() == ["counts"]
        h.set_in_legend(False)
        assert plot.legend().row_labels() == []
