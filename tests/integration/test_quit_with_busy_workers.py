"""Quitting while a worker thread is still busy must exit cleanly (issue #108).

Destroying a graph or a curve no longer waits for its worker thread (SciQLop #137,
SciQLopPlots #109): a data callback stuck in a long HTTP read, or a resample, may still
be running when the application quits. Each scenario runs in its own interpreter,
since a failure is a hang, a Qt abort or a crash at process exit.
"""
import os
import subprocess
import sys
import tempfile
import textwrap
import time

import pytest

EXIT_BUDGET = 15.0

_PRELUDE = textwrap.dedent(
    """
    import threading
    import numpy as np
    from PySide6.QtCore import QTimer
    from PySide6.QtWidgets import QApplication
    from SciQLopPlots import SciQLopPlot

    app = QApplication([])
    plot = SciQLopPlot()
    entered = threading.Event()

    def stuck_callback(start, stop):
        # Stands for a long HTTP read: blocks with the GIL released.
        entered.set()
        threading.Event().wait(60)
        x = np.linspace(start, stop, 16)
        return x, np.sin(x)

    def start_stuck_graph():
        graph = plot.plot(stuck_callback, labels=["sig"])
        assert entered.wait(10), "the callback never started"
        return graph

    def quit_through_the_event_loop():
        QTimer.singleShot(50, app.quit)
        app.exec()
    """
)

SCENARIOS = {
    "graph_removed": """
        graph = start_stuck_graph()
        plot.remove_plottable(graph)
        quit_through_the_event_loop()
    """,
    "graph_still_shown": """
        start_stuck_graph()
        quit_through_the_event_loop()
    """,
    "plot_deleted": """
        from shiboken6 import delete
        start_stuck_graph()
        delete(plot)
        quit_through_the_event_loop()
    """,
    "curve_resampling": """
        x = np.arange(10_000_000, dtype=np.float64)
        y = np.column_stack([np.sin(x * 1e-4), np.cos(x * 1e-4)])
        curve = plot.parametric_curve(x, y, labels=["a", "b"])
        curve.set_data(x, y)
        plot.remove_plottable(curve)
        quit_through_the_event_loop()
    """,
}


def _run(scenario):
    script = _PRELUDE + textwrap.dedent(SCENARIOS[scenario]) + "\nprint('LEFT_EVENT_LOOP', flush=True)\n"
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    start = time.monotonic()
    try:
        # Neutral cwd: `python -c` puts the cwd on sys.path, and from the repo root the
        # source package would shadow the built one.
        result = subprocess.run([sys.executable, "-c", script], env=env, capture_output=True,
                                text=True, timeout=60, cwd=tempfile.gettempdir())
    except subprocess.TimeoutExpired:
        pytest.fail(f"{scenario}: the process hung at exit")
    return result, time.monotonic() - start


@pytest.mark.parametrize("scenario", sorted(SCENARIOS))
def test_quitting_with_a_busy_worker_exits_cleanly(scenario):
    result, seconds = _run(scenario)
    details = f"rc={result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}"
    assert "LEFT_EVENT_LOOP" in result.stdout, details
    assert result.returncode == 0, details
    assert seconds < EXIT_BUDGET, f"exit took {seconds:.1f}s\n{details}"
    assert "QThread: Destroyed while thread" not in result.stderr, details
