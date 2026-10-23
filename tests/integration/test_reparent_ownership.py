"""setParent and Qt layouts move ownership with the Qt parent (widget-ownership design, change 3)."""
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtWidgets import QSplitter, QVBoxLayout, QWidget

from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopPlot, VPlotsAlign


def test_set_parent_none_gives_a_parented_widget_back_to_python(qtbot):
    host = QWidget()
    plot = SciQLopPlot(host)
    plot.setParent(None)
    gone = on_destroyed(plot)
    assert shiboken6.ownedByPython(plot)
    del plot
    collect()
    assert gone == [True]


def test_set_parent_none_gives_a_parented_qobject_back_to_python(qtbot):
    host = QWidget()
    align = VPlotsAlign(host)
    align.setParent(None)
    gone = on_destroyed(align)
    del align
    collect()
    assert gone == [True]


def test_set_parent_gives_a_widget_to_its_new_parent(qtbot):
    host = QWidget()
    plot = SciQLopPlot()
    plot.setObjectName("probe")
    plot.setParent(host)
    gone = on_destroyed(plot)
    del plot
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]


def _moved_by(first, second, add):
    plot = SciQLopPlot(first)
    plot.setObjectName("probe")
    add(plot)
    gone = on_destroyed(plot)
    del plot
    shiboken6.delete(first)
    collect()
    assert (gone, second.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(second)
    assert gone == [True]


def test_a_layout_moves_a_widget_to_its_host(qtbot):
    first, second = QWidget(), QWidget()
    layout = QVBoxLayout(second)
    _moved_by(first, second, layout.addWidget)


def test_a_splitter_moves_a_widget_to_itself(qtbot):
    first, second = QWidget(), QSplitter()
    _moved_by(first, second, second.addWidget)
