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

    def test_failed_colormap_does_not_permanently_disable_plot(self, plot, sample_colormap_data):
        """add_color_map() registers the plottable (sets m_color_map, shows
        the colorscale) before set_data() validates shapes. If set_data()
        throws and nothing unwinds the registration, m_color_map stays
        non-null forever and every later plot.colormap() call silently
        returns None."""
        x, y, z = sample_colormap_data
        bad_y = y[:7]  # neither len(z)/len(x) (1D) nor len(z) (2D)
        with pytest.raises(Exception):
            plot.colormap(x, bad_y, z)

        cmap = plot.colormap(x, y, z)
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

    def test_non_numeric_dtype_does_not_leak_buffer(self, plot, sample_colormap_data):
        """A rejected non-numeric buffer (e.g. bool) must not leak the
        exporter's PyObject ref: PyObject_GetBuffer succeeds and takes a
        strong ref before the dtype check runs, so the reject path must
        release it instead of throwing past the release."""
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        bad_z = z.astype(bool)
        refcount_before = sys.getrefcount(bad_z)
        for _ in range(50):
            with pytest.raises(TypeError):
                cmap.set_data(x, y, bad_z)
        assert sys.getrefcount(bad_z) == refcount_before

    def test_fortran_order_z_rejected(self, plot, sample_colormap_data):
        """PyObject_GetBuffer is requested with PyBUF_ANY_CONTIGUOUS, so a
        transposed/np.asfortranarray z is silently accepted — but the
        colormap data source indexes it as row-major, misrendering every
        cell with no warning. set_data must reject it instead."""
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        fortran_z = np.asfortranarray(z)
        assert not fortran_z.flags["C_CONTIGUOUS"]
        with pytest.raises(Exception):
            cmap.set_data(x, y, fortran_z)

    def test_dimension_exceeding_int_max_rejected(self, plot, sample_colormap_data, tmp_path):
        """nx/ny/nz are validated in full size_t precision but then narrowed
        to int right before being handed to QCPSoADataSource2D, which indexes
        with int. Any dimension >= 2**31 silently wraps to a negative int
        with no error unless set_data rejects it explicitly first.

        A real >=2**31-element buffer needs to be genuinely contiguous (a
        strided/broadcast view is rejected earlier by PyObject_GetBuffer's
        PyBUF_ANY_CONTIGUOUS requirement, same mechanism as
        test_fortran_order_z_rejected) — so it can't be faked without a real
        address range. Densely allocating one would need 2-17GB, violating
        this project's no-OOM-bomb-tests convention. A sparse-file
        numpy.memmap sidesteps that: the mapping is real and contiguous (so
        the buffer export succeeds) but the file has zero allocated disk
        blocks, and no page is ever touched since the size guard fires
        before any data is read.
        """
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)

        n = 2**31 + 5  # one past INT_MAX
        path = tmp_path / "sparse_huge.bin"
        with open(path, "wb") as f:
            f.truncate(n)

        huge = np.memmap(path, dtype=np.int8, mode="r+", shape=(n,))
        assert huge.flags["C_CONTIGUOUS"]
        tiny_x = np.zeros(1, dtype=np.float64)  # nx=1 keeps the 1D-y match: ny == nz/nx == nz

        with pytest.raises(RuntimeError, match="INT_MAX"):
            cmap.set_data(tiny_x, huge, huge)

        del huge

    def test_single_column_rejected(self, plot, sample_colormap_data):
        """A key axis with a single x sample can't define a cell span:
        QCPColorMap2's ViewportDependent resample transform requires at
        least two x positions to rasterize a column
        (plottable-colormap2.cpp: `if (src.xSize() < 2) return nullptr;`)
        and silently bails to a blank render otherwise -- no exception, no
        warning, busy_changed still fires True then False as if the data
        had settled normally. Reject nx == 1 up front instead of shipping
        a data source that renders nothing."""
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        single_x = np.zeros(1, dtype=np.float64)
        single_z = np.ascontiguousarray(z[:, :1])
        with pytest.raises(RuntimeError, match="at least 2"):
            cmap.set_data(single_x, y, single_z)


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

    def test_set_contour_levels_emits_changed_signal(self, plot, sample_colormap_data):
        """set_auto_contour_levels emits contour_levels_changed so the
        inspector's auto-count spin box can refresh; set_contour_levels
        also clears the auto count (mAutoContourCount = 0) but historically
        didn't emit the same signal, leaving the spin box showing a stale
        value."""
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        cmap.set_auto_contour_levels(3)

        received = []
        cmap.contour_levels_changed.connect(lambda: received.append(True))
        cmap.set_contour_levels([0.2, 0.5, 0.8])

        assert received, "set_contour_levels should emit contour_levels_changed"

    def test_set_name_marks_user_named(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        assert cmap.has_user_name() is False
        cmap.set_name("Flux")
        assert cmap.has_user_name() is True

    def test_set_name_emits_name_changed(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        received = []
        cmap.name_changed.connect(lambda name: received.append(name))
        cmap.set_name("Flux")
        assert received == ["Flux"]


class TestColormapAutoScaleY:
    """auto_scale_y's set_data hook must respect a configured max-range-size
    limit on the value axis, not silently revert to the pre-existing zoom.

    Regression for: `set_data` called `_valueAxis->qcp_axis()->setRange(yRange)`
    directly (bypassing `SciQLopPlotAxis::set_range`), so with a max-range-size
    limit configured the `rangeChanged` clamp handler sees `clamped != requested`
    and reverts to `m_last_valid_range` -- auto-scale-y silently no-ops. This is
    the exact anti-pattern PR #76 fixed for the 'm'-rescale path
    (TestRescaleRespectsRangeLimit in test_histogram2d.py); this call site was
    missed.
    """

    @staticmethod
    def _pump(n=80):
        from PySide6.QtCore import QCoreApplication
        for _ in range(n):
            QCoreApplication.processEvents()

    def test_auto_scale_y_respects_range_limit_not_revert(self, plot, sample_colormap_data):
        from SciQLopPlots import SciQLopPlotRange
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        self._pump()
        plot.replot(True)
        self._pump()

        y_axis = cmap.y_axis()
        y_axis.set_max_range_size(3.0)
        y_axis.set_range(SciQLopPlotRange(0.0, 2.0))  # a valid zoom within the limit
        self._pump()

        cmap.set_auto_scale_y(True)
        new_x = np.linspace(0, 10, 50).astype(np.float64)
        new_y = np.linspace(0, 20, 30).astype(np.float64)  # extent 20 >> the 3.0 limit
        new_z = np.random.rand(30, 50).astype(np.float64)
        cmap.set_data(new_x, new_y, new_z)
        self._pump()

        yr = y_axis.range()
        assert not (abs(yr.start() - 0.0) < 0.5 and abs(yr.stop() - 2.0) < 0.5), \
            "auto_scale_y reverted to the pre-existing zoom instead of rescaling to new data"
        assert (yr.stop() - yr.start()) == pytest.approx(3.0, abs=0.5)
        assert 8.0 <= yr.start() <= 9.0 and 11.0 <= yr.stop() <= 12.0


class TestColorScaleAxisRangeChangedSignal:
    """SciQLopPlotColorScaleAxis wires range_changed from two places: the base
    SciQLopPlotAxis ctor connects QCPAxis::rangeChanged (on the color scale's
    internal axis) through the clamp/revert lambda, and the derived ctor
    ALSO connects QCPColorScale::dataRangeChanged directly, unconditionally.

    QCPColorScale::setDataRange() always does both `mColorAxis->setRange(...)`
    (firing the first connection) and `emit dataRangeChanged(...)` (firing
    the second) for one logical change, so range_changed fired twice with
    identical values for both a direct set_range() and a data-driven
    percentile rescale().
    """

    def test_set_range_emits_once(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        zaxis = cmap.z_axis()

        received = []
        zaxis.range_changed.connect(lambda r: received.append((r.start(), r.stop())))
        zaxis.set_range(SciQLopPlotRange(1.0, 9.0))

        assert received == [(1.0, 9.0)], (
            f"expected exactly one range_changed emission, got {received}"
        )

    def test_percentile_rescale_emits_once(self, plot):
        from test_colormap_autoscale_percentile import _make_colormap_with_spread_and_outliers
        cmap, x, y = _make_colormap_with_spread_and_outliers(plot)
        cmap.set_autoscale_percentile_low(2.5)
        cmap.set_autoscale_percentile_high(97.5)
        zaxis = cmap.z_axis()

        received = []
        zaxis.range_changed.connect(lambda r: received.append((r.start(), r.stop())))
        zaxis.rescale()

        assert len(received) == 1, (
            f"expected exactly one range_changed emission, got {received}"
        )


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

    def test_2d_y_axis_does_not_throw(self):
        """y can be 2D (per-timestamp varying frequencies).

        QCPSoADataSource2D supports both 1D y (`ny == nz/nx`) and 2D y
        (`ny == nz`). The size check must accept both — spectrograms with
        a per-timestamp frequency axis send `y` flattened from shape
        `(nx, M)` with `ny = nx*M = nz`, which violates the naive
        `nz == nx*ny` invariant.
        """
        result = _run_colormap_script(
            """
            nx, m = 20, 22
            x = np.linspace(0, 10, nx).astype(np.float64)
            y_2d = np.tile(np.linspace(0, 5, m), (nx, 1)).astype(np.float64)
            z = np.random.rand(nx, m).astype(np.float64)
            cmap = plot.colormap(x, y_2d.ravel(), z)
            cmap.set_data(x, y_2d.ravel(), z)
            print("OK")
            """
        )
        assert result.returncode == 0, (
            "set_data with 2D y axis aborted/threw "
            f"(rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-400:]}\n"
            f"stdout={result.stdout.decode(errors='replace')[-200:]}"
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
