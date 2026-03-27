# P3 Refactoring Design — REFAC-01 through REFAC-05

**Date:** 2026-03-27
**Scope:** Deduplicate plotable and item class hierarchies, extract shared helpers, fix naming inconsistencies.
**Constraint:** No Shiboken binding changes needed — all new classes are internal implementation details.

---

## REFAC-01: ColorMap / Histogram2D Base Class

### Problem
SciQLopColorMap and SciQLopHistogram2D share ~60-70% identical code: axis management, selection state, gradient, legend helpers, naming, busy state, destructor logic.

### Design
New concrete class `SciQLopColorMapBase` inherits `SciQLopColorMapInterface`.

**File:** `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp` + `src/SciQLopColorMapBase.cpp`

**Members moved into base:**
- `SciQLopPlotAxis* _keyAxis`
- `SciQLopPlotAxis* _valueAxis`
- `SciQLopPlotColorScaleAxis* _colorScaleAxis`
- `bool _selected`
- `std::shared_ptr<DataSourceWithBuffers> _dataHolder`

Note: `_got_first_data` lives in `SciQLopPlottableInterface` (see REFAC-04).

**Methods moved into base:**
- Axis: `x_axis()`, `y_axis()`, `z_axis()`, `set_x_axis()`, `set_y_axis()`
- Selection: `selected()`, `set_selected()`
- Identity: `name()`, `set_name()`, `layer()`
- Display: `gradient()`, `set_gradient()`, `busy()`, `set_busy()`, `data()`
- Helpers: `_plot()`, `_legend_item()`
- Destructor: remove plottable from plot

**Abstract virtual the base requires:**
- `QCPAbstractPlottable* plottable() const` — derived returns `_cmap` or `_hist` so the base can forward `setKeyAxis()`/`setValueAxis()` generically.

**Stays in SciQLopColorMap:** `auto_scale_y`, `y_log_scale`, `z_log_scale`, `colorMap()`, `set_data(x, y, z)`
**Stays in SciQLopHistogram2D:** `set_bins`, `key_bins`, `value_bins`, `normalization`, `histogram()`, `set_data(x, y)`

**Not in bindings.xml** — ColorMap and Histogram2D keep their existing declarations.

---

## REFAC-02: Span Impl and Wrapper Base Classes

### Problem
VSpan, HSpan, RSpan are ~95% identical (V/H) or ~80% identical (R) at both impl and wrapper layers.

### Design — Two new classes

#### `impl::SpanBase`

**File:** `include/SciQLopPlots/Items/impl/SpanBase.hpp` + `src/SpanBase.cpp`

Takes a `QCPAbstractSpanItem*` in constructor (since V/H/R impl classes each inherit from different QCP item types, SpanBase can't inherit from QCP directly).

**Methods moved into SpanBase:**
- `set_color(const QColor&)` — brush, selected brush, pen styling
- `color() const`
- `set_borders_color(const QColor&)` — 3px border pen
- `borders_color() const`
- `set_visible(bool)`
- `replot(bool immediate)`
- `set_borders_tool_tip(const QString&)`

**Stays in each impl class:**
- V/H: `set_range()`, `range()`, `select_lower_border()`, `select_upper_border()`, border selection signals
- R: `set_key_range()`, `set_value_range()`, `key_range()`, `value_range()`
- Range-changed signals (different signatures per type)

#### `SciQLopSpanBase`

**File:** `include/SciQLopPlots/Items/SciQLopSpanBase.hpp` + `src/SciQLopSpanBase.cpp`

Inherits `SciQLopMovableItemInterface`. Holds `impl::SpanBase*` pointer, forwards shared methods.

**Methods moved into SciQLopSpanBase:**
- `set_visible`, `visible`, `set_color`, `color`, `set_borders_color`, `borders_color`
- `set_selected`, `selected`, `set_read_only`, `read_only`
- `set_tool_tip`, `tool_tip`, `replot`
- Destructor logic (remove item from plot)
- `parentPlot()`

**Stays in each derived wrapper:**
- Range getters/setters
- Signal connections in constructor (range-changed, border selection)
- Typed `_impl` pointer for range-specific calls

**Not in bindings.xml** — only SciQLopVerticalSpan is exposed and keeps its declaration.

---

## REFAC-03: set_x_axis / set_y_axis Free Function

### Problem
Identical cast-store-forward pattern in 5 classes. REFAC-01 absorbs ColorMap/Histogram2D. Three remain: SciQLopLineGraph, SciQLopSingleLineGraph, SciQLopCurve.

### Design
Free function in `include/SciQLopPlots/Plotables/AxisHelpers.hpp`:

```cpp
inline void apply_axis(SciQLopPlotAxis*& stored,
                       SciQLopPlotAxisInterface* axis,
                       auto&& apply_to_plottable)
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        stored = qcp_axis;
        apply_to_plottable(qcp_axis->qcp_axis());
    }
}
```

Each class's `set_x_axis`/`set_y_axis` becomes a one-liner passing a lambda for the plottable-specific forwarding:

- **LineGraph:** `[this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); }`
- **SingleLineGraph:** `[this](auto* a) { if (_graph) _graph->setKeyAxis(a); }`
- **Curve:** `[this](auto* a) { for (auto p : m_components) qobject_cast<QCPCurve*>(p->plottable())->setKeyAxis(a); }`

Free function chosen over base class because the forwarding logic differs structurally across these 3 classes.

---

## REFAC-04: _got_first_data Helper in Base

### Problem
Same 4-line `if (!_got_first_data && n > 0) { _got_first_data = true; emit request_rescale(); }` pattern in 4 classes.

### Design
Add protected member and helper to `SciQLopPlottableInterface`:

```cpp
protected:
    bool _got_first_data = false;

    void check_first_data(std::size_t n)
    {
        if (!_got_first_data && n > 0)
        {
            _got_first_data = true;
            Q_EMIT request_rescale();
        }
    }
```

**Users:** SciQLopLineGraph, SciQLopSingleLineGraph, SciQLopColorMapBase — call `check_first_data(n)`.

**Exception:** SciQLopHistogram2D keeps its own inline logic (defers rescale to after async pipeline, calls `rescaleDataRange()` first). It uses the inherited `_got_first_data` member directly.

Remove `_got_first_data` declarations from all 4 derived classes.

---

## REFAC-05: MultiPlotsVerticalSpan Naming

### Problem
`MultiPlotsVerticalSpan` uses `get_*/is_*` prefixes while all single-plot span types use bare accessors.

### Design
Clean break rename, no deprecation aliases:

| Current | New |
|---------|-----|
| `get_id()` | `id()` |
| `is_selected()` | `selected()` |
| `get_color()` | `color()` |
| `is_visible()` | `visible()` |
| `get_tool_tip()` | `tool_tip()` |
| `is_read_only()` | `read_only()` |

`range()` — no change (already bare accessor).

**Files:** header, source, bindings.xml (check for explicit method refs), all callers.

---

## Dependency Order

1. **REFAC-04** first (adds `_got_first_data` + `check_first_data` to base) — REFAC-01 uses it.
2. **REFAC-01** (ColorMapBase) — depends on REFAC-04. Uses its own `plottable()` virtual for axis forwarding (not REFAC-03).
3. **REFAC-03** (AxisHelpers.hpp) — independent of REFAC-01. Covers the 3 graph classes that REFAC-01 doesn't touch.
4. **REFAC-02** (SpanBase + SciQLopSpanBase) — independent of all above.
5. **REFAC-05** (naming) — independent of all above.

Items 2-3 can run in parallel after item 1. Items 4-5 can run in parallel with everything.

---

## Risk Assessment

- **Binding breakage:** Low. No new classes exposed to Shiboken. REFAC-05 renames methods on an exposed class but the old names are not in any stable public API.
- **Behavioral change:** None intended. All refactors are structural only.
- **Build breakage:** Medium during implementation (many files touched). Incremental compilation recommended.
- **Test coverage:** Existing reproducer tests (test_p0_bugs.py, test_p1_bugs.py) plus manual gallery tests cover the affected code paths.
