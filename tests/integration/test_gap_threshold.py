"""Issue #118: line graphs' gap detection must be controllable.

Step and state data (instrument modes, flags) change at irregular times.
Gap detection used to break the line at every long step, and a repeated key
made every later step a gap, so only the vertical edges were drawn.
"""
import numpy as np
import pytest
from PySide6.QtGui import QColor, QImage

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange, GraphLineStyle
from conftest import process_events

RED = QColor("#ff0000")
IRREGULAR_KEYS = np.array([0.0, 1.0, 5.0, 6.0, 12.0])
STATES = np.array([1.0, 0.0, 1.0, 0.0, 0.0])


def _drawn_fraction(plot, tmp_path):
    """Share of the pixel columns between the trace's ends that hold some of it."""
    path = str(tmp_path / "plot.png")
    assert plot.save_png(path, 600, 300)
    img = QImage(path)
    columns = [x for x in range(img.width())
               if any(_is_red(img.pixelColor(x, y)) for y in range(img.height()))]
    assert columns, "nothing drawn"
    return len(columns) / (columns[-1] - columns[0] + 1)


def _is_red(c):
    return c.red() > 200 and c.green() < 120 and c.blue() < 120


def _plot(qtbot, x, y, step=False):
    plot = SciQLopPlot()
    qtbot.addWidget(plot)
    graph = plot.plot(x, y, colors=[RED] * (y.shape[1] if y.ndim == 2 else 1))
    if step:
        for c in graph.components():
            c.set_line_style(GraphLineStyle.StepLeft)
    plot.x_axis().set_range(SciQLopPlotRange(x[0] - 0.5, x[-1] + 0.5))
    plot.y_axis().set_range(SciQLopPlotRange(-0.5, 1.5))
    process_events()
    return plot, graph


def _columns(y):
    return np.column_stack([y, y])


@pytest.mark.parametrize("make_y", [lambda y: y, _columns], ids=["single_line", "multi_line"])
class TestGapThreshold:
    def test_default_threshold(self, qtbot, make_y):
        _, graph = _plot(qtbot, IRREGULAR_KEYS, make_y(STATES))
        assert graph.gap_threshold() == pytest.approx(1.5)

    def test_zero_draws_irregular_steps_whole(self, qtbot, tmp_path, make_y):
        plot, graph = _plot(qtbot, IRREGULAR_KEYS, make_y(STATES), step=True)
        assert _drawn_fraction(plot, tmp_path) < 0.6, "default detection should break the steps"

        graph.set_gap_threshold(0)
        assert graph.gap_threshold() == 0
        assert _drawn_fraction(plot, tmp_path) > 0.95


def test_repeated_keys_do_not_hide_the_trace(qtbot, tmp_path):
    x = np.array([0.0, 0.0, 1.0, 1.0, 5.0, 5.0])
    y = np.array([0.0, 1.0, 1.0, 0.0, 0.0, 1.0])
    plot, _ = _plot(qtbot, x, y)
    assert _drawn_fraction(plot, tmp_path) > 0.95
