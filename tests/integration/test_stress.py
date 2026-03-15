"""Automated stress tests for plot panel lifecycle.

Converts the manual StressModel demo into real pytest tests that verify
no crashes during rapid create/destroy cycles.
"""
import gc
import random

import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt


def process_events():
    QApplication.processEvents()


def force_gc():
    gc.collect()
    process_events()
    gc.collect()


@pytest.fixture
def panel(qtbot):
    from SciQLopPlots import SciQLopMultiPlotPanel
    p = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(p)
    return p


def _add_plot_with_graph(panel):
    plot, graph = panel.plot(
        np.linspace(0, 10, 200, dtype=np.float64),
        np.sin(np.linspace(0, 10, 200)).astype(np.float64),
        labels=["test"],
        colors=[QColorConstants.Red],
    )
    return plot


def test_rapid_create_destroy_50_cycles(panel):
    """50 rapid create/destroy cycles without crash."""
    for _ in range(50):
        plot = _add_plot_with_graph(panel)
        process_events()
        panel.remove_plot(plot)
        process_events()
    force_gc()
    assert panel.size() == 0


def test_random_create_remove_100_ops(panel):
    """100 random create/remove operations weighted toward creation."""
    plots = []
    for _ in range(100):
        if not plots or random.random() < 0.7:
            plot = _add_plot_with_graph(panel)
            plots.append(plot)
        else:
            idx = random.randint(0, len(plots) - 1)
            panel.remove_plot(plots.pop(idx))
        process_events()

    # Clean up remaining
    for p in plots:
        panel.remove_plot(p)
    force_gc()
    assert panel.size() == 0


def test_create_many_then_clear(panel):
    """Create 20 plots, then clear all at once."""
    for _ in range(20):
        _add_plot_with_graph(panel)
        process_events()
    assert panel.size() == 20

    panel.clear()
    process_events()
    force_gc()
    assert panel.size() == 0
