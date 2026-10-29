"""API review 2.12: auto_scale_y on 2-D histograms too (it lives on the colour-map base)."""
import numpy as np

from conftest import record


def test_auto_scale_y_follows_the_y_samples(qtbot, plot):
    hist = plot.histogram2d(np.linspace(0.0, 1.0, 100), np.linspace(-3.0, 7.0, 100))
    seen = record(hist.auto_scale_y_changed)
    hist.set_auto_scale_y(True)
    hist.set_data(np.linspace(0.0, 1.0, 100), np.linspace(-30.0, 70.0, 100))
    rng = hist.y_axis().range()
    assert (hist.auto_scale_y(), seen) == (True, [True])
    assert (rng.start(), rng.stop()) == (-30.0, 70.0)


def test_colour_maps_keep_auto_scale_y(plot):
    x = np.linspace(0.0, 1.0, 10)
    cmap = plot.colormap(x, np.linspace(5.0, 9.0, 10), np.outer(x, x))
    cmap.set_auto_scale_y(True)
    cmap.set_data(x, np.linspace(50.0, 90.0, 10), np.outer(x, x))
    rng = cmap.y_axis().range()
    assert (rng.start(), rng.stop()) == (50.0, 90.0)
