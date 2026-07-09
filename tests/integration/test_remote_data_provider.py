"""Integration tests for the remote data provider (out-of-process channel).

A pure-Python responder stands in for the remote worker process: it receives
the channel's data_requested signal, computes, and calls set_data — exercising
the full request-out / data-in round trip in-process, no real IPC.
"""
import numpy as np
import pytest
from multiprocessing import shared_memory
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


def _xy(n=100):
    x = np.linspace(10.0, 20.0, n).astype(np.float64)
    return (x, np.sin(x).astype(np.float64)), "new_data_2d"


def _xy_multi(n=10):
    x = np.linspace(10.0, 20.0, n).astype(np.float64)
    y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
    return (x, y), "new_data_2d"


def _xyz():
    x = np.linspace(10.0, 20.0, 20).astype(np.float64)
    y = np.linspace(0.0, 5.0, 10).astype(np.float64)
    z = np.random.rand(len(y), len(x)).astype(np.float64)
    return (x, y, z), "new_data_3d"


# (factory(plot) -> remote graph, make_data() -> (set_data args, new_data signal))
CASES = [
    pytest.param(lambda p: p.add_remote_line_graph(["B"]), _xy, id="line"),
    pytest.param(lambda p: p.add_remote_curve(["c"]), _xy, id="curve"),
    pytest.param(lambda p: p.add_remote_waterfall(["w"]), _xy_multi, id="waterfall"),
    pytest.param(lambda p: p.add_remote_color_map("cm"), _xyz, id="colormap"),
    pytest.param(lambda p: p.add_remote_histogram2d("h"), _xy, id="histogram2d"),
]


def _channel(plot, factory):
    g = factory(plot)
    QApplication.processEvents()
    return g, g.remote_channel()


@pytest.mark.parametrize("factory,make_data", CASES)
def test_request_emitted_on_range_change(qtbot, plot, factory, make_data):
    _, ch = _channel(plot, factory)
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


@pytest.mark.parametrize("factory,make_data", CASES)
def test_set_data_round_trips_to_new_data(qtbot, plot, factory, make_data):
    _, ch = _channel(plot, factory)
    args, signal = make_data()
    ch.data_requested.connect(lambda r: ch.set_data(*args))
    got = []
    getattr(ch, signal).connect(lambda *bufs: got.append([np.asarray(b) for b in bufs]))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    # the buffers must round-trip unchanged, not just trigger the signal
    for out, expected in zip(got[-1], args):
        assert np.allclose(out, expected)


def test_busy_clears_only_after_data_arrives(qtbot, plot):
    g, ch = _channel(plot, lambda p: p.add_remote_line_graph(["B"]))
    pending = {}
    ch.data_requested.connect(lambda r: pending.setdefault("r", r))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: "r" in pending, timeout=2000)
    assert g.busy() is True  # request out, no data yet
    x = np.linspace(10.0, 20.0, 10).astype(np.float64)
    ch.set_data(x, x)
    qtbot.waitUntil(lambda: g.busy() is False, timeout=2000)


@pytest.mark.parametrize("factory,make_data", CASES)
def test_set_busy_emits_exactly_once_per_call(qtbot, plot, factory, make_data):
    """Each *Remote::set_busy() override forwards to its base class's
    set_busy() (whose busyChanged is already wired to busy_changed once the
    underlying QCP plottable/components exist) and used to *also* emit
    busy_changed explicitly, unconditionally -- double-firing for a single
    call. Call set_busy() directly (bypassing the remote pipeline, whose own
    request/response round trip and rescale machinery can legitimately call
    set_busy more than once per user-visible transition, for reasons
    unrelated to this) to isolate exactly this bug."""
    g, _ch = _channel(plot, factory)
    for target in (True, False, True, False):
        events = []
        callback = lambda v: events.append(v)
        g.busy_changed.connect(callback)
        g.set_busy(target)
        g.busy_changed.disconnect(callback)
        assert events == [target]


def test_zero_copy_shared_memory_buffer(qtbot, plot):
    """A shared-memory-backed numpy array must traverse the channel intact.

    The library legitimately holds a reference to the buffer until the channel's
    data is superseded, so the segment is only unlinked after we evict it with a
    replacement and let the worker settle — freeing it earlier would be a
    use-after-free, exactly the lifetime contract a real remote producer must
    honour.
    """
    _, ch = _channel(plot, lambda p: p.add_remote_line_graph(["B"]))
    shm = shared_memory.SharedMemory(create=True, size=100 * 8)
    try:
        x = np.ndarray((100,), dtype=np.float64, buffer=shm.buf)
        x[:] = np.linspace(10.0, 20.0, 100)
        got = []
        # copy out in the slot: `got` must stay valid after the segment is freed
        ch.new_data_2d.connect(lambda a, b: got.append(np.asarray(a).copy()))
        ch.data_requested.connect(lambda r: ch.set_data(x, x))
        plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
        qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
        # the shared-memory bytes arrived intact through the channel
        assert np.allclose(got[-1], x)
        # release the channel's reference to the shm buffer before unlinking
        ch.set_data(np.zeros(2), np.zeros(2))
        qtbot.wait(100)
    finally:
        shm.close()
        shm.unlink()
