"""A parametric curve must size itself from its data, like every other graph.

SciQLopCurve was the only plottable whose component count came from the label
list rather than the data: `create_graphs()` looped over labels, and the
resampler bounded its output by `line_count()`. So a curve created without
`labels=` had zero components, the resampler emitted zero columns, and
`plot(x, y, graph_type=ParametricCurve)` drew nothing at all — no error, no
warning, just an empty plot.

Line and scatter graphs never had this: SciQLopSingleLineGraph::create_graph
builds its component unconditionally, and SciQLopMultiGraphBase derives its
component count from the data in sync_components().
"""
from collections import Counter

import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import GraphType, SciQLopPlot
from conftest import process_events

N = 300


@pytest.fixture
def spiral():
    a = np.linspace(0, 6 * np.pi, N)
    return (a * np.cos(a)), (a * np.sin(a))


def _ink(plot, tmp_path, name):
    """Foreground pixel count — everything that is not the background fill."""
    path = tmp_path / f"{name}.png"
    assert plot.save_png(str(path), 400, 300) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    assert not img.isNull()
    pixels = Counter(img.pixelColor(x, y).rgb()
                     for y in range(img.height())
                     for x in range(img.width()))
    (_bg, bg_count), = pixels.most_common(1)
    return sum(pixels.values()) - bg_count


class TestComponentCountFollowsTheData:
    def test_unlabelled_curve_creates_one_component(self, qtbot, plot, spiral):
        graph = plot.plot(*spiral, graph_type=GraphType.ParametricCurve)
        # busy() reads the first component's flag, so it is already False while
        # there are no components — wait on the count itself, not on busy().
        qtbot.waitUntil(lambda: graph.plottable_count() == 1, timeout=5000)

    def test_unlabelled_multi_column_curve_creates_one_per_column(
            self, qtbot, plot, spiral):
        x, y = spiral
        graph = plot.plot(x, np.column_stack([y, y * 0.5, y * 0.25]),
                          graph_type=GraphType.ParametricCurve)
        qtbot.waitUntil(lambda: graph.plottable_count() == 3, timeout=5000)

    def test_labels_still_decide_the_count(self, qtbot, plot, spiral):
        x, y = spiral
        graph = plot.plot(x, np.column_stack([y, y * 0.5]),
                          graph_type=GraphType.ParametricCurve,
                          labels=["a", "b"])
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        assert graph.plottable_count() == 2
        assert graph.labels() == ["a", "b"]

    def test_components_shrink_when_a_later_batch_has_fewer_columns(
            self, qtbot, plot, spiral):
        x, y = spiral
        graph = plot.plot(x, np.column_stack([y, y * 0.5, y * 0.25]),
                          graph_type=GraphType.ParametricCurve)
        qtbot.waitUntil(lambda: graph.plottable_count() == 3, timeout=5000)
        graph.set_data(x, y)
        qtbot.waitUntil(lambda: graph.plottable_count() == 1, timeout=5000)

    def test_components_grow_when_a_later_batch_has_more_columns(
            self, qtbot, plot, spiral):
        x, y = spiral
        graph = plot.plot(x, y, graph_type=GraphType.ParametricCurve)
        qtbot.waitUntil(lambda: graph.plottable_count() == 1, timeout=5000)
        graph.set_data(x, np.column_stack([y, y * 0.5]))
        qtbot.waitUntil(lambda: graph.plottable_count() == 2, timeout=5000)


class TestUnlabelledCurveActuallyDraws:
    def test_it_renders_as_much_as_a_labelled_curve(self, qtbot, spiral, tmp_path):
        unlabelled, labelled = SciQLopPlot(), SciQLopPlot()
        for p in (unlabelled, labelled):
            qtbot.addWidget(p)
            p.legend().set_visible(False)
        a = unlabelled.plot(*spiral, graph_type=GraphType.ParametricCurve)
        b = labelled.plot(*spiral, graph_type=GraphType.ParametricCurve,
                          labels=["c"])
        for g in (a, b):
            qtbot.waitUntil(lambda g=g: g.plottable_count() > 0 and not g.busy(),
                            timeout=5000)
        for p in (unlabelled, labelled):
            p.rescale_axes()
        process_events()

        assert (_ink(unlabelled, tmp_path, "u")
                == pytest.approx(_ink(labelled, tmp_path, "l"), rel=0.05))

    def test_the_component_is_reachable_for_styling(self, qtbot, plot, spiral):
        """component(0) returning None is what made this silent."""
        graph = plot.plot(*spiral, graph_type=GraphType.ParametricCurve)
        qtbot.waitUntil(lambda: graph.plottable_count() > 0, timeout=5000)
        assert graph.component(0) is not None


class TestDataRoundTrip:
    def test_set_data_validation_still_rejects_ragged_columns(
            self, qtbot, plot, spiral):
        """A labelled curve keeps its fixed component count as a contract."""
        x, y = spiral
        graph = plot.plot(x, np.column_stack([y, y * 0.5]),
                          graph_type=GraphType.ParametricCurve,
                          labels=["a", "b"])
        qtbot.waitUntil(lambda: not graph.busy(), timeout=5000)
        # set_data surfaces std::invalid_argument as RuntimeError (no explicit
        # mapping in bindings.xml); the existing hardening tests hedge the same way.
        with pytest.raises((ValueError, RuntimeError)):
            graph.set_data(x, y)  # 1 column for 2 labelled components

    def test_empty_data_leaves_no_components(self, qtbot, plot):
        graph = plot.plot(np.array([]), np.array([]),
                          graph_type=GraphType.ParametricCurve)
        process_events()
        assert graph.plottable_count() == 0
