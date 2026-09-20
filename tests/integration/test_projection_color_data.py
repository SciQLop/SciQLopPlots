"""Colouring a projection-plot curve by a scalar (SciQLop issue #141, SciQLopPlots side).

Before: `SciQLopNDProjectionCurves.set_color_data` fell through to the base-class
throw, the only gradient on the `3n` data path was the two-stop
`set_time_color_gradient`, `colors()` returned `[]` after a successful
`set_colors` (the facade keeps its curves in `m_curves`, not in the wrapper's
component list) and there was no way to change the line width.
"""
from collections import Counter

import numpy as np
import pytest
from PySide6.QtGui import QColor, QImage

from SciQLopPlots import ColorGradient, SciQLopNDProjectionPlot
from conftest import process_events

N = 300


@pytest.fixture
def orbit():
    t = np.linspace(0, 4 * np.pi, N)
    return t, 10 * np.cos(t), 10 * np.sin(t), 3 * t / (4 * np.pi)


def _projection(qtbot, orbit):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    graph = proj.parametric_curve(list(orbit), labels=["xy", "yz", "zx"])
    qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
    process_events()
    return proj, graph


def _pane_image(proj, tmp_path, name):
    pane = proj.subplot(0)
    pane.legend().set_visible(False)
    pane.rescale_axes()
    path = tmp_path / f"{name}.png"
    assert pane.save_png(str(path), 400, 400) is True
    return QImage(str(path)).convertToFormat(QImage.Format_ARGB32)


def _ink(img):
    pixels = Counter(img.pixelColor(x, y).rgb()
                     for y in range(img.height()) for x in range(img.width()))
    (_bg, bg_count), = pixels.most_common(1)
    return sum(pixels.values()) - bg_count


def _hues(img):
    buckets = set()
    for y in range(img.height()):
        for x in range(img.width()):
            c = img.pixelColor(x, y)
            if c.saturation() > 100 and c.value() > 60 and c.alpha() > 8:
                buckets.add(c.hue() // 10)
    return len(buckets)


class TestSetColorData:
    def test_is_forwarded_to_the_panes(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        assert graph.time_color_enabled() is False
        graph.set_color_data(orbit[3], ColorGradient.Thermal)
        assert graph.time_color_enabled() is True

    def test_paints_a_full_gradient(self, qtbot, orbit, tmp_path):
        proj, graph = _projection(qtbot, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "jet")) > 8

    def test_length_mismatch_is_a_value_error(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        with pytest.raises(ValueError):
            graph.set_color_data(orbit[3][:10], ColorGradient.Jet)

    def test_non_numeric_data_keeps_its_own_error(self, qtbot, orbit):
        """Not the SystemError the enum converter reports when the buffer
        conversion already left an error pending."""
        _proj, graph = _projection(qtbot, orbit)
        with pytest.raises((TypeError, ValueError)):
            graph.set_color_data(np.array(["a"] * N), ColorGradient.Jet)

    def test_empty_data_turns_colouring_off(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        graph.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        assert graph.time_color_enabled() is False


class TestColorGradientSetter:
    def test_gradient_applies_to_the_3n_data_path(self, qtbot, orbit, tmp_path):
        proj, graph = _projection(qtbot, orbit)
        x, y, z, c = orbit[1], orbit[2], orbit[2] * 0.5, orbit[3]
        graph.set_data([x, y, c, y, z, c, z, x, c])
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        graph.set_time_color_enabled(True)
        process_events()
        two_stop = _hues(_pane_image(proj, tmp_path, "two_stop"))
        assert two_stop > 0, "the 3n data never reached the panes"

        graph.set_color_gradient(ColorGradient.Jet)
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "jet_3n")) > two_stop


class TestColorsAccessor:
    def test_colors_returns_what_set_colors_stored(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        wanted = [QColor("#ff0000"), QColor("#00ff00"), QColor("#0000ff")]
        graph.set_colors(wanted)
        assert [c.name() for c in graph.colors()] == [c.name() for c in wanted]


class TestLineWidth:
    def test_line_width_round_trips(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        graph.set_line_width(4.0)
        assert graph.line_width() == pytest.approx(4.0)

    def test_a_thicker_line_draws_more_ink(self, qtbot, orbit, tmp_path):
        proj, graph = _projection(qtbot, orbit)
        graph.set_line_width(1.0)
        process_events()
        thin = _ink(_pane_image(proj, tmp_path, "thin"))
        graph.set_line_width(5.0)
        process_events()
        assert _ink(_pane_image(proj, tmp_path, "thick")) > thin


class TestColourFollowsTheData:
    def test_a_3n_payload_switches_colouring_on(self, qtbot, orbit):
        _proj, graph = _projection(qtbot, orbit)
        x, y, z, c = orbit[1], orbit[2], orbit[2] * 0.5, orbit[3]
        assert graph.time_color_enabled() is False
        graph.set_data([x, y, c, y, z, c, z, x, c])
        assert graph.time_color_enabled() is True

    def test_a_time_refresh_does_not_replace_the_colour_scalar(
            self, qtbot, orbit, tmp_path):
        """Colour and the time marker used to exclude each other: every n+1
        refresh overwrote the scalar with the time values. The scalar here has
        two values (about 6 hue buckets with antialiasing), where time paints a
        full ramp (about 23)."""
        proj, graph = _projection(qtbot, orbit)
        two_valued = np.where(np.arange(N) < N // 2, 0.0, 1.0)
        graph.set_color_data(two_valued, ColorGradient.Jet)
        graph.set_data(list(orbit))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "after_refresh")) < 12
