"""Reproducer and regression test for the DataProvider cache invalidation bug.

DataProviderInterface::_range_based_update short-circuits when the new range
equals the cached m_current_state (src/DataProducer.cpp:_range_based_update).
When a caller needs to re-fetch because some external state (e.g. knob values)
changed but the range didn't, the pipeline silently drops the request.

Fix: callers can call `graph.invalidate_cache()` to force the next same-range
fetch to bypass the dedup.
"""
import numpy as np
import pytest
from conftest import process_events


def _wait_calls(calls_list, target, qtbot, timeout=2000):
    """Spin the event loop until the callable counter hits `target` or timeout."""
    def done():
        assert len(calls_list) >= target
    qtbot.waitUntil(done, timeout=timeout)


class TestInvalidateCacheForcesRefetch:
    def test_same_range_without_invalidate_dedups(self, plot, qtbot):
        """Baseline: second call with identical range must be deduped (current behavior)."""
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, 10, dtype=np.float64)
            return x, np.sin(x)

        g = plot.line(cb)
        from SciQLopPlots import SciQLopPlotRange
        r = SciQLopPlotRange(0.0, 1.0)

        g.set_range(r)
        _wait_calls(calls, 1, qtbot)
        n_after_first = len(calls)

        # Same range, no invalidate — should NOT re-fetch.
        g.set_range(r)
        for _ in range(10):
            process_events()
        assert len(calls) == n_after_first, "dedup expected without invalidate"

    def test_invalidate_then_same_range_refetches(self, plot, qtbot):
        """Fix behavior: invalidate_cache() forces next same-range fetch."""
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, 10, dtype=np.float64)
            return x, np.sin(x)

        g = plot.line(cb)
        from SciQLopPlots import SciQLopPlotRange
        r = SciQLopPlotRange(0.0, 1.0)

        g.set_range(r)
        _wait_calls(calls, 1, qtbot)
        n_after_first = len(calls)

        g.invalidate_cache()
        g.set_range(r)
        _wait_calls(calls, n_after_first + 1, qtbot)
        assert len(calls) == n_after_first + 1

    def test_invalidate_is_one_shot(self, plot, qtbot):
        """A single invalidate_cache() forces one fetch, not all subsequent calls."""
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, 10, dtype=np.float64)
            return x, np.sin(x)

        g = plot.line(cb)
        from SciQLopPlots import SciQLopPlotRange
        r = SciQLopPlotRange(0.0, 1.0)

        g.set_range(r)
        _wait_calls(calls, 1, qtbot)

        g.invalidate_cache()
        g.set_range(r)
        _wait_calls(calls, 2, qtbot)

        g.set_range(r)
        for _ in range(10):
            process_events()
        assert len(calls) == 2, "second same-range call (no invalidate) must dedup again"


class TestInvalidateCacheOnNonFunctionGraphs:
    def test_noop_on_static_line(self, plot, sample_data, qtbot):
        """Static graph has no pipeline; invalidate_cache() must be a safe no-op."""
        x, y = sample_data
        g = plot.line(x, y)
        g.invalidate_cache()  # must not crash
        process_events()


class TestInvalidateCacheColorMap:
    def test_invalidate_refetches_colormap(self, plot, qtbot):
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, 20, dtype=np.float64)
            y = np.linspace(0, 1, 10, dtype=np.float64)
            z = np.random.rand(10, 20).astype(np.float64)
            return x, y, z

        g = plot.colormap(cb)
        from SciQLopPlots import SciQLopPlotRange
        r = SciQLopPlotRange(0.0, 1.0)

        g.set_range(r)
        _wait_calls(calls, 1, qtbot)
        n = len(calls)

        g.invalidate_cache()
        g.set_range(r)
        _wait_calls(calls, n + 1, qtbot)
        assert len(calls) == n + 1
