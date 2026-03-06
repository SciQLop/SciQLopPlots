import pytest
from event import Event


class FakeSource:
    pass


class FakeRange:
    def __init__(self, start, stop):
        self.start = start
        self.stop = stop


def test_event_construction():
    src = FakeSource()
    ev = Event(source=src, value=42.0, property_name="range")
    assert ev.source is src
    assert ev.value == 42.0
    assert ev.property_name == "range"


def test_event_range_convenience():
    src = FakeSource()
    r = FakeRange(1.0, 2.0)
    ev = Event(source=src, value=r, property_name="range")
    assert ev.range is r


def test_event_range_returns_none_for_non_range():
    src = FakeSource()
    ev = Event(source=src, value="hello", property_name="tooltip")
    assert ev.range is None


def test_event_data_convenience():
    src = FakeSource()
    import numpy as np
    arrays = [np.array([1, 2, 3]), np.array([4, 5, 6])]
    ev = Event(source=src, value=arrays, property_name="data")
    assert ev.data is arrays


def test_event_data_returns_none_for_non_data():
    src = FakeSource()
    ev = Event(source=src, value=42.0, property_name="range")
    assert ev.data is None
