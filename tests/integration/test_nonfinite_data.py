"""Tests for non-finite data (NaN, Inf, -Inf, fill values) in line graphs.

Reproduces the crash from the CDF workbench: Qt assertion in QPointF::operator/
when Inf pixel coordinates reach the line extruder.
"""
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


def _pump_and_replot(plot, rounds=20):
    for _ in range(rounds):
        QCoreApplication.processEvents()
    plot.replot(True)
    for _ in range(rounds):
        QCoreApplication.processEvents()


class TestInfInLineGraph:
    """Inf values in line data must not crash the line extruder."""

    def test_inf_in_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.inf
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_neg_inf_in_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[30] = -np.inf
        _pump_and_replot(plot)
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_inf_in_x(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        x[25] = np.inf
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_both_inf(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        x[40] = np.inf
        y[40] = np.inf
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_consecutive_inf_values(self, plot):
        """Two consecutive Inf points — the difference Inf-Inf = NaN triggers the bug."""
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.inf
        y[51] = np.inf
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_cdf_fill_value_pattern(self, plot):
        """CDF files often use -1e31 as fill value — very large negative."""
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[10:15] = -1e31
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_huge_values_overflow_in_extruder(self, plot):
        """Values like 1e200 cause x*x overflow to Inf in normalized()."""
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[60] = 1e200
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_all_inf(self, plot):
        x = np.arange(10, dtype=np.float64)
        y = np.full(10, np.inf)
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_alternating_inf_and_finite(self, plot):
        x = np.arange(20, dtype=np.float64)
        y = np.sin(x / 5.0)
        y[::2] = np.inf
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None


class TestNaNInLineGraph:
    """NaN values must not crash (existing behavior, regression guard)."""

    def test_nan_in_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.nan
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_nan_in_x(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        x[50] = np.nan
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_mixed_nan_and_inf(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[20] = np.nan
        y[40] = np.inf
        y[60] = -np.inf
        y[80] = np.nan
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_all_nan(self, plot):
        x = np.arange(10, dtype=np.float64)
        y = np.full(10, np.nan)
        graph = plot.line(x, y)
        _pump_and_replot(plot)
        assert graph is not None


class TestMultipleReplots:
    """Multiple replots with non-finite data must not accumulate issues."""

    def test_repeated_replot_with_inf(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.inf
        y[51] = np.inf
        graph = plot.line(x, y)
        for _ in range(5):
            _pump_and_replot(plot)
        assert graph is not None

    def test_data_update_finite_then_inf(self, plot):
        """Start with clean data, then update to data with Inf."""
        x = np.arange(100, dtype=np.float64)
        y_clean = np.sin(x / 10.0)
        graph = plot.line(x, y_clean)
        _pump_and_replot(plot)

        y_dirty = y_clean.copy()
        y_dirty[50] = np.inf
        y_dirty[51] = -np.inf
        graph.set_data(x, y_dirty)
        _pump_and_replot(plot)

    def test_data_update_inf_then_finite(self, plot):
        """Start with Inf data, then update to clean data."""
        x = np.arange(100, dtype=np.float64)
        y_dirty = np.sin(x / 10.0)
        y_dirty[50] = np.inf
        graph = plot.line(x, y_dirty)
        _pump_and_replot(plot)

        y_clean = np.sin(x / 10.0)
        graph.set_data(x, y_clean)
        _pump_and_replot(plot)


class TestInfInScatterGraph:
    """Inf/NaN values in scatter data must not crash drawShape."""

    def test_inf_in_scatter_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.inf
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_neg_inf_in_scatter_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[30] = -np.inf
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_nan_in_scatter_y(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[50] = np.nan
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_mixed_nonfinite_in_scatter(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[20] = np.nan
        y[40] = np.inf
        y[60] = -np.inf
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_all_inf_scatter(self, plot):
        x = np.arange(10, dtype=np.float64)
        y = np.full(10, np.inf)
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_cdf_fill_value_scatter(self, plot):
        x = np.arange(100, dtype=np.float64)
        y = np.sin(x / 10.0)
        y[10:15] = -1e31
        graph = plot.scatter(x, y)
        _pump_and_replot(plot)
        assert graph is not None

    def test_scatter_data_update_to_inf(self, plot):
        x = np.arange(100, dtype=np.float64)
        y_clean = np.sin(x / 10.0)
        graph = plot.scatter(x, y_clean)
        _pump_and_replot(plot)

        y_dirty = y_clean.copy()
        y_dirty[50] = np.inf
        graph.set_data(x, y_dirty)
        _pump_and_replot(plot)
