"""The colour bar spans the same height as the data area (SciQLop screenshot).

The bar ignored the plot's bottom margin: it ran down past the x axis, alongside
the tick labels, and the last time label slid under it.
"""
import numpy as np
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopPlot
from conftest import process_events


def _coloured_rows(img, x):
    return [y for y in range(img.height()) if img.pixelColor(x, y).saturation() > 100]


def test_colour_bar_ends_where_the_data_area_ends(qtbot, tmp_path):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    cmap = plot.plot(np.arange(100.0), np.arange(16.0), np.random.rand(100, 16), name="spec")
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    process_events()
    path = str(tmp_path / "bar.png")
    assert plot.save_png(path, 600, 300)
    img = QImage(path)

    data_rows = _coloured_rows(img, 250)
    bar_x = next(x for x in range(img.width() - 1, 0, -1) if _coloured_rows(img, x))
    bar_rows = _coloured_rows(img, bar_x - 3)
    assert abs(max(bar_rows) - max(data_rows)) <= 2, (max(bar_rows), max(data_rows))
    assert abs(min(bar_rows) - min(data_rows)) <= 2, (min(bar_rows), min(data_rows))



def _lowest_dark_row(img, x):
    return max(y for y in range(img.height()) if img.pixelColor(x, y).value() < 100)


def test_spectrogram_above_a_line_has_no_extra_band(qtbot, tmp_path):
    """SciQLop screenshot: in a panel, an empty band sat under a spectrogram that was not the
    bottom plot. Its colour bar shares its margins through a margin group, and the group's
    margin could not shrink below its first layout's once the time labels were hidden."""
    from SciQLopPlots import SciQLopMultiPlotPanel, PlotType
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    panel.resize(1200, 700)
    panel.show()
    t = np.linspace(1.6e9, 1.6e9 + 1e5, 500)
    map_plot, cmap = panel.plot(t, np.logspace(0, 4, 16), np.random.rand(500, 16) + 1,
                                y_log_scale=True, z_log_scale=True, plot_type=PlotType.TimeSeries)
    map_plot.z_axis().set_label("DEF_omni [(cm^2 s sr keV)^-1]")
    line_plot, _ = panel.plot(t, np.sin(t), plot_type=PlotType.TimeSeries)
    panel.plot(t, np.cos(t), plot_type=PlotType.TimeSeries)
    qtbot.waitUntil(lambda: not cmap.busy(), timeout=5000)
    qtbot.wait(300)   # the panel lays its plots out over a few event loop turns
    below_axis = []   # empty rows under the x axis line
    for p, name in ((line_plot, "line"), (map_plot, "map")):
        path = str(tmp_path / f"{name}.png")
        assert p.save_png(path)   # at the widget's own size: the panel's layout
        img = QImage(path)
        below_axis.append(img.height() - 1 - _lowest_dark_row(img, p.width() // 3))
    line_gap, map_gap = below_axis
    assert abs(map_gap - line_gap) <= 2, f"{map_gap} px under the spectrogram, {line_gap} under a line"


def test_upper_plots_of_a_panel_end_at_their_axis(qtbot, tmp_path):
    """Plots stacked in a panel should sit tight: with its time labels hidden, a plot needs
    no room under its x axis. The colour scale's default 6 px minimum margins (meant for a
    scale used without a margin group) reached every plot through the group, even plots
    without a colour bar."""
    from SciQLopPlots import SciQLopMultiPlotPanel, PlotType
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    panel.resize(1200, 700)
    panel.show()
    t = np.linspace(1.6e9, 1.6e9 + 1e5, 500)
    upper, _ = panel.plot(t, np.sin(t), plot_type=PlotType.TimeSeries)
    panel.plot(t, np.cos(t), plot_type=PlotType.TimeSeries)
    qtbot.wait(300)
    path = str(tmp_path / "upper.png")
    assert upper.save_png(path)
    img = QImage(path)
    below_axis = img.height() - 1 - _lowest_dark_row(img, upper.width() // 3)
    assert below_axis <= 2, f"{below_axis} px under the x axis of an upper plot"
