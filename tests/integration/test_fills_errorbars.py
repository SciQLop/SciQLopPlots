"""Fills, bands and error bars (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest
from PySide6.QtGui import QColor, QImage
from PySide6.QtWidgets import QLabel

from SciQLopPlots import DelegateRegistry, GraphLineStyle, GraphMarkerShape, SciQLopPlot

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

    def test_fill_color_and_fill_to_value_agree_on_a_column_the_graph_lacks(self, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError, match=r"^invalid column 5$"):
            g.set_fill_to_value(5, 0.0)
        with pytest.raises(ValueError, match=r"^invalid column 5$"):
            g.set_fill_color(5, QColor("blue"))

    @pytest.mark.parametrize("clear", ["clear_fill", "clear_error_bars"])
    @pytest.mark.parametrize("column", [-1, 5])
    def test_clearing_a_column_the_graph_lacks_raises_like_setting_it(self, plot, clear, column):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError, match=rf"^invalid column {column}$"):
            getattr(g, clear)(column)

    @pytest.mark.parametrize("clear", ["clear_fill", "clear_error_bars"])
    def test_clearing_a_valid_column_without_decoration_is_a_no_op(self, plot, clear):
        g = plot.line(X, np.column_stack([X, X]))
        getattr(g, clear)(1)


    def test_nan_fill_baseline_raises_value_error(self, plot):
        g = plot.line(X, np.sin(X))
        with pytest.raises(ValueError):
            g.set_fill_to_value(0, float("nan"))
        with pytest.raises(ValueError):
            g.set_fill_to_value(0, float("inf"))
        with pytest.raises(ValueError):
            g.components()[0].set_fill_to_value(float("nan"))
        assert not g.components()[0].has_fill()

    def test_negative_cap_width_raises_value_error(self, plot):
        g = plot.line(X, np.column_stack([X, X - 1, X + 1]))
        with pytest.raises(ValueError):
            g.set_error_bars(0, 1, 2, -1.0)
        v, lo, hi = g.components()
        with pytest.raises(ValueError):
            v.set_error_bars(lo, hi, -1.0)

    def test_column_presets_request_a_replot(self, qtbot, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with qtbot.waitSignal(g.replot, timeout=1000):
            g.set_column_in_legend(1, False)

    def test_graph_level_error_bars_notify_the_component(self, qtbot, plot):
        g = plot.line(X, np.column_stack([X, X - 1, X + 1]))
        with qtbot.waitSignal(g.components()[0].error_bars_changed, timeout=1000):
            g.set_error_bars(0, 1, 2)
        with qtbot.waitSignal(g.components()[0].error_bars_changed, timeout=1000):
            g.clear_error_bars(0)


def _fill_color_row(delegate):
    label = next(l for l in delegate.findChildren(QLabel) if l.text() == "Fill color")
    return label.parentWidget()


class TestInspectorFillRow:
    def test_row_follows_graph_level_fills(self, qtbot, plot):
        g = plot.line(X, np.column_stack([X, X + 1]))
        delegate = DelegateRegistry.instance().create_delegate(g.components()[0], None)
        qtbot.addWidget(delegate)
        row = _fill_color_row(delegate)
        assert row.isHidden()
        g.set_fill_between(0, 1)
        assert not row.isHidden()
        g.set_fill_color(0, QColor(10, 20, 30))
        g.clear_fill(0)
        assert row.isHidden()
        g.set_fill_to_value(0, 0.0)
        assert not row.isHidden()

    def test_picking_a_colour_after_the_fill_went_away_is_harmless(self, qtbot, plot):
        from PySide6.QtCore import Q_ARG, QMetaObject
        from PySide6.QtWidgets import QWidget
        g = plot.line(X, np.sin(X))
        comp = g.components()[0]
        comp.set_fill_to_value(0.0)
        delegate = DelegateRegistry.instance().create_delegate(comp, None)
        qtbot.addWidget(delegate)
        row = _fill_color_row(delegate)
        picker = next(w for w in row.findChildren(QWidget)
                      if w.metaObject().className() == "ColorDelegate")
        g.clear_fill(0)
        # A C++ exception escaping this slot would abort the process.
        QMetaObject.invokeMethod(picker, "colorChanged", Q_ARG(QColor, QColor("blue")))
        assert not comp.has_fill()


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


GAP = (20, 30)


def _empty_in_the_gap(columns):
    """A callback answering data, except empty arrays (a data gap) for the GAP window."""
    calls = []

    def cb(start, stop):
        calls.append((start, stop))
        x = np.empty(0) if (start, stop) == GAP else np.linspace(start, stop, 10)
        return (x, *columns(x))
    return cb, calls


def _show(qtbot, plot, graph, calls, window):
    plot.x_axis().set_range(*window)
    qtbot.waitUntil(lambda: window in calls, timeout=5000)
    expected = np.empty(0) if window == GAP else np.linspace(*window, 10)
    qtbot.waitUntil(lambda: np.array_equal(np.asarray(graph.data()[0]), expected), timeout=5000)


def _through_an_empty_window(qtbot, plot, graph, calls):
    plot.y_axis().set_range(0, 10)
    for window in [(0, 10), GAP, (40, 50)]:
        _show(qtbot, plot, graph, calls, window)


class TestEmptyFetchWindow:
    def test_a_line_keeps_its_names_and_colours(self, qtbot, plot):
        cb, calls = _empty_in_the_gap(lambda x: (np.column_stack([x, x + 1]),))
        g = plot.line(cb, labels=["a", "b"], colors=["red", "green"])
        _through_an_empty_window(qtbot, plot, g, calls)
        assert [c.name() for c in g.components()] == ["a", "b"]
        assert [c.color().name() for c in g.components()] == ["#ff0000", "#008000"]

    def test_a_band_keeps_its_fill(self, qtbot, plot, tmp_path):
        cb, calls = _empty_in_the_gap(lambda x: (np.full_like(x, 3.0), np.full_like(x, 7.0)))
        g = plot.band(cb, color="red")
        _through_an_empty_window(qtbot, plot, g, calls)
        assert g.components()[0].has_fill()

        def centre_is_tinted():
            img = _render(plot, tmp_path)
            return _tinted_red(img.pixelColor(img.width() // 2, img.height() // 2))
        qtbot.waitUntil(centre_is_tinted, timeout=5000)  # the new data is shown asynchronously

    def test_kept_components_stay_editable_in_the_gap(self, qtbot, plot, tmp_path):
        cb, calls = _empty_in_the_gap(
            lambda x: (np.column_stack([np.full_like(x, 3.0), np.full_like(x, 7.0)]),))
        g = plot.line(cb, labels=["a", "b"], colors=["red", "red"])
        plot.y_axis().set_range(0, 10)
        _show(qtbot, plot, g, calls, (0, 10))
        _show(qtbot, plot, g, calls, GAP)
        g.set_fill_between(0, 1)
        _show(qtbot, plot, g, calls, (40, 50))

        def centre_is_tinted():  # only the (40, 50) data can tint it: the gap drew nothing
            img = _render(plot, tmp_path)
            return _tinted_red(img.pixelColor(img.width() // 2, img.height() // 2))
        qtbot.waitUntil(centre_is_tinted, timeout=5000)
        assert g.components()[0].has_fill()

    def test_error_bars_keep_their_stems(self, qtbot, plot, tmp_path):
        cb, calls = _empty_in_the_gap(lambda x: (np.full_like(x, 5.0), 4.0))
        g = plot.error_bars(cb, color="red")
        _through_an_empty_window(qtbot, plot, g, calls)

        def stems_are_drawn():  # markers alone span a few pixels
            rows = _tinted_rows(_render(plot, tmp_path))
            return bool(rows) and max(rows) - min(rows) > 0.5 * plot.height()
        qtbot.waitUntil(stems_are_drawn, timeout=5000)


@pytest.mark.parametrize("to_batch", ["_band_batch", "_error_batch"])
def test_wrapped_callback_rejects_the_dict_batch_form(to_batch):
    import SciQLopPlots
    wrapped = SciQLopPlots._wrapped_callback(
        lambda a, b: {"data": [np.arange(3.0)] * 3, "color": 1}, getattr(SciQLopPlots, to_batch))
    with pytest.raises(TypeError, match="not the"):
        wrapped(0.0, 1.0)


ONE_LINE = {
    "single-line graph": lambda plot: plot.line(X, np.sin(X)).components()[0],
    "curve (not a multigraph)": lambda plot: plot.parametric_curve(X, np.sin(X)).components()[0],
}

BAD_PARTNERS = {
    "missing fill partner": (lambda c: c.set_fill_between(None), "missing"),
    "fill against itself": (lambda c: c.set_fill_between(c), "against itself"),
    "missing error bound": (lambda c: c.set_error_bars(None, c), "missing"),
    "error bars on itself": (lambda c: c.set_error_bars(c, c), "three distinct"),
}


@pytest.mark.parametrize("bad", BAD_PARTNERS)
@pytest.mark.parametrize("kind", ONE_LINE)
def test_a_missing_or_identical_partner_raises_on_any_component(plot, kind, bad):
    """Before the off-thread checks, a curve component only warned "unsupported" here."""
    component = ONE_LINE[kind](plot)
    call, message = BAD_PARTNERS[bad]
    with pytest.raises(ValueError, match=message):
        call(component)


THREE = np.column_stack([X, X - 1, X + 1])

def _first_component(g):
    return g.components()[0]


def _graph(g):
    return g


# setter: (who emits, the signal a slot deletes the graph from, the call)
GRAPH_DECORATIONS = {
    "set_fill_to_value": (_first_component, "fill_changed", lambda g: g.set_fill_to_value(0, 1.0)),
    "set_fill_between": (_first_component, "fill_changed", lambda g: g.set_fill_between(0, 1)),
    "clear_fill": (_first_component, "fill_changed", lambda g: g.clear_fill(0)),
    "set_fill_color": (_first_component, "fill_changed",
                       lambda g: g.set_fill_color(0, QColor("blue"))),
    "set_error_bars": (_first_component, "error_bars_changed", lambda g: g.set_error_bars(0, 1, 2)),
    "clear_error_bars": (_first_component, "error_bars_changed", lambda g: g.clear_error_bars(0)),
    "set_column_line_style": (_graph, "replot",
                              lambda g: g.set_column_line_style(0, GraphLineStyle.NoLine)),
    "set_column_marker_shape": (_graph, "replot",
                                lambda g: g.set_column_marker_shape(0, GraphMarkerShape.Circle)),
    "set_column_in_legend": (_graph, "replot", lambda g: g.set_column_in_legend(0, False)),
    "set_gap_threshold": (_graph, "replot", lambda g: g.set_gap_threshold(2.0)),
}

COMPONENT_DECORATIONS = {
    "set_fill_to_value": ("fill_changed", lambda v, lo, hi: v.set_fill_to_value(1.0)),
    "set_fill_between": ("fill_changed", lambda v, lo, hi: v.set_fill_between(lo)),
    "clear_fill": ("fill_changed", lambda v, lo, hi: v.clear_fill()),
    "set_fill_color": ("fill_changed", lambda v, lo, hi: v.set_fill_color(QColor("blue"))),
    "set_error_bars": ("error_bars_changed", lambda v, lo, hi: v.set_error_bars(lo, hi)),
    "clear_error_bars": ("error_bars_changed", lambda v, lo, hi: v.clear_error_bars()),
}

DECORATION_NAME = r"^(set|clear)_(fill|error)"
# Every multigraph-level setter that ends in replot.
GRAPH_SETTER_NAME = r"^((set|clear)_(fill|error|column)|set_gap_threshold$)"


def _setters(cls, pattern):
    import re
    return {name for name in dir(cls) if re.match(pattern, name)}


def test_every_graph_and_component_decoration_setter_is_in_the_delete_tables(plot):
    graph = plot.line(X, THREE)
    assert _setters(type(graph), GRAPH_SETTER_NAME) == set(GRAPH_DECORATIONS)
    assert _setters(type(graph.components()[0]), DECORATION_NAME) == set(COMPONENT_DECORATIONS)


@pytest.mark.parametrize("setter", GRAPH_DECORATIONS)
def test_a_slot_deleting_the_graph_while_it_decorates_a_column_does_not_crash(plot, setter):
    import shiboken6
    graph = plot.line(X, THREE)
    graph.set_fill_to_value(0, 0.0)
    emitter, signal, apply = GRAPH_DECORATIONS[setter]
    getattr(emitter(graph), signal).connect(lambda *_: shiboken6.delete(graph))
    apply(graph)  # the graph emitted replot on a dead self
    plot.replot()
    assert not shiboken6.isValid(graph)


@pytest.mark.parametrize("victim", ["graph", "component"])
@pytest.mark.parametrize("setter", COMPONENT_DECORATIONS)
def test_a_slot_deleting_the_component_while_it_decorates_itself_does_not_crash(
        plot, setter, victim):
    import shiboken6
    graph = plot.line(X, THREE)
    v, lo, hi = graph.components()
    v.set_fill_to_value(0.0)
    signal, apply = COMPONENT_DECORATIONS[setter]
    doomed = graph if victim == "graph" else v
    getattr(v, signal).connect(lambda *_: shiboken6.delete(doomed))
    apply(v, lo, hi)  # the component emitted replot on a dead self
    plot.replot()
    assert not shiboken6.isValid(v)
