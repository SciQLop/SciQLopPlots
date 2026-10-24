"""save(path, format=...): one entry point for every export format."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot
from conftest import process_events

MAGIC = {"png": b"\x89PNG", "pdf": b"%PDF", "svg": b"<?xml"}
EXTENSIONS = {"x.pdf": "pdf", "x.svg": "svg", "x.png": "png", "x.PNG": "png",
              "x.jpg": "jpg", "x.jpeg": "jpg", "x.bmp": "bmp"}
ACCEPTED = "pdf, svg, png, jpg or bmp"
SAVE_KEYWORDS = dict(width=200, height=100, scale=1.0, quality=80)


@pytest.fixture
def projection(qtbot):
    proj = SciQLopNDProjectionPlot(2)
    qtbot.addWidget(proj)
    proj.resize(400, 200)
    return proj


@pytest.fixture
def panel_with_plot(panel):
    x = np.linspace(0.0, 10.0, 50)
    panel.plot(x, np.sin(x))
    process_events()
    return panel


@pytest.fixture(params=["plot", "projection", "panel_with_plot"])
def exportable(request):
    return request.getfixturevalue(request.param)


def _head(path, size=8):
    return path.read_bytes()[:size]


def _is_format(path, kind):
    if kind == "jpg":
        return _head(path, 2) == b"\xff\xd8"
    if kind == "bmp":
        return _head(path, 2) == b"BM"
    return _head(path).startswith(MAGIC[kind])


@pytest.mark.parametrize("kind", ["png", "pdf", "svg"])
def test_format_beats_the_extension(exportable, tmp_path, kind):
    path = tmp_path / "x.out"
    assert exportable.save(str(path), format=kind) is True
    assert _is_format(path, kind)


def test_format_is_normalised(exportable, tmp_path):
    path = tmp_path / "x.out"
    exportable.save(str(path), format=" .JPEG ")
    assert _is_format(path, "jpg")


@pytest.mark.parametrize("name,kind", EXTENSIONS.items())
def test_extension_still_picks_the_format(exportable, tmp_path, name, kind):
    path = tmp_path / name
    exportable.save(str(path))
    assert _is_format(path, kind)


def test_unknown_format_names_itself_and_the_accepted_ones(exportable, tmp_path):
    with pytest.raises(ValueError, match=f"tiff.*{ACCEPTED}"):
        exportable.save(str(tmp_path / "x.png"), format="tiff")


def test_unknown_extension_without_format_raises(exportable, tmp_path):
    with pytest.raises(ValueError, match="unknown format"):
        exportable.save(str(tmp_path / "x.out"))


def test_keywords_still_work(exportable, tmp_path):
    assert exportable.save(str(tmp_path / "x.jpg"), **SAVE_KEYWORDS) is True


def test_failed_write_raises_oserror(exportable, tmp_path):
    with pytest.raises(OSError):
        exportable.save(str(tmp_path / "no" / "such" / "x.png"))


def test_the_interface_typed_receiver_takes_format(panel_with_plot, tmp_path):
    path = tmp_path / "x.out"
    panel_with_plot.plot_at(0).save(str(path), format="png")
    assert _is_format(path, "png")
