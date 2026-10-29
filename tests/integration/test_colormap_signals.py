"""API review 2.6: colour maps signal their gradient, log scales and percentiles."""
import numpy as np
import pytest

from SciQLopPlots import ColorGradient
from conftest import record

X = np.linspace(1.0, 10.0, 20)
MAKERS = {
    "colormap": lambda p: p.colormap(X, X, np.outer(X, X)),
    "histogram2d": lambda p: p.histogram2d(X, X),
}
CHANGES = {
    "gradient": (lambda m: m.set_gradient(ColorGradient.Viridis), "gradient_changed", ColorGradient.Viridis),
    "y log": (lambda m: m.set_y_log_scale(True), "y_log_scale_changed", True),
    "z log": (lambda m: m.set_z_log_scale(True), "z_log_scale_changed", True),
    "low percentile": (lambda m: m.set_autoscale_percentile_low(5.0), "autoscale_percentile_low_changed", 5.0),
    "high percentile": (lambda m: m.set_autoscale_percentile_high(95.0), "autoscale_percentile_high_changed", 95.0),
}


@pytest.mark.parametrize("kind", MAKERS)
@pytest.mark.parametrize("change", CHANGES)
def test_a_change_signals_once(plot, kind, change):
    apply, signal, value = CHANGES[change]
    cmap = MAKERS[kind](plot)
    seen = record(getattr(cmap, signal))
    apply(cmap)
    apply(cmap)
    assert seen == [value]


def test_the_y_log_signal_follows_a_new_y_axis(plot):
    cmap = MAKERS["colormap"](plot)
    seen = record(cmap.y_log_scale_changed)
    cmap.set_y_axis(plot.y2_axis())
    plot.y_axis().set_log(True)
    plot.y2_axis().set_log(True)
    assert seen == [True]


def test_gradient_is_observable(plot):
    source, target = MAKERS["colormap"](plot), MAKERS["histogram2d"](plot)
    source.on.gradient >> target.on.gradient
    source.set_gradient(ColorGradient.Hot)
    assert target.gradient() == ColorGradient.Hot
