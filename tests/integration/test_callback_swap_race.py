"""Regression guard for the mutex<->GIL lock-ordering hazard in
``SimplePyCallablePWrapper``.

The data-provider worker runs on a C++ ``QThread``. To service a fetch it used
to copy the stored callable *while holding* ``m_callable_mutex``:

    { QMutexLocker lock(&m_callable_mutex); cb = m_callable; }   // copy = Py_INCREF

Copying a ``GetDataPyCallable`` does a Python **incref**, which needs the GIL.
So the worker took the path **mutex -> GIL**. Meanwhile ``set_callable`` /
``callable`` are called from Python (GIL already held) and take the mutex:
**GIL -> mutex**. Inverted lock order => a real deadlock window:

    worker:  holds m_callable_mutex, blocks on PyGILState_Ensure()
    python:  holds GIL (in set_callable), blocks on m_callable_mutex

The fix stores the callable behind a ``shared_ptr`` and snapshots it under the
mutex with a pure C++ atomic refcount bump (no GIL); the Python incref/decref
then happens *outside* the lock. So the worker never takes mutex->GIL.

This test hammers ``set_callable`` / ``callable()`` from a background Python
thread while the main thread drives fetches (``set_range``) and pumps the event
loop. On the pre-fix build this tends to **hang** (caught by ``--timeout``); on
the fixed build it completes and the graph keeps producing data.

Note (honest): a deadlock repro is inherently probabilistic — a clean pass is
strong evidence, not a proof. The value is as a fast hang-detector under the
suite's ``--timeout`` plus a liveness assertion afterwards.
"""
import threading
import time

import numpy as np
import pytest

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


def _make_provider(tag):
    """A cheap (start, stop) -> (x, y) provider tagged so we can tell which
    callable produced the last batch."""
    def cb(start, stop):
        n = 16
        x = np.linspace(start, stop, n, dtype=np.float64)
        # encode the tag in the data so a liveness check can confirm flow
        y = (np.sin(x) + tag).astype(np.float64)
        return x, y
    cb.tag = tag
    return cb


@pytest.mark.timeout(30)
def test_concurrent_set_callable_during_fetch_does_not_deadlock(qtbot):
    """Swap the callable from a worker Python thread while the main thread
    fetches; must not deadlock and must stay live."""
    plot = SciQLopPlot()
    qtbot.addWidget(plot)

    g = plot.line(_make_provider(0.0))
    qtbot.wait(40)

    stop = threading.Event()
    swap_count = [0]
    errors = []

    def swapper():
        i = 0
        try:
            while not stop.is_set():
                i += 1
                # set_callable: GIL -> mutex (the side that used to deadlock
                # against the worker's mutex -> GIL copy)
                g.set_callable(_make_provider(float(i % 7)))
                # callable(): also copies under the lock in the old code
                _ = g.callable()
                swap_count[0] = i
        except Exception as e:  # pragma: no cover - surfaced via assert below
            errors.append(repr(e))

    t = threading.Thread(target=swapper, name="callable-swapper", daemon=True)
    t.start()
    try:
        # Main thread: keep the worker busy fetching while swaps happen.
        deadline = time.monotonic() + 3.0
        r = 0.0
        while time.monotonic() < deadline:
            r += 1.0
            g.set_range(SciQLopPlotRange(r, r + 100.0))
            qtbot.wait(5)  # pumps the event loop and releases the GIL
    finally:
        stop.set()
        t.join(timeout=10.0)

    assert not t.is_alive(), "swapper thread is stuck (deadlock on mutex<->GIL)"
    assert not errors, f"swapper raised: {errors}"
    assert swap_count[0] > 0, "no callable swaps happened — test did not exercise the path"

    # Liveness: the pipeline still services a fresh fetch after all the churn.
    seen = []

    def final_cb(start, stop):
        seen.append((start, stop))
        x = np.linspace(start, stop, 8, dtype=np.float64)
        return x, np.cos(x)

    g.set_callable(final_cb)
    g.set_range(SciQLopPlotRange(10_000.0, 10_100.0))
    qtbot.waitUntil(lambda: len(seen) >= 1, timeout=5000)


@pytest.mark.timeout(30)
def test_callable_getter_during_fetch_is_safe(qtbot):
    """The ``callable()`` getter (copy under lock in the old code) hammered
    concurrently with fetches must not deadlock or corrupt refcounts."""
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    g = plot.line(_make_provider(1.0))
    qtbot.wait(40)

    stop = threading.Event()
    got = [0]
    errors = []

    def reader():
        try:
            while not stop.is_set():
                c = g.callable()  # incref/decref churn from a non-GIL-owner path
                del c
                got[0] += 1
        except Exception as e:  # pragma: no cover
            errors.append(repr(e))

    t = threading.Thread(target=reader, daemon=True)
    t.start()
    try:
        deadline = time.monotonic() + 2.0
        r = 0.0
        while time.monotonic() < deadline:
            r += 1.0
            g.set_range(SciQLopPlotRange(r, r + 50.0))
            qtbot.wait(5)
    finally:
        stop.set()
        t.join(timeout=10.0)

    assert not t.is_alive(), "reader thread stuck (deadlock)"
    assert not errors, f"reader raised: {errors}"
    assert got[0] > 0
