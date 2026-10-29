"""API review 4.10: plot.axes() and plot.data_range()."""
import numpy as np

from SciQLopPlots import SciQLopPlotRange


def test_axes_lists_the_plot_axes(plot):
    axes = plot.axes()
    for axis in (plot.x_axis(), plot.y_axis(), plot.x2_axis(), plot.y2_axis(), plot.z_axis()):
        assert axis in axes
    assert len(axes) == 5


def test_data_range_is_the_union_of_the_plottables_key_ranges(plot):
    assert not plot.data_range().is_valid()
    plot.plot(np.linspace(0.0, 10.0, 20), np.zeros(20))
    plot.plot(np.linspace(-5.0, 3.0, 20), np.zeros(20))
    assert plot.data_range() == SciQLopPlotRange(-5.0, 10.0)
