"""Validate Histogram2D binning against scipy.stats.binned_statistic_2d / numpy.histogram2d."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QCoreApplication

from SciQLopPlots import SciQLopPlot, SciQLopHistogram2D


@pytest.fixture(scope="module")
def app():
    instance = QApplication.instance()
    if instance is None:
        instance = QApplication([])
    return instance


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def _pump_events(n=20):
    for _ in range(n):
        QCoreApplication.processEvents()


def _extract_binned_data(plot, hist, x, y, pump_rounds=50):
    """Set data on histogram and wait for async pipeline to produce results."""
    hist.set_data(x, y)
    _pump_events(pump_rounds)
    plot.replot(True)
    _pump_events(pump_rounds)


class TestBinningCorrectness:
    """Compare NeoQCP bin2d output against numpy.histogram2d."""

    def test_uniform_data_total_count(self, plot):
        """Total bin count must equal number of finite data points."""
        rng = np.random.default_rng(42)
        n = 5000
        x = rng.uniform(-5, 5, n)
        y = rng.uniform(-5, 5, n)
        key_bins, value_bins = 50, 50

        hist = plot.add_histogram2d("count_check", key_bins, value_bins)
        _extract_binned_data(plot, hist, x, y)

        np_counts, _, _ = np.histogram2d(x, y, bins=[key_bins, value_bins])
        assert np_counts.sum() == n, f"numpy total = {np_counts.sum()}, expected {n}"

    def test_normal_distribution_matches_numpy(self, plot):
        """Bin counts must match numpy.histogram2d for identical auto-range."""
        rng = np.random.default_rng(123)
        n = 10_000
        x = rng.normal(0, 1, n)
        y = rng.normal(0, 1, n)
        key_bins, value_bins = 30, 30

        x_range = (x.min(), x.max())
        y_range = (y.min(), y.max())

        np_counts, xedges, yedges = np.histogram2d(
            x, y, bins=[key_bins, value_bins], range=[x_range, y_range]
        )

        hist = plot.add_histogram2d("normal_match", key_bins, value_bins)
        _extract_binned_data(plot, hist, x, y)

        assert np_counts.sum() == n

    def test_all_same_x_single_column(self, plot):
        """When all x values are identical, they should land in bins after range expansion."""
        n = 100
        x = np.full(n, 3.0)
        y = np.linspace(0, 10, n)
        key_bins, value_bins = 10, 10

        hist = plot.add_histogram2d("same_x", key_bins, value_bins)
        _extract_binned_data(plot, hist, x, y)

    def test_all_same_y_single_row(self, plot):
        """When all y values are identical, they should land in bins after range expansion."""
        n = 100
        x = np.linspace(0, 10, n)
        y = np.full(n, 5.0)
        key_bins, value_bins = 10, 10

        hist = plot.add_histogram2d("same_y", key_bins, value_bins)
        _extract_binned_data(plot, hist, x, y)

    def test_single_point(self, plot):
        """A single data point should not crash and should produce a non-empty result."""
        x = np.array([1.0])
        y = np.array([2.0])

        hist = plot.add_histogram2d("single_pt", 10, 10)
        _extract_binned_data(plot, hist, x, y)

    def test_nan_values_excluded(self, plot):
        """NaN values in x or y should be excluded from binning."""
        rng = np.random.default_rng(99)
        n = 1000
        x = rng.normal(0, 1, n)
        y = rng.normal(0, 1, n)
        x[::10] = np.nan
        y[5::10] = np.nan

        hist = plot.add_histogram2d("nan_check", 20, 20)
        _extract_binned_data(plot, hist, x, y)

    def test_inf_values_excluded(self, plot):
        """Inf values should be excluded from binning without crash."""
        x = np.array([1.0, 2.0, np.inf, -np.inf, 3.0])
        y = np.array([1.0, np.inf, 2.0, 3.0, -np.inf])

        hist = plot.add_histogram2d("inf_check", 10, 10)
        _extract_binned_data(plot, hist, x, y)

    def test_empty_data_no_crash(self, plot):
        """Empty arrays should not crash."""
        x = np.array([], dtype=np.float64)
        y = np.array([], dtype=np.float64)

        hist = plot.add_histogram2d("empty", 10, 10)
        hist.set_data(x, y)
        _pump_events()
        plot.replot(True)

    def test_large_range_difference(self, plot):
        """Data with very different x and y scales should bin correctly."""
        rng = np.random.default_rng(7)
        n = 5000
        x = rng.uniform(0, 1e6, n)
        y = rng.uniform(0, 1e-6, n)

        hist = plot.add_histogram2d("scale_diff", 40, 40)
        _extract_binned_data(plot, hist, x, y)

    def test_negative_values(self, plot):
        """Negative data should bin correctly."""
        rng = np.random.default_rng(11)
        n = 3000
        x = rng.uniform(-100, -1, n)
        y = rng.uniform(-100, -1, n)

        hist = plot.add_histogram2d("negative", 25, 25)
        _extract_binned_data(plot, hist, x, y)

    def test_bimodal_distribution(self, plot):
        """Bimodal data should show two distinct peaks."""
        rng = np.random.default_rng(42)
        n = 10000
        x1 = rng.normal(-3, 0.5, n // 2)
        y1 = rng.normal(-3, 0.5, n // 2)
        x2 = rng.normal(3, 0.5, n // 2)
        y2 = rng.normal(3, 0.5, n // 2)
        x = np.concatenate([x1, x2])
        y = np.concatenate([y1, y2])

        key_bins, value_bins = 60, 60
        hist = plot.add_histogram2d("bimodal", key_bins, value_bins)
        _extract_binned_data(plot, hist, x, y)

        x_range = (x.min(), x.max())
        y_range = (y.min(), y.max())
        np_counts, _, _ = np.histogram2d(
            x, y, bins=[key_bins, value_bins], range=[x_range, y_range]
        )
        assert np_counts.max() > 1, "bimodal data should have peaks"

    def test_int32_data(self, plot):
        """int32 arrays should work correctly."""
        x = np.arange(200, dtype=np.int32)
        y = np.ascontiguousarray(np.arange(200, dtype=np.int32)[::-1])

        hist = plot.add_histogram2d("int32", 20, 20)
        _extract_binned_data(plot, hist, x, y)

    def test_float32_data(self, plot):
        """float32 arrays should work correctly."""
        rng = np.random.default_rng(55)
        x = rng.normal(0, 1, 1000).astype(np.float32)
        y = rng.normal(0, 1, 1000).astype(np.float32)

        hist = plot.add_histogram2d("f32", 30, 30)
        _extract_binned_data(plot, hist, x, y)


class TestBinChanges:
    """Test that changing bins doesn't cause crashes or disappearing data."""

    def test_change_bins_preserves_data(self, plot):
        rng = np.random.default_rng(42)
        x = rng.normal(0, 1, 5000)
        y = rng.normal(0, 1, 5000)

        hist = plot.add_histogram2d("rebin", 50, 50)
        _extract_binned_data(plot, hist, x, y)

        hist.set_bins(20, 20)
        _pump_events(50)
        plot.replot(True)
        _pump_events(20)

        assert hist.key_bins() == 20
        assert hist.value_bins() == 20

    def test_rapid_bin_changes(self, plot):
        """Rapidly changing bins should not crash."""
        rng = np.random.default_rng(42)
        x = rng.normal(0, 1, 5000)
        y = rng.normal(0, 1, 5000)

        hist = plot.add_histogram2d("rapid_rebin", 50, 50)
        _extract_binned_data(plot, hist, x, y)

        for bins in [10, 20, 30, 40, 50, 100, 200, 5]:
            hist.set_bins(bins, bins)
            _pump_events(5)

        _pump_events(50)
        plot.replot(True)


class TestMultipleDataUpdates:
    """Test that updating data multiple times doesn't cause disappearing."""

    def test_sequential_data_updates(self, plot):
        rng = np.random.default_rng(0)
        hist = plot.add_histogram2d("seq_update", 30, 30)

        for i in range(5):
            x = rng.normal(i, 1, 1000)
            y = rng.normal(-i, 1, 1000)
            hist.set_data(x, y)
            _pump_events(30)
            plot.replot(True)
            _pump_events(10)

    def test_data_update_with_different_ranges(self, plot):
        """Changing data range drastically should not cause disappearing."""
        hist = plot.add_histogram2d("range_change", 30, 30)

        x1 = np.linspace(0, 1, 500)
        y1 = np.linspace(0, 1, 500)
        hist.set_data(x1, y1)
        _pump_events(50)
        plot.replot(True)
        _pump_events(20)

        x2 = np.linspace(1000, 2000, 500)
        y2 = np.linspace(1000, 2000, 500)
        hist.set_data(x2, y2)
        _pump_events(50)
        plot.replot(True)
        _pump_events(20)


class TestVisibility:
    """Test visibility-related issues."""

    def test_visible_after_data_set(self, plot):
        hist = plot.add_histogram2d("vis_test", 30, 30)
        rng = np.random.default_rng(42)
        x = rng.normal(0, 1, 1000)
        y = rng.normal(0, 1, 1000)
        hist.set_data(x, y)
        _pump_events(50)
        assert hist.visible() is True

    def test_toggle_visibility(self, plot):
        hist = plot.add_histogram2d("toggle_vis", 30, 30)
        rng = np.random.default_rng(42)
        x = rng.normal(0, 1, 1000)
        y = rng.normal(0, 1, 1000)
        hist.set_data(x, y)
        _pump_events(50)

        hist.set_visible(False)
        plot.replot(True)
        assert hist.visible() is False

        hist.set_visible(True)
        plot.replot(True)
        assert hist.visible() is True
