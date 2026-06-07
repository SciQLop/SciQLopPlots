# SciQLopPlots Code Review — 2026-05-31

## Scope

C++ engine (`src/`, `include/SciQLopPlots/`), Python package (`SciQLopPlots/`), threading, memory management, Qt patterns, performance.

---

## CRITICAL

### 1. Double-delete of `m_crosshair` in `_impl::SciQLopPlot::~SciQLopPlot()`

**File**: `src/SciQLopPlot.cpp:110`

```cpp
SciQLopPlot::~SciQLopPlot() {
    delete m_crosshair;  // <-- explicit delete
}
```

The crosshair was constructed at line 57:
```cpp
m_crosshair = new SciQLopCrosshair(this);  // parented to m_impl
```

Because `SciQLopCrosshair` inherits `QObject` and is constructed with `this` as parent, after the destructor body executes, `~QObject` will walk its child list and try to delete `m_crosshair` again — a double-free.

**Fixed by**: not parenting the crosshair, or nulling `m_crosshair` after delete.

---

### 2. `delete plottable` in `SciQLopPlotInterface` may double-free with Qt parent chain

**File**: `include/SciQLopPlots/SciQLopPlotInterface.hpp:415-428`

```cpp
virtual void remove_plottable(SciQLopPlottableInterface* plottable) {
    delete plottable;
}
```

All concrete plottables (`SciQLopLineGraph`, `SciQLopCurve`, etc.) are QObject-parented to `_impl::SciQLopPlot` (a `QCustomPlot`). If `remove_plottable` is called after the parent `QCustomPlot` has started its destruction (or from a signal handler during it), Qt's parent-child cleanup will also try to delete the same pointer.

Same pattern in `SciQLopPlot::~SciQLopPlot()` (`src/SciQLopPlot.cpp:661-667`):
```cpp
while (plottables().size() > 0)
    delete plottable(0);
```

**Fixed by**: using `deleteLater()` instead, or checking/removing from Qt parent first.

---

### 3. Use-after-free in `MultiPlotsVSpanCollection::delete_span`

**File**: `src/MultiPlotsVSpan.cpp:154-163`

```cpp
// create_span:
connect(vspan, &MultiPlotsVerticalSpan::destroyed, this,
        [this, vspan]() { _spans.removeAll(vspan); });

// delete_span:
void MultiPlotsVSpanCollection::delete_span(QPointer<MultiPlotsVerticalSpan> vspan) {
    _spans.removeAll(vspan);
    delete vspan;   // <-- fires ~QObject → destroyed signal → handler accesses vspan
}
```

`delete vspan` triggers `~QObject`, which emits the `destroyed` signal. The connected lambda captures `vspan` by value (raw pointer) and calls `_spans.removeAll(vspan)` — on the already-destructed (or partially-destructed) object. The `QPointer` is still valid at the time of signal emission, but the object is in its destructor. UB.

**Fixed by**: disconnecting before delete, or using `deleteLater()` and letting `destroyed` handle cleanup.

---

## HIGH

### 4. Raw `new/delete` in `GetDataPyCallable` (exception-unsafe)

**File**: `src/PythonInterface.cpp:690-766`

`_impl` is a raw `_GetDataPyCallable_impl*`. The copy constructor, move constructor, `share()`, `steal()`, and the main constructor all use raw `new`. The destructor uses raw `delete`. If any allocation throws between `new` and completion of construction, the object leaks.

**Fixed by**: replacing with `std::unique_ptr<_GetDataPyCallable_impl>`.

---

### 5. Raw `delete` on `QThread` in `SciQLopCurve::clear_resampler()`

**File**: `src/SciQLopCurve.cpp:52-61`

```cpp
void SciQLopCurve::clear_resampler() {
    disconnect(...);
    this->_resampler_thread->quit();
    this->_resampler_thread->wait();
    delete this->_resampler;
    delete this->_resampler_thread;
}
```

`quit()+wait()` before `delete` is safe but fragile: if an exception or early return skips the sequence, the thread leaks. The `QThread` also has no parent.

**Fixed by**: connecting `QThread::finished` → `deleteLater` and possibly `QThread::finished` → `resampler::deleteLater`, then just calling `quit()`.

---

### 6. Triple-mutex locking in `_async_resample_callback` — potential deadlock

**File**: `include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp:96-114`

```cpp
void _async_resample_callback() {
    QMutexLocker locker(&_data_mutex);
    {
        QMutexLocker locker(&_next_data_mutex);  // lock B
        _data = _next_data;
    }
    // ...
    data_copy = _data;  // copies SciQLopPyBuffer (may acquire GIL) under _data_mutex
}
```

The data-push path (in `DataProducer.cpp`) likely acquires mutexes in the opposite order (GIL → `_next_data_mutex` → `_data_mutex`). This creates a textbook lock-ordering deadlock when the Python callback runs under the resampler lock.

**Observation**: `QRecursiveMutex` prevents self-deadlock but not cross-thread ordering inversion. Three distinct mutexes in the same function is a code smell.

**Fixed by**: using a single mutex + atomic swap, or documenting the global lock order.

---

## MEDIUM

### 7. Empty `SciQLopCrosshair` destructor — items may dangle

**File**: `src/SciQLopCrosshair.cpp:74`

```cpp
SciQLopCrosshair::~SciQLopCrosshair() { }
```

`m_vline`, `m_hline`, `m_tooltip` are QCP items parented to the `QCustomPlot` (via `new QCPItemStraightLine(plot)`), **not** to `this` (QObject). If the crosshair outlives its plot (`m_plot` becomes a dangling pointer), these items are leaked/dangling.

**Fixed by**: using `QPointer<QCustomPlot>` for `m_plot`, or `setParent(nullptr)` on items in the destructor.

---

### 8. `remove_plottable` by index/name may `delete nullptr`

**File**: `include/SciQLopPlots/SciQLopPlotInterface.hpp:420-428`

```cpp
virtual void remove_plottable(int index) {
    delete plottable(index);  // plottable(index) may return nullptr
}
```

`delete nullptr` is well-defined (no-op), but silently swallowing a lookup failure makes debugging harder. Should assert or check.

---

### 9. VPlotsAlign eventFilter fires on every resize pixel

**File**: `src/VPlotsAlign.cpp:113`

An `eventFilter` is installed on every plot. Though margin recomputation is debounced with a 16ms timer, the eventFilter itself still runs for every resize event during splitter drags. For 20+ stacked plots, this matters.

**Fixed by**: monitoring `resizeEvent` via a single timer that polls sizes, or an aggregated signal.

---

### 10. `MultiPlotsVerticalSpan::removeObject` is overly broad

**File**: `src/MultiPlotsVSpan.cpp:71-84`

Iterates all spans and deletes those matching `parentPlot()`. If a plot somehow accumulates multiple spans over its lifetime, they're all deleted. At minimum, document this.

---

## LOW / STYLE

### 11. `QPointer` as const-ref parameter

**File**: `src/VPlotsAlign.cpp:98`

```cpp
void updatePlotList(const QList<QPointer<SciQLopPlotInterface>>& plots);
```

`QPointer` adds atomic ref-count overhead. Use `SciQLopPlotInterface*` for parameter passing; `QPointer` is for member storage.

---

### 12. Lambda connections without thread context in VPlotsAlign

**File**: `src/VPlotsAlign.cpp:107-112`

```cpp
connect(p, &SciQLopPlot::y_axis_range_changed, this,
        [this](SciQLopPlotRange) { m_debounce_timer->start(); });
```

Uses default `AutoConnection`. If range changes ever come from a worker thread (which is plausible given the pipeline architecture), the lambda executes in the sender's thread, not the GUI thread. Use `Qt::QueuedConnection` where the receiver is a GUI object.

---

### 13. `PROFILE_HERE_N` in trivial inline function

**File**: `include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp:62`

```cpp
inline XYView make_view(const ResamplerData1d& data, const ResamplerPlotInfo& plot_info) {
    PROFILE_HERE_N("XYView make_view");
    return XYView(data.x, data.y, ...);
}
```

This is a pure constructor call. Profiling a function that does two pointer assignments and a range copy adds noise to traces. Remove or make conditional.

---

## STRENGTHS

| Area | Notes |
|------|-------|
| **RAII** | `unique_ptr` for `InspectorExtensionHolder` on every interface class — consistent and correct |
| **Thread safety** | `shared_ptr` snapshot pattern in `DataProducer` avoids GIL lock-ordering problems |
| **Hover throttle** | 16ms gate in `mouseMoveEvent` (`SciQLopPlot.cpp:241`) prevents 1kHz replot storms |
| **Self-queued resamplers** | Signals connected with `Qt::QueuedConnection` let updates coalesce in the event loop |
| **Zero-cost profiling** | `PROFILE_HERE_N` uses a single relaxed atomic load when disabled (~1ns) |
| **Exception safety** | `SQP_SAFE_SLOT_BEGIN/END` guards every non-trivial slot |
| **Aliasing shared_ptr** | PyBuffer lifetime extended via aliasing `shared_ptr` without refcounting the source — clever design |

---

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 3 |
| HIGH | 3 |
| MEDIUM | 4 |
| LOW | 3 |

The three critical items are all memory-safety bugs (double-free / use-after-free). The high-severity items are exception-unsafe allocations, thread lifecycle fragility, and a potential lock-ordering deadlock. The rest are established patterns that could degrade under load but are not immediately dangerous.

*Generated by Claude Code review agent.*
