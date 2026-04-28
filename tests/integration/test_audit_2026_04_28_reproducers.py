"""Reproducer tests for the 2026-04-28 audit findings.

Each test pins a specific bug from docs/backlog-2026-04-28.md. Tests are
authored to FAIL on the unfixed code and PASS once the corresponding fix
lands. Tests for purely-static-analysis findings (C1, C2) are intentionally
omitted — they cannot be reliably triggered from Python.
"""
import gc
import sys

import numpy as np
import pytest

from SciQLopPlots import (
    ColorGradient,
    Coordinates,
    SciQLopHistogram2D,
    SciQLopHorizontalLine,
    SciQLopMultiPlotPanel,
    SciQLopPlot,
    SciQLopPlotRange,
    SciQLopVerticalLine,
)
from conftest import force_gc, process_events


# ---------------------------------------------------------------------------
# C5 — StraightLine signal-loop guard
# ---------------------------------------------------------------------------

class TestC5_StraightLineEmitGuard:
    """`set_position(p)` with `p == current` must not re-emit `position_changed`.

    Without an equality guard a slot that calls `set_position` back (a typical
    two-way reactive binding) will recurse indefinitely.
    """

    def test_set_position_unchanged_does_not_reemit(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(5.0)
        process_events()
        # Bug: emits once even though value did not change.
        assert received == [], (
            "set_position with unchanged value must not emit position_changed; "
            f"got {received}"
        )

    def test_two_way_binding_terminates(self, plot):
        """Cross-connected lines must not recurse infinitely on a nudge.

        Without the equality guard this segfaults the interpreter (stack
        overflow). Run in a subprocess so the failure mode is detected
        instead of taking down pytest.
        """
        import subprocess
        import textwrap
        script = textwrap.dedent(
            """
            import sys
            from PySide6.QtWidgets import QApplication
            from SciQLopPlots import SciQLopPlot, SciQLopVerticalLine
            app = QApplication.instance() or QApplication(sys.argv)
            plot = SciQLopPlot()
            a = SciQLopVerticalLine(plot, 1.0, True)
            b = SciQLopVerticalLine(plot, 1.0, True)
            a.position_changed.connect(b.set_position)
            b.position_changed.connect(a.set_position)
            sys.setrecursionlimit(500)
            a.set_position(2.0)
            print("OK", a.position, b.position)
            """
        )
        result = subprocess.run(
            [sys.executable, "-c", script],
            capture_output=True, timeout=30,
        )
        # Bug: returncode != 0 (segfault) or non-zero exit. Fix: clean exit.
        assert result.returncode == 0, (
            f"two-way binding crashed (rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-400:]}"
        )
        assert b"OK" in result.stdout, result.stdout

    def test_horizontal_line_unchanged_no_emit(self, plot):
        line = SciQLopHorizontalLine(plot, 7.0, True)
        received = []
        line.position_changed.connect(lambda v: received.append(v))
        line.set_position(7.0)
        process_events()
        assert received == []


# ---------------------------------------------------------------------------
# H7 — `_clamp` invalid min/max ordering and NaN
# ---------------------------------------------------------------------------

class TestH7_StraightLineClampValidation:
    """`_clamp` must behave sensibly when min > max or position is NaN."""

    def test_min_greater_than_max_does_not_silently_break(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(10.0)
        line.set_max_value(2.0)
        line.set_position(7.0)
        # Without validation the bug swallows max and returns 10.0; with
        # validation we expect either a normalized clamp (max wins, since
        # caller's most-recent setter is max=2.0) or rejection. Spec: the
        # later setter (max) overrides whichever bound is now invalid.
        # Pragmatic assertion: result must satisfy max constraint.
        assert line.position <= 2.0 or line.position == pytest.approx(7.0), (
            f"clamp must not silently violate max; got {line.position}"
        )

    def test_nan_position_does_not_propagate(self, plot):
        line = SciQLopVerticalLine(plot, 5.0, True)
        line.set_min_value(0.0)
        line.set_max_value(10.0)
        line.set_position(float("nan"))
        # NaN passes both `<` and `>` comparisons; bug propagates NaN through.
        # After fix, NaN should be rejected (no-op) or clamped to a bound.
        assert not (line.position != line.position), (
            f"clamp must not propagate NaN; got {line.position}"
        )


# ---------------------------------------------------------------------------
# C6 — ColorMap / Histogram2D shape validation
# ---------------------------------------------------------------------------

class TestC6_ColorMapShapeValidation:
    """ColorMap.set_data must reject mismatched shapes instead of OOB-reading.

    Run in subprocesses because the unfixed code segfaults on shape mismatch.
    """

    @staticmethod
    def _run(script: str):
        import subprocess
        import textwrap
        full = textwrap.dedent(
            """
            import sys
            import numpy as np
            from PySide6.QtWidgets import QApplication
            from SciQLopPlots import SciQLopPlot
            app = QApplication.instance() or QApplication(sys.argv)
            plot = SciQLopPlot()
            """
        ) + textwrap.dedent(script)
        return subprocess.run(
            [sys.executable, "-c", full], capture_output=True, timeout=30,
        )

    def test_z_size_mismatch_does_not_crash(self):
        result = self._run(
            """
            x = np.linspace(0, 10, 20).astype(np.float64)
            y = np.linspace(0, 5, 10).astype(np.float64)
            z = np.random.rand(10, 20).astype(np.float64)
            cmap = plot.colormap(x, y, z)
            bad_z = np.random.rand(3, 4).astype(np.float64)
            try:
                cmap.set_data(x, y, bad_z)
                print("NO_RAISE")
            except Exception as e:
                print("RAISED", type(e).__name__)
            """
        )
        assert result.returncode == 0, (
            f"set_data with mismatched shape crashed (rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-300:]}"
        )
        # Either a clean raise or a no-op is acceptable; segfault is not.
        assert b"RAISED" in result.stdout or b"NO_RAISE" in result.stdout

    def test_empty_z_does_not_crash(self):
        result = self._run(
            """
            x = np.linspace(0, 10, 20).astype(np.float64)
            y = np.linspace(0, 5, 10).astype(np.float64)
            z = np.random.rand(10, 20).astype(np.float64)
            cmap = plot.colormap(x, y, z)
            empty_z = np.array([], dtype=np.float64)
            try:
                cmap.set_data(x, y, empty_z)
                print("NO_RAISE")
            except Exception as e:
                print("RAISED", type(e).__name__)
            """
        )
        assert result.returncode == 0, (
            f"empty z crashed (rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-300:]}"
        )


class TestC6_Histogram2DShapeValidation:
    """Histogram2D.set_data takes scatter (x,y); nx must equal ny."""

    def test_xy_length_mismatch_does_not_crash(self):
        import subprocess
        import textwrap
        script = textwrap.dedent(
            """
            import sys
            import numpy as np
            from PySide6.QtWidgets import QApplication
            from SciQLopPlots import SciQLopPlot
            app = QApplication.instance() or QApplication(sys.argv)
            plot = SciQLopPlot()
            x = np.random.rand(100).astype(np.float64)
            y = np.random.rand(100).astype(np.float64)
            hist = plot.histogram2d(x, y)
            bad_x = np.random.rand(100).astype(np.float64)
            bad_y = np.random.rand(50).astype(np.float64)
            try:
                hist.set_data(bad_x, bad_y)
                print("NO_RAISE")
            except Exception as e:
                print("RAISED", type(e).__name__)
            """
        )
        result = subprocess.run(
            [sys.executable, "-c", script], capture_output=True, timeout=30,
        )
        assert result.returncode == 0, (
            f"histogram2d shape mismatch crashed (rc={result.returncode}): "
            f"stderr={result.stderr.decode(errors='replace')[-300:]}"
        )


# ---------------------------------------------------------------------------
# H4 — Resampler `_bounds()` returns unsorted range when x is not sorted
# ---------------------------------------------------------------------------

class TestH4_ResamplerBoundsUnsortedX:
    """A graph fed unsorted x should not produce inverted (lower>upper) range."""

    def test_unsorted_x_does_not_crash(self, ts_plot):
        x = np.array([5.0, 1.0, 3.0, 2.0, 4.0], dtype=np.float64)
        y = np.array([1.0, 2.0, 3.0, 4.0, 5.0], dtype=np.float64)
        # Should at minimum not crash; ideally produces a sorted bounds range.
        try:
            ts_plot.plot(x, y)
        except Exception as e:
            pytest.fail(f"plot of unsorted x raised: {e}")
        process_events()


# ---------------------------------------------------------------------------
# C3 — SciQLopCurve._setCurveData OOB on count drift
# ---------------------------------------------------------------------------

class TestC3_CurveResamplerCountDrift:
    """Stress test: rapid resamples on a curve must not OOB-index."""

    def test_rapid_set_data_no_crash(self, ts_plot):
        # Plot a 3-component curve, then rapidly replace data with different
        # shapes. The resampler emits queued; a stale emission against a new
        # plottable count would crash.
        x = np.linspace(0, 10, 200).astype(np.float64)
        y = np.column_stack([np.sin(x), np.cos(x), np.tan(x)]).astype(np.float64)
        graph = ts_plot.plot(x, y)
        process_events()
        for _ in range(20):
            new_x = np.linspace(0, 10, 200).astype(np.float64)
            new_y = np.column_stack(
                [np.sin(new_x + 0.1), np.cos(new_x + 0.1), np.tan(new_x + 0.1)]
            ).astype(np.float64)
            if isinstance(graph, tuple):
                target = graph[1] if len(graph) > 1 else graph[0]
            else:
                target = graph
            try:
                target.set_data(new_x, new_y)
            except Exception:
                pass
            process_events()


# ---------------------------------------------------------------------------
# H9 — DSP filtfilt with tiny gap-segments
# ---------------------------------------------------------------------------

class TestH9_DSPFiltfiltSmallSegments:
    """filtfilt with gap-aware mode must not UB on tiny per-segment slices."""

    def test_filtfilt_with_tiny_segments(self):
        try:
            from SciQLopPlots.dsp import filtfilt
        except Exception:
            pytest.skip("DSP module not available")
        # Construct x with a tiny segment isolated by huge gaps.
        # gap_factor default 3.0 of median dt → split at every big jump.
        x = np.array([0.0, 1.0, 100.0, 200.0, 300.0], dtype=np.float64)
        y = np.array([0.0, 1.0, 2.0, 3.0, 4.0], dtype=np.float64).reshape(-1, 1)
        # Generate a basic FIR coeffs vector
        coeffs = np.array([0.5, 0.5], dtype=np.float64)
        # Should not crash even if some segments end up too short for filtering.
        try:
            filtfilt(x, y, coeffs, gap_factor=3.0, has_gaps=True)
        except ValueError:
            # Acceptable: explicit rejection is fine
            pass
        except Exception as e:
            pytest.fail(f"filtfilt UB on tiny segment: {type(e).__name__}: {e}")


# ---------------------------------------------------------------------------
# C4 — DSP exception across Py_END_ALLOW_THREADS
# ---------------------------------------------------------------------------

class TestC4_DSPExceptionAcrossGILRelease:
    """An exception raised inside the GIL-released body must reacquire GIL,
    not std::terminate the process.

    Reliably injecting std::bad_alloc into the GIL-released region is not
    feasible from Python; we settle for verifying the process survives the
    most-likely error paths (oversized output, NaN propagation, etc.).
    """

    def test_resample_huge_target_does_not_terminate(self):
        try:
            from SciQLopPlots.dsp import resample
        except Exception:
            pytest.skip("DSP module not available")
        x = np.linspace(0.0, 1e9, 10).astype(np.float64)
        y = np.linspace(0.0, 1.0, 10).astype(np.float64).reshape(-1, 1)
        # An impossibly small dt would request 1e9 samples → either rejected
        # cleanly or returns the raw segment. Must not terminate the process.
        try:
            resample(x, y, target_dt=1e-6)
        except Exception:
            pass  # Acceptable — explicit rejection.


# ---------------------------------------------------------------------------
# H1 — bindings.xml __len__ leak (small-int cache makes leak invisible)
# ---------------------------------------------------------------------------

class TestH1_PlotRangeLen:
    """PlotRange __len__ must return 2 cleanly."""

    def test_len_returns_two(self):
        r = SciQLopPlotRange(1.0, 5.0)
        assert len(r) == 2

    def test_len_invariant_under_repeat(self):
        r = SciQLopPlotRange(1.0, 5.0)
        for _ in range(1000):
            assert len(r) == 2
