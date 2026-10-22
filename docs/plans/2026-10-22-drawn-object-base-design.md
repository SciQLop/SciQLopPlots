# One base for everything drawn

Status: design for API review item 5.3 (docs/api-review-2026-10-20.md), Phase 4, part A. Part B (item factories, `plot.remove(x)`, one ownership model: items 5.2 and phase-2 task 12) gets its own spec and builds on this one.

## Why

Every drawn family declares `visible`, `selected`, `name` and `color` on its own, with different spellings and different signals:

- Plottables have `visible_changed`, `selection_changed` and `name_changed`. Items have no signals at all. Spans emit the camelCase `selectionChanged`. The legend uses `is_visible` and `visibility_changed`.
- Plot groups and the inspector find out what an object can do with `qobject_cast` chains (`qcp_plottables_of`, src/SciQLopGraphInterface.cpp:43-64; the `set_selected` lambdas in src/Registrations.cpp).
- Some setters are silent stubs: text, ellipse and pixmap `set_tool_tip`, text and ellipse `set_movable` (`WARN_ABSTRACT_METHOD`).
- `SciQLopTimeline::set_visible` never emits `visible_changed` (src/SciQLopTimeline.cpp:213-218).
- The Python `name` property writes `objectName` directly and bypasses `set_name()`, so `m_user_named` stays false (SciQLopGraphInterface.hpp:58-61).
- The `name` property is declared 21 times in `bindings.xml`.

## Scope

In:

1. A QObject base, `SciQLopDrawnObject`, that every drawn family derives from.
2. The four properties on it, each with a getter, a setter and a signal.
3. Plot groups and the inspector call the base instead of casting.
4. The three bugs above, each with its own reproducer.
5. Deprecated aliases for the renamed signals and getters.

Out:

- `plot.remove(x)`, item factories, ownership: part B.
- Observing these properties through `.on`: item 5.1.
- Unifying the span classes: item 5.6.
- Axes: they keep `label`, and they are not drawn objects here.
- Legend rows: they are NeoQCP legend entries, not objects. Groups and `set_in_legend` cover what users do with them.
- `tooltip`, `movable`, `read_only`/`editable` on the base: item 5.9. Only the silent stubs are fixed here.

## The base

```cpp
class SciQLopDrawnObject : public QObject
{
    Q_OBJECT
public:
    bool visible() const;          void set_visible(bool visible);
    bool selected() const;         void set_selected(bool selected);
    QString name() const;          void set_name(const QString& name);
    QColor color() const;          void set_color(const QColor& color);

    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void selection_changed(bool selected);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void color_changed(const QColor& color);

protected:
    virtual bool _visible() const;     virtual void _set_visible(bool);
    virtual bool _selected() const;    virtual void _set_selected(bool);
    virtual QColor _color() const;     virtual void _set_color(const QColor&);
};
```

Rules:

1. **The public setter owns the signal.** It queues to the owner thread (`SCIQLOP_ON_OWNER_THREAD`, include/SciQLopPlots/ThreadGuard.hpp:76), returns when the value is unchanged, calls the family's `_set_x`, then emits. Families never emit these four signals themselves, so a missing or doubled signal can't happen.
2. **A state change that doesn't come from the setter still signals.** When NeoQCP changes the state itself (a click selects an item, a group member hides), the family calls a protected `_notify_x(value)` that emits. A click-select must emit `selection_changed` exactly once.
3. **Unsupported means raise.** The default `_x` / `_set_x` throw `sqp::unsupported_call` (include/SciQLopPlots/Debug.hpp), which the bindings map to `NotImplementedError` naming the class. Nothing warns and does nothing.
4. **`name` is `objectName`.** `name_changed` follows `objectNameChanged`. Every family gets a name for free. The Python `name` property maps to `name()` / `set_name()`, declared once on the base in `bindings.xml`.
5. **No new storage.** State stays where it is today (the NeoQCP item, the graph components). The base routes calls and signals.

Families that already derive `QObject` directly derive `SciQLopDrawnObject` instead: `SciQLopPlottableInterface`, `SciQLopGraphComponentInterface`, `SciQLopItemInterface`, `SciQLopStraightLine`, `SciQLopMultiPlotObject`, `SciQLopPlotGroup`, `SciQLopPlotLegendInterface`. Spans already derive `SciQLopItemInterface` through `SciQLopRangeItemInterface` / `SciQLopMovableItemInterface`. `MultiPlotsVSpanCollection` derives `SciQLopMultiPlotObject` but draws nothing itself: its `_x` stay unsupported.

## What each family supports

✓ supported; "raise" = `NotImplementedError` naming the class.

| Family | visible | selected | name | color |
|---|---|---|---|---|
| Line graph, curve, waterfall, ND projection curves | ✓ | ✓ | ✓ | raise ("use colors()") |
| Graph component | ✓ | ✓ | ✓ | ✓ (gains `color_changed`) |
| Colour map, 2-D histogram | ✓ | ✓ | ✓ | raise (gradient) |
| Histogram | ✓ | ✓ | ✓ | raise ("use colors()") |
| Timeline | ✓ (signal bug fixed) | ✓ | ✓ | raise (category colours) |
| Plot group | ✓ (any member visible; signal when that changes, members included) | raise | ✓ (gains `name_changed`) | raise |
| Text, curved line, straight / vertical / horizontal line | ✓ | ✓ (NeoQCP item selection) | ✓ | ✓ |
| Ellipse | ✓ | ✓ | ✓ | ✓ (outline colour) |
| Pixmap | ✓ | ✓ | ✓ | raise |
| Vertical, horizontal, rectangular span | ✓ | ✓ (signal renamed) | ✓ | ✓ |
| `MultiPlotsVerticalSpan` | ✓ | ✓ | ✓ | ✓ |
| `MultiPlotsVerticalLine` | ✓ | raise | ✓ | ✓ |
| `MultiPlotsVSpanCollection` | raise | raise | ✓ | raise |
| Legend (as a whole) | ✓ (`visibility_changed` → `visible_changed`) | raise | ✓ | ✓ |

Notes:

- Every NeoQCP item already has `setSelected` / `selected`; items forward to it. Making an item selectable by click is not part of this item: only the property and its signal.
- A multi-line graph has no single colour. `graph.color` raises and names `colors()`. A single line's colour is `graph.component(0).color`.
- Existing setters that already signal (plottables, components, axes) lose their own `Q_EMIT` for these four signals, so nothing doubles.

## Compatibility

- `selectionChanged` on the three span classes stays as a deprecated alias of `selection_changed` (today's Python alias at `__init__.py:615-617` becomes the deprecation shim, warning on connect).
- The legend's `is_visible()` and `visibility_changed` stay as deprecated aliases. SciQLop calls `legend().is_visible()` (SciQLop/user_api/plot/_plots.py:501).
- `MultiPlotsVerticalSpan` / `MultiPlotsVerticalLine` keep their Python properties (`span.selected = True`, used by SciQLop's catalogue overlay).
- No positional signature changes. No keyword renames.
- New behaviour a caller might notice: `color` / `selected` on a family that can't do it raises `NotImplementedError` where the method didn't exist before (an `AttributeError`). The silent stubs (`set_tool_tip`, `set_movable` on text, ellipse, pixmap) start working.

## Callers simplified

- `SciQLopPlotGroup::visible()` / `set_visible()` loop over members through the base.
- The inspector's `TypeDescriptor.set_selected` lambdas (src/Registrations.cpp) collapse into one call through the base for every registered drawn family.
- `qcp_plottables_of` stays: it maps a wrapper to its NeoQCP plottables, which the base doesn't know about.

## Bindings

- `name` declared once on `SciQLopDrawnObject` in `bindings.xml`, the 21 copies removed.
- Changing the base of bound classes renumbers shiboken converters: the first build after the switch removes every generated `*_wrapper.cpp` and reconfigures (the Phase 1 trap: stale wrappers segfault).
- `NotImplementedError` mapping: the getters and setters of the base get the `returning_raises_python_errors` / `void` equivalent template, so `sqp::unsupported_call` reaches Python from every family.

## Tests

- **Contract test**, parametrised over the table above (one row per family, built by a factory per family). For each ✓ cell: set → get round trip; the signal fires exactly once; setting the same value fires nothing; a call from a worker thread is queued to the GUI thread and applied. For each "raise" cell: `NotImplementedError` naming the class, for the getter and the setter. A drawn family missing from the table fails the test (the test lists every bound subclass of `SciQLopDrawnObject`).
- **A state change from NeoQCP signals once:** selecting an item through NeoQCP (not through `set_selected`) emits `selection_changed` once; hiding a group's last visible member emits the group's `visible_changed`.
- **Reproducers first**, each failing before its fix: timeline `visible_changed`; text, ellipse and pixmap `tool_tip` / `set_tool_tip` and text / ellipse `set_movable`; the `name` property calls `set_name` (`m_user_named` set, observable through whatever reads it).
- **Deprecation:** `span.selectionChanged` connects and warns; `legend.is_visible()` / `legend.visibility_changed` work and warn.
- **Unchanged:** the plot-group tests, the inspector tests, test_item_ownership, the projection and span tests pass as they are.
- **Gate:** the SciQLopPlots suite; SciQLop's full suite and the three plugins (msa, radio, sismo) against the build; an opus and an opencode review of the branch.
