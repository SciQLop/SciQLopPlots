"""Tests for multi-type numpy support."""
import pytest
import numpy as np


class TestMultiDtype:

    def test_line_float32(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = np.sin(x).astype(np.float32)
        g = plot.line(x, y)
        assert g is not None

    def test_line_int16(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = (np.sin(x) * 1000).astype(np.int16)
        g = plot.line(x, y)
        assert g is not None

    def test_line_uint8(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = ((np.sin(x) + 1) * 127).astype(np.uint8)
        g = plot.line(x, y)
        assert g is not None

    def test_colormap_float32(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        z32 = z.astype(np.float32)
        g = plot.plot(x, y, z32)
        assert g is not None

    def test_line_float64_still_works(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        assert g is not None
        data = g.data()
        assert len(data) >= 2
