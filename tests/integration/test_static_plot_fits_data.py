"""plot(x, y) with arrays fits the axes to the data, like a data function's
first batch does. The first batch's request_rescale used to fire before the
plot connected to it, so static graphs stayed on the default 0..5 range."""
import numpy as np
import pytest

from SciQLopPlots import GraphType

x = np.linspace(10, 20, 1000)
y = np.sin(x) - 3  # [-4, -2], outside the default range

GRAPH_TYPES = [GraphType.Line, GraphType.Scatter, GraphType.ParametricCurve, GraphType.Waterfall]


@pytest.mark.parametrize("graph_type", GRAPH_TYPES, ids=lambda g: g.name)
def test_static_plot_fits_its_data(plot, qtbot, graph_type):
    plot.plot(x, y, graph_type=graph_type)
    qtbot.wait(100)
    xr, yr = plot.x_axis().range(), plot.y_axis().range()
    assert xr.start() <= 10 and xr.stop() >= 20
    assert yr.start() < 0


def test_range_set_after_plot_is_kept(plot, qtbot):
    plot.plot(x, y)
    plot.x_axis().set_range(12, 13)
    qtbot.wait(100)
    assert (plot.x_axis().range().start(), plot.x_axis().range().stop()) == (12, 13)


def test_static_colormap_fits_its_data(plot, qtbot):
    ys = np.logspace(0, 3, 16)
    cmap = plot.plot(x, ys, np.ones((x.size, ys.size)))
    qtbot.wait(100)
    xr, yr = plot.x_axis().range(), cmap.y_axis().range()
    assert xr.start() <= 10 and xr.stop() >= 20
    assert yr.stop() >= 1000


def test_waterfall_fits_its_offsets(plot, qtbot):
    traces = np.column_stack([np.sin(x)] * 8)
    plot.plot(x, traces, graph_type=GraphType.Waterfall, offsets=2.5)
    qtbot.wait(100)
    assert plot.y_axis().range().stop() >= 7 * 2.5


def test_panel_waterfall_fits_its_offsets_only_in_its_plot(panel, qtbot):
    zoomed, _ = panel.plot(x, y)
    zoomed.y_axis().set_range(-3.5, -3.4)
    wf_plot, _ = panel.plot(x, np.column_stack([np.sin(x)] * 8),
                            graph_type=GraphType.Waterfall, offsets=2.5)
    qtbot.wait(100)
    assert wf_plot.y_axis().range().stop() >= 7 * 2.5
    assert (zoomed.y_axis().range().start(), zoomed.y_axis().range().stop()) == (-3.5, -3.4)


def test_panel_rescale_axes_fits_every_plot(panel, qtbot):
    plots = [panel.plot(x, y + k)[0] for k in range(2)]
    qtbot.wait(100)
    for p in plots:
        p.y_axis().set_range(100, 101)
    panel.rescale_axes()
    for k, p in enumerate(plots):
        assert p.y_axis().range().start() < k
