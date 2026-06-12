"""Reproducers for the function-graph pipeline lifetime bugs
(docs/backlog-2026-06-10.md C1 + I1).

C1: ``SimplePyCallablePipeline`` took a ``parent`` argument but never forwarded
it to ``QObject``, so the pipeline (and its ``DataProviderWorker`` + worker
``QThread`` + the Python callable held by ``SimplePyCallablePWrapper``) leaked
on every function-graph destruction. Observable from Python: a weakref to the
provider callable survives plot destruction forever.

I1: once C1 is fixed, plot destruction reaches ``~DataProviderWorker`` which
does ``quit(); wait();`` on the GUI thread. Python deletion paths hold the GIL
through the shiboken destructor, so if the worker thread is inside the user
callable (or needs the GIL to leave it), ``wait()`` deadlocks: main thread
holds GIL + blocks on wait(); worker blocks on reacquiring the GIL. Run as a
subprocess so a deadlock fails the test instead of hanging the suite.
"""
import os
import subprocess
import sys
import tempfile
import textwrap
import weakref

import numpy as np
import pytest

from SciQLopPlots import SciQLopPlot
from conftest import force_gc, process_events


def _make_provider():
    def provider(start, stop):
        x = np.linspace(start, stop, 16, dtype=np.float64)
        y = np.sin(x)
        return x, y

    return provider


@pytest.mark.timeout(30)
def test_function_graph_releases_callable_on_plot_destruction(qtbot):
    """Destroying a plot owning a function graph must release the pipeline,
    hence the Python provider callable (C1: leaked QThread pins it forever)."""
    provider = _make_provider()
    provider_ref = weakref.ref(provider)

    plot = SciQLopPlot()  # no qtbot.addWidget: Python owns it, del destroys it
    graph = plot.line(provider)
    qtbot.wait(100)  # let the worker service the initial fetch

    del graph
    del plot
    del provider

    # The wrapper dies on the worker thread (deleteLater on thread finish) and
    # its Py_DECREF is deferred to the GIL-side queue; pump events and perform
    # a GIL-holding binding op (creating/destroying another plot) to drain it.
    for _ in range(50):
        if provider_ref() is None:
            break
        process_events()
        qtbot.wait(10)
        drain = SciQLopPlot()
        drain.line(np.arange(4, dtype=np.float64), np.arange(4, dtype=np.float64))
        del drain
        force_gc()

    assert provider_ref() is None, (
        "provider callable still referenced after plot destruction: "
        "SimplePyCallablePipeline was never deleted (leaked worker QThread)"
    )


_SLOW_PROVIDER_PREAMBLE = textwrap.dedent(
    """
    import sys
    import time

    import numpy as np
    from PySide6.QtCore import QCoreApplication, QEvent
    from PySide6.QtWidgets import QApplication
    from SciQLopPlots import SciQLopPlot, SciQLopMultiPlotPanel, SciQLopPlotRange

    app = QApplication(sys.argv)

    state = {"entered": False, "finished": False}

    def slow_provider(start, stop):
        state["entered"] = True
        time.sleep(2.0)  # worker sits inside the callable during teardown
        state["finished"] = True
        x = np.linspace(start, stop, 16, dtype=np.float64)
        return x, np.sin(x)

    def wait_until_provider_entered():
        deadline = time.monotonic() + 5.0
        while not state["entered"] and time.monotonic() < deadline:
            app.processEvents()
            time.sleep(0.005)
        assert state["entered"], "fetch never reached the provider — test is vacuous"
        assert not state["finished"], "provider already finished — teardown window missed"
    """
)

_DEADLOCK_SCRIPT = _SLOW_PROVIDER_PREAMBLE + textwrap.dedent(
    """
    plot = SciQLopPlot()
    graph = plot.line(slow_provider)
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))  # force a fetch
    wait_until_provider_entered()

    # GUI thread holds the GIL through the shiboken destructor: ~DataProviderWorker
    # quit()/wait() must not deadlock against the worker reacquiring the GIL.
    del graph
    del plot

    for _ in range(20):
        app.processEvents()
        time.sleep(0.01)
    print("TEARDOWN-OK", flush=True)
    """
)


_DEADLOCK_SCRIPT_DELETELATER = _SLOW_PROVIDER_PREAMBLE + textwrap.dedent(
    """
    panel = SciQLopMultiPlotPanel()
    plot, graph = panel.plot(slow_provider)
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))  # force a fetch
    wait_until_provider_entered()

    destroyed = []
    plot.destroyed.connect(lambda *_: destroyed.append(True))

    # remove_plot/clear use deleteLater(): ~DataProviderWorker then runs inside
    # deferred-delete processing driven from Python — the GIL context differs
    # from `del`. processEvents() skips DeferredDelete; flush them explicitly.
    panel.remove_plot(plot)
    del graph
    del plot
    deadline = time.monotonic() + 10.0
    while not destroyed and time.monotonic() < deadline:
        app.processEvents()
        QCoreApplication.sendPostedEvents(None, QEvent.DeferredDelete)
        time.sleep(0.01)
    assert destroyed, "plot was never destroyed — teardown path not exercised"
    print("TEARDOWN-OK", flush=True)
    """
)


def _run_teardown_script(script):
    # Neutral cwd: `python -c` prepends the cwd to sys.path, and from the repo
    # root the source package shadows the installed SciQLopPlots.
    result = subprocess.run(
        [sys.executable, "-c", script],
        env=dict(os.environ),
        capture_output=True,
        text=True,
        timeout=30,
        cwd=tempfile.gettempdir(),
    )
    assert result.returncode == 0, (
        f"teardown subprocess failed (rc={result.returncode})\n"
        f"stdout: {result.stdout}\nstderr: {result.stderr}"
    )
    assert "TEARDOWN-OK" in result.stdout


@pytest.mark.timeout(60)
def test_plot_destruction_during_inflight_fetch_does_not_deadlock():
    """I1: deleting a plot while its worker is inside the Python callable must
    not deadlock the GUI thread in ~DataProviderWorker::wait()."""
    _run_teardown_script(_DEADLOCK_SCRIPT)


@pytest.mark.timeout(60)
def test_remove_plot_during_inflight_fetch_does_not_deadlock():
    """I1 via deleteLater: panel.remove_plot + processEvents from Python while
    the worker is inside the callable must not deadlock wait()."""
    _run_teardown_script(_DEADLOCK_SCRIPT_DELETELATER)
