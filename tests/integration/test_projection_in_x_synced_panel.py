"""A projection plot has no x axis of its own (its subplots do). A panel that syncs x
segfaulted when one was added: the x-axis synchronizer called x_axis() on it and
dereferenced the null. Reported by the SciQLop session."""
import numpy as np

from SciQLopPlots import GraphType, PlotType, SciQLopMultiPlotPanel


def _add_projection(panel):
    z = np.zeros(1)
    panel.plot([z, z, z, z], plot_type=PlotType.Projections,
               graph_type=GraphType.ParametricCurve, labels=["", "", ""])


def test_projection_plot_in_an_x_synced_panel(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    _add_projection(panel)
    assert panel.plot_count() == 1


def test_x_range_on_a_panel_holding_a_projection_plot(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    _add_projection(panel)
    panel.set_x_axis_range(0.0, 10.0)
    assert panel.plot_count() == 1


def test_last_plot_with_an_x_axis_keeps_it_visible(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    xy = panel.create_plot(-1, PlotType.BasicXY)
    _add_projection(panel)
    assert xy.x_axis().visible()
