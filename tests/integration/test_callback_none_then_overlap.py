"""A callback that answers None (no data in that window) must not stop the next,
overlapping window's data from reaching the graph."""
import numpy as np


def _empty_then_data(calls):
    def cb(start, stop):
        calls.append((start, stop))
        if len(calls) == 1:
            return None
        x = np.linspace(start, stop, 32, dtype=np.float64)
        return x, np.sin(x)
    return cb


def test_data_after_a_none_answer_on_an_overlapping_range(plot, qtbot):
    calls = []
    g = plot.line(_empty_then_data(calls))
    plot.x_axis().set_range(0, 10)
    qtbot.waitUntil(lambda: len(calls) >= 1, timeout=2000)
    plot.x_axis().set_range(0, 11)
    qtbot.waitUntil(lambda: len(calls) >= 2, timeout=2000)
    qtbot.waitUntil(lambda: len(g.components()) > 0, timeout=2000)
    assert calls[-1] == (0, 11)
