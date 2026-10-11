"""Projection plot bugs reported from SciQLop against 0.50.1.

1. No labels (or the wrong number) built no curves at all, silently.
2. A batch with the wrong number of buffers was dropped silently.
3. panel.plot(..., plot_type=Projections, name=...) called set_name on the
   (plot, graph) tuple.
4. panel.plot(t, x, y, z, plot_type=Projections) passed four arrays to a binding
   that takes one list.
5. set_name on a projection graph left the legends on their old labels.
6. The time marker was placed once, in pixels: set before the data landed it never
   showed, and it stayed put when the axes moved.
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import (GraphType, PlotType, SciQLopMultiPlotPanel,
                          SciQLopNDProjectionPlot)

t = np.linspace(0, 10, 100)
X, Y, Z = np.cos(t), np.sin(t), t / 10
PROJECTION = dict(plot_type=PlotType.Projections, graph_type=GraphType.ParametricCurve)


@pytest.fixture
def panel(qtbot):
    p = SciQLopMultiPlotPanel(synchronize_x=False)
    qtbot.addWidget(p)
    return p


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    p.resize(900, 300)
    qtbot.addWidget(p)
    p.show()
    return p


def pane_plottables(plot):
    return [len(plot.subplot(i).plottables()) for i in range(plot.subplot_count())]


def test_no_labels_still_draws_a_curve_on_every_pane(panel):
    plot, _ = panel.plot([t, X, Y, Z], **PROJECTION)
    assert pane_plottables(plot) == [1, 1, 1]


@pytest.mark.parametrize("labels", [["a", "b"], ["a", "b", "c", "d"]])
def test_wrong_label_count_raises_and_adds_no_plot(panel, labels):
    with pytest.raises(RuntimeError, match="label"):
        panel.plot([t, X, Y, Z], labels=labels, **PROJECTION)
    assert panel.size() == 0


def test_wrong_buffer_count_raises_and_adds_no_plot(panel):
    with pytest.raises(RuntimeError, match="buffers"):
        panel.plot([t, X, Y, Z, Z], **PROJECTION)
    assert panel.size() == 0


def test_set_data_with_wrong_buffer_count_raises(panel):
    _, graph = panel.plot([t, X, Y, Z], **PROJECTION)
    with pytest.raises(RuntimeError, match="buffers"):
        graph.set_data([t, X, Y, Z, Z])


def test_name_on_a_projection_names_the_graph(panel):
    _, graph = panel.plot(t, np.column_stack([X, Y, Z]), name="orbit", **PROJECTION)
    assert graph.name == "orbit"


def test_four_arrays_plot_like_one_list(panel):
    _, graph = panel.plot(t, X, Y, Z, **PROJECTION)
    assert len(graph.components()) == 3


def test_set_name_renames_every_pane_legend(proj):
    graph = proj.add_reference_curve([t, X, Y, Z])
    graph.set_name("orbit")
    assert [c.name() for c in graph.components()] == ["orbit"] * 3


def red_pixels(plot, tmp_path):
    path = str(tmp_path / "marker.png")
    assert plot.save_png(path, 900, 300)
    img = QImage(path)
    return sum(1 for x in range(img.width()) for y in range(img.height())
               if (c := img.pixelColor(x, y)).red() > 200 and c.green() < 60 and c.blue() < 60)


def test_marker_set_before_the_data_lands_shows_once_it_does(proj, qtbot, tmp_path):
    graph = proj.add_reference_curve([t, X, Y, Z])
    proj.set_time_marker(5.0)
    qtbot.waitUntil(lambda: not graph.busy())
    qtbot.wait(50)
    assert red_pixels(proj, tmp_path) > 0


def test_marker_follows_the_axes(proj, qtbot, tmp_path):
    graph = proj.add_reference_curve([t, X, Y, Z])
    qtbot.waitUntil(lambda: not graph.busy())
    proj.set_time_marker(5.0)
    qtbot.wait(50)
    assert red_pixels(proj, tmp_path) > 0
    for i in range(proj.subplot_count()):
        proj.subplot(i).x_axis().set_range(100.0, 200.0)
        proj.subplot(i).y_axis().set_range(100.0, 200.0)
    qtbot.wait(50)
    assert red_pixels(proj, tmp_path) == 0


def test_removing_a_projection_removes_its_curve_from_every_pane(proj):
    graph = proj.add_reference_curve([t, X, Y, Z])
    proj.remove_plottable(graph)
    assert pane_plottables(proj) == [0, 0, 0]
    assert proj.plottables() == []
