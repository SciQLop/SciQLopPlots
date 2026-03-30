import json
import sys
import time
import warnings
from pathlib import Path

import pytest
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopMultiPlotPanel, PlotType

# Make perfutils importable by the test module
sys.path.insert(0, str(Path(__file__).parent))
from perfutils import (
    N_POINTS, N_COLS, N_PLOTS, COLORS,
    make_data, make_static_data, wait_for_render,
)

BASELINES_FILE = Path(__file__).parent / "baselines.json"


def _load_baselines():
    if BASELINES_FILE.exists():
        return json.loads(BASELINES_FILE.read_text())
    return {}


def _save_baselines(data):
    BASELINES_FILE.write_text(json.dumps(data, indent=2) + "\n")


# ── pytest hooks ──────────────────────────────────────────────

def pytest_addoption(parser):
    parser.addoption(
        "--save-baseline", action="store_true", default=False,
        help="Save current perf results as the new baseline",
    )
    parser.addoption(
        "--perf-threshold", type=float, default=1.5,
        help="Regression threshold multiplier (default: 1.5 = 50%% slower)",
    )


def pytest_terminal_summary(terminalreporter, config):
    results = getattr(config, "_perf_results", None)
    if not results:
        return

    baselines = _load_baselines()
    tr = terminalreporter

    tr.write_sep("=", "perf summary")
    for name, ms in sorted(results.items()):
        baseline = baselines.get(name)
        if baseline:
            ratio = ms / baseline
            marker = "OK" if ratio <= config.getoption("perf_threshold") else "REGRESSION"
            tr.write_line(f"  {name:<30s} {ms:8.1f} ms/iter  (baseline {baseline:.1f}, {ratio:.2f}x) [{marker}]")
        else:
            tr.write_line(f"  {name:<30s} {ms:8.1f} ms/iter  (no baseline)")

    if config.getoption("save_baseline"):
        merged = {**baselines, **results}
        _save_baselines(merged)
        tr.write_line(f"\n  Saved {len(results)} results to {BASELINES_FILE}")


# ── perf_check fixture ───────────────────────────────────────

class PerfCheck:
    def __init__(self, name, iterations, baselines, threshold, results_store):
        self.name = name
        self.iterations = iterations
        self.elapsed_ms = None
        self.per_iter_ms = None
        self._baselines = baselines
        self._threshold = threshold
        self._results = results_store

    def __enter__(self):
        QApplication.processEvents()
        self._start = time.perf_counter_ns()
        return self

    def __exit__(self, *exc):
        self.elapsed_ms = (time.perf_counter_ns() - self._start) / 1e6
        self.per_iter_ms = self.elapsed_ms / self.iterations
        self._results[self.name] = self.per_iter_ms

        baseline = self._baselines.get(self.name)
        if baseline:
            ratio = self.per_iter_ms / baseline
            if ratio > self._threshold:
                warnings.warn(
                    f"PERF REGRESSION: {self.name} = {self.per_iter_ms:.1f} ms/iter "
                    f"(baseline {baseline:.1f} ms, {ratio:.1f}x slower)",
                    stacklevel=3,
                )


@pytest.fixture
def perf_check(request):
    config = request.config
    baselines = _load_baselines()
    threshold = config.getoption("perf_threshold")
    if not hasattr(config, "_perf_results"):
        config._perf_results = {}

    def _make(name, iterations):
        return PerfCheck(name, iterations, baselines, threshold, config._perf_results)
    return _make


# ── panel fixtures ───────────────────────────────────────────

@pytest.fixture
def static_panel(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_x=True)
    qtbot.addWidget(panel)
    panel.resize(1920, 1080)

    labels = [f"ch{i}" for i in range(N_COLS)]
    colors = COLORS[:N_COLS]

    for _ in range(N_PLOTS):
        x, y = make_static_data()
        panel.plot(x, y, labels=labels, colors=colors, plot_type=PlotType.BasicXY)

    panel.show()
    wait_for_render(panel)
    return panel


@pytest.fixture
def callable_panel(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    panel.resize(1920, 1080)

    labels = [f"ch{i}" for i in range(N_COLS)]
    colors = COLORS[:N_COLS]

    for _ in range(N_PLOTS):
        panel.plot(make_data, labels=labels, colors=colors, plot_type=PlotType.TimeSeries)

    panel.show()
    wait_for_render(panel, timeout_s=15)
    return panel
