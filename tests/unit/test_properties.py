import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import ObservableProperty, OnNamespace, OnDescriptor, register_property, _property_registry


class FakeSignal:
    def __init__(self):
        self._slots = []
    def connect(self, slot):
        self._slots.append(slot)
    def disconnect(self, slot):
        self._slots.remove(slot)
    def emit(self, *args):
        for slot in self._slots:
            slot(*args)


class FakeQObject:
    def __init__(self):
        self.range_changed = FakeSignal()
        self._range_val = 42.0
        self.destroyed = FakeSignal()

    def range(self):
        return self._range_val

    def set_range(self, val):
        self._range_val = val


def setup_module():
    _property_registry.clear()
    register_property(
        FakeQObject, "range",
        signal_name="range_changed",
        getter_name="range",
        setter_name="set_range",
        property_type="range",
    )


def test_register_and_lookup():
    assert "range" in _property_registry[FakeQObject]


def test_observable_property_creation():
    obj = FakeQObject()
    ns = OnNamespace(obj)
    prop = ns.range
    assert isinstance(prop, ObservableProperty)
    assert prop.qobject is obj
    assert prop.property_name == "range"
    assert prop.property_type == "range"


def test_on_namespace_unknown_property_raises():
    obj = FakeQObject()
    ns = OnNamespace(obj)
    with pytest.raises(AttributeError, match="no observable property 'nonexistent'"):
        ns.nonexistent


def test_observable_property_get_value():
    obj = FakeQObject()
    ns = OnNamespace(obj)
    prop = ns.range
    assert prop.value == 42.0


def test_observable_property_set_value():
    obj = FakeQObject()
    ns = OnNamespace(obj)
    prop = ns.range
    prop.value = 99.0
    assert obj._range_val == 99.0


def test_on_namespace_with_no_signal():
    register_property(
        FakeQObject, "tooltip",
        signal_name=None,
        getter_name=None,
        setter_name="set_range",
        property_type="string",
    )
    obj = FakeQObject()
    ns = OnNamespace(obj)
    prop = ns.tooltip
    assert prop.signal is None


def test_on_descriptor():
    register_property(
        FakeQObject, "range",
        signal_name="range_changed",
        getter_name="range",
        setter_name="set_range",
        property_type="range",
    )
    FakeQObject.on = OnDescriptor()
    obj = FakeQObject()
    assert isinstance(obj.on, OnNamespace)
    prop = obj.on.range
    assert isinstance(prop, ObservableProperty)
