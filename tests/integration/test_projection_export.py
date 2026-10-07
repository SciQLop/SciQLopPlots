"""SciQLopNDProjectionPlot exports like a plot or a panel. save_png and its
siblings used to hit the abstract SciQLopPlotInterface methods and return False."""
from collections import Counter

import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopNDProjectionPlot
from conftest import process_events


@pytest.fixture
def projection(qtbot):
    phase = np.linspace(0, 4 * np.pi, 2000)
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    proj.resize(900, 300)
    curve = proj.add_reference_curve([np.cos(phase), np.sin(phase), 0.3 * np.sin(2 * phase)],
                                     label="orbit")
    qtbot.waitUntil(lambda: not curve.busy(), timeout=5000)
    process_events()
    return proj


def _ink(img):
    pixels = Counter(img.pixel(x, y) for y in range(0, img.height(), 2)
                     for x in range(0, img.width(), 2))
    (_, background), = pixels.most_common(1)
    return sum(pixels.values()) - background


@pytest.mark.parametrize("ext", ["png", "jpg", "bmp", "pdf"])
def test_projection_saves_every_format(projection, tmp_path, ext):
    path = tmp_path / f"projection.{ext}"
    assert projection.save(str(path), 900, 300) is True
    assert path.stat().st_size > 0


def test_every_pane_is_in_the_picture(projection, tmp_path):
    path = tmp_path / "projection.png"
    assert projection.save_png(str(path), 900, 300) is True
    img = QImage(str(path))
    assert (img.width(), img.height()) == (900, 300)
    thirds = [img.copy(i * 300, 0, 300, 300) for i in range(3)]
    assert all(_ink(t) > 200 for t in thirds)
