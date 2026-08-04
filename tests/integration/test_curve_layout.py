"""Regression: SciQLopCurve must render C-order (n, k) 2D y correctly.

`np.column_stack(...)` produces a C-order (n, k) buffer — the natural way to
build multi-component curve data. CurveResampler used to read component i as
`ys + i*count` (contiguous-per-component, i.e. F-order), silently garbling
C-order input by interleaving the components.

The resampled QCPCurve data is not introspectable from Python, so the check is
pixel-based: two constant components render as two thin horizontal *colored*
lines, while garbled (interleaved) data renders as full-height zigzags. Only
saturated pixels are counted so grey grid lines don't pollute the metric.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage


def _colored_row_fraction(png_path):
    """Fraction of image rows containing at least one saturated (colored) pixel."""
    img = QImage(str(png_path)).convertToFormat(QImage.Format_ARGB32)
    assert not img.isNull()
    w, h = img.width(), img.height()
    rows = 0
    for yy in range(h):
        for xx in range(w):
            c = img.pixelColor(xx, yy)
            if max(c.red(), c.green(), c.blue()) - min(c.red(), c.green(), c.blue()) > 40:
                rows += 1
                break
    return rows / h


class TestCurveLayout:
    def test_corder_2d_y_renders_as_separate_components(self, plot, qtbot, tmp_path):
        n = 200
        x = np.linspace(0.0, 100.0, n)
        # C-order (n, 2): component 0 is flat 0, component 1 is flat 10.
        y = np.column_stack([np.zeros(n), np.full(n, 10.0)])
        assert y.flags["C_CONTIGUOUS"]
        g = plot.parametric_curve(x, y, labels=["zero", "ten"])
        qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
        plot.x_axis().set_range(0, 100)
        plot.y_axis().set_range(-1, 11)
        plot.replot(True)

        path = str(tmp_path / "curve.png")
        assert plot.save_png(path, 640, 480) is True
        coverage = _colored_row_fraction(path)
        # Two thin horizontal lines cover a few percent of rows; an interleaved
        # zigzag spans the full value range and covers nearly every row.
        assert coverage < 0.25, (
            f"colored pixels cover {coverage:.0%} of rows — C-order y was likely "
            "read as contiguous-per-component (F-order), garbling the curves"
        )
