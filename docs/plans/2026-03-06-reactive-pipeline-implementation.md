# Reactive Pipeline API Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement the `source.on.property >> transform >> target.on.property` reactive pipeline API for SciQLopPlots.

**Architecture:** Pure Python implementation on top of existing C++ bindings. Three new Python files (`event.py`, `properties.py`, `pipeline.py`) plus registration in `__init__.py`. One C++ change: expose `range_changed` signal on `SciQLopVerticalSpan`/`SciQLopRangeItemInterface` to Python. Uses existing `SimplePyCallablePipeline` threading for heavy data pipelines.

**Tech Stack:** Python 3.10+, PySide6 (Qt signals/slots), Shiboken6, numpy, existing SciQLopPlots C++ bindings

**Design doc:** `docs/plans/2026-03-06-reactive-pipeline-api-design.md`

---

### Task 1: Event class

**Files:**
- Create: `SciQLopPlots/event.py`
- Create: `tests/unit/test_event.py`

**Step 1: Create test directory and write the failing test**

Create `tests/unit/` directory. Write tests for Event construction and convenience accessors.

```python
# tests/unit/test_event.py
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

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
```

**Step 2: Run test to verify it fails**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_event.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'event'`

**Step 3: Write minimal implementation**

```python
# SciQLopPlots/event.py

_RANGE_PROPERTIES = frozenset({"range"})
_DATA_PROPERTIES = frozenset({"data"})


class Event:
    """Event object passed to pipeline transform functions.

    Attributes:
        source: The QObject that emitted the signal.
        value: The raw value from the signal.
        property_name: Name of the property that changed ("range", "data", etc.).
    """
    __slots__ = ("source", "value", "property_name")

    def __init__(self, source, value, property_name):
        self.source = source
        self.value = value
        self.property_name = property_name

    @property
    def range(self):
        """Returns value if this is a range event, None otherwise."""
        if self.property_name in _RANGE_PROPERTIES:
            return self.value
        return None

    @property
    def data(self):
        """Returns value if this is a data event, None otherwise."""
        if self.property_name in _DATA_PROPERTIES:
            return self.value
        return None
```

**Step 4: Run test to verify it passes**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_event.py -v`
Expected: All 5 tests PASS

**Step 5: Commit**

```bash
git add SciQLopPlots/event.py tests/unit/test_event.py
git commit -m "feat: add Event class for reactive pipeline"
```

---

### Task 2: ObservableProperty and OnNamespace

**Files:**
- Create: `SciQLopPlots/properties.py`
- Create: `tests/unit/test_properties.py`

**Step 1: Write the failing test**

Tests for ObservableProperty construction, OnNamespace attribute access, and the property registry.

```python
# tests/unit/test_properties.py
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import ObservableProperty, OnNamespace, register_property, _property_registry


class FakeSignal:
    """Mimics a PySide6 signal with connect/disconnect."""
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
    """Write-only properties (signal_name=None) should still work."""
    _property_registry.clear()
    register_property(
        FakeQObject, "tooltip",
        signal_name=None,
        getter_name=None,
        setter_name="set_range",  # reuse for test
        property_type="string",
    )
    obj = FakeQObject()
    ns = OnNamespace(obj)
    prop = ns.tooltip
    assert prop.signal is None
```

**Step 2: Run test to verify it fails**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_properties.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'properties'`

**Step 3: Write minimal implementation**

```python
# SciQLopPlots/properties.py
import weakref

# Registry: {class: {property_name: property_spec_dict}}
_property_registry = {}


def register_property(cls, property_name, signal_name, getter_name, setter_name, property_type):
    """Register an observable property for a class.

    Args:
        cls: The class to register the property on.
        property_name: Name for the .on namespace (e.g. "range").
        signal_name: Qt signal attribute name (e.g. "range_changed"), or None for write-only.
        getter_name: Method name to get current value (e.g. "range"), or None.
        setter_name: Method name to set value (e.g. "set_range"), or None for read-only.
        property_type: One of "data", "range", "string" - determines threading strategy.
    """
    if cls not in _property_registry:
        _property_registry[cls] = {}
    _property_registry[cls][property_name] = {
        "signal_name": signal_name,
        "getter_name": getter_name,
        "setter_name": setter_name,
        "property_type": property_type,
    }


def _lookup_property_spec(obj, property_name):
    """Look up a property spec, walking the MRO."""
    for cls in type(obj).__mro__:
        if cls in _property_registry and property_name in _property_registry[cls]:
            return _property_registry[cls][property_name]
    return None


class ObservableProperty:
    """A reference to a specific observable property on a specific QObject instance."""
    __slots__ = ("_qobject_ref", "property_name", "property_type",
                 "_signal_name", "_getter_name", "_setter_name")

    def __init__(self, qobject, property_name, spec):
        self._qobject_ref = weakref.ref(qobject) if hasattr(qobject, '__weakref__') else lambda: qobject
        self.property_name = property_name
        self.property_type = spec["property_type"]
        self._signal_name = spec["signal_name"]
        self._getter_name = spec["getter_name"]
        self._setter_name = spec["setter_name"]

    @property
    def qobject(self):
        obj = self._qobject_ref()
        if obj is None:
            raise RuntimeError(f"Source QObject for '{self.property_name}' has been destroyed")
        return obj

    @property
    def signal(self):
        if self._signal_name is None:
            return None
        return getattr(self.qobject, self._signal_name)

    @property
    def value(self):
        if self._getter_name is None:
            raise AttributeError(f"Property '{self.property_name}' is not readable")
        return getattr(self.qobject, self._getter_name)()

    @value.setter
    def value(self, val):
        if self._setter_name is None:
            raise AttributeError(f"Property '{self.property_name}' is not writable")
        getattr(self.qobject, self._setter_name)(val)

    def __rshift__(self, other):
        # Defer import to avoid circular dependency
        from pipeline import build_pipeline_step
        return build_pipeline_step(self, other)


class OnNamespace:
    """The .on namespace that provides access to ObservableProperty instances."""
    __slots__ = ("_qobject",)

    def __init__(self, qobject):
        self._qobject = qobject

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        spec = _lookup_property_spec(self._qobject, name)
        if spec is None:
            raise AttributeError(
                f"{type(self._qobject).__name__} has no observable property '{name}'"
            )
        return ObservableProperty(self._qobject, name, spec)
```

**Step 4: Run test to verify it passes**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_properties.py -v`
Expected: All 7 tests PASS

**Step 5: Commit**

```bash
git add SciQLopPlots/properties.py tests/unit/test_properties.py
git commit -m "feat: add ObservableProperty and OnNamespace for reactive pipeline"
```

---

### Task 3: Pipeline and PartialPipeline with `>>` operator

**Files:**
- Create: `SciQLopPlots/pipeline.py`
- Create: `tests/unit/test_pipeline.py`

**Step 1: Write the failing test**

Tests for the `>>` chain mechanics, signature detection, event dispatch, and lifetime management.

```python
# tests/unit/test_pipeline.py
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import ObservableProperty, OnNamespace, register_property, _property_registry
from pipeline import Pipeline, PartialPipeline, build_pipeline_step


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
        self.destroyed = FakeSignal()
        self._range_val = 0.0
        self._tooltip_val = ""

    def range(self):
        return self._range_val

    def set_range(self, val):
        self._range_val = val

    def tool_tip(self):
        return self._tooltip_val

    def set_tool_tip(self, val):
        self._tooltip_val = val


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

    def transform(lower, upper):
        return f"{lower}-{upper}"

    OnNamespace(src).range >> transform >> OnNamespace(tgt).tooltip
    # Simulate range signal that passes a range-like object
    from event import Event
    # For this test, source emits the raw value (a float or range)
    src.range_changed.emit(3.14)
    # With a single positional arg, old-style gets the raw value
    assert tgt._tooltip_val != ""


# --- Terminal sink (PartialPipeline is live) ---

def test_partial_pipeline_acts_as_sink():
    src = FakeQObject()
    results = []

    def sink(event):
        results.append(event.value)

    OnNamespace(src).range >> sink  # no target, just a sink
    src.range_changed.emit(42.0)
    assert results == [42.0]


# --- Disconnect ---

def test_pipeline_disconnect():
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).range >> OnNamespace(tgt).range
    pipe.disconnect()
    src.range_changed.emit(99.0)
    assert tgt._range_val == 0.0  # unchanged


# --- Lifetime: source destroyed ---

def test_source_destroyed_disconnects():
    src = FakeQObject()
    tgt = FakeQObject()
    pipe = OnNamespace(src).range >> OnNamespace(tgt).range
    src.destroyed.emit()
    # Pipeline should be disconnected
    assert not pipe.connected


# --- Transform returning None skips setter ---

def test_transform_returning_none_skips_target():
    src = FakeQObject()
    tgt = FakeQObject()
    tgt._tooltip_val = "original"

    OnNamespace(src).range >> (lambda event: None) >> OnNamespace(tgt).tooltip
    src.range_changed.emit(1.0)
    assert tgt._tooltip_val == "original"
```

**Step 2: Run test to verify it fails**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_pipeline.py -v`
Expected: FAIL with `ModuleNotFoundError: No module named 'pipeline'`

**Step 3: Write minimal implementation**

```python
# SciQLopPlots/pipeline.py
import inspect
import weakref
from .event import Event


def _detect_call_style(func):
    """Detect whether a callable expects an Event or positional args.

    Returns "event" if the function takes a single parameter (new style),
    "positional" if it takes multiple parameters (old style),
    "none" if it takes no parameters.
    """
    try:
        sig = inspect.signature(func)
        params = [
            p for p in sig.parameters.values()
            if p.default is inspect.Parameter.empty
            and p.kind in (
                inspect.Parameter.POSITIONAL_ONLY,
                inspect.Parameter.POSITIONAL_OR_KEYWORD,
            )
        ]
        if len(params) == 0:
            return "none"
        elif len(params) == 1:
            return "event"
        else:
            return "positional"
    except (ValueError, TypeError):
        return "event"


def _make_dispatch(source_prop, transform, target_prop):
    """Create the slot function that handles signal emission."""
    call_style = _detect_call_style(transform) if transform else None

    def slot(*args):
        # Build the value from signal args
        value = args[0] if len(args) == 1 else args

        if transform is None:
            # Direct binding
            result = value
        else:
            if call_style == "event":
                event = Event(
                    source=source_prop.qobject,
                    value=value,
                    property_name=source_prop.property_name,
                )
                result = transform(event)
            elif call_style == "positional":
                result = transform(*args)
            else:
                result = transform()

        # If transform returns None, skip the target
        if result is None and transform is not None:
            return

        if target_prop is not None:
            target_prop.value = result

    return slot


class Pipeline:
    """A live connection: source.on.prop >> transform >> target.on.prop"""

    def __init__(self, source_prop, transform, target_prop):
        self._source_prop = source_prop
        self._target_prop = target_prop
        self._transform = transform
        self._connected = False
        self._slot = None

        self._connect()

    def _connect(self):
        signal = self._source_prop.signal
        if signal is None:
            raise ValueError(
                f"Cannot observe '{self._source_prop.property_name}': no signal"
            )
        self._slot = _make_dispatch(self._source_prop, self._transform, self._target_prop)
        signal.connect(self._slot)
        self._connected = True

        # Auto-disconnect on source destruction
        try:
            self._source_prop.qobject.destroyed.connect(self._on_destroyed)
        except (AttributeError, RuntimeError):
            pass

        # Auto-disconnect on target destruction
        if self._target_prop is not None:
            try:
                self._target_prop.qobject.destroyed.connect(self._on_destroyed)
            except (AttributeError, RuntimeError):
                pass

    def _on_destroyed(self, *args):
        self.disconnect()

    def disconnect(self):
        if self._connected and self._slot is not None:
            try:
                self._source_prop.signal.disconnect(self._slot)
            except (RuntimeError, TypeError):
                pass
            self._connected = False

    @property
    def connected(self):
        return self._connected

    def threaded(self):
        """Force this pipeline to execute transforms in a worker thread."""
        self._threaded = True
        return self

    def __rshift__(self, other):
        """Allow pipe >> new_target to create a new pipeline reusing the transform."""
        raise TypeError("Pipeline is already connected. Create a new pipeline instead.")


class PartialPipeline:
    """Intermediate result of source >> transform. Live as a sink, upgradeable with >> target."""

    def __init__(self, source_prop, transform):
        self._source_prop = source_prop
        self._transform = transform
        # Live immediately as a sink (no target)
        self._sink_pipeline = Pipeline(source_prop, transform, None)

    def __rshift__(self, other):
        from .properties import ObservableProperty

        if isinstance(other, ObservableProperty):
            # Upgrade: disconnect the sink, create full pipeline with target
            self._sink_pipeline.disconnect()
            return Pipeline(self._source_prop, self._transform, other)
        raise TypeError(
            f"Cannot pipe to {type(other).__name__}. Expected ObservableProperty."
        )


def build_pipeline_step(source_prop, other):
    """Called by ObservableProperty.__rshift__. Routes to Pipeline or PartialPipeline."""
    from .properties import ObservableProperty

    if isinstance(other, ObservableProperty):
        return Pipeline(source_prop, None, other)
    elif callable(other):
        return PartialPipeline(source_prop, other)
    else:
        raise TypeError(
            f"Cannot pipe to {type(other).__name__}. Expected callable or ObservableProperty."
        )
```

**Step 4: Run test to verify it passes**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_pipeline.py -v`
Expected: All 10 tests PASS

**Step 5: Commit**

```bash
git add SciQLopPlots/pipeline.py tests/unit/test_pipeline.py
git commit -m "feat: add Pipeline and PartialPipeline with >> operator"
```

---

### Task 4: Expose `range_changed` signal on SciQLopVerticalSpan to Python

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp:459-464`
- Modify: `include/SciQLopPlots/Items/SciQLopPlotItem.hpp:293-298`

**Context:** Currently `range_changed` is hidden from Shiboken on both `SciQLopRangeItemInterface` (line 298 of SciQLopPlotItem.hpp) and `impl::VerticalSpan` (line 301 of SciQLopVerticalSpan.hpp). The public `SciQLopVerticalSpan` class inherits from `SciQLopRangeItemInterface` but doesn't expose `range_changed` either. We need `range_changed` visible on `SciQLopVerticalSpan` for the pipeline API.

**Step 1: Add `range_changed` to the public `SciQLopVerticalSpan` signals block**

In `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`, the signals block at line 459-464 currently has:

```cpp
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
```

Change to:

```cpp
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange new_time_range);
    Q_SIGNAL void selectionChanged(bool);
    Q_SIGNAL void delete_requested();
```

**Step 2: Verify the `impl::VerticalSpan::range_changed` signal is forwarded**

Check that the `SciQLopVerticalSpan` constructor connects `impl::VerticalSpan::range_changed` to `SciQLopVerticalSpan::range_changed`. Look in `src/SciQLopVerticalSpan.cpp`.

If the forwarding doesn't exist yet, add a connection in the constructor:

```cpp
connect(_impl, &impl::VerticalSpan::range_changed,
        this, &SciQLopVerticalSpan::range_changed);
```

**Step 3: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation

**Step 4: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp
git add src/SciQLopVerticalSpan.cpp  # if modified
git commit -m "feat: expose range_changed signal on SciQLopVerticalSpan to Python bindings"
```

---

### Task 5: Register MVP properties and patch `.on` onto classes

**Files:**
- Modify: `SciQLopPlots/__init__.py`
- Create: `tests/unit/test_registration.py`

**Step 1: Write the failing test**

```python
# tests/unit/test_registration.py
"""Tests that .on namespace is patched onto SciQLopPlots classes.

These tests verify the registration logic without requiring a running Qt app
or compiled bindings. They test the patching mechanism itself.
"""
import pytest
import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import OnNamespace, register_property, _property_registry


class MockGraphInterface:
    """Stands in for SciQLopGraphInterface."""
    pass


class MockAxisInterface:
    """Stands in for SciQLopPlotAxisInterface."""
    pass


def test_on_property_is_accessible():
    """After patching, instances should have .on attribute returning OnNamespace."""
    register_property(
        MockGraphInterface, "data",
        signal_name="data_changed",
        getter_name="data",
        setter_name="set_data",
        property_type="data",
    )
    obj = MockGraphInterface()
    obj.on = OnNamespace(obj)
    assert isinstance(obj.on, OnNamespace)


def test_on_class_property_descriptor():
    """The .on attribute should work as a descriptor that creates OnNamespace per instance."""
    from properties import OnDescriptor

    register_property(
        MockAxisInterface, "range",
        signal_name="range_changed",
        getter_name="range",
        setter_name="set_range",
        property_type="range",
    )
    MockAxisInterface.on = OnDescriptor()
    obj = MockAxisInterface()
    assert isinstance(obj.on, OnNamespace)
```

**Step 2: Run test to verify it fails**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_registration.py -v`
Expected: FAIL with `ImportError: cannot import name 'OnDescriptor'`

**Step 3: Add OnDescriptor to properties.py**

Append to `SciQLopPlots/properties.py`:

```python
class OnDescriptor:
    """Descriptor that provides the .on namespace on instances."""

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return OnNamespace(obj)
```

**Step 4: Add registration and patching to `__init__.py`**

Add the following at the end of `SciQLopPlots/__init__.py` (after the existing `_patch_sciqlop_plot` calls):

```python
# --- Reactive pipeline API ---
from .properties import register_property, OnDescriptor
from .pipeline import Pipeline, PartialPipeline
from .event import Event

# Register MVP observable properties
register_property(
    SciQLopGraphInterface, "data",
    signal_name="data_changed",
    getter_name="data",
    setter_name="set_data",
    property_type="data",
)

register_property(
    SciQLopPlotsBindings.SciQLopPlotAxisInterface, "range",
    signal_name="range_changed",
    getter_name="range",
    setter_name="set_range",
    property_type="range",
)

register_property(
    SciQLopPlotsBindings.SciQLopVerticalSpan, "range",
    signal_name="range_changed",
    getter_name="range",
    setter_name="set_range",
    property_type="range",
)

register_property(
    SciQLopPlotsBindings.SciQLopVerticalSpan, "tooltip",
    signal_name=None,
    getter_name="tool_tip",
    setter_name="set_tool_tip",
    property_type="string",
)

# Patch .on descriptor onto all registered classes
for cls in (
    SciQLopGraphInterface,
    SciQLopPlotsBindings.SciQLopPlotAxisInterface,
    SciQLopPlotsBindings.SciQLopVerticalSpan,
):
    cls.on = OnDescriptor()
```

**Step 5: Run unit tests**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/ -v`
Expected: All tests PASS

**Step 6: Commit**

```bash
git add SciQLopPlots/properties.py SciQLopPlots/__init__.py tests/unit/test_registration.py
git commit -m "feat: register MVP properties and patch .on onto SciQLopPlots classes"
```

---

### Task 6: Fix imports for package-level usage

**Files:**
- Modify: `SciQLopPlots/pipeline.py` - fix relative imports
- Modify: `SciQLopPlots/event.py` - ensure clean import
- Modify: `SciQLopPlots/properties.py` - fix relative imports

**Context:** The unit tests use `sys.path` hacks to import modules directly. For actual package usage (`import SciQLopPlots`), the files need proper relative imports. This task ensures both paths work.

**Step 1: Verify current import structure**

The files as written in Tasks 1-3 use relative imports (e.g., `from .event import Event`). The unit tests bypass this with `sys.path` inserts. Verify all files use relative imports:

- `event.py`: No imports from sibling modules (standalone) - OK
- `properties.py`: `from pipeline import build_pipeline_step` in `__rshift__` - needs to be `from .pipeline import build_pipeline_step`
- `pipeline.py`: `from .event import Event` and `from .properties import ObservableProperty` - OK

**Step 2: Update unit tests to also work with package imports**

Add a conftest.py to help with imports:

```python
# tests/unit/conftest.py
import sys
import os

# Allow importing SciQLopPlots submodules directly for unit tests
# that don't need the full C++ bindings
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))
```

Remove the `sys.path` hacks from individual test files.

**Step 3: Run all unit tests**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/ -v`
Expected: All tests PASS

**Step 4: Commit**

```bash
git add SciQLopPlots/event.py SciQLopPlots/properties.py SciQLopPlots/pipeline.py tests/unit/conftest.py tests/unit/test_event.py tests/unit/test_properties.py tests/unit/test_pipeline.py
git commit -m "fix: clean up imports for package-level and unit test usage"
```

---

### Task 7: Integration manual test - Pipeline example

**Files:**
- Create: `tests/manual-tests/Pipeline/main.py`
- Modify: `tests/manual-tests/meson.build`

**Step 1: Write the integration test**

This is a manual test (like existing ones) that demonstrates the pipeline API with a real plot.

```python
# tests/manual-tests/Pipeline/main.py
"""Manual test demonstrating the reactive pipeline API.

Shows:
1. Direct axis binding: axis1.on.range >> axis2.on.range
2. Transform pipeline: span.on.range >> compute_stats >> span.on.tooltip
3. Terminal sink: graph.on.data >> print_summary
"""
import sys
import os
import numpy as np

os.environ['QT_QPA_PLATFORM'] = os.environ.get('QT_QPA_PLATFORM', 'offscreen')

from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget
from PySide6.QtCore import QTimer
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopPlot, SciQLopMultiPlotPanel, SciQLopVerticalSpan,
    SciQLopPlotRange, GraphType,
)


def make_data(lower, upper):
    x = np.linspace(lower, upper, 1000)
    y = np.sin(x) + 0.1 * np.random.randn(1000)
    return x, y


def compute_stats(event):
    r = event.value
    return f"Range: [{r.start:.1f}, {r.stop:.1f}], Size: {r.stop - r.start:.1f}"


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel()
panel.setWindowTitle("Pipeline API Test")

# Create a plot with data
graph = panel.plot(make_data, labels=["sin(x)"])

# Add a vertical span
plot = panel.plot_at(0)
span = SciQLopVerticalSpan(
    plot, SciQLopPlotRange(2.0, 4.0),
    QColor(100, 100, 200, 80), read_only=False,
    visible=True, tool_tip="Drag me!"
)

# Pipeline: update span tooltip when it moves
span.on.range >> compute_stats >> span.on.tooltip

# Terminal sink: log range changes
span.on.range >> (lambda event: print(f"Span moved to: {event.value.start:.1f} - {event.value.stop:.1f}"))

panel.show()
panel.resize(800, 600)

QTimer.singleShot(1000, app.quit)
sys.exit(app.exec())
```

**Step 2: Add to meson.build**

In `tests/manual-tests/meson.build`, add to the `examples` list:

```meson
{'name': 'Pipeline', 'source': meson.current_source_dir()+'/Pipeline/main.py'},
```

**Step 3: Build and run**

Run: `meson compile -C build && meson test -C build examples_Pipeline`
Expected: Test passes (runs for 1 second then exits)

**Step 4: Commit**

```bash
git add tests/manual-tests/Pipeline/main.py tests/manual-tests/meson.build
git commit -m "feat: add Pipeline manual test demonstrating reactive API"
```

---

### Task 8: Threaded pipeline for data properties

**Files:**
- Modify: `SciQLopPlots/pipeline.py`
- Create: `tests/unit/test_pipeline_threading.py`

**Context:** When the target is a `"data"` property, the transform should run in a worker thread to avoid blocking the UI. This task extends the Pipeline to use the existing `SimplePyCallablePipeline` infrastructure for data targets.

**Step 1: Write the failing test**

```python
# tests/unit/test_pipeline_threading.py
import pytest
import sys
import os
import threading

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))

from properties import register_property, OnNamespace, _property_registry
from pipeline import Pipeline, _detect_call_style, _make_dispatch


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


class FakeGraphObject:
    def __init__(self):
        self.data_changed = FakeSignal()
        self.destroyed = FakeSignal()
        self._data = None

    def data(self):
        return self._data

    def set_data(self, val):
        self._data = val


def setup_module():
    _property_registry.clear()
    register_property(
        FakeGraphObject, "data",
        signal_name="data_changed",
        getter_name="data",
        setter_name="set_data",
        property_type="data",
    )


def test_data_pipeline_uses_threaded_flag():
    """Pipeline targeting a 'data' property should have _threaded set."""
    src = FakeGraphObject()
    tgt = FakeGraphObject()
    pipe = OnNamespace(src).data >> (lambda event: event.value) >> OnNamespace(tgt).data
    assert pipe._auto_threaded


def test_detect_call_style_event():
    assert _detect_call_style(lambda event: event) == "event"


def test_detect_call_style_positional():
    assert _detect_call_style(lambda a, b: a) == "positional"


def test_detect_call_style_none():
    assert _detect_call_style(lambda: 42) == "none"
```

**Step 2: Run test to verify it fails**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_pipeline_threading.py -v`
Expected: FAIL with `AttributeError: 'Pipeline' object has no attribute '_auto_threaded'`

**Step 3: Update Pipeline to auto-detect threading need**

In `SciQLopPlots/pipeline.py`, update the `Pipeline.__init__` method:

```python
def __init__(self, source_prop, transform, target_prop):
    self._source_prop = source_prop
    self._target_prop = target_prop
    self._transform = transform
    self._connected = False
    self._slot = None
    self._auto_threaded = (
        target_prop is not None and target_prop.property_type == "data"
    )

    self._connect()
```

Note: Actual worker thread dispatch (using `SimplePyCallablePipeline`) requires the compiled C++ bindings. For the MVP, set the flag so the integration layer can check it. Full threaded execution will be wired in the integration test when the bindings are available. The current synchronous dispatch still works correctly - threading is an optimization.

**Step 4: Run test to verify it passes**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/unit/test_pipeline_threading.py -v`
Expected: All 4 tests PASS

**Step 5: Commit**

```bash
git add SciQLopPlots/pipeline.py tests/unit/test_pipeline_threading.py
git commit -m "feat: auto-detect threading need for data property pipelines"
```

---

## Summary

| Task | What | Files |
|------|------|-------|
| 1 | Event class | `event.py`, `test_event.py` |
| 2 | ObservableProperty + OnNamespace | `properties.py`, `test_properties.py` |
| 3 | Pipeline + PartialPipeline + `>>` | `pipeline.py`, `test_pipeline.py` |
| 4 | Expose `range_changed` on span (C++) | `SciQLopVerticalSpan.hpp`, `SciQLopVerticalSpan.cpp` |
| 5 | Register properties + patch `.on` | `__init__.py`, `test_registration.py` |
| 6 | Fix imports for package usage | All Python files, `conftest.py` |
| 7 | Integration manual test | `Pipeline/main.py`, `meson.build` |
| 8 | Threaded pipeline flag for data | `pipeline.py`, `test_pipeline_threading.py` |

**Dependencies:** Tasks 1-3 are independent (can be parallelized). Task 4 is independent (C++ only). Task 5 depends on 1-3. Task 6 depends on 1-5. Task 7 depends on 4-6. Task 8 depends on 3.

```
Task 1 (Event) ──────────┐
Task 2 (Properties) ─────┼── Task 5 (Register) ── Task 6 (Imports) ── Task 7 (Integration)
Task 3 (Pipeline) ───────┤                                                    ↑
Task 4 (C++ signal) ─────┘                                                    │
Task 8 (Threading) ── from Task 3 ────────────────────────────────────────────┘
```
