"""Fills, bands and error bars (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import GraphLineStyle, SciQLopPlot

X = np.linspace(0, 10, 200)


def _render(plot, tmp_path, name="p"):
    plot.resize(600, 400)
    path = str(tmp_path / f"{name}.png")
    plot.save_png(path, 600, 400)
    return QImage(path).convertToFormat(QImage.Format_ARGB32)


def _tinted_red(c):
    # Default fills are the line colour at alpha 64: over a white background that is pink
    # (about 255, 191, 191), so test the red excess, not absolute red.
    return c.red() - c.green() > 30 and c.red() - c.blue() > 30


def _tinted_rows(img):
    return [y for y in range(img.height())
            for x in range(img.width()) if _tinted_red(img.pixelColor(x, y))]


class TestColumnDecorations:
    def test_fill_to_value_colours_below_the_line_only(self, qtbot, plot, tmp_path):
        g = plot.line(X, np.full_like(X, 5.0), colors=["red"])
        g.set_fill_to_value(0, 0.0)
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(-5, 10)
        rows = _tinted_rows(_render(plot, tmp_path))
        line_row = min(rows)
        assert sum(1 for y in rows if y > line_row + 5) > 1000
        assert not any(y < line_row - 3 for y in rows)

    def test_bad_columns_raise_value_error(self, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError):
            g.set_fill_between(0, 0)
        with pytest.raises(ValueError):
            g.set_fill_between(0, 5)
        with pytest.raises(ValueError):
            g.set_error_bars(0, 1, 1)

    def test_presets_apply_to_components_created_later(self, qtbot, plot):
        def cb(start, stop):
            x = np.linspace(start, stop, 50)
            return x, np.column_stack([np.sin(x), np.sin(x) - 1, np.sin(x) + 1])
        g = plot.line(cb)
        g.set_column_line_style(1, GraphLineStyle.NoLine)
        g.set_column_in_legend(2, False)
        g.set_fill_between(1, 2)
        plot.x_axis().set_range(0, 10)
        qtbot.waitUntil(lambda: len(g.components()) == 3, timeout=5000)
        assert g.components()[1].line_style() == GraphLineStyle.NoLine
        assert g.column_in_legend(2) is False
        assert g.column_in_legend(0) is True
