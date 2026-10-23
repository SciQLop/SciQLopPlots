"""SciQLopDrawnObject's rules, on Python subclasses (docs/plans/2026-10-22-drawn-object-base-design.md)."""
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import SciQLopDrawnObject
from conftest import off_thread, record


class Lamp(SciQLopDrawnObject):
    """Supports visible only, like a family overriding one pair of hooks."""

    def __init__(self):
        super().__init__()
        self.on = True
        self.applied = []

    def _visible(self):
        return self.on

    def _set_visible(self, visible):
        self.applied.append(visible)
        self.on = visible


class Dimmer(Lamp):
    """Overrides the public setter too, the way a user subclass might."""

    def __init__(self):
        super().__init__()
        self.calls = []

    def set_visible(self, visible):
        self.calls.append(visible)
        super().set_visible(visible)


class Pair(SciQLopDrawnObject):
    """An aggregate: visible while any part is, and a set reaches every part."""

    def __init__(self):
        super().__init__()
        self.parts = [Lamp(), Lamp()]
        for part in self.parts:
            part.visible_changed.connect(lambda _: self._notify_visible(self._visible()))
        self._seed_visible(self._visible())

    def _always_apply(self):
        return True

    def _visible(self):
        return any(p.visible() for p in self.parts)

    def _set_visible(self, visible):
        for part in self.parts:
            part.set_visible(visible)


def test_a_change_applies_once_and_signals_once(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(False)
    assert (lamp.visible(), lamp.applied, seen) == (False, [False], [False])


def test_the_same_value_neither_applies_nor_signals(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(True)
    assert (lamp.applied, seen) == ([], [])


def test_a_change_behind_the_setter_does_not_swallow_the_next_signal(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(False)
    lamp.on = True  # changed without the setter: no signal, the last emitted value is stale
    lamp.set_visible(False)
    assert seen == [False, False]


def test_notify_drops_repeats_and_seed_records_silently(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp._seed_visible(True)
    lamp._notify_visible(True)
    lamp._notify_visible(False)
    lamp._notify_visible(False)
    assert seen == [False]


def test_a_set_from_a_worker_thread_waits_for_the_gui_thread(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    off_thread(lambda: lamp.set_visible(False))
    assert (lamp.applied, seen) == ([], [])
    qtbot.waitUntil(lambda: seen == [False], timeout=2000)
    assert lamp.applied == [False]


def test_a_python_override_of_set_visible_runs_once_from_a_worker_thread(qtbot):
    dimmer = Dimmer()
    seen = record(dimmer.visible_changed)
    off_thread(lambda: dimmer.set_visible(False))
    qtbot.waitUntil(lambda: seen == [False], timeout=2000)
    assert (dimmer.calls, dimmer.applied) == ([False], [False])


@pytest.mark.parametrize("call", [
    lambda o: o.selected(), lambda o: o.set_selected(True),
    lambda o: o.color(), lambda o: o.set_color(QColor("red")),
], ids=["selected", "set_selected", "color", "set_color"])
def test_an_unsupported_attribute_raises_naming_the_class(qtbot, call):
    with pytest.raises(NotImplementedError, match="SciQLopDrawnObject"):
        call(Lamp())


@pytest.mark.parametrize("call", [
    lambda o: o._selected(), lambda o: o._set_selected(True),
    lambda o: o._color(), lambda o: o._set_color(QColor("red")),
    lambda o: super(Lamp, o)._visible(), lambda o: super(Lamp, o)._set_visible(False),
], ids=["_selected", "_set_selected", "_color", "_set_color", "super()._visible",
        "super()._set_visible"])
def test_an_unsupported_hook_called_from_python_raises_not_implemented(qtbot, call):
    with pytest.raises(NotImplementedError, match="SciQLopDrawnObject"):
        call(Lamp())


def test_an_unsupported_set_from_a_worker_thread_only_warns(qtbot, qtlog):
    lamp = Lamp()
    off_thread(lambda: lamp.set_selected(True))
    qtbot.wait(50)  # the queued call runs here: a throw escaping it would std::terminate
    assert any("set_selected is not supported" in r.message for r in qtlog.records)
    assert lamp.visible()


def test_set_name_is_the_object_name_and_signals_once(qtbot):
    lamp = Lamp()
    seen = record(lamp.name_changed)
    lamp.set_name("desk")
    lamp.set_name("desk")
    assert (lamp.name(), lamp.objectName(), seen) == ("desk", "desk", ["desk"])


def test_an_aggregate_shows_a_hidden_part_without_signalling(qtbot):
    pair = Pair()
    pair.parts[0].set_visible(False)
    seen = record(pair.visible_changed)
    pair.set_visible(True)
    assert (pair.parts[0].visible(), seen) == (True, [])


def test_an_aggregate_signals_once_when_its_last_part_hides(qtbot):
    pair = Pair()
    seen = record(pair.visible_changed)
    pair.set_visible(False)
    assert seen == [False]
