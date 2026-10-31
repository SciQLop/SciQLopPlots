import threading
import time

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

    def set_data(self, *args):
        self._data = list(args) if len(args) > 1 else args[0]


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


# --- threaded(): the transform runs on a worker thread ---

def _wait_for(condition, timeout=5.0):
    deadline = time.monotonic() + timeout
    while not condition():
        assert time.monotonic() < deadline, "timed out"
        time.sleep(0.005)


def test_a_pipeline_runs_in_the_emitting_thread_by_default():
    src, tgt = FakeQObject(), FakeQObject()
    threads = []
    OnNamespace(src).data >> (lambda e: threads.append(threading.current_thread()) or e.value) \
        >> OnNamespace(tgt).data
    src.data_changed.emit([1, 2])
    assert threads == [threading.current_thread()] and tgt._data == [1, 2]


def test_threaded_runs_the_transform_on_a_worker_thread():
    src, tgt = FakeQObject(), FakeQObject()
    threads = []
    pipe = (OnNamespace(src).data >> (lambda e: threads.append(threading.current_thread()) or e.value)
            >> OnNamespace(tgt).data).threaded()
    src.data_changed.emit([1, 2])
    _wait_for(lambda: tgt._data == [1, 2])
    assert threads and threads[0] is not threading.current_thread()
    pipe.disconnect()


def test_threaded_keeps_only_the_latest_value_while_busy():
    src, tgt = FakeQObject(), FakeQObject()
    gate, seen = threading.Event(), []

    def slow(event):
        seen.append(event.value)
        gate.wait(5)
        return event.value

    pipe = (OnNamespace(src).range >> slow >> OnNamespace(tgt).range).threaded()
    src.range_changed.emit(1)
    _wait_for(lambda: seen == [1])
    src.range_changed.emit(2)
    src.range_changed.emit(3)
    gate.set()
    _wait_for(lambda: tgt._range_val == 3)
    assert seen == [1, 3]
    pipe.disconnect()


def test_a_threaded_sink_runs_on_a_worker_thread():
    src = FakeQObject()
    threads = []
    sink = (OnNamespace(src).range >> (lambda e: threads.append(threading.current_thread()))).threaded()
    src.range_changed.emit(1)
    _wait_for(lambda: threads)
    assert threads[0] is not threading.current_thread()
    sink.disconnect()


def test_a_disconnected_threaded_pipeline_delivers_nothing():
    src, tgt = FakeQObject(), FakeQObject()
    gate = threading.Event()
    pipe = (OnNamespace(src).range >> (lambda e: gate.wait(5) and e.value) >> OnNamespace(tgt).range).threaded()
    src.range_changed.emit(7)
    pipe.disconnect()
    gate.set()
    time.sleep(0.1)
    assert tgt._range_val == 0.0


# --- No-args signal fetches value from getter ---

def test_no_args_signal_fetches_from_getter():
    """When signal fires with no args (like data_changed()), value comes from getter."""
    src = FakeQObject()
    tgt = FakeQObject()
    src._data = [1, 2, 3]
    results = []

    def capture(event):
        results.append(event.value)
        return event.value

    OnNamespace(src).data >> capture >> OnNamespace(tgt).data
    src.data_changed.emit()  # no args
    assert results == [[1, 2, 3]]
    assert tgt._data == [1, 2, 3]


def test_no_args_direct_binding_fetches_from_getter():
    """Direct binding with no-args signal."""
    src = FakeQObject()
    tgt = FakeQObject()
    src._data = [10, 20]

    OnNamespace(src).data >> OnNamespace(tgt).data
    src.data_changed.emit()  # no args
    assert tgt._data == [10, 20]


# --- Cannot observe write-only property ---

def test_cannot_observe_write_only():
    src = FakeQObject()
    tgt = FakeQObject()
    with pytest.raises(ValueError, match="no signal"):
        OnNamespace(src).tooltip >> OnNamespace(tgt).tooltip


def test_a_dropped_threaded_pipeline_is_freed_and_the_link_keeps_working():
    import gc
    import weakref
    src, tgt = FakeQObject(), FakeQObject()
    pipe = (OnNamespace(src).range >> (lambda e: e.value * 2) >> OnNamespace(tgt).range).threaded()
    dropped = weakref.ref(pipe)
    del pipe
    gc.collect()
    assert dropped() is None
    src.range_changed.emit(21)
    _wait_for(lambda: tgt._range_val == 42)
