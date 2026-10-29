"""API review 4.7: to_bytes() returns what save() writes, without a file."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot

X = np.linspace(0.0, 10.0, 50)
MAGIC = {"png": b"\x89PNG", "jpg": b"\xff\xd8", "bmp": b"BM", "pdf": b"%PDF", "svg": b"<?xml"}


def _host(request, kind):
    if kind == "projection":
        host = SciQLopNDProjectionPlot(3)
        request.getfixturevalue("qtbot").addWidget(host)
        host.plot([X, X, np.sin(X), np.cos(X)])
        return host
    host = request.getfixturevalue(kind)
    host.plot(X, np.sin(X))
    return host


@pytest.mark.parametrize("kind", ["plot", "panel", "projection"])
@pytest.mark.parametrize("fmt", MAGIC)
def test_to_bytes_gives_the_format(request, kind, fmt):
    data = _host(request, kind).to_bytes(fmt, 200, 150)
    assert isinstance(data, bytes) and data.startswith(MAGIC[fmt])


def test_to_bytes_is_what_save_writes(plot, tmp_path):
    plot.plot(X, np.sin(X))
    plot.save(str(tmp_path / "a.png"), 200, 150)
    assert plot.to_bytes("png", 200, 150) == (tmp_path / "a.png").read_bytes()


def test_an_unknown_format_raises(plot):
    with pytest.raises(ValueError, match="unknown format"):
        plot.to_bytes("gif")
