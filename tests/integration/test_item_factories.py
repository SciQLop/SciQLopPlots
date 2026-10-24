"""Items belong to their plot: QObject parent, null plot, death with the plot (part B)."""
import gc

import pytest
import shiboken6
from PySide6.QtCore import QObject, QPointF, QRectF, SIGNAL
from PySide6.QtGui import QBrush, QColor, QPen, QPixmap
from PySide6.QtWidgets import QApplication, QWidget

from SciQLopPlots import (
    SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopHorizontalLine, SciQLopHorizontalSpan,
    SciQLopPixmapItem, SciQLopPlot, SciQLopPlotRange as R, SciQLopRectangularSpan,
    SciQLopStraightLine, SciQLopTextItem, SciQLopVerticalLine, SciQLopVerticalSpan,
)
from SciQLopPlots import (Coordinates, LineTermination, SciQLopItemInterface,
                          SciQLopNDProjectionPlot)
from conftest import collect, off_thread

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


FACTORIES = {
    "text": (lambda p: p.text("t", QPointF(10, 10)), SciQLopTextItem),
    "pixmap": (lambda p: p.pixmap(QPixmap(4, 4), QRectF(0, 0, 4, 4)), SciQLopPixmapItem),
    "ellipse": (lambda p: p.ellipse(QRectF(0, 0, 4, 4)), SciQLopEllipseItem),
    "ellipse_pen": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QPen(QColor("red")), QBrush(QColor("blue"))),
                    SciQLopEllipseItem),
    "ellipse_colors": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QColor("red"), 2.0, QColor("blue")),
                       SciQLopEllipseItem),
    "curved_line": (lambda p: p.curved_line(QPointF(0, 0), QPointF(5, 5)), SciQLopCurvedLineItem),
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopVerticalLine),
    "horizontal_line": (lambda p: p.horizontal_line(1.0), SciQLopHorizontalLine),
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopVerticalSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopHorizontalSpan),
    "rectangular_span": (lambda p: p.rectangular_span(R(1.0, 2.0), R(1.0, 2.0)),
                         SciQLopRectangularSpan),
}


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_returns_an_item_of_its_plot(plot, kind):
    make, cls = FACTORIES[kind]
    item = make(plot)
    assert type(item) is cls
    assert item.parent() is plot


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_survives_dropping_the_reference(plot, kind):
    make, cls = FACTORIES[kind]
    make(plot)
    gc.collect(); QApplication.processEvents(); gc.collect()
    assert len(plot.findChildren(cls)) == 1


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = FACTORIES[kind][0](plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)


def test_factory_off_thread_raises(plot):
    out = {}

    def call():
        try:
            plot.text("t", QPointF(1, 1))
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


def test_factory_keeps_constructor_argument_names(plot):
    span = plot.vertical_span(R(1.0, 2.0), color=QColor("red"), read_only=True,
                              visible=False, tool_tip="tip", coordinates=Coordinates.Data)
    assert span.tool_tip() == "tip"
    line = plot.curved_line(QPointF(0, 0), QPointF(1, 1), start_terminator=LineTermination.Arrow,
                            stop_terminator=LineTermination.NoneTermination)
    assert line is not None


def test_projection_pane_has_the_factories(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    pane = nd.subplot(0)
    item = pane.text("t", QPointF(1, 1))
    assert item.parent() is pane


def test_projection_plot_itself_has_none(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    with pytest.raises(NotImplementedError):
        nd.text("t", QPointF(1, 1))


def _all_subclasses(cls):
    for sub in cls.__subclasses__():
        yield sub
        yield from _all_subclasses(sub)


NO_PLOT_FACTORY = {
    "SciQLopStraightLine": "deprecated-only: vertical_line / horizontal_line cover it",
    "MultiPlotsVerticalSpan": "panel-only (panel.vertical_span)",
    "MultiPlotsVerticalLine": "panel-only (panel.vertical_line)",
}


def test_every_item_class_has_a_factory():
    covered = {cls.__name__ for _, cls in FACTORIES.values()}
    items = {c.__name__ for c in _all_subclasses(SciQLopItemInterface)} | \
            {c.__name__ for c in _all_subclasses(SciQLopStraightLine)} | {"SciQLopStraightLine"}
    abstract = {n for n in items if n.endswith("Interface")}
    missing = items - covered - abstract - set(NO_PLOT_FACTORY)
    assert not missing, f"item classes with no plot factory: {sorted(missing)}"
