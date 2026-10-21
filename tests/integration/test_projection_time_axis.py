"""The projection plot has exactly one hidden time axis (found while designing the histogram)."""
from PySide6.QtCore import Qt

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlotDummyAxis


def test_projection_plot_has_one_time_axis(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    own = proj.findChildren(SciQLopPlotDummyAxis, options=Qt.FindDirectChildrenOnly)
    assert len(own) == 1
    assert own[0] is proj.time_axis()


def test_projection_time_axis_still_drives_the_signal(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    with qtbot.waitSignal(proj.time_axis_range_changed, timeout=1000):
        proj.time_axis().set_range(0.0, 10.0)
