"""Shared data generation and panel setup for perf tests."""

import time

import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants

from SciQLopPlots import SciQLopMultiPlotPanel, PlotType

N_POINTS = 5_000_000
N_COLS = 4
N_PLOTS = 4
COLORS = [
    QColorConstants.Red,
    QColorConstants.Blue,
    QColorConstants.Green,
    QColorConstants.DarkYellow,
]


def make_data(start, stop, n_cols=N_COLS):
    n = max(int(stop - start), N_POINTS)
    x = np.linspace(start, stop, n, dtype=np.float64)
    cols = [
        np.sin(x * 6.28 * 0.5 + c * 0.7)
        + 0.3 * np.sin(x * 6.28 * 50.0 + c * 0.7)
        + 0.1 * np.sin(x * 6.28 * 5000.0 + c * 0.7)
        for c in range(n_cols)
    ]
    return x, np.column_stack(cols)


def make_static_data(n_points=N_POINTS, n_cols=N_COLS):
    return make_data(0, n_points * 1e-6, n_cols)


def wait_for_render(panel, timeout_s=10):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.05)
    for i in range(panel.plot_count()):
        panel.plot_at(i).replot(True)
    QApplication.processEvents()
