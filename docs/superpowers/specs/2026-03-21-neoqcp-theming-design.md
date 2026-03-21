# NeoQCP Theming Passthrough — Design Spec

## Goal

Expose NeoQCP's `QCPTheme` API to SciQLopPlots users (C++ and Python) as a thin wrapper, enabling light/dark themes and custom color palettes on plots and panels.

## Scope

Minimal passthrough — no extended theming (graph color palettes, gradient defaults), no OS palette detection, no inspector integration. Just wrap what NeoQCP already provides.

## Why a wrapper instead of exposing QCPTheme directly

NeoQCP types are never exposed directly in SciQLopPlots' public API — all NeoQCP concepts are wrapped (e.g., `ColorGradient` enum, `SciQLopOverlay`). A `SciQLopTheme` wrapper maintains this convention and allows snake_case naming consistent with the rest of the API.

## C++ Layer

### SciQLopTheme class

New class in `include/SciQLopPlots/SciQLopTheme.hpp`, implementation in `src/SciQLopTheme.cpp`.

Owns a `QCPTheme*` internally. Exposes:

**Static factories:**
- `static SciQLopTheme* light(QObject* parent = nullptr)`
- `static SciQLopTheme* dark(QObject* parent = nullptr)`

**Color properties (get/set):**
- `background` — QColor
- `foreground` — QColor
- `grid` — QColor
- `sub_grid` — QColor
- `selection` — QColor
- `legend_background` — QColor
- `legend_border` — QColor

**Busy indicator properties (get/set):**
- `busy_indicator_symbol` — QString
- `busy_fade_alpha` — qreal
- `busy_show_delay_ms` — int
- `busy_hide_delay_ms` — int

**Signal:**
- `changed()` — forwarded from internal `QCPTheme::changed()`

All property setters forward to `QCPTheme` and the reactive signal chain handles replot automatically.

### Integration points

**SciQLopPlotInterface** (virtual, with `WARN_ABSTRACT_METHOD` default):
- `virtual void set_theme(SciQLopTheme* theme)`
- `virtual SciQLopTheme* theme() const`

**SciQLopPlot** (override):
- `void set_theme(SciQLopTheme* theme)` — stores a `QPointer<SciQLopTheme>` and calls `setTheme(theme->qcp_theme())` on the underlying QCustomPlot
- `SciQLopTheme* theme() const` — returns stored theme pointer (nullptr if using default)

**SciQLopMultiPlotPanel** (concrete):
- `void set_theme(SciQLopTheme* theme)` — stores a `QPointer<SciQLopTheme>` as the panel-level theme, then iterates child plots via `for (auto& p : plots()) { ... dynamic_cast<SciQLopPlot*> ... }` and calls `set_theme()` on each. Also connects to `plot_added` signal so newly-added plots receive the panel theme automatically.
- `SciQLopTheme* theme() const` — returns the panel-level theme (or nullptr)

### Ownership model

- `SciQLopTheme` factory methods create objects with optional Qt parent ownership
- When `set_theme()` is called on a panel, the panel reparents the `SciQLopTheme` to itself (prevents premature GC from Python side)
- `SciQLopPlot` and `SciQLopMultiPlotPanel` store `QPointer<SciQLopTheme>` to handle the case where a theme is destroyed externally — null pointer results in falling back to QCustomPlot's default owned theme

### Per-plot override semantics

`panel.set_theme()` unconditionally applies the theme to **all** child plots, overwriting any per-plot override. To maintain a per-plot override, call `plot.set_theme()` **after** `panel.set_theme()`. This is the simplest behavior and avoids tracking per-plot override state.

### Internal accessor

`SciQLopTheme` exposes a `QCPTheme* qcp_theme() const` method for internal use by `SciQLopPlot`. This is rejected in Python bindings.

## Python Bindings

Add `SciQLopTheme` as an `object-type` to `bindings.xml`:

```xml
<object-type name="SciQLopTheme">
    <modify-function signature="qcp_theme()const" remove="all"/>
    <!-- static factories, color/busy getters+setters, changed() signal -->
</object-type>
```

On `SciQLopPlotInterface`, add `set_theme(SciQLopTheme*)` and `theme()`.

On `SciQLopMultiPlotPanel`, add `set_theme(SciQLopTheme*)` with parent transfer annotation:

```xml
<modify-function signature="set_theme(SciQLopTheme*)">
    <modify-argument index="1">
        <parent index="this" action="add"/>
    </modify-argument>
</modify-function>
```

### Python usage example

```python
from SciQLopPlots import SciQLopTheme

# Use a preset
theme = SciQLopTheme.dark()
panel.set_theme(theme)

# Customize from preset
theme = SciQLopTheme.dark()
theme.set_background(QColor("#2d2d2d"))
theme.set_foreground(QColor("#f0f0f0"))
panel.set_theme(theme)

# Per-plot override (must be after panel.set_theme)
special_theme = SciQLopTheme.light()
panel.plot_at(0).set_theme(special_theme)
```

## Testing

Add a manual test script `tests/manual-tests/test_theming.py` that:
1. Creates a `SciQLopMultiPlotPanel` with 2-3 plots containing sample data
2. Applies `SciQLopTheme.dark()` to the panel
3. Adds a button/timer to toggle between light and dark
4. Demonstrates per-plot theme override on one plot

## Files to create/modify

| File | Action |
|------|--------|
| `include/SciQLopPlots/SciQLopTheme.hpp` | Create — new wrapper class |
| `src/SciQLopTheme.cpp` | Create — implementation |
| `include/SciQLopPlots/SciQLopPlotInterface.hpp` | Modify — add virtual `set_theme()` / `theme()` with `WARN_ABSTRACT_METHOD` |
| `include/SciQLopPlots/SciQLopPlot.hpp` | Modify — override `set_theme()` / `theme()` |
| `src/SciQLopPlot.cpp` | Modify — implement theme methods |
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` | Modify — add `set_theme()` / `theme()` + `QPointer<SciQLopTheme>` member |
| `src/MultiPlots/SciQLopMultiPlotPanel.cpp` | Modify — implement theme propagation + `plot_added` connection |
| `SciQLopPlots/bindings/bindings.xml` | Modify — add `SciQLopTheme` object-type, theme methods on interfaces, parent transfer annotations |
| `SciQLopPlots/bindings/bindings.h` | Modify — add `#include <SciQLopPlots/SciQLopTheme.hpp>` |
| `src/meson.build` | Modify — add SciQLopTheme.cpp to sources |
| `tests/manual-tests/test_theming.py` | Create — demo/test script |
