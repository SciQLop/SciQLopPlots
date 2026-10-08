"""SciQLop#151: moving the mouse over a plot holding an item anchored far outside
the view aborted. The hit test built an int QRect from a pixel position beyond int
range, and Qt 6.11's checked QRect arithmetic asserts on the overflow."""
import math

import pytest
from PySide6.QtCore import QEvent, QPoint, QPointF, Qt
from PySide6.QtGui import QMouseEvent
from PySide6.QtWidgets import QApplication

from SciQLopPlots import Coordinates, SciQLopTextItem


def _move_mouse_over(plot):
    plot.resize(600, 300)
    plot.show()
    for _ in range(5):
        QApplication.processEvents()
    pos = QPoint(300, 150)
    target = plot.childAt(pos) or plot
    local = QPointF(target.mapFrom(plot, pos))
    QApplication.sendEvent(target, QMouseEvent(
        QEvent.Type.MouseMove, local, QPointF(target.mapToGlobal(local)),
        Qt.MouseButton.NoButton, Qt.MouseButton.NoButton, Qt.KeyboardModifier.NoModifier))
    QApplication.processEvents()


@pytest.mark.parametrize("coord", [1e15, math.nan], ids=["far", "nan"])
def test_mouse_move_over_a_far_text_item(ts_plot, coord):
    ts_plot.x_axis().set_range(0, 3600)
    item = SciQLopTextItem(ts_plot, "far", QPointF(coord, coord), False, Coordinates.Data)
    _move_mouse_over(ts_plot)
    assert item is not None  # reaching here means no abort
