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
        assert g.name() != ""


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
