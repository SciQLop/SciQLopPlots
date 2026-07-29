#!/usr/bin/env python3
"""Pan-vs-zoom replot cost — measures the GPU layer-translation fast path.

A pure horizontal pan is eligible for the translate shortcut
(`stallPixelOffset` -> `QCPLayer::canTranslateInsteadOfRepaint` -> composite-shader
translate + scissor). A zoom never is. So pan should be markedly cheaper than zoom,
and pan cost should stay roughly flat as plots are stacked, while zoom scales with
plot count.

Usage: pan_translate_bench.py [line|colormap] [n_plots] [n_steps]

Run it from outside the source tree with PYTHONPATH pointing at the build dir, e.g.
    cd /tmp && PYTHONPATH=<proj>/build-venv python <proj>/tests/perf/pan_translate_bench.py line 4
"""
import sys
import time

import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlotRange

# Let each async resample land before timing the replot. With SETTLE=False the
# timed replot happens mid-pipeline and measures the translate path in isolation.
SETTLE = True
N_POINTS = 2_000_000
NX, NY = 200_000, 512


def make_panel(kind, n_plots):
    panel = SciQLopMultiPlotPanel(synchronize_x=True)
    panel.resize(1600, 900)
    if kind == "line":
        x = np.linspace(0, 100, N_POINTS, dtype=np.float64)
        y = np.column_stack(
            [np.sin(x * 6.28 * (1 + c)) + 0.1 * np.sin(x * 628.0) for c in range(4)])
        for _ in range(n_plots):
            panel.plot(x, y)
    else:
        x = np.linspace(0, 100, NX, dtype=np.float64)
        y = np.logspace(0, 4, NY, dtype=np.float64)
        xx, yy = np.meshgrid(x, y, indexing="ij")
        z = np.abs(np.sin(xx * 0.1) * np.cos(np.log10(yy) * 2.0)) + 1e-3
        for _ in range(n_plots):
            panel.plot(x, y, z, y_log_scale=True, z_log_scale=True)
    return panel


def settle(seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.01)


def run(panel, n_plots, steps, mode):
    r = panel.plot_at(0).x_axis().range()
    span = r.size()
    lo, hi = r.start(), r.stop()
    out = []
    for _ in range(steps):
        if mode == "pan":
            shift = span * 0.01
            lo, hi = lo + shift, hi + shift
        else:
            c, half = (lo + hi) / 2, (hi - lo) / 2 * 0.99
            lo, hi = c - half, c + half
        panel.set_x_axis_range(SciQLopPlotRange(lo, hi))
        if SETTLE:
            settle(0.05)
        t0 = time.perf_counter_ns()
        for p in range(n_plots):
            panel.plot_at(p).replot(True)
        out.append((time.perf_counter_ns() - t0) / 1e6)
    return np.array(out)


def report(label, d):
    print(f"  {label:6s} mean {d.mean():7.2f} ms | median {np.median(d):7.2f} | "
          f"p90 {np.percentile(d, 90):7.2f} | max {d.max():7.2f}")


def main():
    kind = sys.argv[1] if len(sys.argv) > 1 else "line"
    n_plots = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    steps = int(sys.argv[3]) if len(sys.argv) > 3 else 40

    app = QApplication(sys.argv)
    panel = make_panel(kind, n_plots)
    panel.show()
    settle(8)

    print(f"\n=== {kind}, {n_plots} plot(s), {steps} steps, replot cost per step ===")
    report("pan", run(panel, n_plots, steps, "pan"))
    settle(2)
    report("zoom", run(panel, n_plots, steps, "zoom"))
    panel.close()


if __name__ == "__main__":
    main()
