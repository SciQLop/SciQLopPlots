"""Regression test for the "Buffer must be a numeric type" worker-thread crash.

When a Python data-provider callback returns an object with a non-numeric
PEP-3118 buffer format (bool, datetime64, object, str, struct, float16…),
`PyBuffer`'s constructor throws `std::runtime_error`. The exception used to
escape `_GetDataPyCallable_impl::get_data`, propagate up through
`_threaded_update` on the data-provider worker thread, and call
`std::terminate` — taking down the whole SciQLop process.

Fix: `_collect_buffers` catches and drops the batch. Each parametrized dtype
below would have crashed pre-fix; post-fix the process stays alive and the
graph simply receives no data this round.
"""
import numpy as np
import pytest
from conftest import process_events
from SciQLopPlots import SciQLopPlotRange


def _spin(qtbot, calls, target, timeout=2000):
    qtbot.waitUntil(lambda: len(calls) >= target, timeout=timeout)


@pytest.mark.parametrize("bad_y", [
    pytest.param(lambda x: np.zeros(x.shape, dtype=np.bool_), id="bool"),
    pytest.param(lambda x: x.astype("datetime64[ns]"), id="datetime64"),
    pytest.param(lambda x: x.astype(np.float16), id="float16"),
    pytest.param(lambda x: np.array([str(v) for v in x]), id="str"),
    pytest.param(lambda x: np.array(list(x), dtype=object), id="object"),
])
def test_non_numeric_callback_does_not_crash(plot, qtbot, bad_y):
    """A callback returning a non-numeric buffer must not terminate the process.

    Without the fix this kills the interpreter on the data-provider worker
    thread the first time the rate-limit timer fires. With the fix the
    callback runs, `_collect_buffers` catches the throw, the graph gets no
    new data, and the test reaches the assertion alive.
    """
    calls = []

    def cb(start, stop):
        calls.append((start, stop))
        x = np.linspace(start, stop, 10, dtype=np.float64)
        return x, bad_y(x)

    g = plot.line(cb)
    g.set_range(SciQLopPlotRange(0.0, 1.0))
    _spin(qtbot, calls, 1)
    process_events()


def test_good_callback_still_works_after_bad_one(plot, qtbot):
    """Caching a recovery path: a graph that once returned bad data must
    accept good data on the next range change without leaving the worker in
    a broken state."""
    calls = []
    return_bad = [True]

    def cb(start, stop):
        calls.append((start, stop))
        x = np.linspace(start, stop, 10, dtype=np.float64)
        if return_bad[0]:
            return x, np.zeros(x.shape, dtype=np.bool_)
        return x, np.sin(x)

    g = plot.line(cb)
    g.set_range(SciQLopPlotRange(0.0, 1.0))
    _spin(qtbot, calls, 1)

    return_bad[0] = False
    g.set_range(SciQLopPlotRange(0.0, 2.0))
    _spin(qtbot, calls, 2)
    process_events()
