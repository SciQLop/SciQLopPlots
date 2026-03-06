"""Tests that the property registration and .on patching mechanism works.

Uses mock classes since compiled bindings are not available in unit tests.
"""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import OnNamespace, OnDescriptor, register_property, _property_registry, ObservableProperty


class FakeSignal:
    def __init__(self):
        self._slots = []
    def connect(self, slot):
        self._slots.append(slot)
    def disconnect(self, slot=None):
        if slot is None:
            self._slots.clear()
        else:
            self._slots.remove(slot)
    def emit(self, *args):
        for slot in list(self._slots):
            slot(*args)


class MockGraph:
    def __init__(self):
        self.data_changed = FakeSignal()
        self.destroyed = FakeSignal()
        self._data = None

    def data(self):
        return self._data

    def set_data(self, val):
        self._data = val


class MockAxis:
    def __init__(self):
        self.range_changed = FakeSignal()
        self.destroyed = FakeSignal()
        self._range = 0.0

    def range(self):
        return self._range

    def set_range(self, val):
        self._range = val


class MockSpan:
    def __init__(self):
        self.range_changed = FakeSignal()
        self.destroyed = FakeSignal()
        self._range = 0.0
        self._tooltip = ""

    def range(self):
        return self._range

    def set_range(self, val):
        self._range = val

    def tool_tip(self):
        return self._tooltip

    def set_tool_tip(self, val):
        self._tooltip = val


def setup_module():
    _property_registry.clear()

    register_property(MockGraph, "data",
        signal_name="data_changed", getter_name="data",
        setter_name="set_data", property_type="data")

    register_property(MockAxis, "range",
        signal_name="range_changed", getter_name="range",
        setter_name="set_range", property_type="range")

    register_property(MockSpan, "range",
        signal_name="range_changed", getter_name="range",
        setter_name="set_range", property_type="range")

    register_property(MockSpan, "tooltip",
        signal_name=None, getter_name="tool_tip",
        setter_name="set_tool_tip", property_type="string")

    for cls in (MockGraph, MockAxis, MockSpan):
        cls.on = OnDescriptor()


def test_on_descriptor_returns_namespace():
    obj = MockGraph()
    assert isinstance(obj.on, OnNamespace)


def test_graph_data_property():
    obj = MockGraph()
    prop = obj.on.data
    assert isinstance(prop, ObservableProperty)
    assert prop.property_type == "data"


def test_axis_range_property():
    obj = MockAxis()
    prop = obj.on.range
    assert isinstance(prop, ObservableProperty)
    assert prop.property_type == "range"


def test_span_range_property():
    obj = MockSpan()
    prop = obj.on.range
    assert prop.property_type == "range"


def test_span_tooltip_property():
    obj = MockSpan()
    prop = obj.on.tooltip
    assert prop.property_type == "string"
    assert prop.signal is None


def test_full_pipeline_span_range_to_tooltip():
    """End-to-end: span.on.range >> transform >> span.on.tooltip"""
    span = MockSpan()

    def stats(event):
        return f"range={event.value}"

    span.on.range >> stats >> span.on.tooltip
    span.range_changed.emit(42.0)
    assert span._tooltip == "range=42.0"


def test_full_pipeline_axis_to_axis():
    """End-to-end: axis1.on.range >> axis2.on.range"""
    a1 = MockAxis()
    a2 = MockAxis()
    a1.on.range >> a2.on.range
    a1.range_changed.emit(99.0)
    assert a2._range == 99.0
