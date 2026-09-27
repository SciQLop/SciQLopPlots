"""Range fetches with a prefetch margin (SciQLop issue #143, item 2).

Without a margin every range change refetches exactly the view. With one, the
provider is asked for the view widened by `margin * span` on each side, and a
later view that fits inside what was loaded is not fetched again. Opt-in: a
provider whose answer depends on the requested span (a fixed number of points)
would show coarse data after a zoom-in if its fetches were skipped.
"""
import numpy as np
import pytest
from conftest import process_events
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlotRange


def _settle():
    for _ in range(20):
        process_events()


def _after_initial_fetch(g, calls, qtbot):
    # A new line graph fetches the plot's current range on its own (past the
    # 20 ms rate limiter); let that land so it is not mistaken for the fetch a
    # test triggers.
    qtbot.wait(200)
    return g, calls


def _line_with_calls(plot, qtbot):
    calls = []

    def cb(start, stop):
        calls.append((start, stop))
        x = np.linspace(start, stop, 10, dtype=np.float64)
        return x, np.sin(x)

    return _after_initial_fetch(plot.line(cb), calls, qtbot)


def _fetch(g, calls, qtbot, start, stop):
    n = len(calls)
    g.set_range(SciQLopPlotRange(start, stop))
    qtbot.waitUntil(lambda: len(calls) > n, timeout=2000)
    return calls[-1]


def _no_fetch(g, calls, start, stop):
    n = len(calls)
    g.set_range(SciQLopPlotRange(start, stop))
    _settle()
    return len(calls) == n


class TestDefaultKeepsExactFetches:
    def test_margin_defaults_to_zero(self, plot, qtbot):
        g, _ = _line_with_calls(plot, qtbot)
        assert g.prefetch_margin() == 0.0

    def test_sub_range_refetches_without_margin(self, plot, qtbot):
        g, calls = _line_with_calls(plot, qtbot)
        assert _fetch(g, calls, qtbot, 10.0, 20.0) == (10.0, 20.0)
        assert _fetch(g, calls, qtbot, 12.0, 18.0) == (12.0, 18.0)


class TestFunctionGraphMargin:
    def test_fetch_is_widened_by_the_margin(self, plot, qtbot):
        g, calls = _line_with_calls(plot, qtbot)
        g.set_prefetch_margin(0.5)
        assert g.prefetch_margin() == 0.5
        assert _fetch(g, calls, qtbot, 10.0, 20.0) == (5.0, 25.0)

    def test_view_inside_loaded_range_is_not_refetched(self, plot, qtbot):
        g, calls = _line_with_calls(plot, qtbot)
        g.set_prefetch_margin(0.5)
        _fetch(g, calls, qtbot, 10.0, 20.0)
        assert _no_fetch(g, calls, 12.0, 22.0), "pan inside the margin must reuse the data"
        assert _no_fetch(g, calls, 14.0, 16.0), "zoom-in inside the loaded range too"

    def test_view_leaving_loaded_range_refetches(self, plot, qtbot):
        g, calls = _line_with_calls(plot, qtbot)
        g.set_prefetch_margin(0.5)
        _fetch(g, calls, qtbot, 10.0, 20.0)
        assert _fetch(g, calls, qtbot, 20.0, 30.0) == (15.0, 35.0)

    def test_invalidate_cache_still_forces_a_fetch(self, plot, qtbot):
        g, calls = _line_with_calls(plot, qtbot)
        g.set_prefetch_margin(0.5)
        _fetch(g, calls, qtbot, 10.0, 20.0)
        g.invalidate_cache()
        assert _fetch(g, calls, qtbot, 12.0, 18.0) == (9.0, 21.0)

    def test_colormap_function_takes_the_margin(self, plot, qtbot):
        calls = []

        def cb(start, stop):
            calls.append((start, stop))
            x = np.linspace(start, stop, 20, dtype=np.float64)
            y = np.linspace(0, 1, 10, dtype=np.float64)
            return x, y, np.random.rand(20, 10)

        g, _ = _after_initial_fetch(plot.colormap(cb), calls, qtbot)
        g.set_prefetch_margin(0.25)
        assert _fetch(g, calls, qtbot, 10.0, 14.0) == (9.0, 15.0)


class TestRemoteGraphMargin:
    def test_request_is_widened_and_contained_view_not_requested(self, plot, qtbot):
        g = plot.add_remote_line_graph(["B"])
        QApplication.processEvents()
        ch = g.remote_channel()
        requests = []
        ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
        g.set_prefetch_margin(0.5)
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
        assert requests[-1] == (5.0, 25.0)
        n = len(requests)
        plot.x_axis().set_range(SciQLopPlotRange(11.0, 21.0))
        _settle()
        assert len(requests) == n
