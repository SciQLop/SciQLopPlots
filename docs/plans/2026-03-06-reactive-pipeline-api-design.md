# Reactive Pipeline API Design

## Problem

SciQLopPlots has a powerful but inconsistent reactive data model. Plots can observe axis range changes via callbacks, and graphs can react to other graphs' data changes, but the API varies by source type with hardcoded callback signatures. This makes it difficult to generalize patterns like:

- Span-driven statistics tooltips
- FFT chains across plots
- Spectrogram accumulation from streaming FFT data
- Any "observe X, transform, set Y" pattern

## Design: `source.on.property >> transform >> target.on.property`

### Core Concepts

**Three primitives:**

1. **`ObservableProperty`** - A Python descriptor wrapping a (signal, getter, setter) triplet on any SciQLopPlots QObject. Accessed via the `.on` namespace: `span.on.range`, `graph.on.data`.

2. **`Pipeline`** - The live connection between a source ObservableProperty, an optional transform (Python or C++ callable), and a target ObservableProperty. Manages lifetime via weak refs. Auto-disconnects when source or target is destroyed.

3. **`Event`** - Python object passed to transform functions. Carries `event.source`, `event.value`, `event.property_name`, plus convenience accessors `.range` and `.data`.

### The `>>` Operator

```python
# Full pipeline: source >> transform >> target
pipe = span.on.range >> compute_stats >> span.on.tooltip

# Direct binding: source >> target (no transform)
axis1.on.range >> axis2.on.range

# Terminal sink: source >> callable (no target, just runs)
span.on.range >> save_to_disk

# Pipeline control
pipe.disconnect()   # manual teardown
pipe.threaded()     # force worker thread
```

### Operator Mechanics

```
span.on.range >> transform >> target.on.data

Step 1: ObservableProperty.__rshift__(callable)  → PartialPipeline (live as sink)
Step 2: PartialPipeline.__rshift__(ObservableProperty) → Pipeline (upgraded with target)
Step 1 alt: ObservableProperty.__rshift__(ObservableProperty) → Pipeline (direct binding)
```

`PartialPipeline` is live immediately (acts as a terminal sink). If chained with `>> target`, it upgrades to a full Pipeline.

### The `.on` Namespace

Regular attribute access is unchanged. The `.on` namespace provides ObservableProperty descriptors for piping:

```python
r = span.range          # returns the SciQLopPlotRange value (unchanged)
span.range = new_range  # sets it (unchanged)

span.on.range           # returns ObservableProperty for piping
span.on.range >> f >> target  # creates a pipeline
```

### Event Object

Constructed Python-side from raw signal arguments before calling the transform:

```python
class Event:
    source          # the QObject that emitted
    property_name   # "range", "data", etc.
    value           # the raw value from the signal

    @property
    def range(self): ...  # convenience: returns value if SciQLopPlotRange

    @property
    def data(self): ...   # convenience: returns value if array data
```

### Callback Signature Detection

At pipeline registration time, `inspect.signature` determines dispatch style:

```python
# New style - receives Event
def transform(event):
    return f"Mean: {np.mean(event.data[1]):.2f}"

# Old style - receives raw positional args (backward compat)
def transform(lower, upper):
    return fetch_data(lower, upper)
```

Detection runs once at pipeline creation, not on every event.

### Threading Strategy

| Target property_type | Execution | Rationale |
|---|---|---|
| `"data"` | Threaded via existing DataProviderWorker | Heavy numpy, don't block UI |
| `"range"`, `"string"`, other | Direct (Qt signal/slot) | Lightweight, instant |

The `.threaded()` method forces worker thread execution for any pipeline.

For `"data"` targets, the pipeline reuses the existing `SimplePyCallablePipeline` infrastructure: same worker thread, same 100ms rate limiting, same GIL handling.

### Lifetime Management

- Pipeline holds weak references to source and target QObjects
- Pipeline is stored on the target QObject (dies when target dies)
- Connects to source's `destroyed` signal to auto-disconnect
- `pipe.disconnect()` for manual teardown

### Backward Compatibility

`panel.plot(callback)` continues to work. Internally equivalent to:

```python
graph = panel.line(labels=...)
panel.x_axis.on.range >> callback >> graph.on.data
```

The `observe=` parameter also still works:

```python
graph = panel.plot(callback, observe=span)
# equivalent to: span.on.range >> callback >> graph.on.data
```

Legacy callback signatures `f(lower, upper)` are supported via signature detection with a deprecation path.

### MVP Observable Properties

| Object | Property | Observable (source) | Settable (target) | Type |
|---|---|---|---|---|
| `SciQLopGraphInterface` | `data` | yes | yes | `"data"` |
| `SciQLopPlotAxisInterface` | `range` | yes | yes | `"range"` |
| `SciQLopVerticalSpan` | `range` | yes | yes | `"range"` |
| `SciQLopVerticalSpan` | `tooltip` | no | yes | `"string"` |

### Implementation Layers

**Python-side (new files in `SciQLopPlots/`):**
- `properties.py` - `ObservableProperty`, `OnNamespace`, property registry, `register_property()`
- `pipeline.py` - `Pipeline`, `PartialPipeline`, `Event`, signature detection, threading dispatch
- `__init__.py` - register properties for MVP types, patch `.on` onto classes

**C++ side:**
- No new classes needed for MVP
- Existing signals and getters/setters cover all four MVP properties
- May need to verify `data()` getter exposure in Shiboken bindings

**Unchanged:**
- `DataProviderWorker` / `SimplePyCallablePipeline` - reused for threaded data pipelines
- Resamplers - still sit between data and plot rendering
- `bindings.xml` - no changes for MVP
- NeoQCP / QCustomPlot layer

### Example Use Cases

**FFT chain (the hackish example, now clean):**
```python
graph1 = panel.plot(fetch_data, labels=["Bx","By","Bz"])
graph2 = panel.plot(compute_fft, observe=graph1)
colormap = panel.colormap(labels=["Spectrogram"])
graph2.on.data >> accumulate_spectro >> colormap.on.data
```

**Span statistics tooltip:**
```python
span = panel.add_vertical_span(range, color)
graph = panel.plot(fetch_data, labels=["X","Y","Z"])
span.on.range >> (lambda e: stats(graph, e)) >> span.on.tooltip
```

**Direct axis sync:**
```python
axis1.on.range >> axis2.on.range
```

**Persistent logging:**
```python
graph.on.data >> save_to_disk
```
