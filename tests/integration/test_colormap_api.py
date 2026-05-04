"""Tests for colormap creation, data manipulation, and properties."""
import sys
import subprocess
import tempfile
import textwrap

import pytest
import numpy as np

from SciQLopPlots import (
    SciQLopPlot, SciQLopGraphInterface, SciQLopPlotRange,
    ColorGradient,
)
from conftest import force_gc


# Subprocess cwd: outside the repo so the in-tree `SciQLopPlots/` source dir
# doesn't shadow the installed package (which carries the .so).
_SUBPROCESS_CWD = tempfile.gettempdir()


def _run_colormap_script(body: str) -> subprocess.CompletedProcess:
    """Run a snippet that already has `app`, `plot`, and `np` in scope."""
    full = textwrap.dedent(
        """
        import sys
        import numpy as np
        from PySide6.QtWidgets import QApplication
        from SciQLopPlots import SciQLopPlot
        app = QApplication.instance() or QApplication(sys.argv)
        plot = SciQLopPlot()
        """
    ) + textwrap.dedent(body)
    return subprocess.run(
        [sys.executable, "-c", full], capture_output=True, timeout=30,
        cwd=_SUBPROCESS_CWD,
    )


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


class TestColormapEmptyData:
    """set_data with no time samples must not crash.

    Speasy returns a "well-formed empty" spectrogram for windows that fall
    inside a product's definition but contain no data:
        time.shape  = (0,)
        freq.shape  = (N,)        # constant axis present regardless
        values.shape = (0, N)     # flat size 0
    The previous all-empty shortcut required nx == ny == nz == 0, so this
    case fell through to QCPSoADataSource2D's `Q_ASSERT(nx > 0 ...)` and
    aborted via qFatal. Run in subprocesses because the unfixed code aborts
    the interpreter.
    """

    def test_empty_x_with_constant_y_does_not_crash(self):
        result = _run_colormap_script(
            """
            x0 = np.linspace(0, 10, 20).astype(np.float64)
            y0 = np.linspace(0, 5, 22).astype(np.float64)
            z0 = np.random.rand(22, 20).astype(np.float64)
            cmap = plot.colormap(x0, y0, z0)
            empty_x = np.array([], dtype=np.float64)
            const_y = np.linspace(0, 5, 22).astype(np.float64)
            empty_z = np.empty((22, 0), dtype=np.float64)
            cmap.set_data(empty_x, const_y, empty_z)
            print("OK")
            """
        )
        assert result.returncode == 0, (
            "set_data with empty time / constant freq aborted "
            f"(rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-400:]}"
        )
        assert b"OK" in result.stdout

    def test_all_empty_does_not_crash(self):
        result = _run_colormap_script(
            """
            x0 = np.linspace(0, 10, 20).astype(np.float64)
            y0 = np.linspace(0, 5, 10).astype(np.float64)
            z0 = np.random.rand(10, 20).astype(np.float64)
            cmap = plot.colormap(x0, y0, z0)
            cmap.set_data(
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
                np.array([], dtype=np.float64),
            )
            print("OK")
            """
        )
        assert result.returncode == 0, (
            "set_data with fully empty triple aborted "
            f"(rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-400:]}"
        )
        assert b"OK" in result.stdout

    def test_empty_then_real_data_recovers(self):
        """After an empty set_data, a subsequent populated update must work."""
        result = _run_colormap_script(
            """
            x0 = np.linspace(0, 10, 20).astype(np.float64)
            y0 = np.linspace(0, 5, 22).astype(np.float64)
            z0 = np.random.rand(22, 20).astype(np.float64)
            cmap = plot.colormap(x0, y0, z0)
            cmap.set_data(
                np.array([], dtype=np.float64),
                y0,
                np.empty((22, 0), dtype=np.float64),
            )
            x1 = np.linspace(0, 10, 30).astype(np.float64)
            z1 = np.random.rand(22, 30).astype(np.float64)
            cmap.set_data(x1, y0, z1)
            print("OK")
            """
        )
        assert result.returncode == 0, (
            "set_data recovery after empty payload aborted "
            f"(rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-400:]}"
        )
        assert b"OK" in result.stdout
