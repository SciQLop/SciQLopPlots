# Hide / show curves & graph components — design

Date: 2026-06-08
Branch: `feat/hide-curve-visibility`

## Goal

Restore the ability to hide (and re-show) any curve / graph from two entry points:

1. **The inspector** — a "Visible" checkbox on each plotable component (line graphs,
   curves, ND-projection curves) and on color maps.
2. **The legend** — double-clicking a legend entry toggles that entry's visibility,
   **component by component** for multi-component graphs.

Both entry points must stay in sync: toggling from the legend updates the inspector
checkbox and vice-versa, and the legend entry dims when hidden regardless of which
path triggered it.

## Existing infrastructure (already present, reused as-is)

- `SciQLopGraphComponentInterface::set_visible(bool)` / `visible()` / `visible_changed(bool)`.
- `SciQLopGraphComponent::set_visible()` already handles both cases correctly:
  - single plottable → `m_plottable->setVisible(...)`,
  - multi-component → `mg->component(i).visible = ...`,
  - and emits `visible_changed` + `replot`.
- `SciQLopColorMapBase` has `set_visible` / `visible` / `visible_changed`.
- `SciQLopGraphComponent::_legend_item()` / `_group_legend_item()` already resolve a
  component to its QCP legend entry.
- `BooleanDelegate` (a `QCheckBox` with a `value`/`set_value`/`value_changed` API).

The plumbing exists; only the **UI affordances** that drive it are missing/broken.

## Approach (B — chosen)

Per-component legend behavior already lives inside `SciQLopGraphComponent` (it owns
`selectionChanged` and group `componentClicked` wiring). Visibility-on-double-click is
the only legend behavior that currently lives up in `SciQLopPlot`, poking QCP directly —
which is exactly why it never synced. We move it down into the component to match the
existing pattern.

### 1. Inspector checkboxes

`SciQLopGraphComponentDelegate` (covers line graphs, curves, ND-projection curves — they
share `SciQLopGraphComponentInterface`):

- Add `BooleanDelegate("Visible", component->visible())` as the first row.
- Forward: `value_changed` → `component->set_visible`.
- Reverse: `component->visible_changed` → `checkbox->set_value` wrapped in `QSignalBlocker`
  to avoid feedback loops.

`SciQLopColorMapBaseDelegate`: same checkbox, wired to the color map's existing
`set_visible` / `visible_changed`.

### 2. Legend double-click → component

In `SciQLopGraphComponent`'s constructors:

- Connect the parent plot's `QCustomPlot::legendDoubleClick(QCPLegend*, QCPAbstractLegendItem*, QMouseEvent*)`
  signal to a member slot.
- The slot decides "is this double-click mine?":
  - single-plottable component: the clicked item is this component's `_legend_item()`;
  - multi-component: the clicked item is the group item and its `selectedComponent()`
    equals this component's index.
- When it's a match, call `set_visible(!visible())`. All sync flows from the resulting
  `visible_changed`.

Legend dimming is driven off the signal, not the click:

- Connect `visible_changed` → a small helper that sets the legend item text color
  (normal when visible, gray when hidden). This dims correctly whether the toggle came
  from the legend or the inspector.

### 3. Remove the old path

- Delete `SciQLopPlot::_legend_double_clicked` and the
  `connect(this, &QCustomPlot::legendDoubleClick, this, &SciQLopPlot::_legend_double_clicked)`
  in the `SciQLopPlot` constructor. Its responsibilities now live in the component.

### Group "header" (no component selected) behavior

The old handler had a branch: double-clicking the group header (no specific component
selected) toggled all-on / all-off. With Approach B each component only owns its own
index, so a header double-click (where `selectedComponent() < 0`) is a no-op. This keeps
the model simple and unambiguous — each legend row toggles exactly one thing. (If a
"toggle all" affordance is wanted later it can be added at the graph level, but it is out
of scope here per YAGNI.)

## Files touched

- `src/SciQLopGraphComponentDelegate.cpp` — add Visible checkbox + reverse wiring.
- `src/SciQLopColorMapBaseDelegate.cpp` — add Visible checkbox + reverse wiring.
- `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` — declare the double-click
  slot + legend-dimming helper.
- `src/SciQLopGraphComponent.cpp` — connect `legendDoubleClick` + `visible_changed`,
  implement the slot/helper.
- `src/SciQLopPlot.cpp` + `include/SciQLopPlots/SciQLopPlot.hpp` — remove
  `_legend_double_clicked` and its connection.

## Testing (TDD — tests written first, must fail before, pass after)

Integration tests (pytest, `tests/integration`):

1. **Single-component line graph**: `component.set_visible(False)` flips `component.visible()`
   to `False` and the underlying plottable is hidden; `set_visible(True)` restores it.
2. **Multi-component graph**: toggling one component's visibility leaves the others
   untouched (per-component independence).
3. **Color map**: `set_visible(False)`/`True` round-trips via `visible()`.
4. **Signal sync**: connecting to `visible_changed` and calling `set_visible` emits the
   new value exactly once (this is the contract the inspector checkbox and legend dimming
   both rely on).

Legend double-click is a GUI mouse interaction; we test the contract it depends on
(`visible_changed` emission + `set_visible` semantics) rather than synthesizing legend
mouse events. The double-click → `set_visible` wiring is verified manually in the app.

## Out of scope

- A "toggle all components" affordance.
- Persisting visibility across save/load.
- Any change to how hidden curves participate in autoscale (unchanged from today).
