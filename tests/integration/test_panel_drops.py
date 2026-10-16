"""Drops on a panel. The drop placeholder was a plot to the panel (palette and time axis
synchronised, abstract methods warning on every drag event), and a drop on the splitter
handle between two plots was accepted but reached no callback."""
from PySide6.QtCore import QEvent, QMimeData, QPoint, Qt
from PySide6.QtGui import QDragEnterEvent, QDragMoveEvent, QDropEvent
from PySide6.QtWidgets import QApplication
from SciQLopPlots import PlotDragNDropCallback, PlotType
from conftest import process_events

MIME = "application/x-test-product"


class Recorder(PlotDragNDropCallback):
    def __init__(self, parent):
        super().__init__(MIME, True, parent)
        self.drops = []

    def call(self, plot, mime_data):
        self.drops.append((plot, mime_data.data(MIME).data().decode()))


def _panel_with_two_plots(panel):
    first, second = (panel.create_plot(-1, PlotType.TimeSeries) for _ in range(2))
    recorder = Recorder(panel)
    panel.add_accepted_mime_type(recorder)
    panel.resize(600, 400)
    panel.show()
    process_events()
    return first, second, recorder


def _send(panel, kind, pos, mime):
    args = (Qt.DropAction.CopyAction, mime, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier)
    event = {"enter": lambda: QDragEnterEvent(pos, *args), "move": lambda: QDragMoveEvent(pos, *args),
             "drop": lambda: QDropEvent(pos.toPointF(), *args)}[kind]()
    QApplication.sendEvent(panel.viewport(), event)
    process_events()
    return event


def _mime():
    mime = QMimeData()
    mime.setData(MIME, b"amda/b_gse")
    return mime


def _in_viewport(panel, widget, y):
    return widget.mapTo(panel.viewport(), QPoint(widget.width() // 2, y))


def test_the_drop_placeholder_is_not_a_plot(panel):
    first, second, _ = _panel_with_two_plots(panel)
    mime = _mime()
    _send(panel, "enter", _in_viewport(panel, second, second.height() - 3), mime)
    assert len(panel.child_widgets()) == 3          # the placeholder is there...
    assert panel.plot_count() == 2                  # ...but it is not a plot


def test_a_drop_on_the_handle_between_plots_is_refused(panel):
    first, second, recorder = _panel_with_two_plots(panel)
    mime = _mime()
    _send(panel, "enter", _in_viewport(panel, first, first.height() // 2), mime)
    handle = _in_viewport(panel, first, first.height()) + QPoint(0, 1)
    assert panel.widget_at(handle.toPointF()) is None
    assert not _send(panel, "move", handle, mime).isAccepted()
    assert not _send(panel, "drop", handle, mime).isAccepted()
    assert recorder.drops == []


def test_drops_onto_a_plot_and_at_an_edge_still_arrive(panel):
    first, second, recorder = _panel_with_two_plots(panel)
    mime = _mime()
    middle = _in_viewport(panel, first, first.height() // 2)
    for kind in ("enter", "move", "drop"):
        _send(panel, kind, middle, mime)
    edge = _in_viewport(panel, second, second.height() - 3)
    for kind in ("enter", "move"):
        _send(panel, kind, edge, mime)
    assert _send(panel, "drop", edge, mime).isAccepted()
    assert [(plot is first, data) for plot, data in recorder.drops[:1]] == [(True, "amda/b_gse")]
    assert len(recorder.drops) == 2 and recorder.drops[1][0] not in (first, second)
    assert panel.plot_count() == 3


def test_a_new_callback_for_a_mime_type_replaces_the_old_one(panel):
    import shiboken6
    first = Recorder(panel)
    panel.add_accepted_mime_type(first)
    second = Recorder(panel)
    panel.add_accepted_mime_type(second)
    panel.add_accepted_mime_type(second)   # again: kept, not deleted
    QApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    assert not shiboken6.isValid(first)
    assert shiboken6.isValid(second)
