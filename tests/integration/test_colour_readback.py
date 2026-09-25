"""What a user sets on the colour axis can be read back from Python.

LineGraph.color_data() / color_gradient() and Plot.z_gradient() used not to exist,
so tests had to render a PNG and look for green pixels.
"""
import numpy as np

from SciQLopPlots import ColorGradient, GraphType, SciQLopNDProjectionPlot, SciQLopPlotRange
from conftest import process_events

N = 100


def _columns(t):
    return np.column_stack([np.sin(t), np.cos(t)])


def _line(plot):
    x = np.linspace(0.0, 10.0, N)
    return plot.line(x, _columns(x), labels=["a", "b"])


class TestLineGraph:
    def test_no_colour_data_reads_back_none(self, plot):
        assert _line(plot).color_data() is None

    def test_colour_data_reads_back(self, plot):
        graph = _line(plot)
        values = np.linspace(1.0, 2.0, N)
        graph.set_color_data(values, ColorGradient.Hot)
        assert np.allclose(np.asarray(graph.color_data()), values)

    def test_clearing_the_colour_data_reads_back_none(self, plot):
        graph = _line(plot)
        graph.set_color_data(np.linspace(1.0, 2.0, N), ColorGradient.Hot)
        graph.set_color_data(np.array([], dtype=np.float64), ColorGradient.Hot)
        assert graph.color_data() is None

    def test_a_refresh_of_another_length_drops_the_colour_data(self, plot):
        graph = _line(plot)
        graph.set_color_data(np.linspace(1.0, 2.0, N), ColorGradient.Hot)
        x = np.linspace(0.0, 10.0, N + 5)
        graph.set_data(x, _columns(x))
        assert graph.color_data() is None

    def test_gradient_reads_back(self, plot):
        graph = _line(plot)
        graph.set_color_gradient(ColorGradient.Polar)
        assert graph.color_gradient() == ColorGradient.Polar
        graph.set_color_data(np.linspace(1.0, 2.0, N), ColorGradient.Hot)
        assert graph.color_gradient() == ColorGradient.Hot

    def test_a_coloured_batch_reads_back(self, qtbot, plot):
        color = np.linspace(3.0, 5.0, N)

        def coloured(start, stop):
            t = np.linspace(start, stop, N)
            return {"data": [t, _columns(t)], "color": color}

        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        graph = plot.plot(coloured, labels=["a", "b"], graph_type=GraphType.Line)
        qtbot.waitUntil(lambda: graph.color_data() is not None, timeout=5000)
        assert np.allclose(np.asarray(graph.color_data()), color)


class TestPlotGradient:
    def test_plot_z_gradient_reads_back(self, plot):
        plot.set_z_gradient(ColorGradient.Hot)
        assert plot.z_gradient() == ColorGradient.Hot

    def test_projection_plot_z_gradient_reads_back(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_z_gradient(ColorGradient.Cold)
        process_events()
        assert proj.z_gradient() == ColorGradient.Cold
