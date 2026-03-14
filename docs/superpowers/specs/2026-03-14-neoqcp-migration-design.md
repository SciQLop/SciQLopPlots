# NeoQCP Migration: QCPMultiGraph, QCPColorMap2, Multi-Type NumPy

**Date:** 2026-03-14
**Status:** Draft

## Goal

Migrate SciQLopPlots from QCPGraph/QCPColorMap to NeoQCP's new zero-copy APIs (QCPMultiGraph, QCPColorMap2) and support all numeric numpy dtypes instead of only float64.

QCPGraph2 (single-series) also exists in NeoQCP but QCPMultiGraph supersedes it for SciQLopPlots' use case since SciQLopLineGraph always manages N value columns sharing one key axis.

## Design Decisions

- Feed numpy pointers directly to NeoQCP via `viewData()` — no intermediate resampler
- PyBuffer becomes dtype-aware (type-erased `void*` + format code), no longer double-only
- SciQLopPlots' resampler infrastructure is removed for line graphs and colormaps
- QCPColorMap2's built-in async resampler replaces SciQLopPlots' ColormapResampler
- QCPMultiGraph replaces the list-of-QCPGraph pattern in SciQLopLineGraph
- Lifetime safety: PyBuffers stored alongside data sources; shared wrapper for colormap async
- SciQLopCurve, SciQLopNDProjectionCurves stay unchanged
- QCPTheme is out of scope

## 1. PyBuffer Multi-Type Support

### Current

`_PyBuffer_impl::init_buffer()` rejects any format that isn't `'d'` (float64). `PyBuffer` exposes `double* data()`, `double`-typed iterators, and `ArrayView1D`/`ArrayView2D` hierarchies.

### New

Remove the `format[0] != 'd'` check. Accept any numeric numpy dtype.

**Added to PyBuffer:**
- `char format_code() const` — numpy format character (`'d'`, `'f'`, `'b'`, `'B'`, `'h'`, `'H'`, `'i'`, `'I'`, `'l'`, `'L'`, `'q'`, `'Q'`)
- `void* raw_data() const` — returns `buffer.buf`
- `std::size_t item_size() const` — returns `buffer.itemsize`

**Kept (dtype-agnostic):**
- `shape()`, `ndim()`, `size()`, `flat_size()`, `row_major()`, `is_valid()`

**Removed from PyBuffer (new code should not use these):**
- `std::size` / `std::cbegin` / `std::cend` specializations for PyBuffer

**Deprecated but kept for curve path:**
- `double* data()` — kept for backward compatibility with `Views.hpp`/`AbstractResampler`. Must only be called when `format_code() == 'd'`; throws if called on non-double buffers to prevent silent reinterpretation of memory.

**ArrayView hierarchy (`ArrayView1DIterator`, `ArrayViewBase`, `ArrayView1D`, `ArrayView2D`) and `view()` method:** These are kept for now because `SciQLopCurveResampler` depends on them via `AbstractResampler` and `Views.hpp`. They remain double-only — the curve path is unchanged. They can be removed later when SciQLopCurve is migrated.

PyBuffer becomes a thin type-erased wrapper for new consumers: shape + format + raw pointer + Python lifetime management. Legacy double-only access remains available for the unchanged curve path.

## 2. Dtype Dispatch Utility

New header: `include/SciQLopPlots/Python/DtypeDispatch.hpp`

A template utility that switches on numpy format code and calls a generic lambda with the correct `std::type_identity<T>`:

```cpp
template <typename F>
auto dispatch_dtype(char format_code, F&& func) {
    switch (format_code) {
        case 'f': return func(std::type_identity<float>{});
        case 'd': return func(std::type_identity<double>{});
        case 'b': return func(std::type_identity<int8_t>{});
        case 'B': return func(std::type_identity<uint8_t>{});
        case 'h': return func(std::type_identity<int16_t>{});
        case 'H': return func(std::type_identity<uint16_t>{});
        case 'i': return func(std::type_identity<int32_t>{});
        case 'I': return func(std::type_identity<uint32_t>{});
        case 'l': return func(std::type_identity<long>{});
        case 'L': return func(std::type_identity<unsigned long>{});
        case 'q': return func(std::type_identity<long long>{});
        case 'Q': return func(std::type_identity<unsigned long long>{});
        default: throw std::runtime_error("Unsupported numpy dtype");
    }
}
```

Note: `'l'`/`'L'` map to `long`/`unsigned long` (platform-dependent: 4 bytes on Windows LLP64, 8 bytes on Linux LP64). `'q'`/`'Q'` map to `long long`/`unsigned long long` (always 8 bytes). This matches Python struct module semantics.

**Template explosion:** Nested dispatch (x dtype × y dtype) produces up to 144 instantiations. In practice, keys (timestamps) are almost always `float64`. To limit code bloat, we can constrain keys to `float64` only (timestamps are always doubles in SciQLop) and dispatch only on value types. This reduces to 12 instantiations for graphs and 12 for colormaps. If mixed key types are needed later, the dispatch can be extended.

All template instantiation happens in C++ — Shiboken never sees it.

## 3. SciQLopLineGraph → QCPMultiGraph

### Current

`SciQLopLineGraph` manages a `QList<SciQLopGraphComponent>` wrapping individual `QCPGraph*` instances. A `LineGraphResampler` runs in a worker thread, producing `QList<QVector<QCPGraphData>>` emitted back to the main thread.

### New

Replace with a single `QCPMultiGraph*`. QCPMultiGraph natively handles N value columns sharing one key axis, with per-component names, colors, pens, scatter styles, selection, and adaptive sampling.

**Members:**
- `QCPMultiGraph* _multiGraph` — the single NeoQCP plottable
- `PyBuffer _x, _y` — lifetime anchors for numpy memory

**`set_data(PyBuffer x, PyBuffer y)` flow:**
1. Store new PyBuffers (releases old ones)
2. Assert x format is `'d'` (keys are always float64 timestamps)
3. Dispatch on y dtype via `dispatch_dtype`
4. For 1D y (single line): build `std::vector<std::span<const V>>{values_span}`, call `multiGraph->viewData<double,V>(keys_span, columns)`
5. For 2D y column-major: each column is contiguous — build `std::vector<std::span<const V>>` with one span per column, call `viewData`
6. For 2D y row-major: columns are strided — copy each column into `std::vector<V>`, call `setData(std::move(keys_copy), std::move(column_vectors))`

**Removed:**
- `LineGraphResampler*`, `QThread*`
- `_setGraphData()` slot, `_setGraphDataSig` signal
- `create_resampler()`, `clear_resampler()`
- `lines()`, `line()` accessors

**SciQLopLineGraphFunction / SciQLopColorMapFunction:** These subclasses just combine the base class with `SciQLopFunctionGraph` (reactive data-fetching callback). They call `set_data()` on the base — no changes needed beyond what the base class migration provides.

### SciQLopGraphComponent Adaptation

The variant `graph_or_curve` becomes `QCPGraph* | QCPCurve* | MultiGraphRef` where:

```cpp
struct MultiGraphRef {
    QCPMultiGraph* graph;
    int componentIndex;
};
```

This is **not** a `QCPAbstractPlottable*`, so `SciQLopGraphComponent` needs:

**New constructor:** `SciQLopGraphComponent(QCPMultiGraph* graph, int componentIndex, QObject* parent)` — stores a `MultiGraphRef` instead of setting `m_plottable`. The `m_plottable` member is set to `graph` (so `plottable()` still returns a valid `QCPAbstractPlottable*` for legend items etc.), but `to_variant()` returns the `MultiGraphRef` arm based on whether the stored component index is >= 0.

**Reworked accessors:**

- `plottable()` — for MultiGraphRef, returns `graph` (the QCPMultiGraph is a QCPAbstractPlottable)
- `pen()` / `set_pen()` — delegates to `graph->component(i).pen`
- `color()` / `set_color()` — delegates to component pen
- `visible()` / `set_visible()` — delegates to `graph->component(i).visible`
- `set_selected()` — delegates to `graph->setComponentSelection(i, ...)`
- `line_style()` / `set_line_style()` — delegates to `graph->setLineStyle()` (shared across all components)
- `marker_shape()` / `set_marker_shape()` — delegates to `graph->component(i).scatterStyle`
- `name()` / `set_name()` — delegates to `graph->component(i).name`

Each accessor gets a new visitor arm for the `MultiGraphRef` case.

### SQPQCPAbstractPlottableWrapper Ownership Model

**Current:** One `SciQLopGraphComponent` per `QCPAbstractPlottable` (1:1).

**New for QCPMultiGraph:** One `QCPMultiGraph` plottable, N `SciQLopGraphComponent` wrappers (1:N). Each wrapper holds a `MultiGraphRef{multiGraph, columnIndex}`.

- `qcp_plottables()` — returns `{_multiGraph}` (single plottable, not N copies)
- `set_labels()` — calls `_multiGraph->setComponentNames()`
- `set_colors()` — calls `_multiGraph->setComponentColors()`
- `set_visible()` / `set_selected()` — iterate components as before
- `newComponent<QCPMultiGraph>` is not used — instead `SciQLopLineGraph` creates the `QCPMultiGraph` directly and wraps its components

**Legend:** QCPMultiGraph uses `QCPGroupLegendItem` (expandable per-component legend). Replaces one-legend-entry-per-QCPGraph.

## 4. SciQLopColorMap → QCPColorMap2

### Current

`SciQLopColorMap` holds `ColormapResampler*` + `QThread*`. On `set_data(x, y, z)`, data is pushed to the resampler thread which bins into `QCPColorMapData*` and emits back. Connects to `x_axis->rangeChanged` for re-resampling on pan/zoom.

### New

Replace `QCPColorMap*` with `QCPColorMap2*`. QCPColorMap2 handles async resampling internally via `QCPColormapResampler` + `QCPResamplerScheduler` with request coalescing.

**Lifetime wrapper:**

```cpp
struct PyBufferDataSource2D {
    PyBuffer x, y, z;
    std::shared_ptr<QCPAbstractDataSource2D> source;
};
```

The data source is a `QCPSoADataSource2D<span<const X>, span<const Y>, span<const Z>>`. The PyBuffers keep numpy memory alive. `SciQLopColorMap` stores the `PyBufferDataSource2D` as a member. QCPColorMap2 internally holds a `shared_ptr<QCPAbstractDataSource2D>` — its resampler captures this shared_ptr during async work. When `set_data()` is called again, the old `PyBufferDataSource2D` member is replaced, but the old data source's `shared_ptr` inside NeoQCP stays alive until the resampler finishes (since the resampler captured a copy). The key invariant: `PyBufferDataSource2D` must own both the PyBuffers and the data source so they share lifetime. The `shared_ptr<QCPAbstractDataSource2D>` passed to NeoQCP is the *same* object held inside `PyBufferDataSource2D` — so as long as either SciQLopColorMap or NeoQCP's resampler holds a reference, the numpy memory stays alive.

**`set_data(PyBuffer x, PyBuffer y, PyBuffer z)` flow:**
1. Assert x format is `'d'` (keys are always float64 timestamps)
2. Dispatch on y and z dtypes via `dispatch_dtype`
3. Create `QCPSoADataSource2D<span<const double>, span<const Y>, span<const Z>>`
4. Wrap with PyBuffers in `shared_ptr<PyBufferDataSource2D>`
5. Call `colorMap2->setDataSource(wrapper->source)`
6. Store wrapper as member (keeps it alive between set_data calls)

**Removed:**
- `ColormapResampler*`, `QThread*`
- `_setGraphData(QCPColorMapData*)` slot
- `_resample()` method
- `_data_swap_mutex`

**Kept/mapped 1:1:**
- `gradient()` / `set_gradient()` → `QCPColorMap2::gradient()` / `setGradient()`
- `setColorScale()` → `QCPColorMap2::setColorScale()`
- Gap threshold → `QCPColorMap2::setGapThreshold()`

**auto_scale_y:** On `dataChanged()` (or after `setDataSource`), call `dataSource()->yRange()` and set the value axis range accordingly. This replaces the manual y-range tracking.

## 5. Files Changed

### Deleted

| File | Reason |
|------|--------|
| `include/.../Resamplers/SciQLopLineGraphResampler.hpp` | Replaced by QCPMultiGraph adaptive sampling |
| `include/.../Resamplers/SciQLopColorMapResampler.hpp` | Replaced by QCPColorMap2 async resampler |
| `src/SciQLopLineGraphResampler.cpp` | Deleted with header |
| `src/SciQLopColorMapResampler.cpp` | Deleted with header |

### Modified

| File | Changes |
|------|---------|
| `PythonInterface.hpp` | Add format_code/raw_data/item_size; keep ArrayView for curve path |
| `PythonInterface.cpp` | Remove double-only check, add new accessors |
| `SciQLopLineGraph.hpp/cpp` | QCPMultiGraph, remove resampler, store PyBuffers |
| `SciQLopColorMap.hpp/cpp` | QCPColorMap2, remove resampler, PyBufferDataSource2D wrapper |
| `SciQLopGraphComponent.hpp` | Add MultiGraphRef to variant, new visitor arms |
| `QCPAbstractPlottableWrapper.hpp` | Update qcp_plottables/set_labels/set_colors for 1:N model; update `_set_selected` free function for QCPMultiGraph (uses `setComponentSelection` instead of `data()->dataRange()`) |
| `bindings.xml` | Update for changed PyBuffer API |
| `meson.build` | Remove deleted source files |

### New

| File | Purpose |
|------|---------|
| `include/.../Python/DtypeDispatch.hpp` | Dtype switch utility |

### Unchanged

| File | Reason |
|------|--------|
| `SciQLopCurve.hpp/cpp` | Out of scope |
| `SciQLopCurveResampler.hpp/cpp` | Out of scope |
| `AbstractResampler.hpp/cpp` | Kept for CurveResampler dependency |
| `Views.hpp` | Kept for CurveResampler/AbstractResampler dependency |
| `SciQLopNDProjectionCurves.hpp/cpp` | Uses SciQLopCurve internally, not affected |
| `Items/*`, `Inspector/*`, `Products/*`, `MultiPlots/*` | Not affected |
| `__init__.py`, reactive modules | Python API signatures unchanged |

## 6. Python API Impact

**No breaking changes.** `set_data(x, y)` and `set_data(x, y, z)` signatures are identical. PyBuffer is still the Shiboken bridge type.

**User-visible improvements:**
- Any numeric numpy dtype now accepted for values (float32, int16, uint8, etc.) — existing float64 code unchanged
- Keys (x axis) remain float64 only (timestamps are always doubles)
- For datasets under ~10M points, behavior is equivalent or better
- For very large datasets (>10M), adaptive sampling replaces the hierarchical resampler — may be slower until NeoQCP gets a built-in async graph resampler (documented in NeoQCP memory)

**`data()` accessor:** Both `SciQLopLineGraph::data()` and `SciQLopColorMap::data()` return `QList<PyBuffer>`. After migration, they return the stored `_x`, `_y` (and `_z` for colormap) PyBuffers directly. Consumers that read values must use `raw_data()` + `format_code()` instead of the old `double* data()`. In practice, the Python side accesses the original numpy arrays, not the returned PyBuffers.

**Bindings (bindings.xml) updates:**
- Remove double-typed PyBuffer accessors if exposed
- Add `format_code()`, `raw_data()`, `item_size()` if needed from Python
- QCPMultiGraph / QCPColorMap2 are internal — not exposed directly

## 7. Future Work (Out of Scope)

- **Async graph resampling in NeoQCP:** Two-level hierarchical resampler for >10M point datasets. Documented in NeoQCP memory (`hierarchical-graph-resampling.md`). Would allow removing the "may be slower for huge datasets" caveat.
- **QCPTheme integration:** Expose theme API through SciQLopPlots for consistent light/dark styling.
- **SciQLopCurve migration:** When/if QCPCurve2 is added to NeoQCP. At that point, ArrayView hierarchy and Views.hpp can be removed.
- **Remove legacy double-only ArrayView path:** Once all plotable types use the new zero-copy API.
