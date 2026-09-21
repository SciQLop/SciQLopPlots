"""Gaps met while building a projection notebook (SciQLop issue #141, "related gaps").

- the graph exposed no components, so a curve's pen could not be reached
- `set_visible(False)` on a projection graph did nothing (the base class only warns)
- the legend is repeated in every pane, with no way to show it once
"""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import SciQLopNDProjectionPlot
from conftest import process_events

N = 200
LABELS = ["xy", "yz", "zx"]


@pytest.fixture
def proj(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


@pytest.fixture
def graph(qtbot, proj):
    t = np.linspace(0, 2 * np.pi, N)
    g = proj.parametric_curve([t, 10 * np.cos(t), 10 * np.sin(t), t], labels=LABELS)
    qtbot.waitUntil(lambda: not g.busy(), timeout=5000)
    process_events()
    return g


def _coloured(proj, tmp_path, name):
    """Saturated pixels of pane 0: the curve, without the grey axes."""
    pane = proj.subplot(0)
    pane.legend().set_visible(False)
    pane.rescale_axes()
    path = tmp_path / f"{name}.png"
    assert pane.save_png(str(path), 400, 400) is True
    img = QImage(str(path)).convertToFormat(QImage.Format_ARGB32)
    return sum(1 for y in range(img.height()) for x in range(img.width())
               if img.pixelColor(x, y).saturation() > 100)


class TestComponents:
    def test_one_component_per_pane(self, graph):
        assert [c.name() for c in graph.components()] == LABELS

    def test_a_component_is_reachable_by_index_and_by_name(self, graph):
        assert graph.component(1).name() == "yz"
        assert graph.component("zx").name() == "zx"

    def test_a_component_pen_can_be_changed(self, graph):
        graph.component(0).set_line_width(3.0)
        assert graph.component(0).line_width() == pytest.approx(3.0)


class TestVisibility:
    def test_visible_by_default(self, graph):
        assert graph.visible() is True

    def test_set_visible_false_hides_every_pane(self, proj, graph, tmp_path):
        assert _coloured(proj, tmp_path, "shown") > 0
        graph.set_visible(False)
        process_events()
        assert graph.visible() is False
        assert _coloured(proj, tmp_path, "hidden") == 0

    def test_it_can_be_shown_again(self, proj, graph, tmp_path):
        graph.set_visible(False)
        graph.set_visible(True)
        process_events()
        assert graph.visible() is True
        assert _coloured(proj, tmp_path, "again") > 0

    def test_visible_changed_is_emitted(self, graph):
        seen = []
        graph.visible_changed.connect(seen.append)
        graph.set_visible(False)
        assert seen == [False]


class TestSharedLegend:
    def test_every_pane_keeps_its_legend_by_default(self, proj, graph):
        assert proj.shared_legend() is False
        assert all(proj.subplot(i).legend().is_visible() for i in range(3))

    def test_a_shared_legend_shows_on_the_first_pane_only(self, proj, graph):
        proj.set_shared_legend(True)
        assert [proj.subplot(i).legend().is_visible() for i in range(3)] == [True, False, False]

    def test_switching_it_off_restores_every_legend(self, proj, graph):
        proj.set_shared_legend(True)
        proj.set_shared_legend(False)
        assert all(proj.subplot(i).legend().is_visible() for i in range(3))
