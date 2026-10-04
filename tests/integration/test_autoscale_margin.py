"""Autoscale keeps a margin so ticks at the data extremes (0/1 flags, enum
levels, text tick labels) are not clipped off the axis."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopPlotRange
from conftest import process_events


def _flags(plot):
    x = np.arange(10, dtype=np.float64)
    y = (x % 2).astype(np.float64)
    plot.plot(x, y)
    process_events()
    return plot.y_axis()


def _range(ax):
    r = ax.range()
    return r.start(), r.stop()


def test_default_margin_is_five_percent(plot):
    assert plot.y_axis().autoscale_margin() == pytest.approx(0.05)


def test_rescale_pads_binary_data(plot):
    ax = _flags(plot)
    ax.rescale()
    assert _range(ax) == pytest.approx((-0.05, 1.05))


def test_margin_is_configurable_at_runtime(plot):
    ax = _flags(plot)
    ax.set_autoscale_margin(0.0)
    ax.rescale()
    assert _range(ax) == pytest.approx((0.0, 1.0))
    ax.set_autoscale_margin(0.25)
    ax.rescale()
    assert _range(ax) == pytest.approx((-0.25, 1.25))


def test_margin_is_clamped(plot):
    ax = plot.y_axis()
    ax.set_autoscale_margin(-1.0)
    assert ax.autoscale_margin() == 0.0
    ax.set_autoscale_margin(10.0)
    assert ax.autoscale_margin() == pytest.approx(0.5)


def test_percentile_rescale_is_padded_too(plot):
    ax = _flags(plot)
    ax.set_autoscale_percentile_low(1.0)
    ax.rescale()
    assert _range(ax) == pytest.approx((-0.05, 1.05))


def test_log_axis_margin_is_in_decades(plot):
    x = np.arange(3, dtype=np.float64)
    plot.plot(x, np.array([1.0, 10.0, 100.0]))
    process_events()
    ax = plot.y_axis()
    ax.set_log(True)
    ax.rescale()
    lo, hi = _range(ax)
    assert np.log10(lo) == pytest.approx(-0.1) and np.log10(hi) == pytest.approx(2.1)


def test_time_axis_is_not_padded(ts_plot):
    x = np.linspace(1.7e9, 1.7e9 + 100, 101)
    ts_plot.plot(x, np.sin(x))
    process_events()
    ax = ts_plot.time_axis()
    ax.rescale()
    assert _range(ax) == pytest.approx((1.7e9, 1.7e9 + 100))


def _margin_spin(target, qtbot):
    from PySide6.QtWidgets import QDoubleSpinBox
    from SciQLopPlots import DelegateRegistry
    delegate = DelegateRegistry.instance().create_delegate(target, None)
    qtbot.addWidget(delegate)
    return delegate, delegate.findChild(QDoubleSpinBox, "autoscale_margin")


def test_inspector_sets_the_margin(plot, qtbot):
    ax = plot.y_axis()
    delegate, spin = _margin_spin(ax, qtbot)
    assert spin is not None and spin.value() == pytest.approx(5.0)
    spin.setValue(20.0)
    assert ax.autoscale_margin() == pytest.approx(0.2)


def test_time_axis_inspector_has_no_margin(ts_plot, qtbot):
    _, spin = _margin_spin(ts_plot.time_axis(), qtbot)
    assert spin is None


def test_key_axis_is_not_padded(plot):
    # Padding the key axis widens the requested range, which fetches more data,
    # which rescales again: the range would grow on every batch.
    _flags(plot)
    ax = plot.x_axis()
    ax.rescale()
    assert _range(ax) == pytest.approx((0.0, 9.0))


def test_timeline_lanes_are_not_padded(ts_plot):
    tl = ts_plot.add_timeline()
    tl.set_intervals([1.7e9, 1.7e9 + 20], [1.7e9 + 10, 1.7e9 + 30], lane=["A", "B"])
    process_events()
    ax = ts_plot.y_axis()
    before = _range(ax)
    ax.rescale()
    process_events()
    assert _range(ax) == pytest.approx(before)
