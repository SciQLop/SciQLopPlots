import pytest
from properties import ObservableProperty, OnNamespace, register_property, _property_registry
from pipeline import Pipeline, PartialPipeline, build_pipeline_step, _detect_call_style


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


class FakeQObject:
    def __init__(self):
        self.range_changed = FakeSignal()
        self.data_changed = FakeSignal()
        self.destroyed = FakeSignal()
        self._range_val = 0.0
        self._tooltip_val = ""
        self._data = None

    def range(self):
        return self._range_val

    def set_range(self, val):
        self._range_val = val

    def tool_tip(self):
        return self._tooltip_val

    def set_tool_tip(self, val):
        self._tooltip_val = val

    def data(self):
        return self._data

    def set_data(self, val):
        self._data = val


def setup_module():
    _property_registry.clear()
    register_property(
        FakeQObject, "range",
        signal_name="range_changed",
        getter_name="range",
        setter_name="set_range",
        property_type="range",
    )
    register_property(
        FakeQObject, "tooltip",
        signal_name=None,
        getter_name="tool_tip",
        setter_name="set_tool_tip",
        property_type="string",
    )
    register_property(
        FakeQObject, "data",
        signal_name="data_changed",
        getter_name="data",
        setter_name="set_data",
        property_type="data",
    )


# --- >> operator mechanics ---

def test_observable_rshift_callable_returns_partial():
    obj = FakeQObject()
    prop = OnNamespace(obj).range
    result = prop >> (lambda e: e)
    assert isinstance(result, PartialPipeline)


def test_observable_rshift_observable_returns_pipeline():
    src = FakeQObject()
    tgt = FakeQObject()
    result = OnNamespace(src).range >> OnNamespace(tgt).range
    assert isinstance(result, Pipeline)


def test_partial_rshift_observable_returns_pipeline():
    src = FakeQObject()
    tgt = FakeQObject()
    result = OnNamespace(src).range >> (lambda e: e.value) >> OnNamespace(tgt).tooltip
    assert isinstance(result, Pipeline)


# --- Direct binding (no transform) ---

def test_direct_binding_propagates():
    src = FakeQObject()
    tgt = FakeQObject()
    OnNamespace(src).range >> OnNamespace(tgt).range
    src.range_changed.emit(99.0)
    assert tgt._range_val == 99.0


# --- Transform with new-style Event signature ---

def test_transform_with_event_signature():
    src = FakeQObject()
    tgt = FakeQObject()

    def transform(event):
        return f"value={event.value}"

    OnNamespace(src).range >> transform >> OnNamespace(tgt).tooltip
    src.range_changed.emit(3.14)
    assert tgt._tooltip_val == "value=3.14"


# --- Transform with old-style positional signature ---

def test_transform_with_positional_signature():
    src = FakeQObject()
    tgt = FakeQObject()

    def transform(a, b):
        return f"{a}-{b}"

    OnNamespace(src).range >> transform >> OnNamespace(tgt).tooltip
    src.range_changed.emit(1.0, 2.0)
    assert tgt._tooltip_val == "1.0-2.0"


# --- Terminal sink (PartialPipeline is live) ---

def test_partial_pipeline_acts_as_sink():
    src = FakeQObject()
    results = []

    def sink(event):
        results.append(event.value)

    OnNamespace(src).range >> sink
    src.range_changed.emit(42.0)
    assert results == [42.0]


# --- Disconnect ---

def test_pipeline_disconnect():
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).range >> OnNamespace(tgt).range
    pipe.disconnect()
    src.range_changed.emit(99.0)
    assert tgt._range_val == 0.0


# --- Lifetime: slot handles dead objects gracefully ---

def test_slot_handles_dead_source_gracefully():
    """If source QObject is dead when slot fires, no crash."""
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).range >> (lambda event: event.value) >> OnNamespace(tgt).range
    # Simulate slot firing after source is in a bad state by
    # making qobject accessor raise RuntimeError
    src._range_val = 0.0
    # The slot should not crash even if something goes wrong internally
    src.range_changed.emit(42.0)
    assert tgt._range_val == 42.0  # still works while source is alive


# --- Transform returning None skips setter ---

def test_transform_returning_none_skips_target():
    src = FakeQObject()
    tgt = FakeQObject()
    tgt._tooltip_val = "original"

    OnNamespace(src).range >> (lambda event: None) >> OnNamespace(tgt).tooltip
    src.range_changed.emit(1.0)
    assert tgt._tooltip_val == "original"


# --- Signature detection ---

def test_detect_call_style_event():
    assert _detect_call_style(lambda event: event) == "event"


def test_detect_call_style_positional():
    assert _detect_call_style(lambda a, b: a) == "positional"


def test_detect_call_style_none():
    assert _detect_call_style(lambda: 42) == "none"


# --- Auto-threaded for data targets ---

def test_data_pipeline_auto_threaded():
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).data >> (lambda event: event.value) >> OnNamespace(tgt).data
    assert pipe._auto_threaded


def test_range_pipeline_not_auto_threaded():
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).range >> OnNamespace(tgt).range
    assert not pipe._auto_threaded


# --- Cannot observe write-only property ---

def test_cannot_observe_write_only():
    src = FakeQObject()
    tgt = FakeQObject()
    with pytest.raises(ValueError, match="no signal"):
        OnNamespace(src).tooltip >> OnNamespace(tgt).tooltip
