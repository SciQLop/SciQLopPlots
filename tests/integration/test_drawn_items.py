"""Items under SciQLopDrawnObject: the stubs that did nothing, and items whose plot is gone."""
import pytest
import shiboken6
from PySide6.QtCore import QCoreApplication, QEvent, QPointF, QRectF
from PySide6.QtGui import QColor, QPixmap
from PySide6.QtWidgets import QWidget

from SciQLopPlots import SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopPixmapItem, SciQLopPlot, SciQLopTextItem
from conftest import off_thread, process_events, record

MAKERS = {
    "text": lambda p: p.text("t", QPointF(10, 10)),
    "ellipse": lambda p: p.ellipse(QRectF(10, 10, 20, 20)),
    "pixmap": lambda p: p.pixmap(QPixmap(4, 4), QRectF(1, 1, 2, 2)),
}


@pytest.mark.parametrize("kind", ["text", "pixmap"])
def test_a_tooltip_round_trips(plot, kind):
    item = MAKERS[kind](plot)
    item.set_tool_tip("hello")
    assert item.tool_tip() == "hello"


def test_a_curved_line_reads_the_tooltip_hover_shows_and_round_trips(plot):
    line = plot.curved_line(QPointF(0, 0), QPointF(5, 5), tool_tip="ctor")
    assert line.tool_tip() == "ctor"
    line.set_tool_tip("hello")
    assert line.tool_tip() == "hello"


@pytest.mark.parametrize("kind", ["text", "ellipse", "pixmap"])
def test_movable_round_trips(plot, kind):
    item = MAKERS[kind](plot)
    item.set_movable(True)
    assert item.movable()
    item.set_movable(False)
    assert not item.movable()


def test_selecting_a_text_item_signals_once_though_neoqcp_signals_too(plot):
    text = MAKERS["text"](plot)
    seen = record(text.selection_changed)
    text.set_selected(True)
    assert (text.selected(), seen) == (True, [True])


def _flush_deferred_delete():
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    process_events()


def test_an_item_dies_with_its_plot(qtbot):  # Review Focus 3
    plot = SciQLopPlot()
    text = plot.text("t", QPointF(10, 10))  # parented to the plot since part B
    plot.deleteLater()
    _flush_deferred_delete()
    assert not shiboken6.isValid(text)


def test_a_queued_set_running_after_the_plot_is_gone_is_harmless(qtbot):  # Review Focus 2
    plot = SciQLopPlot()
    text = plot.text("t", QPointF(10, 10))
    off_thread(lambda: text.set_visible(False))
    plot.deleteLater()
    _flush_deferred_delete()  # the plot and the item go first, then the queued set finds nothing
    assert not shiboken6.isValid(text)


def test_an_ellipse_whose_neoqcp_item_is_gone_has_no_colour(qtbot):
    plot = SciQLopPlot()
    ellipse = MAKERS["ellipse"](plot)
    qcp = next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))
    seen = []  # the ellipse outlives its NeoQCP item until the plot deletes its own children
    qcp.destroyed.connect(lambda *_: seen.append(ellipse.color().isValid()))
    plot.deleteLater()
    _flush_deferred_delete()
    assert seen == [False]


def test_a_constructed_item_whose_neoqcp_item_is_gone_ignores_every_set(qtbot):  # Review Focus 3
    plot = SciQLopPlot()
    with pytest.warns(DeprecationWarning):
        text = SciQLopTextItem(plot, "t", QPointF(10, 10))
    qcp = next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))
    seen = [record(text.selection_changed), record(text.visible_changed), record(text.color_changed)]
    read = []

    def set_everything(*_):  # the item is still alive here, its NeoQCP item is not
        text.set_selected(True)
        text.set_visible(True)
        text.set_color(QColor("red"))
        read.append((text.selected(), text.visible(), text.color().isValid()))

    qcp.destroyed.connect(set_everything)
    plot.deleteLater()
    _flush_deferred_delete()
    assert (read, seen) == ([(False, False, False)], [[], [], []])
