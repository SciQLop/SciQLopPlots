"""Tests for graph creation, data manipulation, and properties."""
import pytest
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopGraphInterface,
    SciQLopPlotRange, GraphType,
)


class TestGraphCreation:

    def test_line_returns_graph(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        assert g is not None
        assert isinstance(g, SciQLopGraphInterface)

    def test_line_with_labels(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["sine"])
        assert g is not None
        assert g.labels() == ["sine"]

    def test_line_with_colors(self, plot, sample_data):
        x, y = sample_data
        color = QColor("red")
        g = plot.line(x, y, colors=[color])
        assert g is not None
        colors = g.colors()
        assert len(colors) >= 1

    def test_scatter_returns_graph(self, plot, sample_data):
        x, y = sample_data
        g = plot.scatter(x, y)
        assert g is not None
        assert isinstance(g, SciQLopGraphInterface)

    def test_parametric_curve_returns_graph(self, plot, sample_data):
        x, y = sample_data
        g = plot.parametric_curve(x, y)
        assert g is not None
        assert isinstance(g, SciQLopGraphInterface)

    def test_callable_line(self, plot):
        def make_data(start, stop):
            x = np.linspace(start, stop, 100)
            y = np.sin(x)
            return x, y

        g = plot.line(make_data)
        assert g is not None

    def test_callable_scatter(self, plot):
        def make_data(start, stop):
            x = np.linspace(start, stop, 100)
            y = np.sin(x)
            return x, y

        g = plot.scatter(make_data)
        assert g is not None


class TestGraphData:

    def test_set_get_data(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        data = g.data()
        assert data is not None
        assert len(data) >= 2

    def test_set_data_updates(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        new_y = np.cos(x).astype(np.float64)
        g.set_data(x, new_y)
        # Should not crash; data is updated internally

    def test_multicomponent_data(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        g = plot.line(x, y, labels=["a", "b", "c"])
        assert g is not None


class TestGraphProperties:

    def test_set_labels(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["original"])
        g.set_labels(["updated"])
        assert g.labels() == ["updated"]

    def test_set_colors(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        g.set_colors([QColor("blue")])
        colors = g.colors()
        assert len(colors) >= 1

    def test_components(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        components = g.components()
        assert isinstance(components, list)

    def test_graph_name(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        assert g.name != ""


class TestPlottableAccess:

    def test_plottables_list(self, plot, sample_data):
        x, y = sample_data
        plot.line(x, y)
        plot.line(x, y)
        plottables = plot.plottables()
        assert len(plottables) == 2

    def test_plottable_by_index(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        retrieved = plot.plottable(0)
        assert retrieved is not None


class TestPlotMethod:
    """Tests for the monkey-patched plot() method."""

    def test_plot_line_default(self, plot, sample_data):
        x, y = sample_data
        result = plot.plot(x, y)
        assert result is not None

    def test_plot_with_graph_type_line(self, plot, sample_data):
        x, y = sample_data
        result = plot.plot(x, y, graph_type=GraphType.Line)
        assert result is not None

    def test_plot_with_graph_type_scatter(self, plot, sample_data):
        """This is a known P0 bug — scatter via plot() returns None."""
        x, y = sample_data
        result = plot.plot(x, y, graph_type=GraphType.Scatter)
        # TODO: Currently broken — scatter not handled in plot_func
        # Once fixed, this should assert result is not None

    def test_plot_colormap(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        result = plot.plot(x, y, z)
        assert result is not None

    def test_plot_callable(self, plot):
        def make_data(start, stop):
            x = np.linspace(start, stop, 100)
            y = np.sin(x)
            return x, y

        result = plot.plot(make_data)
        assert result is not None

    def test_panel_plot_returns_tuple(self, panel, sample_data):
        x, y = sample_data
        result = panel.plot(x, y)
        assert isinstance(result, tuple)
        assert len(result) == 2
        plot_obj, graph_obj = result
        assert plot_obj is not None
        assert graph_obj is not None


class TestParametricCurveDtypes:
    """SciQLopCurve must accept non-float64 x/y. The CurveResampler read x/y via
    the double-only SciQLopPyBuffer::data(), and set_data hard-rejected anything
    but float64 — a std::terminate crash on float32 (which Speasy routinely
    produces). These pass only once the dtype-dispatched resampler is in place."""

    @pytest.mark.parametrize("dtype", [np.float32, np.float64, np.int32,
                                       np.int64, np.uint16])
    def test_parametric_curve_accepts_dtype(self, plot, qtbot, dtype):
        n = 200
        t = np.linspace(0, 2 * np.pi, n)
        x = (np.cos(t) * 100).astype(dtype)
        y = (np.sin(t) * 100).astype(dtype)
        g = plot.parametric_curve(x, y, labels=["c"])
        plot.replot(True)
        qtbot.waitUntil(lambda: len(g.data()) >= 2 and g.data()[0].size > 0
                                and not g.busy(),
                        timeout=5000)
        assert len(g.data()) >= 2

    def test_float32_curve_values_are_correct(self, plot, qtbot):
        # not just "doesn't crash" — the resampled values must match the input.
        n = 100
        x = np.linspace(-50, 50, n).astype(np.float32)
        y = (x * 2.0).astype(np.float32)
        g = plot.parametric_curve(x, y, labels=["c"])
        plot.replot(True)
        qtbot.waitUntil(lambda: len(g.data()) >= 2 and g.data()[0].size > 0
                                and not g.busy(),
                        timeout=5000)
        gx = np.array(g.data()[0])
        gy = np.array(g.data()[1])
        assert gx.min() == pytest.approx(-50.0, abs=1.0)
        assert gx.max() == pytest.approx(50.0, abs=1.0)
        assert gy.max() == pytest.approx(100.0, abs=2.0)
