"""API review 1.10: items say read_only like spans and lines; movable warns and maps the inverse."""
import warnings

import pytest
from PySide6.QtCore import QPointF, QRectF
from PySide6.QtGui import QColor, QPixmap

from conftest import off_thread, record

FACTORIES = {
    "text": lambda p, *a, **kw: p.text("t", QPointF(10, 10), *a, **kw),
    "pixmap": lambda p, *a, **kw: p.pixmap(QPixmap(4, 4), QRectF(1, 1, 2, 2), *a, **kw),
    "ellipse": lambda p, *a, **kw: p.ellipse(QRectF(10, 10, 20, 20), *a, **kw),
    "styled ellipse": lambda p, *a, **kw: p.ellipse(QRectF(10, 10, 20, 20), QColor("red"), 2.0,
                                                    QColor("blue"), *a, **kw),
}


def _silently(call):
    with warnings.catch_warnings():
        warnings.simplefilter("error", DeprecationWarning)
        return call()


@pytest.mark.parametrize("kind", FACTORIES)
def test_an_item_is_read_only_by_default(plot, kind):
    assert _silently(lambda: FACTORIES[kind](plot).read_only())


@pytest.mark.parametrize("kind", FACTORIES)
def test_the_factory_takes_read_only(plot, kind):
    item = _silently(lambda: FACTORIES[kind](plot, read_only=False))
    assert not item.read_only()


@pytest.mark.parametrize("kind", FACTORIES)
def test_read_only_round_trips_and_signals_once(plot, kind):
    item = FACTORIES[kind](plot)
    seen = record(item.read_only_changed)
    item.set_read_only(False)
    item.set_read_only(False)
    assert (item.read_only(), seen) == (False, [False])


@pytest.mark.parametrize("kind", FACTORIES)
def test_read_only_is_observable(plot, kind):
    item = FACTORIES[kind](plot)
    item.on.read_only.value = False
    assert not item.read_only()
    with pytest.raises(AttributeError):
        item.on.movable


def test_set_read_only_from_a_worker_lands_on_the_gui_thread(qtbot, plot):
    item = FACTORIES["text"](plot)
    off_thread(lambda: item.set_read_only(False))
    qtbot.waitUntil(lambda: not item.read_only(), timeout=2000)


@pytest.mark.parametrize("kind", FACTORIES)
def test_the_old_methods_warn_and_map_the_inverse(plot, kind):
    item = FACTORIES[kind](plot)
    with pytest.warns(DeprecationWarning, match=r"use item\.set_read_only\(") as seen:
        item.set_movable(True)
    assert not item.read_only() and seen[0].filename == __file__
    with pytest.warns(DeprecationWarning, match=r"use item\.read_only\("):
        assert item.movable()


@pytest.mark.parametrize("kind", FACTORIES)
def test_the_movable_keyword_warns_and_maps_the_inverse(plot, kind):
    with pytest.warns(DeprecationWarning, match=r"use plot\.\w+\(\.\.\., read_only=") as seen:
        item = FACTORIES[kind](plot, movable=True)
    assert not item.read_only() and seen[0].filename == __file__


@pytest.mark.parametrize("kind", FACTORIES)
def test_a_positional_bool_is_refused_not_flipped(plot, kind):
    with pytest.raises(TypeError, match="read_only="):
        FACTORIES[kind](plot, True)


def test_the_deprecated_setter_still_signals(plot):
    item = FACTORIES["text"](plot)
    seen = record(item.read_only_changed)
    with pytest.warns(DeprecationWarning):
        item.set_movable(True)
    assert seen == [False]


def test_the_new_tooltip_setter_is_the_native_attribute():
    from SciQLopPlots import SciQLopSpan
    assert not hasattr(SciQLopSpan.set_tooltip, "__wrapped__")
