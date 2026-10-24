"""Items belong to their plot: QObject parent, null plot, death with the plot (part B)."""
import pytest
import shiboken6
from PySide6.QtCore import QObject, QPointF, QRectF, SIGNAL
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtWidgets import QApplication, QWidget

from SciQLopPlots import (
    SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopHorizontalLine, SciQLopHorizontalSpan,
    SciQLopPixmapItem, SciQLopPlot, SciQLopPlotRange as R, SciQLopRectangularSpan,
    SciQLopStraightLine, SciQLopTextItem, SciQLopVerticalLine, SciQLopVerticalSpan,
)
from conftest import collect

CONSTRUCTORS = {
    "text": lambda p: SciQLopTextItem(p, "t", QPointF(10, 10)),
    "pixmap": lambda p: SciQLopPixmapItem(p, QPixmap(4, 4), QRectF(0, 0, 4, 4)),
    "ellipse": lambda p: SciQLopEllipseItem(p, QRectF(0, 0, 4, 4)),
    "curved_line": lambda p: SciQLopCurvedLineItem(p, QPointF(0, 0), QPointF(5, 5)),
    "straight_line": lambda p: SciQLopStraightLine(p, 1.0),
    "vertical_line": lambda p: SciQLopVerticalLine(p, 1.0),
    "horizontal_line": lambda p: SciQLopHorizontalLine(p, 1.0),
    "vertical_span": lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)),
    "horizontal_span": lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)),
    "rectangular_span": lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
}


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_parents_the_item_to_its_plot(plot, kind):
    item = CONSTRUCTORS[kind](plot)
    assert item.parent() is plot


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_rejects_a_null_plot(kind):
    with pytest.raises(ValueError, match="plot cannot be None"):
        CONSTRUCTORS[kind](None)


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_held_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = CONSTRUCTORS[kind](plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)


def _replots(plot):
    qcp = next(w for w in plot.findChildren(QWidget)
               if w.metaObject().indexOfSignal("afterReplot()") >= 0)
    hits = []
    QObject.connect(qcp, SIGNAL("afterReplot()"), lambda: hits.append(True))
    return hits


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_deleting_an_item_repaints_the_plot(plot, kind):
    item = CONSTRUCTORS[kind](plot)
    for _ in range(3):
        QApplication.processEvents()
    hits = _replots(plot)
    shiboken6.delete(item)
    for _ in range(3):
        QApplication.processEvents()
    assert hits, "the plot was not repainted after the item was detached"
