"""Reproducers for the set_data terminate/OOB class
(docs/backlog-2026-06-10.md C5 + C6 + C7).

C5: function-graph data is delivered to ``set_data`` via a queued Qt signal
from the DataProviderWorker thread. ``set_data`` throws on bad input (e.g.
float32 x — Speasy's default dtype); a throw escaping a queued slot unwinds
out of ``QObject::event`` and terminates the process. The pipeline boundary
must validate-and-drop instead.

C6: only ColorMap/Histogram2D ``set_data`` had ``exception-handling=auto-on``
in bindings.xml; line/curve ``set_data`` exceptions unwound through CPython
frames (crash/UB) instead of raising a Python exception.

C7: no x/y length validation: y spans were built with x's length, so
``set_data(x_100, y_50)`` silently read past the y buffer at render time.
"""
import os
import subprocess
import sys
import textwrap

import numpy as np
import pytest

from SciQLopPlots import GraphType


def _run_script(script, timeout=30):
    return subprocess.run(
        [sys.executable, "-c", script],
        env=dict(os.environ),
        capture_output=True,
        text=True,
        timeout=timeout,
    )


_APP_PRELUDE = textwrap.dedent(
    """
    import sys
    import time

    import numpy as np
    from PySide6.QtWidgets import QApplication
    from SciQLopPlots import SciQLopPlot, SciQLopPlotRange

    app = QApplication(sys.argv)

    def pump(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            app.processEvents()
            time.sleep(0.005)
    """
)


@pytest.mark.timeout(60)
def test_pipeline_float32_x_does_not_terminate():
    """C5: a provider returning float32 x must not kill the app — the queued
    set_data throw used to escape QObject::event -> std::terminate."""
    script = _APP_PRELUDE + textwrap.dedent(
        """
        called = {"n": 0}

        def f32_provider(start, stop):
            called["n"] += 1
            x = np.linspace(start, stop, 64, dtype=np.float32)  # wrong on purpose
            y = np.sin(x).astype(np.float64)
            return x, y

        plot = SciQLopPlot()
        graph = plot.line(f32_provider)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        pump(2.0)
        assert called["n"] >= 1, "provider never called — test is vacuous"
        print("PIPELINE-OK", flush=True)
        """
    )
    result = _run_script(script)
    assert result.returncode == 0, (
        f"app died on a float32-x provider batch (rc={result.returncode})\n"
        f"stdout: {result.stdout}\nstderr: {result.stderr}"
    )
    assert "PIPELINE-OK" in result.stdout


@pytest.mark.timeout(60)
def test_pipeline_length_mismatch_does_not_terminate():
    """C5/C7 via pipeline: mismatched x/y lengths from a provider must be
    dropped, not crash or corrupt."""
    script = _APP_PRELUDE + textwrap.dedent(
        """
        called = {"n": 0}

        def bad_provider(start, stop):
            called["n"] += 1
            x = np.linspace(start, stop, 100, dtype=np.float64)
            y = np.sin(x[:50])  # half the length, wrong on purpose
            return x, y

        plot = SciQLopPlot()
        graph = plot.line(bad_provider)
        plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        pump(2.0)
        assert called["n"] >= 1, "provider never called — test is vacuous"
        print("PIPELINE-OK", flush=True)
        """
    )
    result = _run_script(script)
    assert result.returncode == 0, (
        f"app died on a length-mismatched provider batch (rc={result.returncode})\n"
        f"stdout: {result.stdout}\nstderr: {result.stderr}"
    )
    assert "PIPELINE-OK" in result.stdout


@pytest.mark.timeout(60)
def test_direct_set_data_float32_x_raises_python_exception():
    """C6: graph.set_data(x_f32, y) from Python must raise, not unwind a C++
    exception through CPython frames (subprocess: pre-fix this crashes)."""
    script = _APP_PRELUDE + textwrap.dedent(
        """
        x = np.linspace(0.0, 10.0, 64, dtype=np.float64)
        y = np.sin(x)
        plot = SciQLopPlot()
        graph = plot.line(x, y)

        try:
            graph.set_data(x.astype(np.float32), y)
            print("NO-RAISE", flush=True)
        except Exception as e:
            print(f"CAUGHT {type(e).__name__}", flush=True)
        """
    )
    result = _run_script(script)
    assert result.returncode == 0, (
        f"process crashed instead of raising (rc={result.returncode})\n"
        f"stdout: {result.stdout}\nstderr: {result.stderr}"
    )
    assert "CAUGHT" in result.stdout, f"expected a Python exception, got: {result.stdout}"


class TestLengthValidation:
    """C7: direct set_data with mismatched lengths must raise instead of
    building over-long spans over the y buffer."""

    def test_single_line_length_mismatch_raises(self, plot):
        x = np.linspace(0.0, 10.0, 100, dtype=np.float64)
        y = np.sin(x)
        graph = plot.line(x, y)  # 1D y -> SciQLopSingleLineGraph
        with pytest.raises((ValueError, RuntimeError)):
            graph.set_data(x, y[:50])

    def test_multigraph_row_mismatch_raises(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data  # y is (100, 3) -> SciQLopLineGraph
        graph = plot.line(x, y)
        with pytest.raises((ValueError, RuntimeError)):
            graph.set_data(x, y[:50, :])

    def test_curve_insufficient_y_raises(self, plot):
        x = np.linspace(0.0, 10.0, 100, dtype=np.float64)
        y2 = np.column_stack([np.sin(x), np.cos(x)])
        curve = plot.plot(x, y2, labels=["a", "b"],
                          graph_type=GraphType.ParametricCurve)
        with pytest.raises((ValueError, RuntimeError)):
            curve.set_data(x, np.sin(x))  # one column for two components

    def test_single_line_valid_data_still_works(self, plot):
        x = np.linspace(0.0, 10.0, 100, dtype=np.float64)
        graph = plot.line(x, np.sin(x))
        graph.set_data(x, np.cos(x))  # no exception

    def test_multigraph_valid_2d_still_works(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        graph = plot.line(x, y)
        graph.set_data(x, y * 2.0)  # no exception

    def test_curve_valid_multiline_still_works(self, plot):
        x = np.linspace(0.0, 10.0, 100, dtype=np.float64)
        y2 = np.column_stack([np.sin(x), np.cos(x)])
        curve = plot.plot(x, y2, labels=["a", "b"],
                          graph_type=GraphType.ParametricCurve)
        curve.set_data(x, y2 * 2.0)  # no exception
