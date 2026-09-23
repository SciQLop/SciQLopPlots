"""Line graphs coloured point by point by a scalar (SciQLopPlots #110).

`set_color_data` on a line graph (NeoQCP QCPMultiGraph) used to raise. It now colours
the line through the plot's Z colour scale, the one curves already share: one scale
per plot, a colormap wins it, the range can be pinned or made logarithmic, and a
hidden graph does not count.

See docs/colour-by-scalar-curves-vs-line-graphs.md.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import ColorGradient, GraphType, SciQLopPlot, SciQLopPlotRange
from conftest import process_events

N = 300
PLAIN_HUES = 4  # two components, one colour each, plus antialiasing blends


@pytest.fixture
def plot(qtbot):
    p = SciQLopPlot()
    qtbot.addWidget(p)
    return p


def _two_columns(t):
    return np.column_stack([np.sin(t), 0.5 * np.cos(t)])


def _line(qtbot, plot):
    """Two components: a single column would make a SciQLopSingleLineGraph."""
    t = np.linspace(0, 2 * np.pi, N)
    g = plot.plot(t, _two_columns(t), graph_type=GraphType.Line, labels=["a", "b"])
    process_events()
    return g


def _curve(qtbot, plot):
    t = np.linspace(0, 2 * np.pi, N)
    g = plot.plot(t, np.cos(t), graph_type=GraphType.ParametricCurve, labels=["c"])
    qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
    process_events()
    return g


def _colormap(qtbot, plot, x0=0.0):
    x = np.linspace(x0, x0 + 10, 50)
    y = np.linspace(0, 5, 30)
    z = 100.0 + 10.0 * np.linspace(0, 1, 30 * 50).reshape(30, 50)
    cmap = plot.colormap(x, y, z)
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    process_events()
    return cmap


def _let_the_data_swap_land(qtbot):
    """A refresh of a drawn graph is staged and swapped in by a debounced commit."""
    qtbot.wait(500)
    process_events()


def _range(plot):
    r = plot.z_axis().range()
    return r.start(), r.stop()


def _image(plot, tmp_path, name, rescale=True):
    plot.legend().set_visible(False)
    if rescale:
        plot.rescale_axes()
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    return img.copy(0, 0, int(img.width() * 0.7), img.height())  # without the colour bar


def _saturated(img):
    return [img.pixelColor(x, y) for y in range(img.height()) for x in range(img.width())
            if img.pixelColor(x, y).saturation() > 100 and img.pixelColor(x, y).value() > 60]


def _hues(img):
    return len({c.hue() // 10 for c in _saturated(img)})


def _coloured(img):
    return len(_saturated(img))


def _has_green(img):
    return any(90 <= c.hue() <= 160 for c in _saturated(img))


class TestLineGraphIsColoured:
    def test_a_plain_line_graph_has_one_colour_per_component(self, qtbot, plot, tmp_path):
        _line(qtbot, plot)
        assert _hues(_image(plot, tmp_path, "plain")) <= PLAIN_HUES

    def test_colour_data_draws_the_gradient(self, qtbot, plot, tmp_path):
        _line(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        assert _hues(_image(plot, tmp_path, "jet")) > 8

    def test_any_numeric_dtype_is_accepted(self, qtbot, plot, tmp_path):
        _line(qtbot, plot).set_color_data(np.arange(N, dtype=np.int32), ColorGradient.Jet)
        process_events()
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(N - 1))
        assert _hues(_image(plot, tmp_path, "int")) > 8

    def test_nan_makes_a_gap(self, qtbot, plot, tmp_path):
        line = _line(qtbot, plot)
        values = np.linspace(0.0, 3.0, N)
        line.set_color_data(values, ColorGradient.Jet)
        process_events()
        full = _coloured(_image(plot, tmp_path, "full"))
        values[N // 3: 2 * N // 3] = np.nan
        line.set_color_data(values, ColorGradient.Jet)
        process_events()
        assert _coloured(_image(plot, tmp_path, "gap")) < 0.8 * full

    def test_wrong_length_raises(self, qtbot, plot):
        with pytest.raises(ValueError, match="one colour value per"):
            _line(qtbot, plot).set_color_data(np.linspace(0.0, 1.0, N - 1), ColorGradient.Jet)

    def test_an_empty_buffer_turns_the_colouring_off(self, qtbot, plot, tmp_path):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        line.set_color_data(np.array([], dtype=np.float64), ColorGradient.Jet)
        process_events()
        assert plot.z_axis().visible() is False
        assert _hues(_image(plot, tmp_path, "off")) <= PLAIN_HUES


class TestScale:
    def test_colour_data_shows_the_scale_with_the_data_range(self, qtbot, plot):
        _line(qtbot, plot).set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        assert plot.z_axis().visible() is True
        assert _range(plot) == (pytest.approx(2.0), pytest.approx(7.0))

    def test_a_hidden_line_graph_does_not_count(self, qtbot, plot):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        line.set_visible(False)
        process_events()
        assert plot.z_axis().visible() is False
        line.set_visible(True)
        process_events()
        assert plot.z_axis().visible() is True

    def test_a_hidden_line_graph_is_left_out_of_the_range(self, qtbot, plot):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 30.0, N), ColorGradient.Jet)
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(30.0))
        line.set_visible(False)
        process_events()
        assert _range(plot) == (pytest.approx(0.0), pytest.approx(3.0))

    def test_one_scale_shared_with_a_curve(self, qtbot, plot):
        _curve(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        _line(qtbot, plot).set_color_data(np.linspace(-5.0, 1.0, N), ColorGradient.Jet)
        assert _range(plot) == (pytest.approx(-5.0), pytest.approx(3.0))

    def test_removing_the_coloured_line_graph_hides_the_scale(self, qtbot, plot):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(2.0, 7.0, N), ColorGradient.Jet)
        plot.remove_plottable(line)
        qtbot.waitUntil(lambda: (process_events(), not plot.z_axis().visible())[1], timeout=3000)

    def test_the_scale_gradient_colours_the_line(self, qtbot, plot, tmp_path):
        _line(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Grayscale)
        process_events()
        assert not _has_green(_image(plot, tmp_path, "gray"))
        plot.set_z_gradient(ColorGradient.Jet)
        process_events()
        assert _has_green(_image(plot, tmp_path, "jet"))


class TestPinAndLog:
    def test_setting_the_range_pins_it(self, qtbot, plot):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 100.0))
        assert plot.z_auto_range() is False
        line.set_color_data(np.linspace(0.0, 6.0, N), ColorGradient.Jet)
        assert _range(plot) == (0.0, 100.0)
        plot.set_z_auto_range(True)
        assert _range(plot)[1] == pytest.approx(6.0)

    def test_a_pinned_range_changes_the_colours(self, qtbot, plot, tmp_path):
        _line(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        assert _has_green(_image(plot, tmp_path, "auto"))
        plot.z_axis().set_range(SciQLopPlotRange(0.0, 300.0))
        process_events()
        assert not _has_green(_image(plot, tmp_path, "pinned"))

    def test_log_scale_leaves_non_positive_values_as_gaps(self, qtbot, plot, tmp_path):
        _line(qtbot, plot).set_color_data(np.linspace(-1.0, 1.0, N), ColorGradient.Jet)
        process_events()
        linear = _coloured(_image(plot, tmp_path, "linear"))
        plot.z_axis().set_log(True)
        process_events()
        assert _range(plot)[0] > 0
        assert _coloured(_image(plot, tmp_path, "log")) < 0.75 * linear


class TestColormapWinsTheScale:
    def test_a_colormap_takes_the_scale_over(self, qtbot, plot):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        _colormap(qtbot, plot)
        plot.z_axis().set_range(SciQLopPlotRange(100.0, 110.0))
        assert plot.z_auto_range() is True
        line.set_color_data(np.linspace(0.0, 6.0, N), ColorGradient.Jet)
        assert _range(plot) == (100.0, 110.0)

    def test_the_line_graph_keeps_its_own_colours_under_a_colormap(self, qtbot, plot, tmp_path):
        """Let go of the scale, the line falls back to its own range and gradient."""
        _line(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        _colormap(qtbot, plot, x0=100.0)  # out of view: its own Jet pixels would count
        plot.z_axis().set_range(SciQLopPlotRange(100.0, 110.0))
        process_events()
        plot.x_axis().set_range(SciQLopPlotRange(-1.0, 7.0))
        plot.y_axis().set_range(SciQLopPlotRange(-1.2, 1.2))
        assert _has_green(_image(plot, tmp_path, "own", rescale=False))

    def test_the_scale_comes_back_when_the_colormap_goes(self, qtbot, plot):
        cmap = _colormap(qtbot, plot)
        _line(qtbot, plot).set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        plot.remove_plottable(cmap)
        qtbot.waitUntil(
            lambda: (process_events(), _range(plot) == (pytest.approx(0.0), pytest.approx(3.0)))[1],
            timeout=3000)
        assert plot.z_axis().visible() is True
        assert plot.z_auto_range() is True


class TestRefreshKeepsTheColours:
    def test_same_length_set_data_keeps_them(self, qtbot, plot, tmp_path):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        _image(plot, tmp_path, "first")
        t = np.linspace(0, 2 * np.pi, N)
        line.set_data(t, 0.5 * _two_columns(t))
        _let_the_data_swap_land(qtbot)
        assert plot.z_axis().visible() is True
        assert _hues(_image(plot, tmp_path, "refreshed")) > 8

    def test_another_length_drops_them(self, qtbot, plot, tmp_path):
        line = _line(qtbot, plot)
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        _image(plot, tmp_path, "first")
        t = np.linspace(0, 2 * np.pi, N // 2)
        line.set_data(t, _two_columns(t))
        _let_the_data_swap_land(qtbot)
        assert plot.z_axis().visible() is False
        assert _hues(_image(plot, tmp_path, "dropped")) <= PLAIN_HUES

    def test_a_same_length_callable_refresh_keeps_them(self, qtbot, plot, tmp_path):
        calls = []

        def data(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, N).astype(np.float64)
            return x, np.sin(x)

        line = plot.plot(data, graph_type=GraphType.Line, labels=["f"])
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 6.0))
        qtbot.waitUntil(lambda: (process_events(), len(calls) > 0 and not line.busy())[1],
                        timeout=5000)
        process_events()
        line.set_color_data(np.linspace(0.0, 3.0, N), ColorGradient.Jet)
        process_events()
        seen = len(calls)
        plot.x_axis().set_range(SciQLopPlotRange(0.5, 6.5))
        qtbot.waitUntil(lambda: (process_events(), len(calls) > seen and not line.busy())[1],
                        timeout=5000)
        _let_the_data_swap_land(qtbot)
        assert plot.z_axis().visible() is True
        assert _hues(_image(plot, tmp_path, "callable", rescale=False)) > 8
