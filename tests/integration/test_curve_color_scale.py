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
from PySide6.QtCore import QModelIndex
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QComboBox

from SciQLopPlots import (
    ColorGradient,
    DelegateRegistry,
    GraphType,
    PlotsModel,
    SciQLopMultiPlotPanel,
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


class TestScaleShownByHand:
    def test_a_curve_uses_a_scale_that_was_already_shown(self, qtbot, plot):
        """show_color_scale() is public: with the bar already up, the first coloured
        curve was never attached to it and the range stayed at the axis' default."""
        plot.show_color_scale()
        _curve(qtbot, plot).set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        assert _range(plot) == (pytest.approx(2.0), pytest.approx(7.0))

    def test_a_pinned_range_drives_a_curve_on_a_scale_shown_by_hand(self, qtbot, plot, tmp_path):
        plot.show_color_scale()
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        wide = _hues(_image(plot, tmp_path, "auto"))
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 300.0))
        process_events()
        assert _hues(_image(plot, tmp_path, "pinned")) < wide


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


def _bar_hues(plot, tmp_path, name):
    """Hues in the right fifth of the image: the colour bar."""
    plot.legend().set_visible(False)
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    return len({img.pixelColor(x, y).hue() // 10
                for y in range(img.height()) for x in range(int(img.width() * 0.8), img.width())
                if img.pixelColor(x, y).saturation() > 100 and img.pixelColor(x, y).value() > 60})


def _colormap(qtbot, plot):
    x = np.linspace(0, 10, 50)
    y = np.linspace(0, 5, 30)
    z = 100.0 + 10.0 * np.linspace(0, 1, 30 * 50).reshape(30, 50)
    cmap = plot.colormap(x, y, z)
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    process_events()
    return cmap


class TestColormapOwnershipBothWays:
    def test_a_curve_gradient_does_not_recolour_a_colormap(self, qtbot, plot, tmp_path):
        """The gradient call used to reach the colormap's scale: the ownership
        check only guarded the attach, not the gradient."""
        _colormap(qtbot, plot)
        before = _bar_hues(plot, tmp_path, "before")
        assert before > 8
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        process_events()
        assert _bar_hues(plot, tmp_path, "after") == before

    def test_a_colormap_added_after_a_curve_takes_the_scale_over(self, qtbot, plot):
        """The controller used to keep treating the scale as its own once it had shown
        it, so curve and colormap shared one scale and a range set for the colormap
        pinned the curves' auto-range."""
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        _colormap(qtbot, plot)
        plot.z_axis().set_range(SciQLopPlotRange(100.0, 110.0))
        assert plot.z_auto_range() is True
        curve.set_color_data(np.linspace(0.0, 6.0, N), ColorGradient.Jet)
        assert _range(plot) == (100.0, 110.0)


class TestColormapGoesAway:
    def test_the_scale_goes_with_the_colormap_when_nothing_is_coloured(self, qtbot, plot):
        cmap = _colormap(qtbot, plot)
        assert plot.z_axis().visible() is True
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), not plot.z_axis().visible())[1], timeout=3000)

    def test_curves_take_the_scale_back_when_the_colormap_is_removed(self, qtbot, plot):
        cmap = _colormap(qtbot, plot)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
                        timeout=3000)
        assert plot.z_axis().visible() is True
        assert plot.z_auto_range() is True


class TestGradientSurvivesTheColormap:
    def test_a_curve_gradient_asked_during_ownership_shows_when_the_colormap_goes(
            self, qtbot, plot, tmp_path):
        """The curve's request was dropped while the colormap owned the scale, so the
        curves came back in the colormap's Jet."""
        cmap = _colormap(qtbot, plot)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
                        timeout=3000)
        assert plot.z_axis().visible() is True
        assert _bar_hues(plot, tmp_path, "gray") <= 2

    def test_the_users_gradient_set_during_ownership_is_not_stomped(self, qtbot, plot, tmp_path):
        cmap = _colormap(qtbot, plot)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.set_z_gradient(ColorGradient.Grayscale)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
                        timeout=3000)
        assert _bar_hues(plot, tmp_path, "gray") <= 2

    def test_reclaiming_keeps_the_auto_range(self, qtbot, plot):
        cmap = _colormap(qtbot, plot)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), plot.z_axis().visible())[1], timeout=3000)
        process_events()
        assert plot.z_auto_range() is True


def _plot_children(panel, plot):
    model = PlotsModel.instance()
    for row in range(model.rowCount(QModelIndex())):
        top = model.index(row, 0, QModelIndex())
        if PlotsModel.object(top) is not panel:
            continue
        for prow in range(model.rowCount(top)):
            pidx = model.index(prow, 0, top)
            if PlotsModel.object(pidx) is plot:
                return [PlotsModel.object(model.index(r, 0, pidx))
                        for r in range(model.rowCount(pidx))]
    raise AssertionError("plot not found in the inspector")


class TestPinSurvivesTheColormap:
    def test_a_pinned_range_comes_back_when_the_colormap_goes(self, qtbot, plot):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 50.0))
        assert plot.z_auto_range() is False
        cmap = _colormap(qtbot, plot)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), plot.z_axis().visible())[1], timeout=3000)
        process_events()
        assert plot.z_auto_range() is False
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(50.0))


class TestPinAgainstAColormapThatDrivesTheScale:
    def test_a_pin_survives_a_colormap_that_rescales_the_scale(self, qtbot, plot):
        """A callable colormap delivers its data after it was added and then drives the
        shared scale to its own range: the pin set before it was lost on reclaim."""
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 50.0))

        def data(start, stop):
            x = np.linspace(start, stop, 50).astype(np.float64)
            y = np.linspace(0, 5, 30).astype(np.float64)
            return x, y, 100.0 + 10.0 * np.random.rand(30, 50)

        cmap = plot.colormap(data)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
        process_events()
        assert _range(plot) != (pytest.approx(0.0), pytest.approx(50.0)), "the colormap never drove the scale"
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), plot.z_axis().visible())[1], timeout=3000)
        process_events()
        assert plot.z_auto_range() is False
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(50.0))

    def test_a_pin_survives_an_explicit_rescale_beside_a_colormap(self, qtbot, plot):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 50.0))
        cmap = _colormap(qtbot, plot)
        plot.z_axis().rescale()
        process_events()
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), plot.z_axis().visible())[1], timeout=3000)
        process_events()
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(50.0))


class TestScaleShownAndHiddenByHand:
    def test_a_gradient_picked_before_the_curve_shows_on_a_scale_shown_by_hand(
            self, qtbot, plot, tmp_path):
        plot.show_color_scale()
        plot.set_z_gradient(ColorGradient.Grayscale)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        assert _bar_hues(plot, tmp_path, "gray") <= 2

    def test_a_scale_shown_by_hand_goes_with_the_last_coloured_curve(self, qtbot, plot):
        plot.show_color_scale()
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        curve.set_color_data(np.array([]), ColorGradient.Jet)
        process_events()
        assert plot.z_axis().visible() is False

    def test_a_scale_hidden_by_hand_comes_back_with_the_next_coloured_curve(self, qtbot, plot):
        first = _curve(qtbot, plot)
        first.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.hide_color_scale()
        assert plot.z_axis().visible() is False
        second = _curve(qtbot, plot)
        second.set_color_data(np.linspace(1.0, 4.0, N), ColorGradient.Jet)
        process_events()
        assert plot.z_axis().visible() is True
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(4.0))


class TestGradientChosenWhileDisabled:
    def test_a_gradient_set_while_the_curve_scale_is_off_applies_when_it_is_back(
            self, qtbot, plot, tmp_path):
        plot.set_curve_color_scale_enabled(False)
        plot.set_z_gradient(ColorGradient.Grayscale)
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        plot.set_curve_color_scale_enabled(True)
        process_events()
        assert _bar_hues(plot, tmp_path, "gray") <= 2


class TestInspectorFollowsTheScale:
    def test_the_colour_axes_leave_the_inspector_with_the_colormap(self, qtbot):
        panel = SciQLopMultiPlotPanel()
        qtbot.addWidget(panel)
        plot = SciQLopPlot()
        panel.add_plot(plot)
        process_events()
        cmap = _colormap(qtbot, plot)
        process_events()
        assert plot.z_axis() in _plot_children(panel, plot)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), plot.z_axis() not in _plot_children(panel, plot))[1],
                        timeout=3000)
        assert plot.y2_axis() not in _plot_children(panel, plot)


def _pick_in_inspector(qtbot, plot, gradient_name):
    delegate = DelegateRegistry.instance().create_delegate(plot.z_axis(), None)
    qtbot.addWidget(delegate)
    combo = next(c for c in delegate.findChildren(QComboBox)
                 if c.metaObject().className() == "ColorGradientDelegate")
    combo.setCurrentIndex(combo.findText(gradient_name))
    process_events()
    return delegate


class TestInspectorGradientIsRemembered:
    def test_a_gradient_picked_in_the_inspector_survives_a_colormap(self, qtbot, plot, tmp_path):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        delegate = _pick_in_inspector(qtbot, plot, "Grayscale")
        assert _bar_hues(plot, tmp_path, "picked") <= 2
        cmap = _colormap(qtbot, plot)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
                        timeout=3000)
        assert _bar_hues(plot, tmp_path, "after") <= 2
        del delegate

    def test_an_open_inspector_does_not_record_the_colormaps_own_gradient(self, qtbot, plot, tmp_path):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        delegate = _pick_in_inspector(qtbot, plot, "Grayscale")
        cmap = _colormap(qtbot, plot)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
                        timeout=3000)
        assert _bar_hues(plot, tmp_path, "after") <= 2
        del delegate


class TestUserGradientBesideAColormap:
    def test_set_z_gradient_still_works_when_the_user_asks(self, qtbot, plot, tmp_path):
        """Only a curve's own gradient call is kept away from a colormap's scale; the
        plot's public setter is an explicit request."""
        _colormap(qtbot, plot)
        assert _bar_hues(plot, tmp_path, "jet") > 8
        plot.set_z_gradient(ColorGradient.Grayscale)
        process_events()
        assert _bar_hues(plot, tmp_path, "gray") <= 2


def test_destroying_a_plot_that_shows_a_scale_is_safe(qtbot):
    from shiboken6 import delete
    plot = SciQLopPlot()
    curve = _curve(qtbot, plot)
    curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
    assert plot.z_axis().visible() is True
    delete(plot)
    process_events()


class TestVisibilityFollowsTheScale:
    def test_hiding_the_only_coloured_curve_hides_the_scale(self, qtbot, plot):
        curve = _curve(qtbot, plot)
        curve.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        assert plot.z_axis().visible() is True
        curve.set_visible(False)
        process_events()
        assert plot.z_axis().visible() is False
        curve.set_visible(True)
        process_events()
        assert plot.z_axis().visible() is True

    def test_hiding_a_projection_graph_hides_the_shared_scale(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        t = np.linspace(0, 2 * np.pi, N)
        g = proj.parametric_curve([t, np.cos(t), np.sin(t), t], labels=["a", "b", "c"])
        qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
        g.set_color_data(t, ColorGradient.Jet)
        assert proj.z_axis().visible() is True
        g.set_visible(False)
        process_events()
        assert proj.z_axis().visible() is False
        g.set_visible(True)
        process_events()
        assert proj.z_axis().visible() is True


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
