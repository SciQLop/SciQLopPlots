"""Fills, bands and error bars (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest
from PySide6.QtGui import QColor, QImage

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

    def test_negative_column_raises_value_error(self, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError):
            g.set_column_line_style(-1, GraphLineStyle.NoLine)
        with pytest.raises(ValueError):
            g.set_column_in_legend(-1, False)

    def test_fill_color_without_a_fill_raises_value_error(self, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError):
            g.set_fill_color(0, QColor("blue"))


class TestComponentDecorations:
    def test_component_fill_between(self, qtbot, plot, tmp_path):
        g = plot.line(X, np.column_stack([np.full_like(X, 3.0), np.full_like(X, 7.0)]),
                      colors=["red", "red"])
        low, high = g.components()
        low.set_fill_between(high)
        assert low.has_fill() and low.component_index() == 0
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(0, 10)
        img = _render(plot, tmp_path)
        assert _tinted_red(img.pixelColor(img.width() // 2, img.height() // 2))
        low.clear_fill()
        assert not low.has_fill()

    def test_fill_between_another_graph_raises(self, plot):
        a = plot.line(X, np.sin(X)).components()[0]
        b = plot.line(X, np.cos(X)).components()[0]
        with pytest.raises(ValueError):
            a.set_fill_between(b)

    def test_curve_components_raise_not_implemented(self, plot):
        from SciQLopPlots import GraphType
        c = plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve).components()[0]
        with pytest.raises(NotImplementedError):
            c.set_fill_to_value(0.0)

    def test_fill_changed_is_emitted(self, qtbot, plot):
        comp = plot.line(X, np.sin(X)).components()[0]
        with qtbot.waitSignal(comp.fill_changed, timeout=1000):
            comp.set_fill_to_value(0.0)

    def test_component_fill_color_and_error_bars(self, plot):
        g = plot.line(X, np.column_stack([X, X - 1, X + 1]))
        v, lo, hi = g.components()
        v.set_fill_to_value(0.0)
        v.set_fill_color(QColor(10, 20, 30, 90))
        assert v.fill_color() == QColor(10, 20, 30, 90)
        v.set_error_bars(lo, hi, 4.0)
        v.clear_error_bars()


def _wide_tint(img, y):
    # Wide rows only: the legend swatch is tinted too but a few pixels wide.
    return sum(_tinted_red(img.pixelColor(x, y)) for x in range(img.width())) > img.width() // 2


class TestFactories:
    def test_band_shades_between_lower_and_upper(self, qtbot, plot, tmp_path):
        plot.band(X, np.full_like(X, 3.0), np.full_like(X, 7.0), name="b", color="red")
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(0, 10)
        img = _render(plot, tmp_path)
        rows = [y for y in range(img.height()) if _wide_tint(img, y)]
        assert _tinted_red(img.pixelColor(img.width() // 2, img.height() // 2))
        assert 0.25 * img.height() < max(rows) - min(rows) < 0.5 * img.height()

    def test_band_is_one_legend_row(self, plot):
        g = plot.band(X, X - 1, X + 1, name="b")
        assert g.name == "b"
        assert g.column_in_legend(0) and not g.column_in_legend(1)

    def test_band_callback_passes_empty_batches_through(self, qtbot, plot):
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            if len(calls) == 1:
                return None
            x = np.linspace(start, stop, 20)
            return x, x - 1, x + 1
        g = plot.band(cb)
        plot.x_axis().set_range(0, 10)
        qtbot.waitUntil(lambda: len(calls) == 1, timeout=5000)
        plot.x_axis().set_range(20, 30)
        qtbot.waitUntil(lambda: len(g.components()) == 2, timeout=5000)

    def test_error_bars_symmetric_and_asymmetric(self, plot):
        x = np.arange(5.0)
        g = plot.error_bars(x, x, 0.5)
        assert len(g.components()) == 3
        data = np.asarray(g.data()[1])
        np.testing.assert_allclose(data[:, 2] - data[:, 1], 1.0)
        g2 = plot.error_bars(x, x, 0.2, np.full(5, 0.7))
        np.testing.assert_allclose(np.asarray(g2.data()[1])[:, 2] - x, 0.7)

    def test_error_bars_length_mismatch_raises(self, plot):
        with pytest.raises(ValueError):
            plot.error_bars(np.arange(5.0), np.arange(5.0), np.ones(3))

    def test_error_bars_callback_reaches_three_columns(self, qtbot, plot):
        g = plot.error_bars(lambda a, b: (np.linspace(a, b, 10), np.ones(10), 0.1))
        plot.x_axis().set_range(0, 10)
        qtbot.waitUntil(lambda: len(g.components()) == 3, timeout=5000)

    def test_panel_band_returns_plot_and_graph(self, qtbot, panel):
        p, g = panel.band(X, X - 1, X + 1)
        assert g is not None and p is not None

    def test_bad_arity_raises_type_error(self, plot):
        with pytest.raises(TypeError):
            plot.band(X, X)
