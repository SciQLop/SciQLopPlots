#!/usr/bin/env python3
"""
Headless perf scenarios for SciQLopPlots colormap hot paths.

Tests colormaps with Y/Z log scales, data gaps, NaN values,
and dynamic Y-scale changes through the full Python -> C++ path.

Usage: colormap-perf.py <scenario> [iterations] [--no-barrier]

Scenarios:
  synced_pan       — synchronized X-axis pan across stacked colormap plots
  y_rescale        — change Y-axis range (triggers colormap rebuild)
  full_replot      — full replot of multi-colormap panel
  panel_setup      — create panel + colormaps + populate with data
  callable_pan     — pan with callable pipeline (data fetch + resample + render)
"""

import os
import signal
import sys
import time

import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt

from SciQLopPlots import (
    SciQLopMultiPlotPanel,
    SciQLopPlotRange,
    GraphType,
    PlotType,
)

# ── Barrier ────────────────────────────────────────────────────

USE_BARRIER = True


def wait_for_profiler():
    if not USE_BARRIER:
        return
    sys.stderr.write("  [ready — waiting for SIGCONT]\n")
    sys.stderr.flush()
    os.kill(os.getpid(), signal.SIGSTOP)


# ── Data generation ────────────────────────────────────────────

DEFAULT_NX = 500_000
DEFAULT_NY = 1024
DEFAULT_PLOTS = 4
NAN_FRACTION = 0.02
GAP_FRACTION = 0.05


def make_colormap_data(nx=DEFAULT_NX, ny=DEFAULT_NY):
    """Generate colormap data with log-spaced Y, data gaps, and NaNs."""
    x = np.linspace(0, 100, nx, dtype=np.float64)
    y = np.logspace(0, 4, ny, dtype=np.float64)

    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.sin(xx * 0.1) * np.cos(np.log10(yy) * 2.0) + 0.5
    z = np.abs(z) + 1e-3  # keep positive for log Z scale

    rng = np.random.default_rng(42)

    # inject NaNs
    nan_count = int(nx * ny * NAN_FRACTION)
    nan_rows = rng.integers(0, nx, nan_count)
    nan_cols = rng.integers(0, ny, nan_count)
    z[nan_rows, nan_cols] = np.nan

    # inject data gaps (full rows of NaN)
    gap_count = int(nx * GAP_FRACTION)
    gap_rows = rng.choice(nx, gap_count, replace=False)
    z[gap_rows, :] = np.nan

    return x, y, z


def make_callable_colormap(nx=DEFAULT_NX, ny=DEFAULT_NY):
    """Return a callable (start, stop) -> (x, y, z) with gaps and NaNs."""
    y_axis = np.logspace(0, 4, ny, dtype=np.float64)

    def provider(start, stop):
        if stop <= start:
            return None
        n = max(int((stop - start) * (nx / 100.0)), 2)
        x = np.linspace(start, stop, n, dtype=np.float64)
        yy = np.log10(y_axis)
        xx_norm = (x - start) / max(stop - start, 1e-9)
        z = np.abs(
            np.sin(xx_norm[:, None] * 6.28 * 3) * np.cos(yy[None, :] * 2.0)
        ) + 1e-3

        rng = np.random.default_rng(int(start) & 0xFFFF)

        nan_count = int(n * ny * NAN_FRACTION)
        if nan_count > 0:
            z[rng.integers(0, n, nan_count), rng.integers(0, ny, nan_count)] = np.nan

        gap_count = max(int(n * GAP_FRACTION), 1)
        gap_rows = rng.choice(n, min(gap_count, n), replace=False)
        z[gap_rows, :] = np.nan

        return x, y_axis.copy(), z

    provider.__name__ = "colormap_with_gaps"
    return provider


# ── Panel setup ────────────────────────────────────────────────


def setup_static_panel(n_plots=DEFAULT_PLOTS, nx=DEFAULT_NX, ny=DEFAULT_NY):
    """Create panel with static colormap data, Y/Z log scales."""
    panel = SciQLopMultiPlotPanel(synchronize_x=True)
    panel.resize(1920, 1080)

    for _ in range(n_plots):
        x, y, z = make_colormap_data(nx, ny)
        _plot, graph = panel.plot(x, y, z, y_log_scale=True, z_log_scale=True)

    panel.show()
    QApplication.processEvents()

    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.05)

    for i in range(n_plots):
        panel.plot_at(i).replot(True)
    QApplication.processEvents()

    return panel


def setup_callable_panel(n_plots=DEFAULT_PLOTS, ny=DEFAULT_NY):
    """Create panel with callable colormap pipeline."""
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.resize(1920, 1080)

    for _ in range(n_plots):
        panel.plot(
            make_callable_colormap(ny=ny),
            graph_type=GraphType.ColorMap,
            plot_type=PlotType.TimeSeries,
            y_log_scale=True,
            z_log_scale=True,
        )

    panel.show()
    QApplication.processEvents()

    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.05)

    return panel


# ── Timer helper ───────────────────────────────────────────────

class PerfTimer:
    def __init__(self):
        self._start = None

    def start(self):
        self._start = time.perf_counter_ns()

    def elapsed_ms(self):
        return (time.perf_counter_ns() - self._start) / 1e6


# ── Scenarios ──────────────────────────────────────────────────

def scenario_synced_pan(iters):
    panel = setup_static_panel()
    r = panel.plot_at(0).x_axis().range()
    pan_step = r.size() * 0.005

    sys.stderr.write(
        f"synced_pan: {DEFAULT_PLOTS} colormaps × {DEFAULT_NX}x{DEFAULT_NY}, "
        f"log Y+Z, gaps+NaN, step={pan_step:.4f}, {iters} iters\n"
    )
    wait_for_profiler()

    timer = PerfTimer()
    timer.start()
    current = r
    for _ in range(iters):
        current = SciQLopPlotRange(current.start() + pan_step, current.stop() + pan_step)
        panel.set_x_axis_range(current)
        for i in range(DEFAULT_PLOTS):
            panel.plot_at(i).replot(True)
    ms = timer.elapsed_ms()
    sys.stderr.write(f"  total: {ms:.1f} ms, per-iter: {ms / iters:.1f} ms\n")
    panel.close()
    panel.deleteLater()


def scenario_y_rescale(iters):
    panel = setup_static_panel()

    sys.stderr.write(
        f"y_rescale: {DEFAULT_PLOTS} colormaps × {DEFAULT_NX}x{DEFAULT_NY}, "
        f"log Y+Z, gaps+NaN, {iters} iters\n"
    )
    wait_for_profiler()

    timer = PerfTimer()
    timer.start()
    for i in range(iters):
        # alternate between zooming in on low energies and full range
        if i % 2 == 0:
            yr = SciQLopPlotRange(1.0, 100.0)     # zoom into low range
        else:
            yr = SciQLopPlotRange(1.0, 10000.0)   # full range
        for p in range(DEFAULT_PLOTS):
            plot = panel.plot_at(p)
            plot.y_axis().set_range(yr)
            plot.replot(True)
    ms = timer.elapsed_ms()
    sys.stderr.write(f"  total: {ms:.1f} ms, per-iter: {ms / iters:.1f} ms\n")
    panel.close()
    panel.deleteLater()


def scenario_full_replot(iters):
    panel = setup_static_panel()

    sys.stderr.write(
        f"full_replot: {DEFAULT_PLOTS} colormaps × {DEFAULT_NX}x{DEFAULT_NY}, "
        f"log Y+Z, gaps+NaN, {iters} iters\n"
    )
    wait_for_profiler()

    timer = PerfTimer()
    timer.start()
    for _ in range(iters):
        for p in range(DEFAULT_PLOTS):
            panel.plot_at(p).replot(True)
    ms = timer.elapsed_ms()
    sys.stderr.write(f"  total: {ms:.1f} ms, per-iter: {ms / iters:.1f} ms\n")
    panel.close()
    panel.deleteLater()


def scenario_panel_setup(iters):
    sys.stderr.write(
        f"panel_setup: {DEFAULT_PLOTS} colormaps × {DEFAULT_NX}x{DEFAULT_NY}, "
        f"log Y+Z, gaps+NaN, {iters} iters\n"
    )
    wait_for_profiler()

    timer = PerfTimer()
    timer.start()
    for _ in range(iters):
        panel = setup_static_panel()
        panel.close()
        panel.deleteLater()
        QApplication.processEvents()
    ms = timer.elapsed_ms()
    sys.stderr.write(f"  total: {ms:.1f} ms, per-iter: {ms / iters:.1f} ms\n")


def scenario_callable_pan(iters):
    panel = setup_callable_panel()
    r = panel.plot_at(0).x_axis().range()
    pan_step = r.size() * 0.005

    sys.stderr.write(
        f"callable_pan: {DEFAULT_PLOTS} colormaps × {DEFAULT_NY}ch, "
        f"log Y+Z, gaps+NaN, step={pan_step:.4f}, {iters} iters\n"
    )
    wait_for_profiler()

    timer = PerfTimer()
    timer.start()
    current = r
    for _ in range(iters):
        current = SciQLopPlotRange(current.start() + pan_step, current.stop() + pan_step)
        panel.set_time_axis_range(current)
        for p in range(DEFAULT_PLOTS):
            panel.plot_at(p).replot(False)
        QApplication.processEvents()
    ms = timer.elapsed_ms()
    sys.stderr.write(f"  total: {ms:.1f} ms, per-iter: {ms / iters:.1f} ms\n")
    panel.close()
    panel.deleteLater()


# ── Main ───────────────────────────────────────────────────────

SCENARIOS = {
    "synced_pan":    (scenario_synced_pan,    100),
    "y_rescale":     (scenario_y_rescale,      50),
    "full_replot":   (scenario_full_replot,     20),
    "panel_setup":   (scenario_panel_setup,      3),
    "callable_pan":  (scenario_callable_pan,    50),
}


def main():
    global USE_BARRIER

    args = [a for a in sys.argv[1:] if a != "--no-barrier"]
    if len(args) != len(sys.argv[1:]):
        USE_BARRIER = False

    if not args:
        print(f"Usage: {sys.argv[0]} [--no-barrier] <scenario> [iterations]", file=sys.stderr)
        print("Scenarios:", file=sys.stderr)
        for name, (_, default_iters) in SCENARIOS.items():
            print(f"  {name:<18s} (default {default_iters} iters)", file=sys.stderr)
        sys.exit(1)

    name = args[0]
    iters = int(args[1]) if len(args) >= 2 else None

    if name not in SCENARIOS:
        print(f"Unknown scenario: {name}", file=sys.stderr)
        sys.exit(1)

    fn, default_iters = SCENARIOS[name]
    fn(iters if iters and iters > 0 else default_iters)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    main()
