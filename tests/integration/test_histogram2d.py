"""Integration tests for SciQLopHistogram2D wrapper."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication

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


class TestHistogram2DCreation:
    def test_add_histogram2d_returns_correct_type(self, plot):
        hist = plot.add_histogram2d("test")
        assert isinstance(hist, SciQLopHistogram2D)

    def test_default_bins(self, plot):
        hist = plot.add_histogram2d("test")
        assert hist.key_bins() == 100
        assert hist.value_bins() == 100

    def test_custom_bins(self, plot):
        hist = plot.add_histogram2d("test", 50, 60)
        assert hist.key_bins() == 50
        assert hist.value_bins() == 60

    def test_histogram_in_plottables(self, plot):
        hist = plot.add_histogram2d("test")
        assert len(plot.plottables()) == 1
        assert plot.plottables()[0] is hist


class TestHistogram2DData:
    def test_set_data_float64(self, plot):
        hist = plot.add_histogram2d("test")
        x = np.random.default_rng(0).normal(0, 1, 1000)
        y = np.random.default_rng(1).normal(0, 1, 1000)
        hist.set_data(x, y)
        data = hist.data()
        assert len(data) == 2
        np.testing.assert_array_equal(np.array(data[0]), x)
        np.testing.assert_array_equal(np.array(data[1]), y)

    def test_set_data_float32(self, plot):
        hist = plot.add_histogram2d("test")
        x = np.float32(np.random.default_rng(0).normal(0, 1, 500))
        y = np.float32(np.random.default_rng(1).normal(0, 1, 500))
        hist.set_data(x, y)
        data = hist.data()
        assert len(data) == 2

    def test_set_data_int32(self, plot):
        hist = plot.add_histogram2d("test")
        x = np.int32(np.arange(100))
        y = np.int32(np.arange(100))
        hist.set_data(x, y)
        data = hist.data()
        assert len(data) == 2

    def test_empty_data_returns_empty(self, plot):
        hist = plot.add_histogram2d("test")
        assert hist.data() == []


class TestHistogram2DProperties:
    def test_set_bins(self, plot):
        hist = plot.add_histogram2d("test")
        hist.set_bins(30, 40)
        assert hist.key_bins() == 30
        assert hist.value_bins() == 40

    def test_normalization_default_none(self, plot):
        hist = plot.add_histogram2d("test")
        assert hist.normalization() == 0

    def test_set_normalization_column(self, plot):
        hist = plot.add_histogram2d("test")
        hist.set_normalization(1)
        assert hist.normalization() == 1

    def test_set_normalization_back_to_none(self, plot):
        hist = plot.add_histogram2d("test")
        hist.set_normalization(1)
        hist.set_normalization(0)
        assert hist.normalization() == 0

    def test_name(self, plot):
        hist = plot.add_histogram2d("my_hist")
        assert hist.name == "my_hist"


class TestHistogram2DReplot:
    def test_replot_empty_no_crash(self, plot):
        plot.add_histogram2d("test")
        plot.replot(True)

    def test_replot_with_data_no_crash(self, plot):
        hist = plot.add_histogram2d("test", 50, 50)
        rng = np.random.default_rng(42)
        hist.set_data(rng.normal(0, 1, 5000), rng.normal(0, 1, 5000))
        plot.replot(True)

    def test_replot_after_bins_change_no_crash(self, plot):
        hist = plot.add_histogram2d("test")
        rng = np.random.default_rng(42)
        hist.set_data(rng.normal(0, 1, 5000), rng.normal(0, 1, 5000))
        hist.set_bins(20, 20)
        plot.replot(True)
