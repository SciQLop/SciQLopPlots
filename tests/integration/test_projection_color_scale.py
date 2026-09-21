"""One colour scale for a whole projection plot (SciQLop issue #141).

A projection plot is N panes showing the same quantity, so it gets exactly one
scale: shared by every pane and every graph, shown once, never one per pane.
Curves coloured by a scalar read range, log scale and gradient from it.
"""
from collections import Counter

import numpy as np
import pytest
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QComboBox

from SciQLopPlots import ColorGradient, DelegateRegistry, SciQLopNDProjectionPlot, SciQLopPlotRange
from conftest import process_events

N = 300


@pytest.fixture
def orbit():
    t = np.linspace(0, 4 * np.pi, N)
    return t, 10 * np.cos(t), 10 * np.sin(t), 3 * t / (4 * np.pi)


def _graph(qtbot, proj, orbit, labels=("xy", "yz", "zx")):
    graph = proj.parametric_curve(list(orbit), labels=list(labels))
    qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
    process_events()
    return graph


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


def _visible_scales(proj):
    return [i for i in range(3) if proj.subplot(i).z_axis().visible()]


def _range(axis):
    r = axis.range()
    return r.start(), r.stop()


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


def _coloured(img):
    """Saturated pixels: the curve, without axes, ticks and labels (grey)."""
    return sum(1 for y in range(img.height()) for x in range(img.width())
               if img.pixelColor(x, y).saturation() > 100)


def _has_green(img):
    """Jet passes through green; a blue-to-red two-stop ramp never does."""
    return any(90 <= img.pixelColor(x, y).hue() <= 160
               and img.pixelColor(x, y).saturation() > 100
               for y in range(img.height()) for x in range(img.width()))


def _hues(img):
    buckets = set()
    for y in range(img.height()):
        for x in range(img.width()):
            c = img.pixelColor(x, y)
            if c.saturation() > 100 and c.value() > 60 and c.alpha() > 8:
                buckets.add(c.hue() // 10)
    return len(buckets)


class TestOneScalePerPlot:
    def test_the_plot_exposes_a_z_axis(self, proj):
        assert proj.z_axis() is not None

    def test_no_scale_is_shown_without_a_colour_scalar(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit)
        assert _visible_scales(proj) == []
        assert proj.z_axis().visible() is False

    def test_a_colour_scalar_shows_exactly_one_scale(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit).set_color_data(orbit[3], ColorGradient.Jet)
        assert len(_visible_scales(proj)) == 1
        assert proj.z_axis().visible() is True

    def test_a_second_graph_does_not_add_a_second_scale(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit).set_color_data(orbit[3], ColorGradient.Jet)
        second = _graph(qtbot, proj, orbit, labels=("a", "b", "c"))
        second.set_color_data(orbit[3] * 2, ColorGradient.Jet)
        assert len(_visible_scales(proj)) == 1


class TestAutoRange:
    def test_range_follows_the_data(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit).set_color_data(orbit[3], ColorGradient.Jet)
        lo, hi = _range(proj.z_axis())
        assert lo == pytest.approx(orbit[3].min())
        assert hi == pytest.approx(orbit[3].max())

    def test_range_covers_every_graph(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit).set_color_data(orbit[3], ColorGradient.Jet)
        second = _graph(qtbot, proj, orbit, labels=("a", "b", "c"))
        second.set_color_data(orbit[3] * 10, ColorGradient.Jet)
        lo, hi = _range(proj.z_axis())
        assert (lo, hi) == (pytest.approx(0.0), pytest.approx(30.0))

    def test_nan_does_not_poison_the_range(self, qtbot, proj, orbit):
        values = orbit[3].copy()
        values[0] = np.nan
        _graph(qtbot, proj, orbit).set_color_data(values, ColorGradient.Jet)
        lo, hi = _range(proj.z_axis())
        assert np.isfinite([lo, hi]).all()
        assert hi == pytest.approx(3.0)

    def test_a_constant_scalar_gets_a_usable_range(self, qtbot, proj, orbit, tmp_path):
        _graph(qtbot, proj, orbit).set_color_data(np.full(N, 7.0), ColorGradient.Jet)
        lo, hi = _range(proj.z_axis())
        assert lo < 7.0 < hi
        process_events()
        assert _ink(_pane_image(proj, tmp_path, "flat")) > 0


class TestPinnedRange:
    def test_setting_the_range_switches_auto_range_off(self, qtbot, proj, orbit):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        assert proj.z_auto_range() is True
        proj.z_axis().set_range(SciQLopPlotRange(0.0, 100.0))
        assert proj.z_auto_range() is False
        graph.set_color_data(orbit[3] * 2, ColorGradient.Jet)
        assert _range(proj.z_axis()) == (0.0, 100.0)

    def test_a_pinned_range_changes_how_the_curve_is_coloured(
            self, qtbot, proj, orbit, tmp_path):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        process_events()
        auto = _hues(_pane_image(proj, tmp_path, "auto"))
        proj.z_axis().set_range(SciQLopPlotRange(0.0, 300.0))
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "pinned")) < auto / 2

    def test_auto_range_can_be_switched_back_on(self, qtbot, proj, orbit):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        proj.z_axis().set_range(SciQLopPlotRange(0.0, 100.0))
        proj.set_z_auto_range(True)
        assert _range(proj.z_axis())[1] == pytest.approx(3.0)


class TestScaleDrivesTheCurve:
    def test_the_scale_gradient_colours_the_curve(self, qtbot, proj, orbit, tmp_path):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Grayscale)
        process_events()
        gray = _hues(_pane_image(proj, tmp_path, "gray"))
        proj.set_z_gradient(ColorGradient.Jet)
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "jet")) > gray + 5

    def test_log_scale_leaves_non_positive_values_as_gaps(
            self, qtbot, proj, orbit, tmp_path):
        t = np.linspace(0, 2 * np.pi, N)  # one turn: the orbit must not overdraw its own gap
        graph = _graph(qtbot, proj, (t, 10 * np.cos(t), 10 * np.sin(t), t))
        graph.set_color_data(np.linspace(-1.0, 1.0, N), ColorGradient.Jet)
        process_events()
        linear = _coloured(_pane_image(proj, tmp_path, "linear"))
        proj.z_axis().set_log(True)
        process_events()
        assert proj.z_axis().log() is True
        assert _coloured(_pane_image(proj, tmp_path, "log")) < 0.75 * linear

    def test_the_label_is_settable(self, qtbot, proj, orbit):
        _graph(qtbot, proj, orbit).set_color_data(orbit[3], ColorGradient.Jet)
        proj.z_axis().set_label("n [cm^-3]")
        assert proj.z_axis().label() == "n [cm^-3]"


def _three_n(orbit):
    x, y, z, c = orbit[1], orbit[2], orbit[2] * 0.5, orbit[3]
    return [x, y, c, y, z, c, z, x, c]


class TestGradientChoice:
    def test_a_gradient_chosen_before_the_first_scalar_is_kept(
            self, qtbot, proj, orbit, tmp_path):
        """The scale used to be stamped with the default blue-to-red ramp the
        moment the first scalar arrived, discarding an earlier choice."""
        graph = _graph(qtbot, proj, orbit)
        proj.set_z_gradient(ColorGradient.Jet)
        graph.set_data(_three_n(orbit))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        process_events()
        assert _has_green(_pane_image(proj, tmp_path, "kept"))

    def test_a_preset_can_be_reapplied_after_a_two_stop_gradient(
            self, qtbot, proj, orbit, tmp_path):
        from PySide6.QtGui import QColor
        graph = _graph(qtbot, proj, orbit)
        graph.set_data(_three_n(orbit))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        proj.set_z_gradient(ColorGradient.Jet)
        proj.set_time_color_gradient(QColor("green"), QColor("green"))
        process_events()
        assert _hues(_pane_image(proj, tmp_path, "green")) <= 3
        proj.set_z_gradient(ColorGradient.Jet)
        process_events()
        assert _has_green(_pane_image(proj, tmp_path, "jet_again"))


class TestInspectorComboFollowsTheGradient:
    def test_a_two_stop_gradient_reads_custom_and_a_preset_can_be_picked_again(
            self, qtbot, proj, orbit, tmp_path):
        """The combo kept naming the last preset while the bar showed a two-stop ramp,
        and picking that same preset did nothing."""
        from PySide6.QtGui import QColor
        graph = _graph(qtbot, proj, orbit)
        graph.set_data(_three_n(orbit))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        proj.set_z_gradient(ColorGradient.Jet)
        delegate = DelegateRegistry.instance().create_delegate(proj.z_axis(), None)
        qtbot.addWidget(delegate)
        combo = next(c for c in delegate.findChildren(QComboBox)
                     if c.metaObject().className() == "ColorGradientDelegate")
        assert combo.currentText() == "Jet"
        proj.set_time_color_gradient(QColor("green"), QColor("green"))
        process_events()
        assert combo.currentText() == "Custom"
        combo.setCurrentIndex(combo.findText("Jet"))
        process_events()
        assert _has_green(_pane_image(proj, tmp_path, "jet_from_custom"))
        assert combo.findText("Custom") < 0, "a dead Custom entry is left in the popup"


class TestScaleGoesAwayWithTheColour:
    """The scale used to stay on screen, frozen at the range of data that no longer
    exists, after the last scalar was cleared or the last coloured graph removed."""

    def test_clearing_the_scalar_hides_the_scale(self, qtbot, proj, orbit):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        assert proj.z_axis().visible() is True
        graph.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        process_events()
        assert proj.z_axis().visible() is False
        assert _visible_scales(proj) == []

    def test_colouring_again_shows_it_again_with_the_new_range(self, qtbot, proj, orbit):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        graph.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        graph.set_color_data(orbit[3] * 100, ColorGradient.Jet)
        assert len(_visible_scales(proj)) == 1
        assert _range(proj.z_axis())[1] == pytest.approx(300.0)

    def test_removing_the_last_coloured_graph_hides_the_scale(self, qtbot, proj, orbit):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(orbit[3], ColorGradient.Jet)
        assert proj.z_axis().visible() is True
        proj.remove_plottable(graph)
        qtbot.waitUntil(lambda: (process_events(), not proj.z_axis().visible())[1], timeout=3000)

    def test_removing_one_of_two_coloured_graphs_keeps_the_scale(self, qtbot, proj, orbit):
        first = _graph(qtbot, proj, orbit)
        first.set_color_data(orbit[3], ColorGradient.Jet)
        second = _graph(qtbot, proj, orbit, labels=("a", "b", "c"))
        second.set_color_data(orbit[3] * 10, ColorGradient.Jet)
        proj.remove_plottable(second)
        qtbot.waitUntil(
            lambda: (process_events(), _range(proj.z_axis())[1] == pytest.approx(3.0))[1],
            timeout=3000)
        assert proj.z_axis().visible() is True

    def test_clearing_the_scalar_is_not_a_gradient_choice(self, qtbot, proj, orbit, tmp_path):
        graph = _graph(qtbot, proj, orbit)
        graph.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        graph.set_data(_three_n(orbit))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        process_events()
        assert not _has_green(_pane_image(proj, tmp_path, "default_ramp"))


def test_the_plot_does_not_expose_its_graph_internal_hook(proj):
    assert not hasattr(proj, "update_color_scale")
