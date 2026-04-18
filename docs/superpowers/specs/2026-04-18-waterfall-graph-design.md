# Waterfall Graph — Design Spec

**Date:** 2026-04-18
**Status:** Design approved, ready for implementation plan
**Scope:** Expose `QCPWaterfallGraph` as a first-class SciQLopPlots plotable, at parity with line / scatter / curve / colormap.

## Problem

NeoQCP ships `QCPWaterfallGraph` (inherits `QCPMultiGraph`) with a `QCPWaterfallDataAdapter` that transforms per-trace values in coordinate space — supporting uniform/custom offsets, per-trace amplitude normalization, and a global gain. SciQLopPlots has no wrapper for it, so Python users cannot construct waterfall/record-section plots.

The target is not a minimal wrapper. Waterfall must be a *first-class citizen*: the same Python ergonomics, Inspector integration, reactive `.on.*` properties, and callable/function variant that every other plotable enjoys.

## Goals

- `plot.plot(x, y, graph_type=GraphType.Waterfall, ...)` works for both in-memory data and `GetDataPyCallable` callbacks.
- `plot.add_waterfall(name, ...)` constructs an empty waterfall, like `add_histogram2d`.
- Runtime mutation of `offset_mode`, `uniform_spacing`, `offsets`, `normalize`, `gain` via setters, signals, and `.on.*` reactive properties.
- Inspector delegate for interactive editing of all five knobs.
- Tooltip via `SciQLopCrosshair` shows the *raw* signal amplitude at the hovered key, not the post-transform (offset + normalize + gain) displayed Y.
- Function variant (`SciQLopWaterfallGraphFunction`) for SciQLop's reactive pipeline.
- Zero breakage of existing `SciQLopLineGraph` behavior or public API.

## Non-goals (v1)

Inherited from the NeoQCP spec:
- Wiggle fill (positive/negative half-fill).
- Amplitude scale bar.
- Horizontal waterfall (traces offset along X).
- Vertical key axis support.
- `lsImpulse` line style.

## Architecture

### Class hierarchy

```
SQPQCPAbstractPlottableWrapper
  └── SciQLopMultiGraphBase              (new)
       ├── SciQLopLineGraph              (refactored to derive from base)
       │    └── SciQLopLineGraphFunction (existing, unchanged)
       └── SciQLopWaterfallGraph         (new)
            └── SciQLopWaterfallGraphFunction (new)
```

`SciQLopMultiGraphBase` centralises everything shared between the two multi-trace plotables:

- `QCPMultiGraph* _multiGraph` base pointer.
- `PyBuffer _x, _y`, `std::shared_ptr<void> _dataHolder` (dataGuard pattern).
- `_pendingLabels`, `_keyAxis`, `_valueAxis`.
- Buffer-dispatch in `build_data_source(x, y)` handling 1D / 2D row-major / 2D column-major via `dispatch_dtype`, creating `QCPSoAMultiDataSource` or `QCPRowMajorMultiDataSource` as appropriate.
- `sync_components()` — one `SciQLopGraphComponent` per column, with label propagation.
- `set_data`, `data`, `set_x_axis`, `set_y_axis`, `x_axis`, `y_axis`, `line_count`.
- Pure virtual factory hook `create_multi_graph(QCPAxis*, QCPAxis*)` returning either `new QCPMultiGraph(...)` or `new QCPWaterfallGraph(...)`.

This follows the REFAC-01 (`SciQLopColorMapBase`) and REFAC-02 (`SpanBase`) patterns — documented working extraction, three classes share state, subclasses differ only in the underlying QCP plottable type plus optional extra setters.

### Files touched

**New**
- `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp`
- `src/SciQLopMultiGraphBase.cpp`
- `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`
- `src/SciQLopWaterfallGraph.cpp`
- `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp`
- `src/SciQLopWaterfallDelegate.cpp`
- `tests/integration/test_waterfall.py`
- `tests/manual-tests/basics/waterfall.py`

**Modified**
- `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`, `src/SciQLopLineGraph.cpp` — inherit from `SciQLopMultiGraphBase`, override only `create_multi_graph`; public API bit-for-bit preserved.
- `include/SciQLopPlots/SciQLopPlotInterface.hpp` — add `waterfall(x, y, ...)` and `waterfall(callable, ...)` overloads.
- `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp` — add `add_waterfall(...)` overloads and `plot_impl` routing for `GraphType::Waterfall`.
- `include/SciQLopPlots/enums.hpp` — new `WaterfallOffsetMode { Uniform, Custom }`; append `Waterfall` to `GraphType`.
- `SciQLopPlots/bindings/bindings.xml` — object-types for `SciQLopMultiGraphBase`, `SciQLopWaterfallGraph`, `SciQLopWaterfallGraphFunction`; enum-type for `WaterfallOffsetMode`; `Waterfall` entry in `GraphType`.
- `SciQLopPlots/bindings/bindings.h` — `#include` new headers.
- `SciQLopPlots/meson.build` — sources + `moc_headers`.
- `SciQLopPlots/__init__.py` — `plot_func` / `plot` dispatch gains `Waterfall` branch; register reactive properties for the five knobs; attach `OnDescriptor`.
- `src/Registrations.cpp` — add `types.register_type<SciQLopWaterfallGraph>({...})` block plus `delegates.register_type<SciQLopWaterfallDelegate>()`.

## Python API

### Entry points

```python
# Data-driven, unified plot()
wf = plot.plot(x, y, graph_type=GraphType.Waterfall,
               offsets=0.5,                 # float → Uniform mode, spacing=0.5
               # offsets=[0, 1.2, 3.5, ...] # array → Custom mode, len == y.shape[1]
               # offsets=None (default)     # Uniform mode, spacing=1.0
               normalize=True,
               gain=1.0,
               labels=["ch0", "ch1", ...],
               colors=[...],
               name="seismo")

# Callable / reactive
wf = plot.plot(my_callback, graph_type=GraphType.Waterfall,
               offsets=..., normalize=..., gain=..., labels=..., colors=...)

# Direct construction
wf = plot.add_waterfall("seismo", labels=[...], colors=[...],
                        offsets=..., normalize=True, gain=1.0)
wf.set_data(x, y)
```

### `offsets` kwarg polymorphism

Resolved in Python, not C++. `SciQLopPlots.__init__._apply_waterfall_kwargs` inspects the kwarg and dispatches to the C++ setters after construction:

- `None` → `set_offset_mode(Uniform)` (keep default spacing).
- `float`/`int` → `set_offset_mode(Uniform) + set_uniform_spacing(v)`.
- sequence → `set_offset_mode(Custom) + set_offsets(list(v))` (length-validated against `line_count()`; `ValueError` on mismatch).

The C++ `waterfall(...)` / `add_waterfall(...)` entry points take no offsets/normalize/gain kwargs; those knobs are applied from Python after the object is returned.

### Backward compatibility

- `GraphType::Waterfall` is appended at the end — binary-compatible addition.
- `plot()` signature is unchanged; new kwargs (`offsets`, `normalize`, `gain`) are keyword-only with defaults, so existing calls behave identically.
- `SciQLopLineGraph` public API, signals, and data-path are preserved verbatim through the base-class refactor. Regression tests in `tests/integration/test_bug_reproducers.py` and `test_p0_bugs.py` / `test_p1_bugs.py` provide the guardrail.

## C++ API

### `SciQLopMultiGraphBase`

```cpp
class SciQLopMultiGraphBase : public SQPQCPAbstractPlottableWrapper {
    Q_OBJECT
protected:
    QCPMultiGraph* _multiGraph = nullptr;
    PyBuffer _x, _y;
    std::shared_ptr<void> _dataHolder;
    QStringList _pendingLabels;
    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;

    virtual QCPMultiGraph* create_multi_graph(QCPAxis* k, QCPAxis* v) = 0;
    void clear_graphs(bool graph_already_removed = false);
    void build_data_source(const PyBuffer& x, const PyBuffer& y);
    void sync_components();

public:
    SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                          SciQLopPlotAxis* key_axis, SciQLopPlotAxis* value_axis,
                          const QStringList& labels, QVariantMap metaData);
    ~SciQLopMultiGraphBase() override;

    Q_SLOT void set_data(PyBuffer x, PyBuffer y) override;
    QList<PyBuffer> data() const noexcept override;
    std::size_t line_count() const noexcept;

    void set_x_axis(SciQLopPlotAxisInterface*) noexcept override;
    void set_y_axis(SciQLopPlotAxisInterface*) noexcept override;
    SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graphs(const QStringList& labels);
};
```

### `SciQLopLineGraph` (refactored)

```cpp
class SciQLopLineGraph : public SciQLopMultiGraphBase {
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* k, QCPAxis* v) override {
        return new QCPMultiGraph(k, v);
    }
public:
    SciQLopLineGraph(QCustomPlot*, SciQLopPlotAxis*, SciQLopPlotAxis*,
                     const QStringList& labels = {}, QVariantMap metaData = {});
};
```

Constructor signature, signals, and public slots are unchanged. `SciQLopLineGraphFunction` is unchanged.

### `SciQLopWaterfallGraph`

```cpp
class SciQLopWaterfallGraph : public SciQLopMultiGraphBase {
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* k, QCPAxis* v) override {
        return new QCPWaterfallGraph(k, v);
    }
    QCPWaterfallGraph* waterfall() const {
        return static_cast<QCPWaterfallGraph*>(_multiGraph);
    }

public:
    SciQLopWaterfallGraph(QCustomPlot*, SciQLopPlotAxis*, SciQLopPlotAxis*,
                          const QStringList& labels = {}, QVariantMap metaData = {});

    Q_SLOT void set_offset_mode(WaterfallOffsetMode mode);
    Q_SLOT void set_uniform_spacing(double spacing);
    Q_SLOT void set_offsets(const QVector<double>& offsets);
    Q_SLOT void set_normalize(bool enabled);
    Q_SLOT void set_gain(double gain);

    WaterfallOffsetMode offset_mode() const;
    double uniform_spacing() const;
    QVector<double> offsets() const;
    bool normalize() const;
    double gain() const;

    double raw_value_at(int component, double key) const;

    Q_SIGNAL void offset_mode_changed(WaterfallOffsetMode);
    Q_SIGNAL void uniform_spacing_changed(double);
    Q_SIGNAL void offsets_changed(QVector<double>);
    Q_SIGNAL void normalize_changed(bool);
    Q_SIGNAL void gain_changed(double);
};

class SciQLopWaterfallGraphFunction : public SciQLopWaterfallGraph,
                                       public SciQLopFunctionGraph {
    Q_OBJECT
public:
    SciQLopWaterfallGraphFunction(QCustomPlot*, SciQLopPlotAxis*, SciQLopPlotAxis*,
                                  GetDataPyCallable&& callable,
                                  const QStringList& labels = {},
                                  QVariantMap metaData = {});
};
```

### Plot-level entry points

```cpp
// SciQLopPlot
SciQLopWaterfallGraph* add_waterfall(
    const QString& name,
    const QStringList& labels = {},
    const QList<QColor>& colors = {},
    std::variant<std::monostate, double, QVector<double>> offsets = {},
    bool normalize = true, double gain = 1.0);

SciQLopWaterfallGraphFunction* add_waterfall(
    GetDataPyCallable callable,
    const QString& name, /* same kwargs */);

// SciQLopPlotInterface (to match line()/scatter())
SciQLopGraphInterface* waterfall(const PyBuffer& x, const PyBuffer& y, ...);
SciQLopGraphInterface* waterfall(GetDataPyCallable callable, ...);
```

### Enums

```cpp
// enums.hpp
enum class WaterfallOffsetMode { Uniform, Custom };
Q_DECLARE_METATYPE(WaterfallOffsetMode);

enum class GraphType {
    Line, ParametricCurve, Scatter, ColorMap,
    Waterfall    // appended
};
```

## Inspector delegate

`SciQLopWaterfallDelegate` mirrors `SciQLopColorMapDelegate` layout. Registered via `DelegateRegistry` keyed by `TypeDescriptor<SciQLopWaterfallGraph>`.

| Row | Widget | Bound to |
|---|---|---|
| Offset mode | `QComboBox` (Uniform / Custom) | `offset_mode` / `set_offset_mode` |
| Uniform spacing | `QDoubleSpinBox` (grayed-disabled when mode == Custom) | `uniform_spacing` / `set_uniform_spacing` |
| Offsets | scrollable per-trace `QDoubleSpinBox` grid (grayed-disabled when mode == Uniform) | `offsets` / `set_offsets` |
| Normalize | `QCheckBox` | `normalize` / `set_normalize` |
| Gain | `QDoubleSpinBox` (0.01 – 1e6, 3 decimals) | `gain` / `set_gain` |

Per-trace `SciQLopGraphComponent` children appear as tree children of the waterfall, inherited from `SciQLopMultiGraphBase::sync_components()` — unchanged from `SciQLopLineGraph` behavior.

## Reactive `.on.*` wiring

In `SciQLopPlots/__init__.py`:

```python
register_property(SciQLopWaterfallGraph, "offset_mode",
                  signal_name="offset_mode_changed",
                  getter_name="offset_mode", setter_name="set_offset_mode",
                  property_type="enum")
register_property(SciQLopWaterfallGraph, "spacing",
                  signal_name="uniform_spacing_changed",
                  getter_name="uniform_spacing", setter_name="set_uniform_spacing",
                  property_type="float")
register_property(SciQLopWaterfallGraph, "offsets",
                  signal_name="offsets_changed",
                  getter_name="offsets", setter_name="set_offsets",
                  property_type="array")
register_property(SciQLopWaterfallGraph, "normalize",
                  signal_name="normalize_changed",
                  getter_name="normalize", setter_name="set_normalize",
                  property_type="bool")
register_property(SciQLopWaterfallGraph, "gain",
                  signal_name="gain_changed",
                  getter_name="gain", setter_name="set_gain",
                  property_type="float")

SciQLopWaterfallGraph.on = OnDescriptor()
```

`.on.data` is inherited from `SciQLopGraphInterface` — no additional registration needed.

## Tooltip — raw-value readback

`SciQLopWaterfallGraph::raw_value_at(component, key)` indexes directly into `_y` using `_x`-binary-searched key → row, dispatching on dtype and layout (row-major vs col-major). Values returned are pre-normalize, pre-gain, pre-offset — the original buffer amplitude.

`SciQLopCrosshair::build_tooltip_html` gains a `qobject_cast<SciQLopWaterfallGraph*>(pl)` branch that calls `raw_value_at` instead of the inherited transformed-Y path. All other plottable tooltip paths are untouched.

Out-of-range keys are clamped to the nearest endpoint in `_x`, and `raw_value_at` returns the corresponding endpoint sample's raw amplitude (finite). NaN is reserved for genuinely undefined cases — invalid buffers, empty data, or out-of-range component index — where the crosshair formats NaN as `—` (existing behavior).

## Error handling

Per `shiboken_addfunction_exception_pattern` memory: every `<add-function>` with throwing C++ is wrapped in `<inject-code>` try/catch translating:
- `std::out_of_range` → `IndexError`
- `std::invalid_argument` → `ValueError`
- `std::domain_error` → `TypeError`
- fallback `std::exception` → `RuntimeError`

Validator coverage:
- Keys dtype must be float64 (inherited from base `build_data_source` — same rule as `SciQLopLineGraph`).
- `set_offsets(v)` with `v.size() != line_count()` → `std::invalid_argument` (`ValueError`).
- `offsets` kwarg array of wrong length → validated at `set_data` once column count is known → `ValueError`.
- `set_gain(g)` passes through to `QCPWaterfallGraph::setGain`; NeoQCP accepts any finite value (negative gain flips polarity — a legitimate use). No extra validation added here.

## Testing

### Integration (`tests/integration/test_waterfall.py`)

Construction & data paths:
- `test_add_waterfall_via_plot_graph_type`
- `test_add_waterfall_direct`
- `test_waterfall_function_variant`
- `test_waterfall_1d_y`, `test_waterfall_2d_row_major`, `test_waterfall_2d_col_major`
- `test_waterfall_float32_y`, `test_waterfall_int_y` (dtype dispatch)

`offsets` kwarg polymorphism:
- `test_offsets_float_selects_uniform`
- `test_offsets_array_selects_custom`
- `test_offsets_none_defaults`
- `test_offsets_array_wrong_len_raises`
- `test_offsets_non_numeric_raises`

Runtime mutation:
- `test_set_offset_mode_switches`
- `test_set_uniform_spacing_emits_signal`
- `test_set_offsets_emits_signal`
- `test_set_normalize_emits_signal`
- `test_set_gain_emits_signal`
- `test_set_offsets_wrong_len_raises`

Reactive pipeline:
- `test_on_gain_propagates`
- `test_on_offsets_propagates`
- `test_on_data_fires_on_set_data`

Tooltip / raw-value readback:
- `test_raw_value_at_row_major`
- `test_raw_value_at_col_major`
- `test_raw_value_at_with_normalize_true`
- `test_raw_value_at_out_of_range_returns_nan`

Component wrappers:
- `test_component_count_matches_n_cols`
- `test_component_names_from_labels`

Error paths:
- `test_waterfall_x_not_float64_raises`
- `test_waterfall_empty_buffer_noop`
- `test_waterfall_mismatched_x_y_shapes_raises`
- `test_set_gain_negative_flips_polarity` (smoke: no exception, signal emits)

Function variant:
- `test_waterfall_function_refetch_on_range_change`
- `test_waterfall_function_preserves_offsets_across_refetch`
- `test_waterfall_function_gain_changes_emit_signal`

Regression gate for `SciQLopLineGraph`:
- Existing `tests/integration/test_p0_bugs.py`, `test_p1_bugs.py`, `test_bug_reproducers.py` pass unchanged.
- New `test_line_graph_set_data_behavior_unchanged` — snapshot-style check on component count, `data()` return, signal emission order.

### Manual / gallery (`tests/manual-tests/basics/waterfall.py`)

Two tabs:
1. **Channel stack** — 8 synthetic magnetometer-like channels, `normalize=True`, uniform spacing, labelled.
2. **Record section** — 16 traces with custom offsets (distance-like), `normalize=False`, gain=1.0.

Both expose live gain slider, normalize checkbox, offsets editor. Linked into `tests/manual-tests/gallery.py` alongside `histogram2d.py`.

### Build / CI

- New sources added to `SciQLopPlots/meson.build` sources + `moc_headers`.
- New integration tests run in existing `integration-tests` CI job — no new dependency.
- Unity build check: `--unity=on` local debug build compiles clean before merge (DSP and REFAC-08 both surfaced unity pitfalls; this is a mandatory pre-merge step).

### No-regression guardrails

Before merging:
1. Full `tests/integration/` suite passes including all pre-existing `SciQLopLineGraph`-related tests.
2. `gallery.py` runs show line-graph panels render identically to pre-refactor baseline.
3. Exported-symbol diff on `SciQLopLineGraph` in the built `.so` shows only inheritance-related changes — no public signature drift.
