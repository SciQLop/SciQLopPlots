"""A signal connected to a range setter keeps working after the receiver's
Python wrapper is garbage-collected.

0.50.0 replaced set_range & co. with Python wrappers (for date inputs). connect()
then saw a Python bound method, which PySide holds weakly, instead of the C++
slot: dropping the wrapper silently cut the connection. SciQLop hit it with
panel.time_range_changed.connect(graph.set_range) on a graph nobody kept.
"""
import gc

import numpy as np

from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotRange
from conftest import process_events


def _settle(qtbot):
    gc.collect()
    for _ in range(5):
        process_events()
    qtbot.wait(50)


def test_panel_time_range_drives_an_unreferenced_graph(qtbot):
    panel = SciQLopMultiPlotPanel(synchronize_time=True)
    qtbot.addWidget(panel)
    calls = []

    def fetch(start, stop):
        calls.append((start, stop))
        x = np.linspace(start, stop, 10)
        return x, np.zeros_like(x)

    _, graph = panel.plot(fetch)
    panel.time_range_changed.connect(graph.set_range)
    del graph
    _settle(qtbot)
    calls.clear()

    panel.set_time_axis_range(SciQLopPlotRange(300.0, 400.0))
    qtbot.waitUntil(lambda: (300.0, 400.0) in calls, timeout=3000)


def test_axis_link_survives_the_receiver_wrapper(qtbot):
    a, b = SciQLopPlot(), SciQLopPlot()
    qtbot.addWidget(a)
    qtbot.addWidget(b)
    a.x_axis().range_changed.connect(b.x_axis().set_range)
    _settle(qtbot)

    a.x_axis().set_range(SciQLopPlotRange(12.0, 13.0))
    process_events()
    r = b.x_axis().range()
    assert (r.start(), r.stop()) == (12.0, 13.0)


def _shadowed_meta_methods():
    """Qt slots and signals whose Python attribute is no longer the compiled method."""
    import inspect
    from PySide6.QtCore import QMetaMethod, QObject
    import SciQLopPlots.SciQLopPlotsBindings as bindings

    kinds = (QMetaMethod.MethodType.Slot, QMetaMethod.MethodType.Method, QMetaMethod.MethodType.Signal)
    for cls in [c for c in vars(bindings).values() if isinstance(c, type) and issubclass(c, QObject)]:
        meta = cls.staticMetaObject
        for i in range(meta.methodCount()):
            method = meta.method(i)
            if method.methodType() not in kinds:
                continue
            name = bytes(method.name()).decode()
            attr = inspect.getattr_static(cls, name, None)
            if inspect.isfunction(attr) or isinstance(attr, property):
                yield f"{cls.__name__}.{name}"


def test_no_python_wrapper_replaces_a_qt_slot():
    """Every wrapper of a slot reopens this bug: do the conversion in bindings.xml instead."""
    assert sorted(set(_shadowed_meta_methods())) == []
