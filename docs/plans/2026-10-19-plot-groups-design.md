# Plot groups, legend exclusion and pane names

Status: design, not implemented. 2026-10-19.

## Why

SciQLop's planetary scenes are 3-pane `SciQLopNDProjectionPlot`s.
Each boundary model (Shue, Jelínek) is 3 curves per pane: a solid centre and dashed min/max.
With the Earth circle, the legend has 7 rows and covers most of a ~100 px tall pane.
`set_shared_legend(True)` removes the repeats in panes 2 and 3, but pane 1 still shows 7 rows.

The inspector tree has the same root cause: "one graph drawn in N panes" is N separate objects.
A projection-level trajectory shows as one node with N children that all have the same label.

A legend outside the panes was rejected: it takes space from the plots.

## What we add

1. `plot.group(name, members)` returns a `SciQLopPlotGroup`: one legend row and one tree node for a set of plottables and/or items.
2. `plottable.set_in_legend(False)`: the plottable has no legend row. Used for decorations (the Earth circle).
3. `projection.set_pane_names([...])`: short pane names for the tree, kept across relabels.
4. The tree shows a `SciQLopNDProjectionCurves` with its per-pane members named after their pane.

## 1. SciQLopPlotGroup

```python
shue = plot.group("Shue 1998 (Dp=2 nPa)", [centre, low, high])
shue.visible = False          # hides all three
shue.selected = True          # selects all three
shue.add(other); shue.remove(low)
shue.members                  # list of plottables and items
region = plot.group("Region", [ellipse, rect, label])  # drag one, all move
```

### What it is

- A `QObject` created by its plot (`SciQLopPlot` or `SciQLopNDProjectionPlot`), and parented to it.
- **It is not a plottable.** `SciQLopPlottableInterface` brings data, ranges, axes and resampling, which mean nothing for a group.
- It holds members, it doesn't own them. Members stay owned by their plot.
  - A member that is deleted leaves the group (QPointer, `destroyed`).
  - Deleting the group (`group.delete()` or the plot's `remove_group`) gives members their legend rows back and changes nothing else.
  - Removing the group *with* its members is a separate call: `plot.remove_group(g, remove_members=True)`.
- A member is in at most one group. Adding it to a second group moves it.
- Members must belong to the group's plot. On a projection plot, members may be in any pane. Anything else raises `ValueError` from the factory and from `add`.
- An empty group stays alive, with no legend row.

### Actions, applied per capability

The group asks each member what it supports. There is no common member base class.

| Action | Plottables | Items |
|---|---|---|
| `visible` | `set_visible` | `set_visible` |
| `selected` | `set_selected` | `set_selected` where the item supports it |
| remove with members | plot removes it | plot removes it |
| legend | members lose their row; the group has one | items have no rows |
| drag | not movable | `SciQLopMovableItemInterface::move(dx, dy)` |

- `group.visible` is true if any member is visible. Setting it sets every member.
- A member toggled on its own (inspector, Python) stays toggled. The group's state is derived, never stored.

### Legend row

- The group's row is a new NeoQCP legend item, `QCPGroupPlottablesLegendItem`, in the legend of the pane that holds its first plottable member.
  - Icon: the first plottable member's own legend icon (its pen, line style, marker).
  - Text: the group name.
- On a projection plot, each pane with plottable members gets one row. With `set_shared_legend(True)` only pane 1's legend shows, so the group appears once.
- Double-click toggles `group.visible`, like a plottable's row today. A single click selects the group.
- The row is greyed when the group is hidden, using the same alpha rule as `_apply_legend_visibility_style`.

### Dragging

- A group is draggable when every member is a movable item in **the same pane**, and none of them is constrained to one axis.
  - Excluded: straight lines (move on one axis only) and curved lines (`move` is a no-op). Adding one to a group makes the group non-draggable. It isn't an error.
  - Any plottable member makes the group non-draggable.
- When a draggable group's member gets a drag, it calls `group->move(dx, dy)` instead of its own `move`. The group then calls `move(dx, dy)` on every member, including the dragged one.
  - Pixel deltas are shared by everything in one pane, whatever each item's coordinate mode.
- Hook: `SciQLopPlotItem::mouseMoveEvent` checks for a group before calling `move`.
- Each member still emits its own `moved` signal.

### Inspector

- The group is a tree node under its plot, with its members as children.
- Grouped members are listed under the group only, not also under the plot or pane.
- Deleting the group node deletes only the group (members come back to the plot node).

## 2. set_in_legend

- `SciQLopPlottableInterface::set_in_legend(bool)` / `in_legend()`, true by default.
- Implementation: `removeFromLegend()` / `addToLegend()` on each underlying QCP plottable. A projection curve forwards to every pane curve.
- A plottable in a group already has no row of its own. Its `in_legend` flag is kept and applies again when it leaves the group.

## 3. Pane names

- `SciQLopNDProjectionPlot::set_pane_names(QStringList)`. One name per pane, so the size must match `subplot_count()`. Empty list = automatic.
- `_apply_axis_labels` uses these names when set. Otherwise it keeps today's `x_label-y_label`.
- No automatic short name: "XY" can't be derived reliably from "X MESO [R_M]".

## 4. Projection curves in the tree

- Today a `SciQLopNDProjectionCurves` node shows one child per pane, all with the same label. `pane_plottables()` hides those pieces from their panes on purpose, so the trajectory is listed once.
- Change: its children are named after their pane (the pane's object name, so `set_pane_names` applies).
- The legend label stays the same for every pane, because the label is also the legend text.

## Out of scope

- Legend outside the panes (rejected, see Why).
- Groups across plots of a panel: no shared pixel space, and no shared legend.
- Nested groups.
- Group membership in the JS/wasm API. Add it once the C++ side settles.

## Tests (written first)

- Group of 3 curves: legend has 1 row; double-click hides all 3; deleting a member leaves 2; deleting the group brings back 3 rows.
- `set_in_legend(False)` on a curve and on a projection curve: no row in any pane.
- Projection group across 3 panes with shared legend: exactly 1 visible row.
- Draggable group: drag one ellipse, all members move by the same pixel delta. A group containing a straight line or a curve doesn't move as a group.
- Member from another plot: `ValueError`.
- Tree: group node with members as children; grouped members not listed twice; projection curve children named by pane; `set_pane_names` survives `set_dimension_pairs`.

## Open questions

1. Should a single click on the group row select all members (drawing-tool style), or only the row?
2. Should hiding one member by hand change how the group row looks (partly hidden)?
3. Is `plot.group(...)` the right factory, or `SciQLopPlotGroup(plot, name, members)`?
