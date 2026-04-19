"""Integration tests for SciQLopHistogram2D wrapper."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import (
    SciQLopPlot,
    SciQLopHistogram2D,
    SciQLopMultiPlotPanel,
    GraphType,
)


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


class TestHistogram2DVisibility:
    def test_set_visible_roundtrips(self, plot):
        hist = plot.add_histogram2d("test")
        assert hist.visible() is True
        hist.set_visible(False)
        assert hist.visible() is False
        hist.set_visible(True)
        assert hist.visible() is True


class TestHistogram2DLogScale:
    def test_set_z_log_scale_roundtrips(self, plot):
        hist = plot.add_histogram2d("test")
        hist.set_z_log_scale(True)
        assert hist.z_log_scale() is True
        hist.set_z_log_scale(False)
        assert hist.z_log_scale() is False

    def test_set_y_log_scale_roundtrips(self, plot):
        hist = plot.add_histogram2d("test")
        hist.set_y_log_scale(True)
        assert hist.y_log_scale() is True
        hist.set_y_log_scale(False)
        assert hist.y_log_scale() is False


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


class TestHistogram2DCallable:
    def test_callable_histogram2d_returns_interface(self, plot):
        rng = np.random.default_rng(0)

        def producer(start, stop):
            return rng.normal(0, 1, 500), rng.normal(0, 1, 500)

        hist = plot.histogram2d(producer, name="cb")
        assert hist is not None

    def test_callable_custom_bins(self, plot):
        def producer(start, stop):
            return np.arange(100.0), np.arange(100.0)

        hist = plot.histogram2d(producer, name="cb", key_bins=25, value_bins=30)
        assert hist.key_bins() == 25
        assert hist.value_bins() == 30


class TestHistogram2DPanel:
    def test_panel_histogram2d_static(self, app):
        panel = SciQLopMultiPlotPanel()
        rng = np.random.default_rng(0)
        x = rng.normal(0, 1, 300)
        y = rng.normal(0, 1, 300)
        p, hist = panel.histogram2d(x, y, name="static_panel")
        assert p is not None
        assert hist is not None
        del panel

    def test_panel_histogram2d_callable(self, app):
        panel = SciQLopMultiPlotPanel()
        rng = np.random.default_rng(0)

        def producer(start, stop):
            return rng.normal(0, 1, 300), rng.normal(0, 1, 300)

        p, hist = panel.histogram2d(producer, name="cb_panel",
                                     key_bins=40, value_bins=40)
        assert p is not None
        assert hist is not None
        del panel


class TestHistogram2DDispatcher:
    def test_plot_dispatcher_static(self, plot):
        rng = np.random.default_rng(0)
        x = rng.normal(0, 1, 400)
        y = rng.normal(0, 1, 400)
        hist = plot.plot(x, y, graph_type=GraphType.Histogram2D,
                         name="disp_static", key_bins=30, value_bins=30)
        assert hist is not None
        assert hist.key_bins() == 30

    def test_plot_dispatcher_callable(self, plot):
        rng = np.random.default_rng(0)

        def producer(start, stop):
            return rng.normal(0, 1, 400), rng.normal(0, 1, 400)

        hist = plot.plot(producer, graph_type=GraphType.Histogram2D,
                         name="disp_cb", key_bins=30, value_bins=30)
        assert hist is not None
        assert hist.key_bins() == 30

    def test_panel_plot_dispatcher_static(self, app):
        panel = SciQLopMultiPlotPanel()
        rng = np.random.default_rng(0)
        x = rng.normal(0, 1, 300)
        y = rng.normal(0, 1, 300)
        result = panel.plot(x, y, graph_type=GraphType.Histogram2D,
                            name="panel_disp", key_bins=20, value_bins=20)
        assert result is not None
        del panel

    def test_reject_histogram2d_kwargs_for_line(self, plot):
        x = np.arange(10.0)
        y = np.arange(10.0)
        with pytest.raises(TypeError, match="Histogram2D"):
            plot.plot(x, y, graph_type=GraphType.Line, key_bins=10)

    def test_reject_waterfall_kwargs_for_histogram2d(self, plot):
        rng = np.random.default_rng(0)
        x = rng.normal(0, 1, 100)
        y = rng.normal(0, 1, 100)
        with pytest.raises(TypeError, match="Waterfall"):
            plot.plot(x, y, graph_type=GraphType.Histogram2D, gain=2.0)
