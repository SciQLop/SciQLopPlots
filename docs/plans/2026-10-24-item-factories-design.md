# Item factories, one `remove`, one ownership model

Status: design, 2026-10-24, revision 3 after two opencode reviews. API review items 1.1, 1.2 and 5.2, phase-2 task 12 (docs/api-review-2026-10-20.md). Part B of item 5.3; builds on the drawn-object base (docs/plans/2026-10-22-drawn-object-base-design.md) and follows the widget-ownership rule (docs/plans/2026-10-23-widget-ownership-design.md). Survey of the current code: every claim below about "today" comes from it.

## The rule

What a plot draws belongs to the plot. Plottables already do: they come only from plot factories and die with the plot. Items join them: `plot.text(...)`, `plot.vertical_span(...)` and the other item factories return an object the plot owns. Dropping the Python reference does nothing; `plot.remove(x)` or the plot's death deletes it. The panel does the same for what it draws across its plots (`panel.vertical_span(...)`, `panel.vertical_line(...)`).

The old item constructors keep today's behaviour (the Python object owns the item) and are deprecated.

## What is wrong today

1. Items belong to their Python object, plottables to their plot: `SciQLopTextItem(plot, ...)` without keeping the result vanishes at the next garbage collection (review 1.1).
2. Items come from constructors, plottables from factories, and removal has five forms: `remove_plottable`, `remove_group`, `delete_span`, `del item`, `item.deleteLater()` (review 1.2).
3. Each item class picks its C++ parent differently: Text, StraightLine and the vertical/horizontal lines have none; Pixmap, Ellipse, CurvedLine and the spans take the plot. After the plot dies, some wrappers are deleted and others live on as inert objects.
4. A null plot raises `RuntimeError` (Text), `ValueError` (Pixmap, Ellipse, CurvedLine), or dereferences null (StraightLine, the lines, the spans).
5. A removed non-span item stays on screen until something else replots: `qcp_item_remove` does not replot (spans do).
6. `remove_plottable` deletes a plottable of another plot without complaint; an unknown index or name is a silent no-op.

## Changes

### 1. Item factories on plots

On `SciQLopPlotInterface`, defaulting to `NotImplementedError` like the plottable factories (a `WidgetPlot` or a projection plot has none; a projection's panes, from `subplot(i)`, are plots and have them). Implemented on `SciQLopPlot`, so time-series plots and projection panes get them.

| Factory | Arguments (after the plot, same names and defaults as today's constructor) | Returns |
|---|---|---|
| `text` | `text, position, movable=False, coordinates=Pixels` | `SciQLopTextItem` |
| `pixmap` | `pixmap, rect, movable=False, coordinates=Pixels` | `SciQLopPixmapItem` |
| `ellipse` | the three constructor forms: `(rect, ...)`, `(rect, pen, brush, ...)`, `(rect, line_color, line_width, fill_color, ...)` | `SciQLopEllipseItem` |
| `curved_line` | `start, stop, start_terminator=NoneTermination, stop_terminator=Arrow, coordinates=Pixels, tool_tip=""` | `SciQLopCurvedLineItem` |
| `vertical_line` / `horizontal_line` | `position, movable=False, coordinates=Data` | `SciQLopVerticalLine` / `SciQLopHorizontalLine` |
| `vertical_span` | `range, color=..., read_only=False, visible=True, tool_tip="", coordinates=Data` | `SciQLopVerticalSpan` |
| `horizontal_span` | `range, color=..., read_only=False, visible=True, tool_tip=""` | `SciQLopHorizontalSpan` |
| `rectangular_span` | `key_range, value_range, color=..., read_only=False, visible=True, tool_tip=""` | `SciQLopRectangularSpan` |

`SciQLopStraightLine` with an `orientation` argument gets no factory: the two line factories cover it.

Every factory requires the plot's thread (raises off it, like `plot.group`) and returns a C++-owned object (`<define-ownership class="target" owner="c++"/>` on the return, as `group` does). The factories are not part of `plot()` dispatch: `plot()` is for data.

### 2. Item factories on panels

`panel.vertical_span(range, color=..., read_only=False, visible=True, tool_tip="", id="")` returns a `MultiPlotsVerticalSpan`, `panel.vertical_line(position, color=..., read_only=False, visible=True, tool_tip="")` a `MultiPlotsVerticalLine`, both owned by the panel (they are already its QObject children). Their per-plot parts keep today's life: built when a plot joins the panel, deleted when it leaves, rebuilt if it comes back; `plot.remove` rejects them (section 4). `MultiPlotsVSpanCollection` and its `create_span` / `delete_span` stay as they are: a collection is a view the application builds over the panel's spans.

### 3. One C++ parent for every item

Every item's QObject parent is its plot, set in the constructor, whichever way it was built: Text, StraightLine and the lines get the parent the other classes already have. So the plot's death deletes every item wrapper (a held Python reference becomes invalid), and `plot.findChildren(...)` sees all of them. A Python-owned item (old constructor) still dies with its Python object first if that comes first.

The plot deletes its plottables explicitly before its QCustomPlot dies (src/SciQLopPlot.cpp:930-936); item wrappers die later, as QObject children, after the QCP plot has deleted their QCP items. The invariant this relies on, and that a test pins: an item destructor tolerates an already-dead QCP item (every wrapper holds it through a QPointer and detaches through `qcp_item_remove` / `SciQLopSpanBase`, which skip a null one).

A null plot raises `ValueError` in every item constructor (and the factories can't receive one).

### 4. `plot.remove(x)` and `panel.remove(x)`

`plot.remove(x)` removes anything the plot draws: a plottable (graphs, colour maps, histograms, timelines...), a group, or an item. In C++ it is one overload per accepted kind: `remove(SciQLopPlottableInterface*)`, `remove(SciQLopPlotGroup*)`, `remove(SciQLopItemInterface*)`, `remove(SciQLopStraightLine*)` (the lines derive the drawn base directly). So the other drawn objects (the legend, a graph component) are a `TypeError` from the binding: a component lives and dies with its graph (no API removes one alone), the legend is the plot's, the title goes with `set_title("")`.

It deletes `x` now, as `remove_plottable` and `remove_group` do today, and the plot repaints. That holds when `remove` runs inside a signal the plot emits from its own mouse or key event (`selection_changed` on a click): the self-delete work made deleting a drawn object there safe, and only the plot or panel widget itself must not be deleted from its own event. The Python reference becomes invalid. A group drops a removed plottable from its members (groups hold `QPointer`s and prune destroyed members).

- `x` must be drawn by this plot: else `ValueError`, the message naming the plot it belongs to if any. That includes the per-plot parts of a panel-wide span or line: removing one alone would leave the panel object out of step, so `ValueError` says to remove the panel object. A parent check can't tell them apart (a per-plot span's QObject parent is the plot too): the panel object marks each part it builds, and `remove` checks the mark.
- `None` (a null pointer once converted) raises `ValueError`, like every null argument in the library, not a silent no-op.
- From another thread the removal queues to the plot's thread and is dropped if `x` dies first (the `SCIQLOP_ON_OWNER_THREAD_WITH` rule `remove_plottable` follows).
- No ownership markup on the argument. A Python-owned item (old constructor) deleted from C++ invalidates its wrapper without a double free, as `item.deleteLater()` already does (tests/integration/test_item_ownership.py:163-174). Markup moving ownership on the argument would also run before the checks, so a rejected `remove` would leave the item owned by nobody.

`panel.remove(x)` removes a panel-wide span or line (deleted now) or a plot (as `remove_plot`: the widget is deleted later, since Qt forbids deleting a widget inside its own event). A foreign span, line or plot, or None, raises `ValueError`. Another kind of object is a `TypeError` from the binding, as for `plot.remove`.

`remove_plottable` and `remove_group` stay as deprecated aliases of `plot.remove` with one change: `remove_plottable` raises `ValueError` for a plottable of another plot (it deleted it). `remove_plottable(None)` stays a no-op, as do `remove_plottable(index)` and `remove_plottable(name)` for an unknown index or name (deprecated, unchanged). `delete_span` stays.

### 5. A removed item disappears at once

`qcp_item_remove` queues a replot after detaching, like the spans' base: one fix in the shared helper, so every item's destructor, `remove`, and the old `del item` all repaint.

### 6. Deprecation of the constructors

Calling an item constructor from Python warns `DeprecationWarning: SciQLopTextItem(plot, ...) is deprecated: use plot.text(...)`, once per call site (default warning filter; `stacklevel=2` so the warning names the caller's line, not the wrapper's). The constructed object behaves as today: the Python object owns it. C++ code (the panel objects build their per-plot spans and lines with `new`) is unaffected. This step is separable: it lands after the factories and `remove`, together with moving the tests, examples and guide to the factories. The warning goes in the existing `_accept_ptr_handle` `__init__` wrapper (SciQLopPlots/__init__.py), which already covers the ten plot-taking item classes; `MultiPlotsVerticalSpan` and `MultiPlotsVerticalLine` get the same wrapper for the warning. Removal: at the next major version.

## Who is affected

- Code that builds items with constructors: a warning, no behaviour change. Moving to the factories changes one thing: the item stays until `remove` or the plot's death, instead of until the last reference goes. Code that dropped references to delete items calls `plot.remove(item)`.
- `remove_plottable(x)` with a plottable of another plot now raises `ValueError` (it silently deleted it).
- Text, StraightLine and line wrappers built with the old constructors now die with their plot (they lived on inert); a call on them after the plot's death raises "already deleted" instead of returning NaN. Same as Pixmap, Ellipse, CurvedLine and spans today.
- SciQLop pins SciQLopPlots: its item constructors keep working with a warning; moving it to the factories is a SciQLop patch, done with the user's approval, after this lands.

## Not in scope

- The three span APIs (review 1.15, item 5.6) and `delete_requested` having no default handler: item 5.6.
- A list of the plot's items (`plot.items()`), an items-changed signal: nothing needs them yet.
- Items on a `WidgetPlot` or the panel's other plot kinds beyond what `SciQLopPlot` provides.

## Tests

- A walk over every bound drawn class: each item class has its plot factory (or is listed as panel-only, abstract or deprecated-only), and each factory returns the right class, parented to the plot.
- Factory ownership, per item class: build, drop the reference, collect: still drawn (painted pixels, as `test_item_ownership.py` measures) and `plot.findChildren` finds it; delete the plot: the held wrapper is invalid.
- `plot.remove(x)` for a plottable, a group and each item class (factory-built and constructor-built): gone from the screen at the next paint without another replot, wrapper invalid, no double free (run under the poisoned-malloc settings of the self-delete sweep and under ASan once). `remove` from inside `x`'s own signal. Foreign `x`, a per-plot part of a panel span or line, `None`: `ValueError`. From a worker thread: applied after events are processed; dropped if `x` died first.
- `panel.remove` for a span, a line, a plot, and a foreign object.
- Deprecation: each constructor warns once, names the factory, and its item is still Python-owned (the existing `test_item_ownership.py` cases, under `pytest.warns`).
- Null plot: `ValueError` in every item constructor.
- `remove_plottable(foreign)`: `ValueError`; `remove_plottable(None)` a no-op; `remove_plottable` and `remove_group` warn.
- `plot.remove` of the legend or a graph component: `TypeError`. Removing a grouped plottable: the group no longer lists it.
- After the plot is deleted, every item wrapper (factory- and constructor-built) is invalid, and deleting the plot with items of every class runs clean under ASan.
- Every test, example and guide snippet that builds items for another purpose moves to the factories (about 245 call sites in about 35 files); the test suite runs with `DeprecationWarning` from SciQLopPlots as an error outside the tests that check the warning, so a missed call site fails.
- Self-delete sweep (tests/sweeps/self_delete) builds its items with the factories; 0 crashes, 0 hangs.
- Docs: the guide's overlay section, groups section and pipelines section, README and the manual examples use the factories and `remove`; the "Who keeps a plot alive" section gains the item rule.
