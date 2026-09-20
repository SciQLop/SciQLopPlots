"""`SciQLopPlot.show_color_scale()` puts the colour scale in the layout without a
colormap. A colormap added afterwards must still be bound to it: before, the bind
only happened while the scale was hidden, so showing it first left the map
unscaled and the scale's range did nothing to it.

The scale is pinned to a sliver at the bottom of the data. A bound map saturates
to the top colour (dark red in Jet) above it; an unbound one ignores the scale and
paints the bottom colour (navy). The plain case (scale not shown first) is the
control."""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange
from conftest import process_events


@pytest.mark.parametrize("show_first", [False, True])
def test_the_scale_range_drives_the_colormap(qtbot, tmp_path, show_first):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    if show_first:
        plot.show_color_scale()
    x = np.linspace(0, 10, 50)
    y = np.linspace(0, 5, 30)
    z = 100.0 + 10.0 * np.linspace(0, 1, 30 * 50).reshape(30, 50)
    cmap = plot.colormap(x, y, z)
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    process_events()
    plot.z_axis().set_range(SciQLopPlotRange(100.0, 100.2))
    process_events()
    path = tmp_path / "cmap.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    centre = img.pixelColor(150, 130)
    assert centre.red() > centre.blue(), f"map is not saturated to the top colour: {centre.name()}"
