"""Robust (percentile-based) autoscale for the value (y) axis on line, curve,
and scatter graphs.

A single huge spike or extreme outlier blows up the y range and turns the rest
of the trace into a flat line. A percentile autoscale clamps the y range to
e.g. the 2.5/97.5 percentile of the *visible* data, making the rescale robust
against outliers. Default percentiles are 0/100, so the behaviour is identical
to plain min/max unless configured.

Mirrors test_colormap_autoscale_percentile.py for the colormap z-axis case.
"""
import pytest
import numpy as np
from PySide6.QtCore import QCoreApplication
from PySide6.QtWidgets import QGroupBox

from SciQLopPlots import DelegateRegistry


def _pump_events(n=50):
    for _ in range(n):
        QCoreApplication.processEvents()


def _line_with_outliers(plot):
    """1D line whose y is a smooth sine plus two extreme spikes inside the x window."""
    n = 200
    x = np.linspace(0., 199., n).astype(np.float64)
    y = np.sin(x * 0.1).astype(np.float64)  # range roughly [-1, 1]
    y[40] = 1.0e6
    y[120] = -1.0e6
    g = plot.line(x, y)
    _pump_events()
    plot.replot(True)
    _pump_events()
    return g, x, y


def _multicomponent_line_with_outliers(plot):
    """Multi-component line: 3 components, outliers on the third one only."""
    n = 200
    x = np.linspace(0., 199., n).astype(np.float64)
    y = np.column_stack([
        np.sin(x * 0.1),
        np.cos(x * 0.1),
        0.5 * np.sin(x * 0.2),
    ]).astype(np.float64)  # all components in roughly [-1, 1]
    y[40, 2] = 1.0e6
    y[120, 2] = -1.0e6
    g = plot.line(x, y)
    _pump_events()
    plot.replot(True)
    _pump_events()
    return g, x, y


def _parametric_curve_with_outliers(plot):
    """Parametric curve (x non-monotonic) with extreme y spikes.

    A label is required so SciQLopCurve creates a QCPCurve component; without
    labels the wrapper is silent (no components, visible() returns False) and
    no rescale path would pool from it.
    """
    n = 200
    t = np.linspace(0., 2. * np.pi, n).astype(np.float64)
    x = np.cos(t).astype(np.float64)
    y = np.sin(t).astype(np.float64)
    y[40] = 1.0e6
    y[120] = -1.0e6
    g = plot.parametric_curve(x, y, labels=["curve"])
    _pump_events()
    plot.replot(True)
    _pump_events()
    return g, x, y


class TestPercentileConfig:

    def test_default_percentiles(self, plot):
        ax = plot.y_axis()
        assert ax.autoscale_percentile_low() == 0.0
        assert ax.autoscale_percentile_high() == 100.0

    def test_set_percentiles(self, plot):
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        assert ax.autoscale_percentile_low() == 2.5
        assert ax.autoscale_percentile_high() == 97.5

    def test_percentile_setters_clamp(self, plot):
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(-10.0)
        ax.set_autoscale_percentile_high(200.0)
        assert ax.autoscale_percentile_low() == 0.0
        assert ax.autoscale_percentile_high() == 100.0


class TestSingleLinePercentileRescale:

    def test_full_range_includes_outliers(self, plot):
        g, x, y = _line_with_outliers(plot)
        plot.x_axis().set_range(float(x[0]), float(x[-1]))
        _pump_events()
        ax = plot.y_axis()
        ax.rescale()
        r = ax.range()
        # 0/100 → plain min/max path; outliers stay in
        assert r.start() <= -1.0e6 + 1.0
        assert r.stop() >= 1.0e6 - 1.0

    def test_percentile_excludes_outliers(self, plot):
        g, x, y = _line_with_outliers(plot)
        plot.x_axis().set_range(float(x[0]), float(x[-1]))
        _pump_events()
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        ax.rescale()
        r = ax.range()
        # two extremes out of 200 are deep in the clipped tails; range stays
        # well within the smooth-sine envelope of roughly [-1, 1].
        assert abs(r.start()) < 1.5
        assert abs(r.stop()) < 1.5


class TestMultiComponentPercentileRescale:

    def test_pools_across_components(self, plot):
        g, x, y = _multicomponent_line_with_outliers(plot)
        plot.x_axis().set_range(float(x[0]), float(x[-1]))
        _pump_events()
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        ax.rescale()
        r = ax.range()
        # outliers are in component 2; both tails should be clipped
        assert abs(r.start()) < 1.5
        assert abs(r.stop()) < 1.5


class TestVisibleKeyRangeFilter:

    def test_outliers_outside_visible_range_already_excluded(self, plot):
        g, x, y = _line_with_outliers(plot)
        # restrict x window to skip both outlier indices (40 and 120)
        plot.x_axis().set_range(60.0, 100.0)
        _pump_events()
        ax = plot.y_axis()
        ax.rescale()
        r = ax.range()
        # without percentile filtering, the visible window has no outliers
        assert abs(r.start()) < 1.5
        assert abs(r.stop()) < 1.5

    def test_percentile_with_restricted_range_still_clips(self, plot):
        """If percentile is configured but the visible window has no outliers,
        the percentile path runs over a small pool and still yields a sensible
        range (no NaN, no infinity)."""
        g, x, y = _line_with_outliers(plot)
        plot.x_axis().set_range(60.0, 100.0)
        _pump_events()
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        ax.rescale()
        r = ax.range()
        assert np.isfinite(r.start()) and np.isfinite(r.stop())
        assert abs(r.start()) < 1.5
        assert abs(r.stop()) < 1.5


NUMERIC_DTYPES = [
    np.float32, np.float64,
    np.int8, np.uint8,
    np.int16, np.uint16,
    np.int32, np.uint32,
    np.int64, np.uint64,
]


def _y_with_outliers(dtype, n=200):
    """Smooth baseline in dtype's middle range, with two extreme outliers.

    Outlier magnitudes are clamped to 2^52 so the int->double cast inside the
    percentile pool stays exact for 64-bit dtypes (double has 53-bit mantissa).
    """
    if np.issubdtype(dtype, np.integer):
        info = np.iinfo(dtype)
        # Cap |value| at 2^52 so int->double is lossless for int64/uint64.
        SAFE_DOUBLE = 1 << 52
        type_max = min(int(info.max), SAFE_DOUBLE)
        type_min = max(int(info.min), -SAFE_DOUBLE) if info.min < 0 else 0
        mid = (type_min + type_max) // 2
        amp = min(50, (type_max - type_min) // 8) or 1
        baseline = mid + (np.sin(np.linspace(0., 2. * np.pi, n)) * amp)
        y = baseline.astype(dtype)
        lo, hi = type_min, type_max
    else:
        y = (np.sin(np.linspace(0., 2. * np.pi, n)) * 100.).astype(dtype)
        lo, hi = -1.0e6, 1.0e6
    y[40] = hi
    y[120] = lo
    return y


def _single_line(plot, dtype):
    n = 200
    x = np.linspace(0., 199., n).astype(np.float64)
    plot.line(x, _y_with_outliers(dtype, n))
    _pump_events()
    plot.x_axis().set_range(float(x[0]), float(x[-1]))
    _pump_events()
    return plot.y_axis()


def _multi_line(plot, dtype):
    n = 200
    x = np.linspace(0., 199., n).astype(np.float64)
    # 3-component y, outliers on the third column only. Sibling columns sit at
    # the same baseline level so the percentile pool is dominated by the
    # outlier-free middle band and the assertion can detect the clip.
    third = _y_with_outliers(dtype, n)
    baseline_val = int(third[100])  # any non-outlier index
    col0 = np.full(n, baseline_val, dtype=dtype)
    col1 = np.full(n, baseline_val, dtype=dtype)
    y = np.column_stack([col0, col1, third]).astype(dtype)
    plot.line(x, y)
    _pump_events()
    plot.x_axis().set_range(float(x[0]), float(x[-1]))
    _pump_events()
    return plot.y_axis()


def _curve(plot, dtype, qtbot):
    n = 200
    t = np.linspace(0., 2. * np.pi, n).astype(np.float64)
    x = np.cos(t).astype(np.float64)
    y = _y_with_outliers(dtype, n)
    g = plot.parametric_curve(x, y, labels=["c"])
    qtbot.waitUntil(lambda: len(g.data()) >= 2
                            and g.data()[0].size > 0
                            and not g.busy(),
                    timeout=5000)
    plot.x_axis().set_range(-1.5, 1.5)
    _pump_events()
    return plot.y_axis()


def _colormap(plot, dtype):
    nx, ny = 50, 40
    x = np.linspace(0., 49., nx).astype(np.float64)
    y = np.linspace(0., 39., ny).astype(np.float64)
    if np.issubdtype(dtype, np.integer):
        info = np.iinfo(dtype)
        mid = (int(info.min) + int(info.max)) // 2
        z = np.full((nx, ny), mid, dtype=dtype)
        z[10, 5] = info.max
        z[20, 7] = info.min
    else:
        z = np.full((nx, ny), 0.5, dtype=dtype)
        z[10, 5] = 1.0e6
        z[20, 7] = -1.0e6
    cmap = plot.colormap(x, y, z.ravel())
    _pump_events()
    return cmap  # percentile config + rescale go through the colormap, not the axis


def _rescale_default(target):
    if hasattr(target, "z_axis"):  # colormap
        target.z_axis().rescale()
    else:
        target.rescale()


def _rescale_with_percentile(target, low, high):
    target.set_autoscale_percentile_low(low)
    target.set_autoscale_percentile_high(high)
    if hasattr(target, "z_axis"):
        target.z_axis().rescale()
    else:
        target.rescale()


def _span(target):
    r = target.range() if hasattr(target, "z_axis") is False else target.z_axis().range()
    return r.stop() - r.start()


class TestPercentileMatrix:
    """All graph kinds × all numeric dtypes: setting a non-trivial percentile
    must clip the rescaled range tighter than the plain min/max would."""

    @pytest.mark.parametrize("dtype", NUMERIC_DTYPES,
                             ids=lambda d: np.dtype(d).name)
    def test_single_line(self, plot, dtype):
        ax = _single_line(plot, dtype)
        _rescale_default(ax)
        full = _span(ax)
        _rescale_with_percentile(ax, 2.5, 97.5)
        clipped = _span(ax)
        assert np.isfinite(clipped)
        assert clipped < full * 0.5

    @pytest.mark.parametrize("dtype", NUMERIC_DTYPES,
                             ids=lambda d: np.dtype(d).name)
    def test_multi_line(self, plot, dtype):
        ax = _multi_line(plot, dtype)
        _rescale_default(ax)
        full = _span(ax)
        _rescale_with_percentile(ax, 2.5, 97.5)
        clipped = _span(ax)
        assert np.isfinite(clipped)
        assert clipped < full * 0.5

    # SciQLopCurve's CurveResampler reads y via PyBuffer::data() which is only
    # valid for float64 (documented in PythonInterface.hpp). Non-double y crashes
    # upstream of our percentile path, so the curve case is float64-only here.
    @pytest.mark.parametrize("dtype", [np.float64], ids=lambda d: np.dtype(d).name)
    def test_parametric_curve(self, plot, qtbot, dtype):
        ax = _curve(plot, dtype, qtbot)
        _rescale_default(ax)
        full = _span(ax)
        _rescale_with_percentile(ax, 2.5, 97.5)
        clipped = _span(ax)
        assert np.isfinite(clipped)
        assert clipped < full * 0.5

    @pytest.mark.parametrize("dtype", NUMERIC_DTYPES,
                             ids=lambda d: np.dtype(d).name)
    def test_colormap(self, plot, dtype):
        cmap = _colormap(plot, dtype)
        _rescale_default(cmap)
        full = _span(cmap)
        _rescale_with_percentile(cmap, 2.5, 97.5)
        clipped = _span(cmap)
        assert np.isfinite(clipped)
        assert clipped < full * 0.5


class TestParametricCurvePercentileRescale:

    def test_percentile_excludes_outliers(self, plot, qtbot):
        g, x, y = _parametric_curve_with_outliers(plot)
        plot.x_axis().set_range(-1.5, 1.5)
        # SciQLopCurve gathers from the resampler's committed data; wait until
        # the worker thread has swapped _next_data into _data.
        qtbot.waitUntil(lambda: len(g.data()) >= 2
                                and g.data()[0].size > 0
                                and not g.busy(),
                        timeout=5000)
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        ax.rescale()
        r = ax.range()
        # the +/- 1e6 spikes must be clipped; the curve sits in [-1, 1]
        assert abs(r.start()) < 1.5
        assert abs(r.stop()) < 1.5


class TestRescaleNoCrash:

    def test_rescale_with_no_graphs(self, plot):
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        # empty plot — percentile path finds nothing, falls through to min/max
        # which also finds nothing, must not crash
        ax.rescale()

    def test_rescale_nonfinite_values_ignored(self, plot):
        n = 100
        x = np.linspace(0., 99., n).astype(np.float64)
        y = np.full(n, 0.5, dtype=np.float64)
        y[10] = np.nan
        y[20] = np.inf
        y[30] = -np.inf
        plot.line(x, y)
        _pump_events()
        plot.replot(True)
        _pump_events()
        plot.x_axis().set_range(float(x[0]), float(x[-1]))
        _pump_events()
        ax = plot.y_axis()
        ax.set_autoscale_percentile_low(2.5)
        ax.set_autoscale_percentile_high(97.5)
        ax.rescale()
        r = ax.range()
        assert np.isfinite(r.start()) and np.isfinite(r.stop())


class TestInspectorUI:
    """The percentile spinboxes live on the value-axis delegate (numeric,
    non-color-scale axes only)."""

    def _group_titles(self, target, qtbot):
        delegate = DelegateRegistry.instance().create_delegate(target, None)
        assert delegate is not None
        qtbot.addWidget(delegate)
        return [g.title() for g in delegate.findChildren(QGroupBox)]

    def test_value_axis_delegate_has_percentile_group(self, plot, qtbot):
        assert "Autoscale percentile" in self._group_titles(plot.y_axis(), qtbot)

    def test_x_axis_delegate_has_no_percentile_group(self, plot, qtbot):
        # The rescale pool only collects graphs whose y_axis() == this, so the
        # group is a no-op on key (x) axes. Hidden from the delegate to avoid
        # the misleading control.
        assert "Autoscale percentile" not in self._group_titles(plot.x_axis(), qtbot)

    def test_time_axis_delegate_has_no_percentile_group(self, ts_plot, qtbot):
        time_ax = ts_plot.time_axis()
        assert "Autoscale percentile" not in self._group_titles(time_ax, qtbot)
