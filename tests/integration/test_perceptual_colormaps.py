"""Perceptually uniform and colour-blind-friendly gradients.

Viridis, Cividis, Magma, Inferno, Plasma (sequential), Turbo (a smooth Jet) and
Coolwarm (diverging) come after the existing gradients, whose values stay put.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import ColorGradient
from conftest import process_events

# name -> (value, last colour of the gradient)
NEW = {
    "Viridis": (12, (253, 231, 37)),
    "Cividis": (13, (254, 232, 56)),
    "Magma": (14, (252, 253, 191)),
    "Inferno": (15, (252, 255, 164)),
    "Plasma": (16, (240, 249, 33)),
    "Turbo": (17, (122, 4, 3)),
    "Coolwarm": (18, (180, 4, 38)),
}


def test_existing_gradients_keep_their_values():
    assert ColorGradient.Grayscale == 0
    assert ColorGradient.Jet == 10
    assert ColorGradient.Hues == 11


@pytest.mark.parametrize("name", NEW)
def test_new_gradient_is_listed_with_its_value(name):
    assert getattr(ColorGradient, name) == NEW[name][0]
    assert getattr(ColorGradient, name) in list(ColorGradient)


@pytest.mark.parametrize("name", NEW)
def test_a_colormap_takes_the_gradient(plot, name):
    gradient = getattr(ColorGradient, name)
    x, y = np.linspace(0.0, 1.0, 20), np.linspace(0.0, 1.0, 10)
    cmap = plot.plot(x, y, np.random.rand(len(y), len(x)))
    cmap.set_gradient(gradient)
    assert cmap.gradient() == gradient


@pytest.mark.parametrize("name", NEW)
def test_a_line_graph_takes_the_gradient(plot, name):
    gradient = getattr(ColorGradient, name)
    x = np.linspace(0.0, 10.0, 50)
    line = plot.line(x, np.column_stack([np.sin(x), np.cos(x)]), labels=["a", "b"])
    line.set_color_data(np.linspace(0.0, 1.0, 50), gradient)
    process_events()
    assert line.color_gradient() == gradient
    assert plot.z_gradient() == gradient


@pytest.mark.parametrize("name", NEW)
def test_a_colormap_draws_the_gradient_top_colour(plot, tmp_path, name):
    """Every cell at the top of the range but one, which pins the bottom: the plot
    area shows the gradient's last colour, which tells the seven maps apart."""
    x, y = np.linspace(0.0, 1.0, 20), np.linspace(0.0, 1.0, 10)
    z = np.ones((len(y), len(x)))
    z[0, 0] = 0.0
    cmap = plot.plot(x, y, z)
    cmap.set_gradient(getattr(ColorGradient, name))
    plot.rescale_axes()
    process_events()
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path))
    c = img.pixelColor(int(img.width() * 0.4), int(img.height() * 0.4))
    expected = NEW[name][1]
    assert all(abs(a - b) <= 3 for a, b in zip((c.red(), c.green(), c.blue()), expected)), \
        ((c.red(), c.green(), c.blue()), expected)
