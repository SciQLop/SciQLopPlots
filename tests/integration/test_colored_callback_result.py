"""A data batch that carries its own colour axis (SciQLop colour-axis virtual products).

A callback may return ``{"data": [t, ...], "color": c}`` instead of a plain list, and
a remote channel may call ``set_data_colored(data, color)``. The colour travels in the
same emission as its data (``new_data_colored``), so a refresh can never pair new data
with old colours. A plain list keeps its old meaning: three buffers to a line graph are
not a colour axis.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import (
    ColorGradient,
    GraphType,
    SciQLopNDProjectionPlot,
    SciQLopPlotRange,
)
from conftest import process_events

N = 200


def _range(axis):
    r = axis.range()
    return r.start(), r.stop()


def _columns(t):
    return np.column_stack([np.sin(t), np.cos(t)])


def _image(plot, tmp_path, name):
    plot.legend().set_visible(False)
    plot.rescale_axes()
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    return img.copy(0, 0, int(img.width() * 0.7), img.height())  # without the colour bar


def _has_green(img):
    return any(90 <= c.hue() <= 160
               for c in (img.pixelColor(x, y)
                         for y in range(img.height()) for x in range(img.width()))
               if c.saturation() > 100 and c.value() > 60)


def _wait_idle(qtbot, graph):
    qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
    process_events()


def _callback_line(qtbot, plot, callback):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(callback, labels=["a", "b"], graph_type=GraphType.Line)
    _wait_idle(qtbot, graph)
    return graph


class TestCallbackLineGraph:
    def test_a_coloured_result_colours_the_line(self, qtbot, plot):
        def coloured(start, stop):
            t = np.linspace(start, stop, N)
            return {"data": [t, _columns(t)], "color": np.linspace(2.0, 7.0, N)}

        _callback_line(qtbot, plot, coloured)
        qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
        assert _range(plot.z_axis()) == (pytest.approx(2.0), pytest.approx(7.0))

    def test_a_same_length_refresh_takes_the_new_colours(self, qtbot, plot):
        calls = []

        def coloured(start, stop):
            calls.append(start)
            t = np.linspace(start, stop, N)
            lo = 10.0 * len(calls)
            return {"data": [t, _columns(t)], "color": np.linspace(lo, lo + 1.0, N)}

        _callback_line(qtbot, plot, coloured)
        before = len(calls)
        plot.x_axis().set_range(SciQLopPlotRange(1.0, 11.0))
        qtbot.waitUntil(lambda: len(calls) > before, timeout=5000)
        lo = 10.0 * len(calls)
        qtbot.waitUntil(
            lambda: (process_events(), _range(plot.z_axis())[0] == pytest.approx(lo))[1],
            timeout=5000)
        assert _range(plot.z_axis()) == (pytest.approx(lo), pytest.approx(lo + 1.0))

    def test_the_stored_gradient_is_used(self, qtbot, plot, tmp_path):
        def coloured(start, stop):
            t = np.linspace(start, stop, N)
            return {"data": [t, _columns(t)], "color": np.linspace(0.0, 1.0, N)}

        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        graph = plot.plot(coloured, labels=["a", "b"], graph_type=GraphType.Line)
        graph.set_color_gradient(ColorGradient.Grayscale)
        _wait_idle(qtbot, graph)
        qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
        process_events()
        assert not _has_green(_image(plot, tmp_path, "gray"))

    def test_a_wrong_length_colour_drops_the_batch(self, qtbot, plot):
        def bad(start, stop):
            t = np.linspace(start, stop, N)
            return {"data": [t, _columns(t)], "color": np.linspace(0.0, 1.0, N - 1)}

        _callback_line(qtbot, plot, bad)
        qtbot.wait(200)
        process_events()
        assert plot.z_axis().visible() is False

    def test_a_plain_three_buffer_list_is_not_a_colour_axis(self, qtbot, plot):
        def plain(start, stop):
            t = np.linspace(start, stop, N)
            return [t, _columns(t), np.linspace(2.0, 7.0, N)]

        _callback_line(qtbot, plot, plain)
        qtbot.wait(200)
        process_events()
        assert plot.z_axis().visible() is False


class TestRemoteLineGraph:
    def test_set_data_colored_colours_and_clears_busy(self, qtbot, plot):
        graph = plot.add_remote_line_graph(["a", "b"])
        process_events()
        channel = graph.remote_channel()
        t = np.linspace(10.0, 20.0, N)

        def answer(_range):
            channel.set_data_colored([t, _columns(t)], np.linspace(3.0, 5.0, N))

        channel.data_requested.connect(answer)
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
        qtbot.waitUntil(lambda: graph.busy() is False, timeout=5000)
        assert _range(plot.z_axis()) == (pytest.approx(3.0), pytest.approx(5.0))

    def test_set_data_colored_emits_new_data_colored(self, qtbot, plot):
        graph = plot.add_remote_line_graph(["a", "b"])
        process_events()
        channel = graph.remote_channel()
        t = np.linspace(10.0, 20.0, N)
        color = np.linspace(3.0, 5.0, N)
        got = []
        channel.new_data_colored.connect(
            lambda data, c: got.append(([np.asarray(d) for d in data], np.asarray(c))))
        channel.data_requested.connect(
            lambda _r: channel.set_data_colored([t, _columns(t)], color))
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        qtbot.waitUntil(lambda: len(got) > 0, timeout=5000)
        data, c = got[-1]
        assert np.allclose(data[0], t)
        assert np.allclose(data[1], _columns(t))
        assert np.allclose(c, color)


class TestProjection:
    def test_a_coloured_result_keeps_time_and_feeds_the_scale(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.time_axis().set_range(SciQLopPlotRange(100.0, 200.0))

        def trajectory(start, stop):
            t = np.linspace(start, stop, N)
            a = np.linspace(0, 2 * np.pi, N)
            return {"data": [t, np.cos(a), np.sin(a), 0.5 * np.cos(a)],
                    "color": np.linspace(1.0, 4.0, N)}

        graph = proj.add_model_curve(trajectory, "traj")
        _wait_idle(qtbot, graph)
        qtbot.waitUntil(lambda: proj.z_axis().visible(), timeout=5000)
        assert _range(proj.z_axis()) == (pytest.approx(1.0), pytest.approx(4.0))
        assert all(p is not None for p in graph.positions_at_time(150.0))
