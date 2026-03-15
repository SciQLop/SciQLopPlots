"""Tests for colormap creation, data manipulation, and properties."""
import pytest
import numpy as np

from SciQLopPlots import (
    SciQLopPlot, SciQLopGraphInterface, SciQLopPlotRange,
    ColorGradient,
)
from conftest import force_gc


class TestColormapCreation:

    def test_colormap_returns_interface(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        assert cmap is not None

    def test_colormap_with_name(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z, name="heat")
        assert cmap is not None

    def test_callable_colormap(self, plot):
        def make_data(start, stop):
            x = np.linspace(start, stop, 50).astype(np.float64)
            y = np.linspace(0, 5, 30).astype(np.float64)
            z = np.random.rand(30, 50).astype(np.float64)
            return x, y, z

        cmap = plot.colormap(make_data)
        assert cmap is not None


class TestColormapData:

    def test_set_data(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        new_z = np.random.rand(30, 50).astype(np.float64)
        cmap.set_data(x, y, new_z)
        # Should not crash; data is updated internally

    def test_data_roundtrip(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        data = cmap.data()
        assert data is not None


class TestColormapProperties:

    def test_gradient_roundtrip(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        grad = cmap.gradient()
        assert grad is not None

    def test_set_gradient(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        cmap.set_gradient(ColorGradient.Jet)
        assert cmap.gradient() == ColorGradient.Jet

    def test_y_log_scale(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        cmap.set_y_log_scale(True)
        assert cmap.y_log_scale() is True
        cmap.set_y_log_scale(False)
        assert cmap.y_log_scale() is False

    def test_z_log_scale(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        cmap.set_z_log_scale(True)
        assert cmap.z_log_scale() is True
        cmap.set_z_log_scale(False)
        assert cmap.z_log_scale() is False

    def test_delete_no_crash(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        del cmap
        force_gc()
