"""Histogram2D logarithmic bin spacing (set_x_bins_log / set_y_bins_log)."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QCoreApplication

from SciQLopPlots import SciQLopPlot


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


def _pump_events(n=50):
    for _ in range(n):
        QCoreApplication.processEvents()


class TestLogBinScale:
    def test_defaults_linear(self, plot):
        hist = plot.add_histogram2d("defaults", 20, 20)
        assert hist.x_bins_log() is False
        assert hist.y_bins_log() is False

    def test_set_x_bins_log_roundtrips(self, plot):
        hist = plot.add_histogram2d("xlog", 20, 20)
        hist.set_x_bins_log(True)
        assert hist.x_bins_log() is True
        assert hist.y_bins_log() is False
        hist.set_x_bins_log(False)
        assert hist.x_bins_log() is False

    def test_set_y_bins_log_roundtrips(self, plot):
        hist = plot.add_histogram2d("ylog", 20, 20)
        hist.set_y_bins_log(True)
        assert hist.y_bins_log() is True
        assert hist.x_bins_log() is False

    def test_log_bins_changed_signal(self, plot):
        hist = plot.add_histogram2d("sig", 20, 20)
        seen = []
        hist.x_bins_log_changed.connect(seen.append)
        hist.set_x_bins_log(True)
        _pump_events(5)
        assert seen == [True]
        # No re-emit when unchanged.
        hist.set_x_bins_log(True)
        _pump_events(5)
        assert seen == [True]

    def test_log_bins_render_decade_data_no_crash(self, plot):
        """Log bins on log axes with decade-spanning data must render cleanly."""
        rng = np.random.default_rng(3)
        x = 10.0 ** rng.uniform(0, 4, 5000)
        y = 10.0 ** rng.uniform(0, 4, 5000)

        hist = plot.add_histogram2d("logdata", 40, 40)
        plot.x_axis().set_log(True)
        plot.y_axis().set_log(True)
        hist.set_x_bins_log(True)
        hist.set_y_bins_log(True)
        hist.set_data(x, y)
        _pump_events()
        plot.replot(True)
        _pump_events()
        assert hist.visible() is True

    def test_non_positive_with_log_bins_no_crash(self, plot):
        """Non-positive samples are dropped under log binning without crashing."""
        x = np.array([-5.0, 1.0, 10.0, 100.0, 0.0, 1000.0])
        y = np.array([1.0, 2.0, 3.0, 4.0, 5.0, 6.0])

        hist = plot.add_histogram2d("nonpos", 10, 10)
        hist.set_x_bins_log(True)
        hist.set_data(x, y)
        _pump_events()
        plot.replot(True)
        _pump_events()
