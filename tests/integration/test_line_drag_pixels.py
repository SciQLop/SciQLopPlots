"""Dragging a Pixels-mode line moves it by the mouse delta (pixels are axis-rect relative)."""
import pytest
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from SciQLopPlots import Coordinates, SciQLopPlotRange
from conftest import process_events

START_PX = 300.0
DRAG_PX = 40


def _canvas(plot):
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


def _point(vertical, along, across):
    return QPoint(along, across) if vertical else QPoint(across, along)


def _hit_centre(canvas, line, vertical):
    """Centre of the run of canvas pixels (along the drag axis) that select the line."""
    across = (canvas.height() if vertical else canvas.width()) // 2
    size = canvas.width() if vertical else canvas.height()
    hits = []
    for along in range(10, size - 10):
        QTest.mouseClick(canvas, Qt.LeftButton, Qt.NoModifier, _point(vertical, along, across))
        process_events()
        if line.selected():
            hits.append(along)
            line.set_selected(False)
    return (hits[0] + hits[-1]) // 2, across


def _drag(canvas, vertical, start, end, across):
    QTest.mousePress(canvas, Qt.LeftButton, Qt.NoModifier, _point(vertical, start, across))
    for along in range(start, end + 1, 2):
        QTest.mouseMove(canvas, _point(vertical, along, across))
        process_events()
    QTest.mouseRelease(canvas, Qt.LeftButton, Qt.NoModifier, _point(vertical, end, across))
    process_events()


@pytest.mark.parametrize("vertical", [True, False])
def test_dragging_a_pixels_line_moves_it_by_the_mouse_delta(qtbot, plot, vertical):
    plot.resize(800, 400)
    plot.show()
    qtbot.waitExposed(plot)
    axis = plot.x_axis() if vertical else plot.y_axis()
    axis.set_range(SciQLopPlotRange(0.0, 1000.0))
    process_events()
    make = plot.vertical_line if vertical else plot.horizontal_line
    line = make(START_PX, read_only=False, coordinates=Coordinates.Pixels)
    plot.replot(True)
    process_events()
    start, across = _hit_centre(_canvas(plot), line, vertical)
    _drag(_canvas(plot), vertical, start, start + DRAG_PX, across)
    assert line.position() == pytest.approx(START_PX + DRAG_PX, abs=2.0)
