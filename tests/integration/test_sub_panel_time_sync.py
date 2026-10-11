"""A time-synchronised child panel follows its parent's time range (SciQLop).

The child got the parent's range once, when added, and never again: the parent's
synchroniser pushed new ranges to its plots only.
"""
import numpy as np
import pytest
from PySide6 import QtCore

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, SciQLopPlotRange

R = (1.0e9, 1.0e9 + 3600)
LATER = (1.0e9 + 7200, 1.0e9 + 10800)


def _series(start, stop):
    return [np.linspace(start, stop, 10), np.zeros(10)]


@pytest.fixture
def panels(qtbot):
    parent = SciQLopMultiPlotPanel(synchronize_time=True)
    qtbot.addWidget(parent)
    sub = SciQLopMultiPlotPanel(parent, synchronize_x=False, synchronize_time=True)
    parent.add_panel(sub)
    plot, _ = sub.plot(_series, labels=["v"], plot_type=PlotType.TimeSeries)
    qtbot.wait(100)
    return parent, sub, plot


def _shows(plot, rng):
    r = plot.time_axis().range()
    assert (r.start(), r.stop()) == pytest.approx(rng, abs=1e-3), (r.start(), r.stop())


@pytest.mark.parametrize("parent_has_a_plot", [False, True])
def test_parent_range_reaches_the_child_panel(qtbot, panels, parent_has_a_plot):
    parent, sub, plot = panels
    if parent_has_a_plot:
        parent.plot(_series, labels=["p"], plot_type=PlotType.TimeSeries)
        qtbot.wait(100)
    parent.set_time_axis_range(SciQLopPlotRange(*R))
    qtbot.waitUntil(lambda: _shows(plot, R), timeout=2000)
    parent.set_time_axis_range(SciQLopPlotRange(*LATER))
    qtbot.waitUntil(lambda: _shows(plot, LATER), timeout=2000)


def test_child_range_reaches_the_parent_plots(qtbot, panels):
    parent, sub, _ = panels
    parent_plot, _ = parent.plot(_series, labels=["p"], plot_type=PlotType.TimeSeries)
    qtbot.wait(100)
    sub.set_time_axis_range(SciQLopPlotRange(*LATER))
    qtbot.waitUntil(lambda: _shows(parent_plot, LATER), timeout=2000)


@pytest.mark.parametrize("orientation, expected", [
    (QtCore.Qt.Orientation.Horizontal, [True, True]),
    (QtCore.Qt.Orientation.Vertical, [False, True]),
])
def test_side_by_side_plots_each_keep_their_time_labels(qtbot, orientation, expected):
    """Hiding all but the last plot's time labels is a rule for a vertical stack."""
    panel = SciQLopMultiPlotPanel(None, False, True, orientation)
    qtbot.addWidget(panel)
    plots = [panel.plot(_series, labels=[n], plot_type=PlotType.TimeSeries)[0] for n in "ab"]
    qtbot.wait(100)
    assert [p.x_axis().tick_labels_visible() for p in plots] == expected
