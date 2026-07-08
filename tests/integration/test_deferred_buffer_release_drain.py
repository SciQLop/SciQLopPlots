"""Regression guard for D-18 (colormap-pipeline review, 2026-07-06):
the deferred Python-object-release queue in PythonInterface.cpp only drained
when some thread later happened to call back into one of a handful of
PythonInterface entry points (_inc_ref, _dec_ref's fast path, init_buffer,
get_data). If the app went idle after the last such call, anything enqueued
stayed pinned indefinitely.

``SimplePyCallablePWrapper`` (backing function-graph/callback plotting)
documents the exact mechanism (see its class comment in DataProducer.hpp):
the worker thread takes a ``shared_ptr<GetDataPyCallable>`` snapshot before
calling the Python callable, so a concurrent ``set_callable()`` swap is safe.
``GetDataPyCallable::get_data`` acquires the GIL for the duration of the
Python call via a scope-local ``PyAutoScopedGIL`` and releases it before
returning — so if the worker thread's snapshot is the *last* reference by
the time it goes out of scope back in ``SimplePyCallablePWrapper::get_data``,
that reference drop happens off the GIL, deterministically, whenever the
callable is swapped out from the main thread while an old fetch is still in
flight (a real callback that does I/O or otherwise blocks/sleeps takes long
enough to make this reliable — no thread-pool timing luck required, unlike
the colormap resample pipeline).
"""
import sys
import time

import numpy as np
import pytest

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


def _slow_callable(start, stop):
    time.sleep(0.2)  # releases the GIL: gives the main thread a window to
                      # swap the callable while this fetch is still in flight
    x = np.linspace(start, stop, 8, dtype=np.float64)
    return x, np.sin(x)


def _fast_callable(start, stop):
    x = np.linspace(start, stop, 8, dtype=np.float64)
    return x, np.cos(x)


@pytest.mark.timeout(30)
def test_worker_thread_callable_release_drains_without_further_api_calls(qtbot):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)

    baseline_refcount = sys.getrefcount(_slow_callable)

    g = plot.line(_slow_callable)
    g.set_range(SciQLopPlotRange(0.0, 100.0))  # worker snapshots _slow_callable

    qtbot.wait(20)  # let the worker thread enter the callable's sleep

    g.set_callable(_fast_callable)  # drops the main thread's own reference;
                                     # the worker's in-flight snapshot is now
                                     # the sole owner of _slow_callable's wrapper

    elevated_refcount = sys.getrefcount(_slow_callable)
    assert elevated_refcount > baseline_refcount, (
        "sanity check failed: the worker's callable snapshot should still "
        "be alive here, or this test isn't exercising the in-flight race"
    )

    # Let the in-flight fetch finish — pure Qt event pumping, no call into
    # any PythonInterface entry point — so the worker thread drops its
    # snapshot off the GIL, enqueuing the release.
    qtbot.wait(500)

    # No further SciQLopPlots calls from here on: only an opportunistic
    # Py_AddPendingCall-driven drain (installed at enqueue time) can free
    # anything now.
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline and sys.getrefcount(_slow_callable) > baseline_refcount:
        pass

    assert sys.getrefcount(_slow_callable) == baseline_refcount, (
        "deferred callable release never drained without a further "
        "PythonInterface API call (queue only drains on next traffic, "
        "not opportunistically)"
    )
