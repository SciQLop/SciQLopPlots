"""API review 1.17: a colour map's gradient and log scales are its axes' state: the last call wins."""
import numpy as np

from SciQLopPlots import ColorGradient

X = np.linspace(1.0, 10.0, 20)


def test_the_plot_z_axis_and_the_colormap_share_the_gradient(plot):
    cmap = plot.colormap(X, X, np.outer(X, X))
    cmap.set_gradient(ColorGradient.Viridis)
    plot.set_z_gradient(ColorGradient.Hot)
    assert cmap.gradient() == ColorGradient.Hot
    cmap.set_gradient(ColorGradient.Jet)
    assert plot.z_gradient() == ColorGradient.Jet


def test_the_axes_and_the_colormap_share_the_log_scales(plot):
    cmap = plot.colormap(X, X, np.outer(X, X))
    cmap.set_z_log_scale(True)
    plot.z_axis().set_log(False)
    cmap.y_axis().set_log(True)
    assert cmap.y_axis() is plot.y2_axis()
    assert (cmap.z_log_scale(), cmap.y_log_scale()) == (False, True)
