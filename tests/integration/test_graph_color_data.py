"""Per-point colour data (`set_color_data`) across graph types.

`set_color_data(values, gradient)` maps a third variable onto a graph's colour.
It used to exist only on SciQLopSingleLineGraph; every other plottable inherited
an abstract no-op that discarded the data without telling the caller. Curves had
a parallel, incompatible mechanism (`set_color_values` + a two-colour ramp) whose
`draw()` override also silently dropped scatter markers and line styles.
"""
from collections import Counter

import numpy as np
import pytest
from PySide6.QtCore import QPointF
from PySide6.QtGui import QImage

from SciQLopPlots import (
    ColorGradient,
    GraphLineStyle,
    GraphMarkerShape,
    GraphType,
    SciQLopPlot,
)
from conftest import process_events

N = 400


@pytest.fixture
def spiral():
    """(x, y, c) — a parametric curve plus a monotonic colour variable."""
    a = np.linspace(0, 6 * np.pi, N)
    return (a * np.cos(a)), (a * np.sin(a)), a


def _render(plot, tmp_path, name):
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 500, 400) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    assert not img.isNull()
    return img


def _ink(plot, tmp_path, name):
    """Foreground pixel count of a rendered plot.

    "Foreground" is everything that is not the single most common colour, i.e.
    not the background fill. Counting ink rather than comparing to a fixed corner
    pixel keeps this valid whatever theme the plot renders with.
    """
    img = _render(plot, tmp_path, name)
    pixels = Counter(img.pixelColor(x, y).rgb()
                     for y in range(img.height())
                     for x in range(img.width()))
    (_background, background_count), = pixels.most_common(1)
    return sum(pixels.values()) - background_count


def _hues(plot, tmp_path, name):
    """Number of distinct 10-degree hue buckets among saturated pixels.

    Counting distinct RGB values does not separate "one colour" from "a
    gradient": antialiasing alone yields hundreds of blends of a single hue.
    Hue buckets do — a single-colour curve lands in one or two, a curve tinted
    through a gradient spreads across many.
    """
    img = _render(plot, tmp_path, name)
    buckets = set()
    for y in range(img.height()):
        for x in range(img.width()):
            c = img.pixelColor(x, y)
            if c.saturation() > 100 and c.value() > 60 and c.alpha() > 8:
                buckets.add(c.hue() // 10)
    return len(buckets)


def _curve(qtbot, plot, spiral, *, line_style=GraphLineStyle.Line,
           marker=GraphMarkerShape.NoMarker):
    """A parametric curve carrying one drawn component.

    A curve only creates components for the labels it is given, and the
    resampler that fills them runs off-thread — hence both the label and the
    wait.
    """
    x, y, _c = spiral
    g = plot.plot(x, y, graph_type=GraphType.ParametricCurve, labels=["c"])
    # The legend icon honours line style and markers on its own, which would
    # mask a curve that does not — hide it so _ink() only sees the plot area.
    plot.legend().set_visible(False)
    qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
    g.component(0).set_line_style(line_style)
    g.component(0).set_marker_shape(marker)
    plot.rescale_axes()  # otherwise only a sliver of the curve is in view
    return g


class TestCurveColorData:
    @pytest.mark.parametrize("dtype", [np.float64, np.float32, np.int32,
                                       np.uint16])
    def test_set_color_data_paints_many_colours(self, qtbot, spiral, tmp_path,
                                                dtype):
        """Any numeric dtype maps onto the gradient, not just float64."""
        plain, tinted = SciQLopPlot(), SciQLopPlot()
        for p in (plain, tinted):
            qtbot.addWidget(p)
        _curve(qtbot, plain, spiral)
        _curve(qtbot, tinted, spiral).set_color_data(
            spiral[2].astype(dtype), ColorGradient.Jet)
        process_events()

        assert _hues(plain, tmp_path, "plain") <= 2, "baseline is not single-hued"
        assert _hues(tinted, tmp_path, f"tinted_{dtype}") > 8

    def test_set_color_data_turns_time_colouring_on(self, qtbot, plot, spiral):
        curve = _curve(qtbot, plot, spiral)
        assert curve.time_color_enabled() is False
        curve.set_color_data(spiral[2], ColorGradient.Jet)
        assert curve.time_color_enabled() is True

    def test_length_mismatch_is_rejected(self, qtbot, plot, spiral):
        curve = _curve(qtbot, plot, spiral)
        with pytest.raises(ValueError):
            curve.set_color_data(spiral[2][:10], ColorGradient.Jet)

    def test_non_numeric_data_is_rejected(self, qtbot, plot, spiral):
        curve = _curve(qtbot, plot, spiral)
        strings = np.array(["a"] * N)
        with pytest.raises((TypeError, ValueError)):
            curve.set_color_data(strings, ColorGradient.Jet)

    def test_empty_data_clears_the_colouring(self, qtbot, plot, spiral):
        curve = _curve(qtbot, plot, spiral)
        curve.set_color_data(spiral[2], ColorGradient.Jet)
        assert curve.time_color_enabled() is True
        curve.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        assert curve.time_color_enabled() is False

    def test_a_constant_colour_variable_still_renders(self, qtbot, plot, spiral, tmp_path):
        """Zero colour range must not blank the curve out."""
        curve = _curve(qtbot, plot, spiral)
        curve.set_color_data(np.full(N, 7.0), ColorGradient.Jet)
        process_events()
        assert _ink(plot, tmp_path, "flat") > 0


class TestCurveHonoursStyleWhileColoured:
    """The coloured draw() path must not ignore line style or markers.

    Colouring is switched on through the older `set_color_values` +
    `set_time_color_enabled` pair on purpose: the defect lives in the custom
    `draw()`, independently of which setter armed it.
    """

    def _ink_for(self, qtbot, spiral, tmp_path, name, **style):
        p = SciQLopPlot()
        qtbot.addWidget(p)
        curve = _curve(qtbot, p, spiral, **style)
        curve.set_color_values(spiral[2].tolist())
        curve.set_time_color_enabled(True)
        process_events()
        return _ink(p, tmp_path, name)

    def test_no_line_draws_less_ink_than_a_connected_line(
            self, qtbot, spiral, tmp_path):
        connected = self._ink_for(
            qtbot, spiral, tmp_path, "connected",
            line_style=GraphLineStyle.Line, marker=GraphMarkerShape.Circle)
        dotted = self._ink_for(
            qtbot, spiral, tmp_path, "dotted",
            line_style=GraphLineStyle.NoLine, marker=GraphMarkerShape.Circle)
        assert dotted < connected

    def test_markers_are_drawn_when_the_line_is_off(
            self, qtbot, spiral, tmp_path):
        with_markers = self._ink_for(
            qtbot, spiral, tmp_path, "markers",
            line_style=GraphLineStyle.NoLine, marker=GraphMarkerShape.Circle)
        without = self._ink_for(
            qtbot, spiral, tmp_path, "nomarkers",
            line_style=GraphLineStyle.NoLine, marker=GraphMarkerShape.NoMarker)
        assert with_markers > without


class TestScatterColorData:
    def test_scatter_color_data_paints_many_colours(self, qtbot, spiral, tmp_path):
        x, y, c = spiral
        plain, tinted = SciQLopPlot(), SciQLopPlot()
        for p in (plain, tinted):
            qtbot.addWidget(p)
            g = p.plot(x, y, graph_type=GraphType.Scatter,
                       marker=GraphMarkerShape.Circle)
            p.legend().set_visible(False)
            g.component(0).set_line_style(GraphLineStyle.NoLine)
            if p is tinted:
                g.set_color_data(c, ColorGradient.Jet)
            p.rescale_axes()
        process_events()

        # Fewer buckets than the curve case: hollow circle outlines leave far
        # less saturated ink than a filled marker or a stroked line.
        assert _hues(plain, tmp_path, "s_plain") <= 2, "baseline is not single-hued"
        assert _hues(tinted, tmp_path, "s_tinted") > 5


class TestUnsupportedColorData:
    """Where per-point colour is not implemented, say so instead of no-op'ing."""

    def test_multi_component_line_graph_reports_it_is_unsupported(
            self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.plot(x, y, graph_type=GraphType.Line)
        with pytest.raises(RuntimeError, match="per-point colour"):
            graph.set_color_data(x, ColorGradient.Jet)

    def test_colormap_reports_it_is_unsupported(self, plot, sample_colormap_data):
        cmap = plot.plot(*sample_colormap_data, graph_type=GraphType.ColorMap)
        with pytest.raises(RuntimeError, match="per-point colour"):
            cmap.set_color_data(sample_colormap_data[0], ColorGradient.Jet)

    def test_the_error_names_the_offending_type(self, plot,
                                                sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.plot(x, y, graph_type=GraphType.Line)
        with pytest.raises(RuntimeError, match="SciQLopLineGraph"):
            graph.set_color_data(x, ColorGradient.Jet)


class TestPositionAtTime:
    def test_returns_the_point_nearest_a_time(self, qtbot, plot, spiral):
        x, y, c = spiral
        curve = _curve(qtbot, plot, spiral)
        curve.set_time_values(c)
        process_events()
        point = curve.position_at_time(c[N // 2])
        assert isinstance(point, QPointF)
        assert point.x() == pytest.approx(x[N // 2], abs=1e-6)
        assert point.y() == pytest.approx(y[N // 2], abs=1e-6)

    def test_returns_none_without_time_values(self, qtbot, plot, spiral):
        curve = _curve(qtbot, plot, spiral)
        process_events()
        assert curve.position_at_time(1.0) is None

    def test_clamps_to_the_ends_of_the_time_range(self, qtbot, plot, spiral):
        x, y, c = spiral
        curve = _curve(qtbot, plot, spiral)
        curve.set_time_values(c)
        process_events()
        assert curve.position_at_time(c[0] - 1e6).x() == pytest.approx(x[0], abs=1e-6)
        assert curve.position_at_time(c[-1] + 1e6).x() == pytest.approx(x[-1], abs=1e-6)
