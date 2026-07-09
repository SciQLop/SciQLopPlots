"""Regression test for I-01: the very first colormap ``set_data()`` batch
triggered TWO full O(nz) z-range scans instead of one.

Root cause: ``SciQLopGraphInterface::check_first_data`` emits
``request_rescale()`` exactly once, on the true first batch. Separately,
``SciQLopPlot::_configure_color_map`` wires *both* ``request_rescale`` and
``data_changed`` to ``rescale_axes()`` via independent queued connections.
On the first batch both signals fire, so ``rescale_axes()`` (and therefore
the z-axis's expensive full scan) ran twice for what is logically one
event. Every subsequent batch only fires ``data_changed`` — that per-batch
scan is real/necessary and out of scope here.

Real-timing assertions are flaky, so this counts actual invocations of the
z-axis rescale path via the built-in tracing facility (an
``SciQLopPlots.tracing::ScopedZone`` was added at the top of
``SciQLopPlotColorScaleAxis::rescale()`` for exactly this purpose): every
call to ``rescale_axes()`` on a colormap plot runs the z-axis's ``rescale()``
once (z_axis() is always included in ``m_axes_to_rescale``), so counting
"colorscale.rescale" trace events on the first batch directly measures how
many times the redundant full scan fired.
"""
import json

import numpy as np
from PySide6.QtCore import QCoreApplication

from SciQLopPlots import tracing


def _drain_queued_connections(n=20):
    # Deliberately NOT qtbot.wait()/time.sleep(): the auto-scale rescale this
    # test targets also rescales the plot's x-axis, whose range_changed then
    # re-triggers this callable-backed colormap's own async fetch pipeline
    # (a real, separate, legitimate re-fetch — out of scope here). That
    # fetch runs on a worker thread and only lands back on this thread after
    # real wall-clock time elapses, so giving it any risks folding an
    # unrelated extra rescale into the count and flaking the assertion.
    # A tight processEvents() loop with no sleeping drains the two
    # same-turn queued connections from the *first* batch (the ones this
    # test is about) without yielding enough real time for that unrelated
    # worker-thread round-trip to complete.
    for _ in range(n):
        QCoreApplication.processEvents()


def _rescale_event_count(trace_path):
    with open(trace_path) as f:
        data = json.load(f)
    return sum(1 for e in data["traceEvents"] if e.get("name") == "colorscale.rescale")


def test_first_batch_triggers_exactly_one_colorscale_rescale(plot, qtbot, tmp_path):
    plot.set_auto_scale(True)

    x = np.linspace(0, 10, 50).astype(np.float64)
    y = np.linspace(0, 5, 30).astype(np.float64)
    z = np.random.default_rng(0).random((30, 50)).astype(np.float64)

    # A callable-backed colormap wires _configure_color_map's request_rescale /
    # data_changed connections at creation time, before any data exists (the
    # static plot.colormap(x, y, z) overload does it the other way around —
    # set_data() first, connections after — so it never hits this race). This
    # mirrors the real-world shape of the bug: a live data provider (e.g. a
    # Speasy-backed callback) where the colormap is wired up before its first
    # batch arrives.
    cmap = plot.colormap(lambda start, stop: (x, y, z))

    trace_path = str(tmp_path / "trace.json")
    assert tracing.enable(trace_path) is True
    try:
        cmap.set_data(x, y, z)
        _drain_queued_connections()
    finally:
        tracing.flush()
        tracing.disable()

    count = _rescale_event_count(trace_path)
    assert count == 1, (
        f"expected exactly 1 colorscale rescale on the first batch, got {count} "
        "(request_rescale and data_changed both triggering rescale_axes() on "
        "the same first batch)"
    )
