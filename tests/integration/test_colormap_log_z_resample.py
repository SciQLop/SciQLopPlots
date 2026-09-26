"""A log colour scale averages zoomed-out cells in log space.

With many more samples than pixels, each drawn cell mixes several samples.
Alternating 1 and 1e4 on a [1, 1e4] log scale must land mid-scale (100, green
on Jet), not near the top (linear mean 5000, dark red).
"""
import numpy as np
from PySide6.QtGui import QImage

from SciQLopPlots import ColorGradient
from conftest import process_events


def test_zoomed_out_log_z_colormap_shows_the_geometric_mean(plot, tmp_path, qtbot):
    plot.resize(400, 300)
    plot.show()
    qtbot.waitExposed(plot)
    nx = 20000
    x = np.arange(nx, dtype=np.float64)
    y = np.array([1.0, 2.0])
    z = np.repeat(np.where(np.arange(nx) % 2, 1e4, 1.0), 2).reshape(nx, 2)
    cmap = plot.plot(x, y, z)
    cmap.set_gradient(ColorGradient.Jet)
    plot.z_axis().set_log(True)
    plot.rescale_axes()
    plot.z_axis().set_range(1.0, 1e4)

    def centre_colour():
        process_events()
        path = tmp_path / "cmap.png"
        assert plot.save_png(str(path), 400, 300)
        img = QImage(str(path))
        return img.pixelColor(img.width() // 3, img.height() // 2)

    qtbot.waitUntil(lambda: centre_colour().name() != "#ffffff", timeout=3000)
    c = centre_colour()
    assert c.red() < 200 and c.green() > 200, (c.red(), c.green(), c.blue())
