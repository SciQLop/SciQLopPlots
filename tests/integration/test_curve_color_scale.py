"""A Z colour scale for curves on ordinary plots (SciQLop issue #141).

`set_color_data` on a curve coloured it over its own min/max with its own gradient
and drew no scale, so the values could not be read and the range could not be
pinned. The plot's colour scale, the one colormaps use, now serves its curves.
A colormap on the same plot keeps the scale to itself. Projection panes switch
this off: their plot has one scale for all panes.

See docs/colour-by-scalar-curves-vs-line-graphs.md.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import (
    ColorGradient,
    GraphType,
    SciQLopNDProjectionPlot,
    SciQLopPlot,
    SciQLopPlotRange,
)
from conftest import process_events

N = 300


@pytest.fixture
def plot(qtbot):
    p = SciQLopPlot()
    qtbot.addWidget(p)
    return p


def _curve(qtbot, plot, scale=1.0):
    t = np.linspace(0, 2 * np.pi, N)
    g = plot.plot(t, np.sin(t), graph_type=GraphType.ParametricCurve, labels=["c"])
    qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
    process_events()
    return g


def _range(plot):
    r = plot.z_axis().range()
    return r.start(), r.stop()


def _image(plot, tmp_path, name):
    plot.legend().set_visible(False)
    plot.rescale_axes()
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    return img.copy(0, 0, int(img.width() * 0.7), img.height())  # without the colour bar


def _hues(img):
    return len({img.pixelColor(x, y).hue() // 10
                for y in range(img.height()) for x in range(img.width())
                if img.pixelColor(x, y).saturation() > 100 and img.pixelColor(x, y).value() > 60})


def _coloured(img):
    return sum(1 for y in range(img.height()) for x in range(img.width())
               if img.pixelColor(x, y).saturation() > 100)


def _has_green(img):
    return any(90 <= img.pixelColor(x, y).hue() <= 160 and img.pixelColor(x, y).saturation() > 100
               for y in range(img.height()) for x in range(img.width()))


class TestScaleAppears:
    def test_no_scale_without_a_colour_scalar(self, qtbot, plot):
        _curve(qtbot, plot)
        assert plot.z_axis().visible() is False

    def test_a_colour_scalar_shows_the_scale_with_the_data_range(self, qtbot, plot):
        _curve(qtbot, plot).set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        assert plot.z_axis().visible() is True
        assert _range(plot) == (pytest.approx(2.0), pytest.approx(7.0))

    def test_clearing_the_scalar_hides_it_again(self, qtbot, plot):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        assert plot.z_axis().visible() is True
        curve.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        process_events()
        assert plot.z_axis().visible() is False

    def test_removing_the_coloured_curve_hides_it(self, qtbot, plot):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        assert plot.z_axis().visible() is True
        plot.remove_plottable(curve)
        qtbot.waitUntil(lambda: (process_events(), not plot.z_axis().visible())[1], timeout=3000)


class TestOneScaleForSeveralCurves:
    def test_the_range_covers_every_curve(self, qtbot, plot):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 30.0, N), ColorGradient.Jet)
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(30.0))


class TestScaleControls:
    def test_setting_the_range_pins_it(self, qtbot, plot):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        assert plot.z_auto_range() is True
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 100.0))
        assert plot.z_auto_range() is False
        curve.set_color_data(np.linspace(0.0, 6.0, N), ColorGradient.Jet)
        assert _range(plot) == (0.0, 100.0)
        plot.set_z_auto_range(True)
        assert _range(plot)[1] == pytest.approx(6.0)

    def test_the_scale_gradient_colours_the_curve(self, qtbot, plot, tmp_path):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        process_events()
        assert not _has_green(_image(plot, tmp_path, "gray"))
        plot.set_z_gradient(ColorGradient.Jet)
        process_events()
        assert _has_green(_image(plot, tmp_path, "jet"))

    def test_log_scale_leaves_non_positive_values_as_gaps(self, qtbot, plot, tmp_path):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(-1.0, 1.0, N), ColorGradient.Jet)
        process_events()
        linear = _coloured(_image(plot, tmp_path, "linear"))
        plot.z_axis().set_log(True)
        process_events()
        assert plot.z_axis().log() is True
        assert _range(plot)[0] > 0
        assert _coloured(_image(plot, tmp_path, "log")) < 0.75 * linear

    def test_a_pinned_range_changes_the_colours(self, qtbot, plot, tmp_path):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        assert _has_green(_image(plot, tmp_path, "auto"))
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 300.0))
        process_events()
        assert not _has_green(_image(plot, tmp_path, "pinned"))


class TestOptOut:
    def test_disabled_curves_colour_themselves_without_a_scale(self, qtbot, plot, tmp_path):
        plot.set_curve_color_scale_enabled(False)
        assert plot.curve_color_scale_enabled() is False
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        assert plot.z_axis().visible() is False
        assert _hues(_image(plot, tmp_path, "own")) > 8


class TestColormapKeepsItsScale:
    def test_a_curve_does_not_take_the_scale_from_a_colormap(self, qtbot, plot):
        x = np.linspace(0, 10, 50)
        y = np.linspace(0, 5, 30)
        z = 100.0 + 10.0 * np.linspace(0, 1, 30 * 50).reshape(30, 50)
        cmap = plot.colormap(x, y, z)
        qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
        process_events()
        plot.z_axis().set_range(SciQLopPlotRange(100.0, 110.0))
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        assert _range(plot) == (100.0, 110.0)


class TestProjectionPanesStayQuiet:
    def test_a_projection_plot_still_has_exactly_one_scale(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        t = np.linspace(0, 2 * np.pi, N)
        g = proj.parametric_curve([t, np.cos(t), np.sin(t), t], labels=["a", "b", "c"])
        qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
        g.set_color_data(t, ColorGradient.Jet)
        process_events()
        assert [proj.subplot(i).z_axis().visible() for i in range(3)] == [False, False, True]
        assert all(proj.subplot(i).curve_color_scale_enabled() is False for i in range(3))
