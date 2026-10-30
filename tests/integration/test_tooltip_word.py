"""API review 1.10: one word, tooltip: tooltip() / set_tooltip() / tooltip_changed / tooltip=."""
import inspect
import warnings

import numpy as np
import pytest
from PySide6.QtCore import QMetaMethod, QPointF, QRectF
from PySide6.QtGui import QPixmap
from shibokensupport.signature import get_signature

import SciQLopPlots
import SciQLopPlots.SciQLopPlotsBindings as bindings
from SciQLopPlots import SciQLopPlotRange
from conftest import record

R = SciQLopPlotRange
X = np.linspace(0.0, 10.0, 20)
ITEMS = {
    "text": lambda p: p.text("t", QPointF(10, 10)),
    "ellipse": lambda p: p.ellipse(QRectF(10, 10, 20, 20)),
    "pixmap": lambda p: p.pixmap(QPixmap(4, 4), QRectF(1, 1, 2, 2)),
    "curved line": lambda p: p.curved_line(QPointF(0, 0), QPointF(5, 5)),
    "span": lambda p: p.span(R(1.0, 2.0)),
    "line": lambda p: p.vertical_line(1.0),
}


def _silently(call):
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        return call()


@pytest.mark.parametrize("kind", ITEMS)
def test_the_tooltip_round_trips_silently(plot, kind):
    item = ITEMS[kind](plot)
    _silently(lambda: item.set_tooltip("hello"))
    assert _silently(item.tooltip) == "hello"


@pytest.mark.parametrize("kind", ITEMS)
def test_the_old_methods_warn_and_still_work(plot, kind):
    item = ITEMS[kind](plot)
    with pytest.warns(DeprecationWarning, match=r"use item\.set_tooltip\("):
        item.set_tool_tip("old")
    with pytest.warns(DeprecationWarning, match=r"use item\.tooltip\("):
        assert item.tool_tip() == "old"


@pytest.mark.parametrize("kind", ["span", "line"])
def test_tooltip_changed_signals_and_is_observable(plot, kind):
    item = ITEMS[kind](plot)
    seen = record(item.tooltip_changed)
    item.on.tooltip.value = "via on"
    assert (item.tooltip(), seen) == ("via on", ["via on"])


@pytest.mark.parametrize("kind", ITEMS)
def test_on_does_not_offer_the_old_spelling(plot, kind):
    with pytest.raises(AttributeError):
        ITEMS[kind](plot).on.tool_tip


FACTORIES = {
    "plot.span": lambda h, kw: h.plot.span(R(1.0, 2.0), **kw),
    "plot.vertical_line": lambda h, kw: h.plot.vertical_line(1.0, **kw),
    "plot.ellipse": lambda h, kw: h.plot.ellipse(QRectF(1, 1, 2, 2), **kw),
    "plot.curved_line": lambda h, kw: h.plot.curved_line(QPointF(0, 0), QPointF(5, 5), **kw),
    "panel.vertical_span": lambda h, kw: h.panel.vertical_span(R(1.0, 2.0), **kw),
}


class Hosts:
    def __init__(self, plot, panel):
        self.plot, self.panel = plot, panel


@pytest.mark.parametrize("factory", FACTORIES)
def test_factories_take_tooltip(plot, panel, factory):
    panel.plot(X, X)
    item = _silently(lambda: FACTORIES[factory](Hosts(plot, panel), {"tooltip": "new"}))
    assert item.tooltip() == "new"


@pytest.mark.parametrize("factory", FACTORIES)
def test_factories_still_take_tool_tip_with_a_warning_at_the_caller(plot, panel, factory):
    panel.plot(X, X)
    receiver = factory.split(".")[0]
    with pytest.warns(DeprecationWarning, match=rf"use {receiver}\.\w+\(\.\.\., tooltip=") as seen:
        item = FACTORIES[factory](Hosts(plot, panel), {"tool_tip": "old"})
    assert item.tooltip() == "old" and seen[0].filename == __file__


def _parameter_names(method):
    signatures = getattr(method, "__signature__", None) or get_signature(method)
    signatures = signatures if isinstance(signatures, list) else [signatures]
    return {name for sig in signatures if isinstance(sig, inspect.Signature) for name in sig.parameters}


def _slot_names(cls):
    meta = cls.staticMetaObject
    return {bytes(meta.method(i).name()).decode() for i in range(meta.methodCount())
            if meta.method(i).methodType() == QMetaMethod.MethodType.Slot}


def _methods_taking_a_tooltip():
    accessors = {"tooltip", "set_tooltip", "tool_tip", "set_tool_tip"}
    for _, cls in inspect.getmembers(bindings, inspect.isclass):
        slots = _slot_names(cls) if hasattr(cls, "staticMetaObject") else set()
        for name, method in vars(cls).items():
            if (not name.startswith("_") and callable(method) and not inspect.isclass(method)
                    and name not in slots | accessors and "tooltip" in _parameter_names(method)):
                yield cls, name


def test_every_method_taking_a_tooltip_takes_tool_tip_too():
    listed = {(cls, name) for cls, names in SciQLopPlots._TOOLTIP_KEYWORD_METHODS.items() for name in names}
    assert set(_methods_taking_a_tooltip()) == listed
