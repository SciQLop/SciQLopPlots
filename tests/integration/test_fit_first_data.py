"""set_fit_first_data(False) keeps a plot's ranges when its graphs get their first data.
SciQLop's scene plots need fixed ±extent panes; they emptied the protected
m_axes_to_rescale instead, which also broke the rescale shortcut."""
import numpy as np

x = np.linspace(10, 20, 1000)
y = np.sin(x) - 3  # [-4, -2], outside the ranges set below


def _hold(plot):
    plot.set_fit_first_data(False)
    plot.x_axis().set_range(-1, 1)
    plot.y_axis().set_range(-1, 1)


def _ranges(plot):
    return [(r.start(), r.stop()) for r in (plot.x_axis().range(), plot.y_axis().range())]


def test_fits_first_data_by_default(plot):
    assert plot.fit_first_data()


def test_static_graph_keeps_the_ranges(plot, qtbot):
    _hold(plot)
    plot.plot(x, y)
    qtbot.wait(100)
    assert _ranges(plot) == [(-1, 1), (-1, 1)]


def test_first_batch_of_an_empty_graph_keeps_the_ranges(plot, qtbot):
    _hold(plot)
    graph = plot.plot(x[:0], y[:0])
    graph.set_data(x, y)
    qtbot.wait(100)
    assert _ranges(plot) == [(-1, 1), (-1, 1)]


def test_static_colormap_keeps_the_ranges(plot, qtbot):
    _hold(plot)
    plot.plot(x, np.logspace(0, 3, 16), np.ones((x.size, 16)))
    qtbot.wait(100)
    assert _ranges(plot)[0] == (-1, 1)


def test_explicit_rescale_still_fits(plot, qtbot):
    _hold(plot)
    plot.plot(x, y)
    qtbot.wait(100)
    plot.rescale_axes()
    xr = plot.x_axis().range()
    assert xr.start() <= 10 and xr.stop() >= 20
