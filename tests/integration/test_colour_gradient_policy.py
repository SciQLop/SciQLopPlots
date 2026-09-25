"""Which gradient a plot's shared colour scale shows.

One rule for line graphs, curves and projections: the plot's gradient wins, unless
the graph was given one explicitly (set_color_gradient, or set_color_data's
gradient argument). A coloured data batch never brings a gradient of its own.
"""
import numpy as np

from SciQLopPlots import ColorGradient, GraphType, SciQLopNDProjectionPlot, SciQLopPlotRange
from conftest import process_events

N = 100


def _columns(t):
    return np.column_stack([np.sin(t), np.cos(t)])


def _coloured_line(start, stop):
    t = np.linspace(start, stop, N)
    return {"data": [t, _columns(t)], "color": np.linspace(1.0, 2.0, N)}


def _coloured_curve(start, stop):
    t = np.linspace(start, stop, N)
    return {"data": [np.cos(t), np.sin(t)], "color": np.linspace(1.0, 2.0, N)}


def _wait_coloured(qtbot, plot):
    qtbot.waitUntil(lambda: plot.z_axis().visible(), timeout=5000)
    process_events()


class TestLineGraph:
    def test_a_coloured_batch_keeps_the_plot_gradient(self, qtbot, plot):
        plot.set_z_gradient(ColorGradient.Hot)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        plot.plot(_coloured_line, labels=["a", "b"], graph_type=GraphType.Line)
        _wait_coloured(qtbot, plot)
        assert plot.z_gradient() == ColorGradient.Hot

    def test_an_explicit_graph_gradient_wins(self, qtbot, plot):
        plot.set_z_gradient(ColorGradient.Hot)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        graph = plot.plot(_coloured_line, labels=["a", "b"], graph_type=GraphType.Line)
        graph.set_color_gradient(ColorGradient.Cold)
        _wait_coloured(qtbot, plot)
        assert plot.z_gradient() == ColorGradient.Cold

    def test_a_gradient_picked_on_the_plot_survives_refreshes(self, qtbot, plot):
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        graph = plot.plot(_coloured_line, labels=["a", "b"], graph_type=GraphType.Line)
        graph.set_color_gradient(ColorGradient.Cold)
        _wait_coloured(qtbot, plot)
        plot.set_z_gradient(ColorGradient.Polar)
        plot.x_axis().set_range(SciQLopPlotRange(1.0, 11.0))
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        qtbot.wait(100)
        process_events()
        assert plot.z_gradient() == ColorGradient.Polar


class TestCurve:
    def test_setting_a_gradient_on_an_uncoloured_curve_leaves_the_plot_alone(self, plot):
        plot.set_z_gradient(ColorGradient.Hot)
        t = np.linspace(0.0, 6.0, N)
        curve = plot.parametric_curve(np.cos(t), np.sin(t), labels=["c"])
        curve.set_color_gradient(ColorGradient.Cold)
        process_events()
        assert plot.z_gradient() == ColorGradient.Hot

    def test_a_coloured_batch_brings_the_explicit_curve_gradient(self, qtbot, plot):
        plot.set_z_gradient(ColorGradient.Hot)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        curve = plot.plot(_coloured_curve, labels=["c"], graph_type=GraphType.ParametricCurve)
        curve.set_color_gradient(ColorGradient.Cold)
        _wait_coloured(qtbot, plot)
        assert plot.z_gradient() == ColorGradient.Cold


class TestProjection:
    def test_a_coloured_batch_keeps_the_plot_gradient(self, qtbot):
        proj = SciQLopNDProjectionPlot(3)
        qtbot.addWidget(proj)
        proj.set_z_gradient(ColorGradient.Hot)
        proj.time_axis().set_range(SciQLopPlotRange(100.0, 200.0))

        def trajectory(start, stop):
            t = np.linspace(start, stop, N)
            a = np.linspace(0, 2 * np.pi, N)
            return {"data": [t, np.cos(a), np.sin(a), 0.5 * np.cos(a)],
                    "color": np.linspace(1.0, 4.0, N)}

        proj.add_model_curve(trajectory, "traj")
        qtbot.waitUntil(lambda: proj.z_axis().visible(), timeout=5000)
        assert proj.z_gradient() == ColorGradient.Hot
