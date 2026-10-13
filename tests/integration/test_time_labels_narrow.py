"""Time labels must not overlap on a narrow plot (a phone, or a narrow window): the time axis
asked for five ticks whatever its width, and its two-line labels ran into each other."""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopTimeSeriesPlot
from conftest import process_events


def _label_clusters(img, top):
    """x extents of the ink under the plot's data area, split where 4+ columns are blank."""
    ink = [any(img.pixelColor(x, y).value() < 128 for y in range(top, img.height()))
           for x in range(img.width())]
    clusters, start, blank = [], None, 0
    for x, inked in enumerate(ink + [False] * 4):
        if inked:
            start = x if start is None else start
            blank = 0
        elif start is not None:
            blank += 1
            if blank >= 4:
                clusters.append((start, x - blank))
                start, blank = None, 0
    return clusters


@pytest.mark.parametrize("width", [320, 420, 900])
@pytest.mark.parametrize("with_colour_bar", [False, True], ids=["line", "spectrogram"])
def test_time_labels_do_not_overlap(qtbot, tmp_path, width, with_colour_bar):
    plot = SciQLopTimeSeriesPlot()
    qtbot.addWidget(plot)
    plot.resize(width, 250)
    plot.show()
    t = np.linspace(1.6e9, 1.6e9 + 5 * 86400, 1000)
    if with_colour_bar:   # its axes and bar narrow the data area
        plot.plot(t, np.logspace(1, 4, 16), np.random.rand(1000, 16) + 1)
        plot.z_axis().set_label("flux [cm^-2 s^-1 sr^-1 eV^-1]")
    else:
        plot.plot(t, np.sin(t / 3e4))
    plot.x_axis().set_range(t[0], t[-1])
    qtbot.wait(200)
    process_events()
    path = str(tmp_path / "narrow.png")
    assert plot.save_png(path)
    img = QImage(path)
    widest = max(b - a for a, b in _label_clusters(img, img.height() - 32))
    # One "yyyy/MM/dd" label is about 70-100 px wide; two that ran together are far wider.
    assert widest < 130, f"a time label {widest} px wide at plot width {width}: labels overlap"
