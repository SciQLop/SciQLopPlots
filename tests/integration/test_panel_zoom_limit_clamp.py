"""The panel's time range is the one its plots took, zoom-limit clamp included (SciQLop).

0.51.0 regression: the panel stored the requested 5 days while its plot, limited to
1 day, showed 1 day. The plot's clamped echo was dropped while the synchroniser was
propagating the request.
"""
import pytest

from SciQLopPlots import PlotType, SciQLopMultiPlotPanel, SciQLopPlotRange

DAY = 86400.0


def _span(r):
    return r.stop() - r.start()


def test_panel_range_reflects_the_plot_zoom_limit(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    plot = panel.create_plot(0, PlotType.TimeSeries)
    plot.time_axis().set_max_range_size(DAY)
    panel.set_time_axis_range(SciQLopPlotRange(0.0, 5 * DAY))
    qtbot.wait(50)
    assert _span(plot.time_axis().range()) == pytest.approx(DAY)
    assert _span(panel.time_axis_range()) == pytest.approx(DAY)
