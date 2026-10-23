"""Widgets follow PySide's ownership rules: ownership follows the Qt parent.

docs/plans/2026-10-23-widget-ownership-design.md. Built with a parent, a widget lives as long
as the parent, whatever happens to the Python reference. Built without one, it belongs to its
Python object.
"""
import threading

import pytest
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtGui import QColor, QFont
from PySide6.QtWidgets import QWidget

from conftest import collect, on_destroyed
from SciQLopPlots import (FontDelegate, InspectorView, PlotsTreeView, ProductsView, PropertiesPanel,
                          PropertyDelegateBase, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopPlotInterface, SciQLopPlotPanelInterface,
                          SciQLopTimeSeriesPlot)

_KEEP = []  # objects a builder needs alive for as long as the test session


def _kept(obj):
    _KEEP.append(obj)
    return obj


WIDGETS = {
    "SciQLopPlot": lambda parent: SciQLopPlot(parent),
    "SciQLopTimeSeriesPlot": lambda parent: SciQLopTimeSeriesPlot(parent),
    "SciQLopNDProjectionPlot": lambda parent: SciQLopNDProjectionPlot(3, parent),
    "SciQLopMultiPlotPanel": lambda parent: SciQLopMultiPlotPanel(parent),
    "SciQLopMultiPlotPanel, keywords": lambda parent: SciQLopMultiPlotPanel(
        parent, synchronize_x=False, synchronize_time=True),
    "SciQLopPlotInterface": lambda parent: SciQLopPlotInterface(parent),
    "SciQLopPlotPanelInterface": lambda parent: SciQLopPlotPanelInterface(parent),
    "InspectorView": lambda parent: InspectorView(parent),
    "PlotsTreeView": lambda parent: PlotsTreeView(parent),
    "ProductsView": lambda parent: ProductsView(parent),
    "PropertiesPanel": lambda parent: PropertiesPanel(parent),
    "FontDelegate": lambda parent: FontDelegate(QFont(), QColor("red"), parent),
    "PropertyDelegateBase": lambda parent: PropertyDelegateBase(_kept(QObject()), parent),
}


def _probe(name, parent):
    widget = WIDGETS[name](parent)
    widget.setObjectName("probe")
    return widget, on_destroyed(widget)


@pytest.mark.parametrize("name", sorted(WIDGETS))
def test_a_widget_built_with_a_parent_lives_as_long_as_the_parent(qtbot, name):
    host = QWidget()
    widget, gone = _probe(name, host)
    assert (widget.parent() is host, shiboken6.ownedByPython(widget)) == (True, False)
    del widget
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]  # the parent owned it: no leak


@pytest.mark.parametrize("name", sorted(WIDGETS))
def test_a_widget_built_without_a_parent_belongs_to_python(qtbot, name):  # guard
    widget, gone = _probe(name, None)
    del widget
    collect()
    assert gone == [True]


def test_a_widget_dies_with_a_python_owned_parent_that_is_collected(qtbot):  # review focus 1
    host = QWidget()
    plot = SciQLopPlot(host)
    gone = on_destroyed(plot)
    del host
    collect()
    assert (gone, shiboken6.isValid(plot)) == ([True], False)
    with pytest.raises(RuntimeError):
        plot.objectName()


def test_a_queued_call_to_a_child_is_dropped_when_its_parent_dies_first(qtbot):  # review focus 4
    host = QWidget()
    plot = SciQLopPlot(host)
    worker = threading.Thread(target=lambda: plot.set_title("late"))
    worker.start()
    worker.join(timeout=10)
    assert not worker.is_alive()
    shiboken6.delete(host)
    collect()  # set_title would run here; Qt dropped it with its context object
    assert not shiboken6.isValid(plot)


class _Panel(SciQLopMultiPlotPanel):
    """Like SciQLop's TimeSyncPanel: a Python subclass passing its parent to the base by keyword."""

    def __init__(self, parent):
        super().__init__(parent=parent, synchronize_x=False, synchronize_time=True)


def test_a_python_subclass_built_with_a_parent_lives_as_long_as_the_parent(qtbot):
    host = QWidget()
    panel = _Panel(host)
    panel.setObjectName("probe")
    gone = on_destroyed(panel)
    assert (panel.parent() is host, shiboken6.ownedByPython(panel)) == (True, False)
    del panel
    collect()
    assert (gone, type(host.findChild(QObject, "probe"))) == ([], _Panel)
    shiboken6.delete(host)
    assert gone == [True]
