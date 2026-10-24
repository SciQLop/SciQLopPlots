# One span class, one line class

Status: design, 2026-10-25, revision 3 after two opencode reviews.

This is API review item 5.6, and row 1.15 (docs/api-review-2026-10-20.md). The decisions:
- one span class and one line class, with a plot or panel scope;
- `span()` plus the vertical and horizontal shorthands, with `rectangular_span` deprecated;
- accessors are methods;
- lines are read-only by default;
- two stages: **Plan A** adds the new model behind the old names (this spec). **Plan B** removes the old names last, together with the SciQLop patch (outlined at the end).

A bug round lands first: pixel-mode drag, panel line selection, collection visibility, loop snapshots. The facts below come from the survey of the current code.

## What is wrong today

1. **Seven classes for two concepts.**
   - Spans: `SciQLopVerticalSpan`, `SciQLopHorizontalSpan`, `SciQLopRectangularSpan`, `MultiPlotsVerticalSpan`.
   - Lines: `SciQLopStraightLine`, its two orientation subclasses, `MultiPlotsVerticalLine`.
2. **One concept, several names.**
   - The range is `range` on vertical and horizontal spans, `key_range`/`value_range` on rectangular ones.
   - Editability is `read_only` on spans and panel lines, `movable` on plot lines (opposite sense).
   - `.on.tooltip` works on the vertical span only.
3. **Methods or properties by class.** Per-plot spans use methods; the panel span uses properties. So `span.selected = True` silently does nothing on a per-plot span.
4. **Uneven features.**
   - Coordinates and `pixel_range` exist on the vertical span only.
   - Lines have no coordinates getter.
   - Panel objects have no borders or line style.
   - Lines have no `delete_requested`.
5. **Missing kinds.** There's no horizontal or rectangular panel span and no horizontal panel line.
6. **Dead or stub API.**
   - Spans carry raising `movable`/`position`/`move` stubs.
   - The border-selection signals never fire from user input.
7. **Defaults differ by scope.**
   - Plot spans are alpha 100, panel spans alpha 255.
   - Plot lines are black and fixed, panel lines grey and movable.

## Plan A: the model

### `SciQLopSpan`

A new class derived from `SciQLopDrawnObject`. It does not derive from `SciQLopItemInterface`: that interface's drawn hooks and `_follow_selection` are written against one QCP item (Items/SciQLopPlotItem.hpp:154-207), and a panel-scope span has zero or many.

- **Bounded axes.** A span bounds the key axis, the value axis, or both. Which, is fixed at creation.
  - Why fixed: NeoQCP has three span items (QCPItemVSpan/HSpan/RSpan). Swapping the item would rebuild selection, signals and the GPU layer mid-life.
  - `kind()` returns the new enum `SpanKind`: `Vertical` (key bounded), `Horizontal` (value bounded) or `Rectangular` (both).
  - `key_range()` / `value_range()` return the range, or None for an unbounded axis.
  - `set_key_range` / `set_value_range` raise ValueError on an unbounded axis.
  - `key_range_changed` / `value_range_changed` fire for the axes the span bounds.
  - Python reads `key_range() is None` and passes `span(key_range=None, ...)`. The C++ mechanism is settled by the plan's first task, a spike:
    - if shiboken 6.11.2 converts `std::optional<SciQLopPlotRange>` both ways, the getters and the factory arguments use it;
    - otherwise C++ offers `kind()` plus raising getters, and the bindings map None with inject-code on the two getters (`%PYARG_0 = Py_None` when the axis is unbounded) and on the `span()` arguments (`Py_None` means unbounded).

    Either way, the Python behaviour above is the contract.
- **One-axis shortcut.** `range()` / `set_range()` / `range_changed` read and write the bounded axis of a vertical or horizontal span. On a rectangular span they raise ValueError, and the signal never fires. `range_changed` fires together with the matching `key_range_changed` or `value_range_changed`: one user change gives one emission of each.
- **Coordinates.**
  - `coordinates()` / `set_coordinates()` / `coordinates_changed` (Data or Pixels), applied to every bounded axis.
  - `pixel_range()` (one-axis), plus `pixel_key_range()` / `pixel_value_range()`.
- **The rest:**
  - `read_only()` / `set_read_only()` / `read_only_changed`, default False;
  - `tool_tip()` / `set_tool_tip()` / `tool_tip_changed`;
  - `borders_color`, `line_width`, `line_style`, which style the border pen, as today (the guide says so);
  - `id()`, a uuid unless given at creation;
  - `delete_requested`.
- **Scope.** `scope()` returns the owning plot or panel, as `QObject*` (dynamic in Python). `is_panel_part()` is True for the per-plot parts of a panel-scope span.

### `SciQLopStraightLine`

The existing class, extended. It keeps its name, because it already is the one line implementation.

- `position()` / `set_position()` / `position_changed`.
- `orientation()`, fixed at creation.
- `coordinates()` / `set_coordinates()` / `coordinates_changed`. The setter is new work: it re-derives the position in the new coordinates and repaints.
- `read_only()` / `set_read_only()` / `read_only_changed`, default True at both scopes. `movable()` / `set_movable()` stay as inverse aliases, deprecated in Python.
- `tool_tip()` / `set_tool_tip()` / `tool_tip_changed`.
- `line_width`, `line_style`, `color`.
- The min/max clamps, with new getters `min_value()` / `max_value()` (None when unset).
- `delete_requested`: emitted from NeoQCP's Delete key, as spans get it.
- `scope()` and `is_panel_part()`, as for spans.
- It derives from `SciQLopDrawnObject`, as today.

### Scope: one class, two targets

Inside, each class holds a small target, `PlotTarget` or `PanelTarget`. The public class doesn't branch on scope.
- `PlotTarget` owns one QCP item. It implements the drawn hooks (visible, selected) on it, and follows its click selection.
- `PanelTarget` owns one plot-scope part per `SciQLopPlot` in the panel, and implements the drawn hooks over them: state is held by the panel object and written to the parts. It carries over what `MultiPlotsVerticalSpan` / `MultiPlotsVerticalLine` and `SciQLopMultiPlotObject` do today:
  - parts are built when a plot joins and deleted when it leaves (`plot_list_changed`);
  - parts are held as `QPointer`s, because a plot can die before the span hears of it;
  - parts are marked with `mark_panel_part`, so `plot.remove(part)` refuses them;
  - every loop over parts uses the snapshot discipline (`for_each_span`), so a slot deleting a part is safe;
  - a part's user edits (drag, click selection, Delete) flow up and back out to the other parts: `range_changed`, `selection_changed`, `delete_requested`, `position_changed`;
  - **one emission per user edit.** While the panel object writes a change out to its parts, a reentrancy flag on the target ignores the parts' echoes. One drag on one part of a panel span with N plots gives exactly one `range_changed` and one `key_range_changed` from the panel object.

  Panel-scope objects are the panel's QObject children, C++-owned.

### The old names in Plan A

The seven old names stay as **C++ subclasses with no behaviour of their own**:
- `SciQLopVerticalSpan`, `SciQLopHorizontalSpan` and `SciQLopRectangularSpan` (plot scope), and `MultiPlotsVerticalSpan` (panel scope), derive from `SciQLopSpan`;
- `SciQLopVerticalLine` and `SciQLopHorizontalLine` derive from `SciQLopStraightLine`, as today;
- `MultiPlotsVerticalLine` (panel scope) derives from `SciQLopStraightLine`.

Each only fixes the kind and scope in its constructor. They are built only by the old entry points, which all stay as they are:
- the deprecated constructors;
- `MultiPlotsVSpanCollection.create_span`;
- interactive creation (`span_created`);
- the per-plot parts of an old-class panel object.

**The factories return the new classes,** `SciQLopSpan` and `SciQLopStraightLine`, for every kind and scope. Every factory is new on this branch (2026-10-24, unreleased), so this breaks no released caller.

So everything SciQLop uses today keeps the old classes and their behaviour unchanged: `isinstance(x, SciQLopVerticalSpan)`, `findChildren(SciQLopVerticalSpan)`, the `MultiPlotsVerticalSpanPtr` handle and the `__init__.py` tables. Code written against the factories meets one class with methods only.

Consequences:
- The V/H spans stop being `SciQLopRangeItemInterface`, and the rectangular span stops being `SciQLopMovableItemInterface`. Those bound types stay for the other items. Nothing in the repo checks `isinstance` against them for spans; Plan B decides their fate.
- The two old panel classes stop deriving from `SciQLopMultiPlotObject`. Two QObject bases aren't possible, and `SciQLopSpan` is a `SciQLopDrawnObject`. So their plumbing methods `addObject` / `removeObject` / `replotAll` / `updatePlotList`, and the snake-case aliases of those in `_SNAKE_CASE_ALIASES`, go away for these two classes. `MultiPlotsVSpanCollection` keeps them.
- The old panel properties (`range`, `selected`, `color`, `visible`, `tooltip`, `read_only`, `id`, `position`, `line_width`) stay on the two `MultiPlots*` subclasses during Plan A, so SciQLop keeps working. A property shadows the base method of the same name on that subclass: on those objects `span.range` is a value and `span.range()` fails, exactly as today. Objects from the factories are plain `SciQLopSpan` / `SciQLopStraightLine`, with methods only. Plan B drops the properties.

### Defaults of the new factories, the same at both scopes

The old entry points (constructors, `create_span`, `span_created`) keep their current defaults, so SciQLop's spans and cursor lines look and behave as today. The table applies to what the factories build.

| | span | line |
|---|---|---|
| colour | grey (100,100,100), alpha 100 | black |
| read_only | False | True |
| coordinates | Data | Data |

For the factories this means panel spans are alpha 100, and panel lines are black and read-only by default.

## Factories

On `SciQLopPlotInterface` (NotImplementedError by default), `SciQLopPlot` and `SciQLopMultiPlotPanel`, with the same signatures:

| factory | arguments (all defaulted after the first required ones) |
|---|---|
| `span` | `key_range=None, value_range=None, color=grey/100, read_only=False, visible=True, tool_tip="", coordinates=Data, id=""`; ValueError if both ranges are None. **The defaults are defined here only**: the shorthands forward the rest of their arguments with the same names and defaults |
| `vertical_span` | `range, **the rest of span's arguments**` = `span(key_range=range, ...)` |
| `horizontal_span` | the same, = `span(value_range=range, ...)` |
| `rectangular_span` | plots only (it exists today); deprecated with `rectangular_span(...) is deprecated: use plot.span(...)`. Panels don't get it |
| `straight_line` | `position, orientation, color=black, read_only=True, visible=True, tool_tip="", coordinates=Data` |
| `vertical_line` / `horizontal_line` | `position, **the rest of straight_line's arguments**` (the defaults are defined on `straight_line` only) |

The plot line factories took `movable` second. It is replaced by `color`, then `read_only`.
- Positional calls passing a bool second now fail on the colour slot.
- The keyword `movable=` keeps working on the plot-scope line factories, inverted to `read_only`, with `movable=... is deprecated: use plot.vertical_line(..., read_only=...)`.

All new deprecation messages use the form `... is deprecated: use plot.X(...)` / `use panel.X(...)`, so the suite's pyproject filter turns missed call sites into errors. They are emitted from Python wrappers (stacklevel 2), in the existing deprecation tables.

The `SciQLopStraightLine` entry in `_ITEM_FACTORIES` changes its hint to `use plot.straight_line(...)`.

`plot.remove` and `panel.remove` gain `SciQLopSpan*` and `SciQLopStraightLine*` overloads. A wrong-scope object raises ValueError, naming the scope to use. A panel part still raises "part of a panel-wide object".

## Python surface

- **`.on`.** The convention is per class, so on `SciQLopSpan`:
  - `.on.range` resolves on every span and raises on use for a rectangular one;
  - `.on.key_range` / `.on.value_range` read None and raise on write for an unbounded axis.

  The guide states this rule as it does for `color` on multi-column graphs, and tests pin the raises. `.on.coordinates`, `.on.read_only` and `.on.tool_tip` become observable. The `tooltip` exception moves from `SciQLopVerticalSpan` to `SciQLopSpan` and `SciQLopStraightLine`, so `.on.tooltip` works on every span and line. `.on.position` no longer resolves on spans.
- `selectionChanged` moves to `SciQLopSpan`, so every span has it.
- No new smart-pointer type in Plan A. `MultiPlotsVerticalSpanPtr` stays as it is; Plan B renames it.
- Old and new objects differ on purpose during Plan A. On an old `MultiPlotsVerticalSpan`, `.on.range` reads the property, and `.on.position` resolves as today. On a factory span, both are methods (`position` not at all). A pipeline built on an old panel object is not portable to a new one until the call site migrates. The migration note says so.

## Panel extras

- `MultiPlotsVSpanCollection` is **legacy-only in Plan A.** It keeps its API and builds and holds its own `MultiPlotsVerticalSpan`s, with the visibility rules from the bug round. It does not accept factory-built spans. Plan B retypes it on `SciQLopSpan`.
- Interactive creation still emits `span_created(MultiPlotsVerticalSpan*)`, vertical only.
- The JS API (wasm/) is unchanged. Its C++ side keeps compiling, because the old names stay. CI compiles it; this branch doesn't.

## Migration in Plan A

Every test, guide snippet, README line and manual test moves to:
- the new factories (`span`, `straight_line`);
- `read_only=` for lines;
- methods instead of panel properties;
- `findChildren(SciQLopSpan)` or `findChildren(SciQLopStraightLine)`, filtered by `is_panel_part()` or `scope()` where the old code relied on the class boundary. About 12 sites today pick "the parts on this plot" with `[0]`: test_panel_api.py:245, test_drawn_panel_objects.py:44, test_item_self_delete.py:144, test_plot_remove.py:22/117/216, test_multiplots_vertical_line.py:152 and others.

The old names stay in the tests that are about them.

## Plan B (outline, landing last with the SciQLop patch)

- Delete the seven subclasses.
- Serve the old names as deprecated Python aliases: small objects with `__call__` (the old argument order, through a table) and `__instancecheck__` / `__subclasscheck__`. Note that `inspect.isclass(alias)` is False, and that `findChildren(alias)` fails with shiboken's TypeError.
- Rewrite the import-time tables in `__init__.py` that name them: `_ITEM_FACTORIES`, `_DEPRECATED_ALIASES`, `_SNAKE_CASE_ALIASES`, `_ON_EXCEPTIONS`, `_PTR_HANDLE_TYPES`.
- Rename the Ptr type to `SciQLopSpanPtr`, with the old spelling an alias in the `SciQLopPlots` namespace only.
- Drop the panel properties.
- Remove the border-selection API. With it go the panel span's mirror slots and the hand-emitting test test_drawn_panel_objects.py:131.
- Remove the spans' movable stubs.
- Decide on `SciQLopRangeItemInterface`.

Plan B gets its own spec section and plan when it comes.

## Who is affected by Plan A

- **Plot line factories** (new on this branch): the second positional argument is now `color`, and the default stays read-only. `movable=` warns.
- **The factory defaults** differ from the old entry points (panel lines read-only and black, panel spans alpha 100). The old entry points don't change.
- **The old panel classes lose their plumbing methods** `addObject` / `removeObject` / `replotAll` / `updatePlotList` and their snake-case aliases.
- **`rectangular_span`** warns.
- **C++:** the V/H/R spans and the panel span change base class (they are now `SciQLopSpan`). The interface-typed `isinstance` checks (`SciQLopRangeItemInterface`) stop matching spans.
- **SciQLop** keeps working without a patch: the old names, the panel properties, `MultiPlotsVSpanCollection` and `span_created` all stay.

## Not in scope

- Mouse creation of horizontal or rectangular spans.
- Spans and lines in the inspector.
- A default handler for `delete_requested`.
- Spans on projection plots or WidgetPlot.

## Tests

- **Every kind × scope** (3 span kinds × 2 scopes, 2 orientations × 2 scopes):
  - built by each factory, which returns the plain new class (never an old subclass);
  - every accessor that applies reads and writes;
  - every signal fires once per change;
  - the defaults match the table.
- **Bounded axes:** None for an unbounded axis; setting it raises; `range()` on a rectangular span raises; `span()` with no range raises; `.on.range` on a rectangular span raises on use.
- **Signal pairing:** one `set_range` on a vertical span gives exactly one `range_changed` and one `key_range_changed`.
- **Coordinates:** Pixels on every kind and scope, read back through the pixel accessors; `coordinates_changed`; the line's `set_coordinates` keeps the line on screen in place. The bug-round drag reproducers stay.
- **Panel scope:**
  - one drag on one part of a two-plot panel span gives exactly one `range_changed` and one `key_range_changed` from the panel object, and the other part follows;
  - parts follow plots joining and leaving;
  - a plot destroyed while a panel span lives is handled through the QPointer path, with no crash;
  - edits on a part reach the panel object and the other parts;
  - `is_panel_part()` is true on parts and false otherwise;
  - `plot.remove(part)` and a wrong-scope remove refuse;
  - `panel.remove(x)` deletes it with its parts;
  - the factories raise off-thread at both scopes.
- **Old names:** `isinstance` positives and negatives (other kind, other scope; a plot-scope part of a panel span is a `SciQLopVerticalSpan`, not a `MultiPlotsVerticalSpan`); the old constructors still work, with their existing deprecation; the panel properties still work.
- **Lines:** `read_only` default True; `movable=` warns and inverts; `delete_requested` fires on Delete; the min/max getters.
- **Deprecations:** `rectangular_span` and `movable=` warn with the filter-matching message.
- **`.on`:** `tool_tip`, `range`, `key_range`, `read_only`, `position` (lines) and `coordinates` are observable; `.on.tooltip` works on every span and line; `.on.position` is absent on spans.
- **Self-delete sweep and ASan** with the new classes at both scopes: 0 crashes. The `test_drawn_contract` walk includes `SciQLopSpan` and the panel-scope line.
- **Migration:** the suite's DeprecationWarning filter catches what's missed.
