"""Robust (percentile-based) autoscale for the colormap z (color) axis.

A few extreme cells in a spectrogram blow out the color scale, especially on a
log color axis. A percentile autoscale clamps the z range to e.g. the 2.5/97.5
percentile of the *visible* data, making it robust against outliers. Default
percentiles are 0/100, so the behaviour is identical to plain min/max unless
configured.
"""
import pytest
import numpy as np
from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QGroupBox

from SciQLopPlots import SciQLopPlotRange, DelegateRegistry


def _pump_events(n=50):
    for _ in range(n):
        QCoreApplication.processEvents()


def _make_colormap_with_outliers(plot):
    """Colormap whose z is 0.5 everywhere except two extreme cells.

    z is laid out x-outer (row-major, x is the slow axis): shape (nx, ny).
    The +1e6 outlier sits at x-index 10 (x=10), the -1e6 at x-index 20 (x=20),
    both well inside the y window.
    """
    nx, ny = 50, 40
    x = np.linspace(0, 49, nx).astype(np.float64)
    y = np.linspace(0, 39, ny).astype(np.float64)
    z = np.full((nx, ny), 0.5, dtype=np.float64)
    z[10, 5] = 1.0e6
    z[20, 7] = -1.0e6
    cmap = plot.colormap(x, y, z.ravel())
    return cmap, x, y


class TestZPercentileRange:

    def test_full_range_includes_outliers(self, plot):
        cmap, x, y = _make_colormap_with_outliers(plot)
        r = cmap.z_percentile_range(
            SciQLopPlotRange(x[0], x[-1]), SciQLopPlotRange(y[0], y[-1]), 0.0, 100.0
        )
        assert r.start() == pytest.approx(-1.0e6)
        assert r.stop() == pytest.approx(1.0e6)

    def test_percentile_excludes_outliers(self, plot):
        cmap, x, y = _make_colormap_with_outliers(plot)
        r = cmap.z_percentile_range(
            SciQLopPlotRange(x[0], x[-1]), SciQLopPlotRange(y[0], y[-1]), 2.5, 97.5
        )
        # two extremes out of 2000 cells are far below the 2.5% tails
        assert abs(r.start()) < 1.0
        assert abs(r.stop()) < 1.0

    def test_visible_filter_excludes_out_of_range_cells(self, plot):
        cmap, x, y = _make_colormap_with_outliers(plot)
        # restrict visible x so the +1e6 cell (x=10) is outside; -1e6 (x=20) stays
        r = cmap.z_percentile_range(
            SciQLopPlotRange(15.0, 49.0), SciQLopPlotRange(y[0], y[-1]), 0.0, 100.0
        )
        assert r.stop() == pytest.approx(0.5)
        assert r.start() == pytest.approx(-1.0e6)

    def test_nan_and_inf_cells_ignored(self, plot):
        nx, ny = 30, 20
        x = np.linspace(0, 29, nx).astype(np.float64)
        y = np.linspace(0, 19, ny).astype(np.float64)
        z = np.full((nx, ny), 2.0, dtype=np.float64)
        z[0, 0] = np.nan
        z[1, 1] = np.inf
        z[2, 2] = -np.inf
        cmap = plot.colormap(x, y, z.ravel())
        r = cmap.z_percentile_range(
            SciQLopPlotRange(x[0], x[-1]), SciQLopPlotRange(y[0], y[-1]), 0.0, 100.0
        )
        assert np.isfinite(r.start()) and np.isfinite(r.stop())
        assert r.start() == pytest.approx(2.0)
        assert r.stop() == pytest.approx(2.0)


class TestPercentileConfig:

    def test_default_percentiles(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        assert cmap.autoscale_percentile_low() == 0.0
        assert cmap.autoscale_percentile_high() == 100.0

    def test_set_percentiles(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        cmap.set_autoscale_percentile_low(2.5)
        cmap.set_autoscale_percentile_high(97.5)
        assert cmap.autoscale_percentile_low() == 2.5
        assert cmap.autoscale_percentile_high() == 97.5


class TestRescaleRouting:

    def test_rescale_uses_percentile_when_configured(self, plot):
        cmap, x, y = _make_colormap_with_outliers(plot)
        cmap.set_autoscale_percentile_low(2.5)
        cmap.set_autoscale_percentile_high(97.5)
        zaxis = cmap.z_axis()
        zaxis.rescale()
        r = zaxis.range()
        assert abs(r.start()) < 1.0
        assert abs(r.stop()) < 1.0


def _make_histogram_with_spike(plot):
    """Histogram whose counts are roughly uniform except one densely packed bin.

    5000 points spread over [-5, 5]^2 give a low, even count per bin; an extra
    3000 points piled at the origin make a single extreme-count bin.
    """
    rng = np.random.default_rng(7)
    spread_x = rng.uniform(-5, 5, 5000)
    spread_y = rng.uniform(-5, 5, 5000)
    spike_x = rng.normal(0, 0.01, 3000)
    spike_y = rng.normal(0, 0.01, 3000)
    x = np.concatenate([spread_x, spike_x]).astype(np.float64)
    y = np.concatenate([spread_y, spike_y]).astype(np.float64)
    hist = plot.add_histogram2d("spike", 50, 50)
    hist.set_data(x, y)
    _pump_events()
    plot.replot(True)
    _pump_events()
    return hist


class TestHistogram2DPercentile:

    def test_default_percentiles_on_histogram(self, plot):
        hist = plot.add_histogram2d("h", 20, 20)
        assert hist.autoscale_percentile_low() == 0.0
        assert hist.autoscale_percentile_high() == 100.0

    def test_full_range_includes_spike(self, plot):
        hist = _make_histogram_with_spike(plot)
        r = hist.z_percentile_range(
            SciQLopPlotRange(-6.0, 6.0), SciQLopPlotRange(-6.0, 6.0), 0.0, 100.0
        )
        # the dense origin bin holds hundreds/thousands of points
        assert r.stop() > 100.0

    def test_percentile_excludes_spike(self, plot):
        hist = _make_histogram_with_spike(plot)
        full = hist.z_percentile_range(
            SciQLopPlotRange(-6.0, 6.0), SciQLopPlotRange(-6.0, 6.0), 0.0, 100.0
        )
        clamped = hist.z_percentile_range(
            SciQLopPlotRange(-6.0, 6.0), SciQLopPlotRange(-6.0, 6.0), 2.5, 97.5
        )
        assert np.isfinite(clamped.stop())
        assert clamped.stop() < full.stop()

    def test_rescale_uses_percentile_when_configured(self, plot):
        hist = _make_histogram_with_spike(plot)
        full_max = hist.z_percentile_range(
            SciQLopPlotRange(-6.0, 6.0), SciQLopPlotRange(-6.0, 6.0), 0.0, 100.0
        ).stop()
        hist.set_autoscale_percentile_low(2.5)
        hist.set_autoscale_percentile_high(97.5)
        zaxis = hist.z_axis()
        zaxis.rescale()
        clamped_max = zaxis.range().stop()
        assert np.isfinite(clamped_max)
        assert clamped_max < full_max


class TestInspectorUI:
    """Color-scale percentile spinboxes live on the "Color Scale" (z) axis
    delegate (PR #69), where users find them next to gradient/log/label.
    They are NOT exposed on the colormap or histogram product nodes."""

    def _group_titles(self, target, qtbot):
        delegate = DelegateRegistry.instance().create_delegate(target, None)
        assert delegate is not None
        qtbot.addWidget(delegate)
        return [g.title() for g in delegate.findChildren(QGroupBox)]

    def test_z_axis_delegate_has_percentile_group_with_colormap(
        self, plot, qtbot, sample_colormap_data
    ):
        x, y, z = sample_colormap_data
        plot.colormap(x, y, z)
        assert "Autoscale percentile" in self._group_titles(plot.z_axis(), qtbot)

    def test_z_axis_delegate_has_percentile_group_with_histogram(self, plot, qtbot):
        plot.add_histogram2d("h", 20, 20)
        assert "Autoscale percentile" in self._group_titles(plot.z_axis(), qtbot)

    def test_colormap_delegate_does_not_carry_percentile(
        self, plot, qtbot, sample_colormap_data
    ):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        # Old design parked it on the colormap node; the move is intentional.
        assert "Autoscale percentile" not in self._group_titles(cmap, qtbot)

    def test_histogram_delegate_does_not_carry_percentile(self, plot, qtbot):
        hist = plot.add_histogram2d("h", 20, 20)
        assert "Autoscale percentile" not in self._group_titles(hist, qtbot)
