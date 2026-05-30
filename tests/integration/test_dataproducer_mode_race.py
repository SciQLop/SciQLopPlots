"""Reproducer for D-003: DataProviderInterface drops a queued request when a
range-based and a data-based update race.

`SimplePyCallablePipeline::call(range)` arms a 20ms rate-limit timer WITHOUT
emitting `_state_changed`; `call(x, y)` emits `_state_changed` only when
`m_has_pending_change` was false. So a `call(range)` immediately followed by a
`call(x, y)` overwrites the shared `m_next_state` and suppresses the second
wake-up — the worker eventually runs once and services only the *last* variant.
The other fetch is silently dropped.

Both fetch paths are reachable on a single provider in real use: every
SciQLopFunctionGraph wires `range_changed -> call(range)` in its constructor,
and `observe(graph)` adds `data_changed -> call(x, y)`. Here we drive the two
overloads of `call` directly, which is the same code path.
"""
import numpy as np
import pytest
from conftest import process_events
from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


def _make_graph(qtbot, events):
    p = SciQLopPlot()
    qtbot.addWidget(p)

    def cb(*a):
        if len(a) == 2 and np.isscalar(a[0]):       # range path: (lower, upper)
            events.append(("range", round(float(a[0]), 1)))
            x = np.linspace(a[0], a[1], 8, dtype=np.float64)
            return x, np.sin(x)
        events.append(("data", len(a)))             # data path: (x, y) buffers
        x = np.asarray(a[0], dtype=np.float64)
        return x, np.asarray(a[1], dtype=np.float64)

    g = p.line(cb)
    qtbot.wait(60)
    events.clear()
    return p, g


def _settle(qtbot, ms=120):
    # The rate-limit timer is 20ms; give it room plus event-loop turns.
    deadline = ms
    while deadline > 0:
        process_events()
        qtbot.wait(10)
        deadline -= 10


def test_range_then_data_keeps_both(qtbot):
    """A range fetch followed immediately by a data fetch must service BOTH."""
    events = []
    p, g = _make_graph(qtbot, events)

    xb = np.linspace(0.0, 1.0, 8, dtype=np.float64)
    yb = np.cos(xb)

    g.call(SciQLopPlotRange(100.0, 200.0))   # arms 20ms timer, pending=True
    g.call(xb, yb)                           # overwrites m_next_state, no wake
    _settle(qtbot)

    modes = [e[0] for e in events]
    assert "data" in modes, f"data fetch missing: {events}"
    assert "range" in modes, f"range fetch was DROPPED: {events}"


def test_fuzz_no_request_is_dropped(qtbot):
    """Fuzz: over many randomized range/data bursts, every distinct request the
    caller makes must produce at least one callback invocation of that mode."""
    import random
    rng = random.Random(1234)

    events = []
    p, g = _make_graph(qtbot, events)
    xb = np.linspace(0.0, 1.0, 8, dtype=np.float64)
    yb = np.cos(xb)

    drops = 0
    trials = 60
    for i in range(trials):
        events.clear()
        ops = [("range", 1000.0 + i), ("data", None)]
        rng.shuffle(ops)
        for kind, val in ops:
            if kind == "range":
                g.call(SciQLopPlotRange(val, val + 50.0))
            else:
                g.call(xb, yb)
        _settle(qtbot, ms=120)
        modes = {e[0] for e in events}
        if not {"range", "data"}.issubset(modes):
            drops += 1

    assert drops == 0, f"{drops}/{trials} mixed bursts dropped a request"
