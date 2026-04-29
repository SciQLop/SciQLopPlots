# Inspector Properties Expansion — Design

**Date**: 2026-04-29
**Scope**: Expand the SciQLopPlots inspector so users can configure more properties of plots, plotables, and axes from the GUI. Most notably: expose Histogram2D binning, ColorMap contour controls, plot axis range/label, and plot-level toggles (crosshair, equal aspect, scroll factor).

## Goals

- Surface existing C++ configuration knobs that have no GUI exposure today
- Add `marker_size` (and improve `marker_pen` exposure) on the graph component interface, since marker controls without size are limited
- Keep the delegate hierarchy mirrored on the plotable hierarchy (no logic duplication between ColorMap and Histogram2D delegates)
- Maintain the existing widget/signal pattern (signal-blocker reverse path, `QFormLayout` rows, `QGroupBox` for groups of 2+ related fields)

## Non-goals

- Theme/palette editing (separate object, separate delegate, separate spec)
- Manual list-based contour level editing (auto count covers the common case)
- Date/time-aware range editor for time axes (needs its own design pass)
- Plot title (no plot-level title API exists)

## Delegate Hierarchy

```
PropertyDelegateBase
├── SciQLopColorMapBaseDelegate     [NEW]   shared rows for ColorMap and Histogram2D
│   ├── SciQLopColorMapDelegate     [edit]  + auto_scale_y + Contours group
│   └── SciQLopHistogram2DDelegate  [NEW]   + Binning group + Normalization
├── SciQLopGraphComponentDelegate   [edit]  + Marker group (pen color, size)
├── SciQLopPlotAxisDelegate         [edit]  + label + Range group (non-time, non-color-scale)
├── SciQLopPlotDelegate             [edit]  + crosshair + equal aspect + scroll factor
└── SciQLopWaterfallDelegate        [edit]  retrofitted to use Offsets QGroupBox for consistency
```

`SciQLopColorMapBaseDelegate::compatible_type = SciQLopColorMapBase`. The base ctor renders shared rows; subclasses call the base ctor then append their own rows.

The delegate registry resolves the most-derived registered match; both `SciQLopColorMapDelegate` and `SciQLopHistogram2DDelegate` register independently of the base.

## Per-Delegate Field Inventory

### `SciQLopColorMapBaseDelegate` (shared)

| Field | Widget | Notes |
|---|---|---|
| Color gradient | `ColorGradientDelegate` | Moved from existing colormap delegate |

Log-z is **not** added here — it is already exposed via the z-axis's own `SciQLopPlotAxisDelegate`.

### `SciQLopColorMapDelegate` (extends base)

| Field | Widget | Notes |
|---|---|---|
| Auto scale Y | `BooleanDelegate` | Loose row |
| **Contours** group: |  |  |
| Auto level count | `QSpinBox` (0 = manual) | Range 0–50 |
| Contour color | `ColorDelegate` | Maps to `set_contour_color` |
| Contour width | `QDoubleSpinBox` | Range 0.1–10.0, decimals 1 |
| Show labels | `BooleanDelegate` | Maps to `set_contour_labels_enabled` |

### `SciQLopHistogram2DDelegate` (extends base, NEW)

| Field | Widget | Notes |
|---|---|---|
| **Binning** group: |  |  |
| Key bins | `QSpinBox` | Range 1–100000 |
| Value bins | `QSpinBox` | Range 1–100000 |
| Normalization | `QComboBox` | `None` / `Per-column` (maps to `QCPHistogram2D::Normalization`) |

### `SciQLopGraphComponentDelegate` (extended)

| Field | Widget | Notes |
|---|---|---|
| Existing `LineDelegate` | unchanged | Width, line style, color, marker shape |
| **Marker** group (NEW): |  |  |
| Marker pen color | `ColorDelegate` | Wired to `set_marker_pen` (existing setter) |
| Marker size | `QDoubleSpinBox` | Range 1.0–30.0, decimals 1; needs new interface method |

### `SciQLopPlotAxisDelegate` (extended)

| Field | Widget | Notes |
|---|---|---|
| Tick labels visible | `BooleanDelegate` | Existing |
| Log scale | `BooleanDelegate` | Existing, skipped for time axes |
| Color gradient | `ColorGradientDelegate` | Existing, color-scale axes only |
| Label | `QLineEdit` | NEW; all axis types |
| **Range** group (NEW): |  |  |
| Min | `QDoubleSpinBox` | Skipped for time and color-scale axes |
| Max | `QDoubleSpinBox` | Same |

Range group uses `editingFinished` rather than `valueChanged` to avoid firing mid-typing. When the model emits `range_changed`, both spinboxes update atomically under `QSignalBlocker`.

### `SciQLopPlotDelegate` (extended)

| Field | Widget | Notes |
|---|---|---|
| Legend visibility | `LegendDelegate` | Existing |
| Auto scale | `BooleanDelegate` | Existing |
| Crosshair enabled | `BooleanDelegate` | NEW |
| Equal aspect ratio | `BooleanDelegate` | NEW |
| Scroll factor | `QDoubleSpinBox` | NEW; range 1.0–10.0, decimals 2 |

### `SciQLopWaterfallDelegate` (consistency retrofit)

Wrap existing Mode / Uniform spacing / Offsets list rows in an **Offsets** `QGroupBox`. `Normalize` and `Gain` stay loose. No behavior change.

## Required Plotable / Interface Changes

| Type | Addition |
|---|---|
| `SciQLopGraphComponentInterface` | Add abstract `set_marker_size(qreal)` and `marker_size() const` |
| `SciQLopGraphComponent` | Implement `set_marker_size` / `marker_size` using `QCPScatterStyle::setSize` via the same variant visit pattern as `set_marker_pen` |
| `SciQLopGraphComponentInterface` | Emit `marker_pen_changed(QPen)` and `marker_size_changed(qreal)` from concrete setters |
| `SciQLopHistogram2D` | Emit `bins_changed(int, int)` and `normalization_changed(int)` |
| `SciQLopColorMap` | Emit `contour_levels_changed`, `contour_pen_changed`, `contour_labels_enabled_changed` (auto count, pen, labels) |
| `SciQLopPlot` | Verify / add `crosshair_enabled_changed`, `equal_aspect_ratio_changed`, `scroll_factor_changed` signals |

## Signal Wiring

For every new control, two-way binding follows the Waterfall delegate pattern:

1. **Widget → model**: connect widget's edit signal to plotable setter
2. **Model → widget**: connect plotable's `xxx_changed` signal to widget setter, **wrapped in `QSignalBlocker`** to prevent the rebuild cycle

## Build & Registration

**`meson.build`** — append to the inspector sources list:
- `src/SciQLopColorMapBaseDelegate.cpp`
- `src/SciQLopHistogram2DDelegate.cpp`

**`src/Registrations.cpp`** — append:
- `delegates.register_type<SciQLopColorMapBaseDelegate>();`
- `delegates.register_type<SciQLopHistogram2DDelegate>();`

The existing `SciQLopColorMapDelegate` registration stays — it now inherits from the base.

## Python Bindings

`bindings.xml` updates:
- Verify `SciQLopHistogram2D::set_bins` and `set_normalization` are exposed (they are public C++ — re-check the typesystem)
- Add bindings for the new `SciQLopGraphComponentInterface::set_marker_size` / `marker_size`

Internal `*_changed` signals do not need explicit binding entries unless Python tests subscribe to them.

## Testing

**File**: `tests/manual-tests/test_inspector_properties.py`. Run via `meson test -C build` from the SciQLop venv (per project test conventions).

### Per-control tests (×~25 controls)

For each control:
1. **Initial state**: construct plotable → instantiate delegate → assert widget reflects model's current value
2. **Forward path**: change widget programmatically → assert model property updated
3. **Reverse path**: change model from code → assert widget reflects new value
4. **Signal-loop guard**: change widget once, count emit fires, assert no recursive feedback

### Per-delegate dispatch tests (×6 delegates)

- Selecting a `SciQLopHistogram2D` resolves to `SciQLopHistogram2DDelegate` (not base, not colormap delegate)
- Selecting a `SciQLopColorMap` resolves to `SciQLopColorMapDelegate`
- Both subclasses share the gradient row inherited from `SciQLopColorMapBaseDelegate`

### Axis-type guard tests

| Axis type | Range group | Log row | Gradient row | Label row |
|---|---|---|---|---|
| Numeric | yes | yes | no | yes |
| Time | **no** | **no** | no | yes |
| Color-scale | **no** | yes | yes | yes |

### Group rendering tests

For each delegate with `QGroupBox` children, find each box by title, assert it contains the expected child widgets in the expected order.

### Edge cases

- Histogram2D `set_bins(1, 1)` — minimum widget value accepted
- Histogram2D `set_bins(100000, 100000)` — upper bound clamp
- ColorMap auto-contour count = 0 — manual mode reflected
- Plot scroll factor at boundaries (1.0, 10.0)
- Axis range with min ≥ max — model rejects gracefully, widget snaps back to last valid range

### Multi-instance isolation

Two plots, each with a histogram — edit bins on plot A, assert plot B's histogram is unchanged.

### Waterfall regression

After the group retrofit, all existing Waterfall tests still pass; mode / spacing / offsets / normalize / gain still drive the model.

**Estimated coverage**: ~30 test functions, ~80–100 assertions.

## Risks

- **Widget feedback loops**: forward+reverse binding without `QSignalBlocker` produces infinite emit loops. Mitigated by the existing pattern and the loop-guard tests.
- **`set_range` mid-typing**: the user typing `100` into a min spinbox would briefly fire `set_range(1, max)`, `set_range(10, max)`, `set_range(100, max)`. Mitigated by using `editingFinished` for both range spinboxes.
- **Bindings drift**: new signals added to `SciQLopGraphComponentInterface` may need typesystem updates if Python tests use them. Mitigated by a binding-build smoke check after the interface change.

## Out of scope (future work)

- Manual contour level list editor (per-level value editing)
- Date/time-aware axis range editor for time axes
- Plot title and theme/palette editor
- Live ColorMap interpolation toggle
- Per-axis font and color
