"""Integration tests for the remote data provider (out-of-process channel).

A pure-Python responder stands in for the remote worker process: it receives
the channel's data_requested signal, computes, and calls set_data — exercising
the full request-out / data-in round trip in-process, no real IPC.
"""
import numpy as np
from multiprocessing import shared_memory
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange


def _remote_line_graph(plot):
    g = plot.add_remote_line_graph(["B"])
    QApplication.processEvents()
    return g


def _remote_curve(plot):
    g = plot.add_remote_curve(["c"])
    QApplication.processEvents()
    return g


def _remote_waterfall(plot):
    g = plot.add_remote_waterfall(["w"])
    QApplication.processEvents()
    return g


def _remote_color_map(plot):
    g = plot.add_remote_color_map("cm")
    QApplication.processEvents()
    return g


def _remote_histogram2d(plot):
    g = plot.add_remote_histogram2d("h")
    QApplication.processEvents()
    return g


def test_request_emitted_on_range_change(qtbot, plot):
    g = _remote_line_graph(plot)
    ch = g.remote_channel()
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


def test_set_data_round_trip_emits_new_data(qtbot, plot):
    g = _remote_line_graph(plot)
    ch = g.remote_channel()
    x = np.linspace(10.0, 20.0, 100).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    ch.data_requested.connect(lambda r: ch.set_data(x, y))
    got = []
    ch.new_data_2d.connect(lambda a, b: got.append((np.asarray(a), np.asarray(b))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    # the buffers must round-trip unchanged, not just trigger the signal
    out_x, out_y = got[-1]
    assert np.allclose(out_x, x)
    assert np.allclose(out_y, y)


def test_busy_clears_only_after_data_arrives(qtbot, plot):
    g = _remote_line_graph(plot)
    ch = g.remote_channel()
    pending = {}
    ch.data_requested.connect(lambda r: pending.setdefault("r", r))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: "r" in pending, timeout=2000)
    assert g.busy() is True  # request out, no data yet
    x = np.linspace(10.0, 20.0, 10).astype(np.float64)
    ch.set_data(x, x)
    qtbot.waitUntil(lambda: g.busy() is False, timeout=2000)


def test_zero_copy_shared_memory_buffer(qtbot, plot):
    """A shared-memory-backed numpy array must traverse the channel intact.

    The library legitimately holds a reference to the buffer until the channel's
    data is superseded, so the segment is only unlinked after we evict it with a
    replacement and let the worker settle — freeing it earlier would be a
    use-after-free, exactly the lifetime contract a real remote producer must
    honour.
    """
    g = _remote_line_graph(plot)
    ch = g.remote_channel()
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


# ---------------------------------------------------------------------------
# SciQLopCurveRemote
# ---------------------------------------------------------------------------

def test_curve_request_emitted_on_range_change(qtbot, plot):
    g = _remote_curve(plot)
    ch = g.remote_channel()
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


def test_curve_set_data_round_trip(qtbot, plot):
    g = _remote_curve(plot)
    ch = g.remote_channel()
    x = np.linspace(10.0, 20.0, 100).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    ch.data_requested.connect(lambda r: ch.set_data(x, y))
    got = []
    ch.new_data_2d.connect(lambda a, b: got.append((np.asarray(a), np.asarray(b))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    out_x, out_y = got[-1]
    assert np.allclose(out_x, x)
    assert np.allclose(out_y, y)


# ---------------------------------------------------------------------------
# SciQLopWaterfallGraphRemote
# ---------------------------------------------------------------------------

def test_waterfall_request_emitted_on_range_change(qtbot, plot):
    g = _remote_waterfall(plot)
    ch = g.remote_channel()
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


def test_waterfall_set_data_round_trip(qtbot, plot):
    g = _remote_waterfall(plot)
    ch = g.remote_channel()
    x = np.linspace(10.0, 20.0, 10).astype(np.float64)
    y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
    ch.data_requested.connect(lambda r: ch.set_data(x, y))
    got = []
    ch.new_data_2d.connect(lambda a, b: got.append((np.asarray(a), np.asarray(b))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    out_x, out_y = got[-1]
    assert np.allclose(out_x, x)
    assert np.allclose(out_y, y)


# ---------------------------------------------------------------------------
# SciQLopColorMapRemote
# ---------------------------------------------------------------------------

def test_colormap_request_emitted_on_range_change(qtbot, plot):
    g = _remote_color_map(plot)
    ch = g.remote_channel()
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


def test_colormap_set_data_round_trip(qtbot, plot):
    g = _remote_color_map(plot)
    ch = g.remote_channel()
    x = np.linspace(10.0, 20.0, 20).astype(np.float64)
    y = np.linspace(0.0, 5.0, 10).astype(np.float64)
    z = np.random.rand(len(y), len(x)).astype(np.float64)
    ch.data_requested.connect(lambda r: ch.set_data(x, y, z))
    got = []
    ch.new_data_3d.connect(lambda a, b, c: got.append((np.asarray(a), np.asarray(b), np.asarray(c))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    out_x, out_y, out_z = got[-1]
    assert np.allclose(out_x, x)
    assert np.allclose(out_y, y)
    assert np.allclose(out_z, z)


# ---------------------------------------------------------------------------
# SciQLopHistogram2DRemote
# ---------------------------------------------------------------------------

def test_histogram2d_request_emitted_on_range_change(qtbot, plot):
    g = _remote_histogram2d(plot)
    ch = g.remote_channel()
    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(requests) > 0, timeout=2000)
    assert requests[-1] == (10.0, 20.0)


def test_histogram2d_set_data_round_trip(qtbot, plot):
    g = _remote_histogram2d(plot)
    ch = g.remote_channel()
    x = np.linspace(10.0, 20.0, 200).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    ch.data_requested.connect(lambda r: ch.set_data(x, y))
    got = []
    ch.new_data_2d.connect(lambda a, b: got.append((np.asarray(a), np.asarray(b))))
    plot.x_axis().set_range(SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: len(got) > 0, timeout=2000)
    out_x, out_y = got[-1]
    assert np.allclose(out_x, x)
    assert np.allclose(out_y, y)
