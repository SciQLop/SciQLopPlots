"""panel.create_plot() takes every PlotType: Projections segfaulted (the switch left the plot
null and added it), reported by the SciQLop session for its scene API."""
from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot


def test_create_plot_makes_a_projection_plot(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    plot = panel.create_plot(-1, PlotType.Projections)
    assert isinstance(plot, SciQLopNDProjectionPlot)
    assert panel.plot_count() == 1


def test_create_plot_makes_every_type(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    for plot_type in (PlotType.BasicXY, PlotType.TimeSeries, PlotType.Projections):
        assert panel.create_plot(-1, plot_type) is not None, plot_type
    assert panel.plot_count() == 3
