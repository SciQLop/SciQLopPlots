#!/usr/bin/env python3
"""Reproduces the MMS spectrogram pan lag, and attributes each GUI freeze.

Two panel stacks, identical data and pan pattern, so they are a controlled A/B:

  --stack standalone   bare SciQLopMultiPlotPanel + speasy callables
  --stack sciqlop      SciQLop's plot_product path (the drag-and-drop equivalent)

Measured difference (2026-07-29): the speasy cache-deserialization GIL lock-out
(~300-590 ms) appears in BOTH, so it belongs to speasy/pysciqlop_cache. The
~700 ms CPU-bound freeze at load and a 5-10x higher baseline pan lag appear only
with --stack sciqlop.

TRAP: SciQLop's `PlotPanel.zoom_limit_seconds` defaults to ONE DAY and silently
clamps any wider time_range. Several rounds of measurement were invalidated by it
before this was found, so this script sets it to 0 and prints the effective range.

Requires speasy, so run it with SciQLop's venv python and PYTHONPATH pointing at
the SciQLopPlots build dir:

    cd /tmp && PYTHONPATH=<proj>/build-venv <sciqlop-venv>/bin/python \\
        <proj>/tests/perf/mms_pan_repro.py --stack standalone
"""
import argparse
import os
import sys
import threading
import time
from datetime import datetime, timedelta, timezone

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gui_stall_probe import GILProbe, Heartbeat, attribute  # noqa: E402

DIS = "cda/MMS1_FPI_FAST_L2_DIS-MOMS"
SPECTRO = f"{DIS}/mms1_dis_energyspectr_omni_fast"
DENSITY = f"{DIS}/mms1_dis_numberdensity_fast"
SPECTRO_LEAF = "mms1_dis_energyspectr_omni_fast"
DENSITY_LEAF = "mms1_dis_numberdensity_fast"


def _install_threadlocal_speasy_cache():
    """speasy's cache is thread-local; provider callbacks run on worker threads."""
    from speasy.core.cache import _cache

    class ThreadStorage:
        def __init__(self):
            self._storage = {}

        def __getattr__(self, item):
            return self._storage.get(threading.get_native_id(), {}).get(item, None)

        def __setattr__(self, key, value):
            if key == "_storage":
                object.__setattr__(self, key, value)
                return
            self._storage.setdefault(threading.get_native_id(), {})[key] = value

    _cache._data._local = ThreadStorage()


def pump(seconds):
    from PySide6.QtWidgets import QApplication

    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)


def build_standalone(start, stop):
    import speasy as spz
    from speasy.core import datetime64_to_epoch

    from SciQLopPlots import GraphType, PlotType, SciQLopMultiPlotPanel

    def spectro(s, e):
        v = spz.get_data(SPECTRO, s, e)
        if v is None:
            return None
        return (datetime64_to_epoch(v.time), v.axes[1].values.astype(np.float64),
                v.values.astype(np.float64))

    def density(s, e):
        v = spz.get_data(DENSITY, s, e)
        if v is None:
            return None
        return datetime64_to_epoch(v.time), v.values.astype(np.float64)

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.resize(1600, 1000)
    panel.set_time_axis_range(start, stop)
    for _ in range(2):
        panel.plot(spectro, name="dis_omni", graph_type=GraphType.ColorMap,
                   plot_type=PlotType.TimeSeries, y_log_scale=True, z_log_scale=True)
    panel.plot(density, labels=["N"], plot_type=PlotType.TimeSeries)
    panel.show()

    r = panel.plot_at(0).x_axis().range()
    print(f"  effective range: {datetime.fromtimestamp(r.start(), timezone.utc)} -> "
          f"{datetime.fromtimestamp(r.stop(), timezone.utc)}", flush=True)

    def set_range(lo, hi):
        panel.set_time_axis_range(datetime.fromtimestamp(lo, timezone.utc),
                                  datetime.fromtimestamp(hi, timezone.utc))

    return set_range


def _find_product_path(leaf_name, max_nodes=400_000):
    from PySide6.QtCore import QModelIndex, Qt

    from SciQLopPlots import ProductsModel

    model = ProductsModel.instance()
    queue, seen = [(QModelIndex(), [])], 0
    while queue:
        parent, path = queue.pop(0)
        if model.canFetchMore(parent):
            model.fetchMore(parent)
        for row in range(model.rowCount(parent)):
            idx = model.index(row, 0, parent)
            name = model.data(idx, Qt.DisplayRole)
            seen += 1
            if seen > max_nodes:
                return None
            if name == leaf_name:
                return "//".join(path + [name])
            queue.append((idx, path + [name]))
    return None


def build_sciqlop(start, stop):
    from SciQLop.sciqlop_app import start_sciqlop

    print("  starting SciQLop ...", flush=True)
    start_sciqlop()
    pump(5)

    spectro_path = _find_product_path(SPECTRO_LEAF)
    density_path = _find_product_path(DENSITY_LEAF)
    print(f"  spectro: {spectro_path}\n  density: {density_path}", flush=True)
    if not spectro_path:
        sys.exit("product not found in the tree — is the speasy provider loaded?")

    from SciQLop.user_api import TimeRange
    from SciQLop.user_api.plot import create_plot_panel

    panel = create_plot_panel()
    panel.zoom_limit_seconds = 0  # default is ONE DAY and silently clamps
    panel.time_range = TimeRange(start, stop)
    panel.plot_product(spectro_path)
    panel.plot_product(spectro_path)
    if density_path:
        panel.plot_product(density_path)
    print(f"  effective range: {panel.time_range}", flush=True)

    def set_range(lo, hi):
        panel.time_range = TimeRange(datetime.fromtimestamp(lo, timezone.utc),
                                     datetime.fromtimestamp(hi, timezone.utc))

    return set_range


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stack", choices=("standalone", "sciqlop"), default="standalone")
    ap.add_argument("--days", type=float, default=90.0)
    ap.add_argument("--stop", default="2025-06-01")
    ap.add_argument("--bursts", type=int, default=8)
    ap.add_argument("--steps", type=int, default=25)
    ap.add_argument("--break-s", type=float, default=6.0)
    ap.add_argument("--flip", type=int, default=3,
                    help="reverse pan direction every N steps; back-and-forth panning "
                         "is what makes requests pile up")
    ap.add_argument("--load-wait", type=float, default=2.0,
                    help="do NOT wait for the first fetch — pan into it")
    args = ap.parse_args()

    _install_threadlocal_speasy_cache()

    from PySide6.QtWidgets import QApplication

    app = QApplication(sys.argv)
    stop = datetime.fromisoformat(args.stop).replace(tzinfo=timezone.utc)
    start = stop - timedelta(days=args.days)

    print(f"\n=== {args.stack}: {args.days} days ending {args.stop} ===")
    set_range = (build_standalone if args.stack == "standalone" else build_sciqlop)(start, stop)

    hb, probe = Heartbeat(), GILProbe()
    probe.start()

    hb.start("initial load")
    pump(args.load_wait)
    hb.stop()
    hb.report("initial load")

    span = (stop - start).total_seconds()
    lo, hi, step = start.timestamp(), stop.timestamp(), span * 0.02
    print(f"  {args.bursts} bursts x {args.steps} pans of {step / 3600:.2f} h "
          f"(flip every {args.flip}), {args.break_s:.0f} s breaks", flush=True)

    for b in range(args.bursts):
        hb.start(f"pan burst {b + 1}")
        for k in range(args.steps):
            direction = 1 if (k // args.flip) % 2 == 0 else -1
            lo += step * direction
            hi += step * direction
            set_range(lo, hi)
            pump(hb.tick_ms / 1000)
        hb.stop()
        hb.report(f"pan burst {b + 1}")

        hb.start(f"break {b + 1}")
        pump(args.break_s)
        hb.stop()
        hb.report(f"  break {b + 1}")

    hb.start("settle")
    pump(10)
    hb.stop()
    hb.report("settle")

    probe.stop()
    attribute(hb, probe)


if __name__ == "__main__":
    main()
