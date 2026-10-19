# Plot groups, legend exclusion and pane names

Status: v1 implemented 2026-10-19 (NeoQCP 523aaf4 + ddb12a4, SciQLopPlots on top), after a code review by opencode. Revision 3 of the design, after two opencode reviews; see "Where v1 differs" at the end.

## Why

SciQLop's planetary scenes are 3-pane `SciQLopNDProjectionPlot`s.
Each boundary model (Shue, Jelínek) is 3 curves per pane: a solid centre and dashed min/max.
With the Earth circle, the legend has 7 rows and covers most of a ~100 px tall pane.
`set_shared_legend(True)` removes the repeats in panes 2 and 3, but pane 1 still shows 7 rows.

The inspector tree has the same root cause: "one graph drawn in N panes" is N separate objects.
A projection-level trajectory shows as one node with N children that all have the same label.
Pane nodes are named after both full axis labels ("X MESO [R_M]-Y MESO [R_M]").

A legend outside the panes was rejected: it takes space from the plots.

## Scope

v1:

1. **NeoQCP:** `QCPPlottableGroup`, one legend row for a set of plottables in one `QCustomPlot`.
2. **SciQLopPlots:** `SciQLopPlotGroup`, the Python-facing group, with one NeoQCP group per pane.
3. **NeoQCP + SciQLopPlots:** `set_in_legend(False)` for decorations (the Earth circle).
4. **SciQLopPlots:** `set_pane_names([...])` for short pane node names.
5. **SciQLopPlots:** projection-curve tree children labelled by pane.

Later, each needing its own groundwork first (see "Later"): groups in the inspector tree, items as members, dragging, `remove_group(..., remove_members=True)`.

The scene legend only needs plottables, so v1 fully fixes it.

## 1. NeoQCP: QCPPlottableGroup

### Why NeoQCP

- The legend row, its icon and dimming are `QCPLegend` concerns. `QCPGroupLegendItem` (multi-component rows) already lives there.
- `QCustomPlot::removePlottable` is the one exit for plottables, and it already calls `removeFromLegend()` before `delete` (`core.cpp:1297-1315`). The group drops members at the same point.
- The JS/wasm API is built on NeoQCP, so it gets groups too.

### Shape

```cpp
class QCPPlottableGroup : public QObject {
public:
    QString name() const; void setName(const QString&);
    QList<QCPAbstractPlottable*> members() const;
    bool addMember(QCPAbstractPlottable*);     // false if it belongs to another QCustomPlot
    void removeMember(QCPAbstractPlottable*);
    bool visible() const;                       // any member visible
    void setVisible(bool);                      // plain NeoQCP use; SciQLopPlots doesn't call it
signals:
    void membersChanged();
    void doubleClicked();                       // from its legend row
};
QCPPlottableGroup* QCustomPlot::addPlottableGroup(const QString& name);
bool QCustomPlot::removePlottableGroup(QCPPlottableGroup*);   // members get their rows back
QList<QCPPlottableGroup*> QCustomPlot::plottableGroups() const;
QCPPlottableGroup* QCPAbstractPlottable::group() const;       // back-link, null if none
bool QCPAbstractPlottable::wantsLegend() const;               // see §3
```

### Membership and lifetime

- Members are tracked with `QPointer`. The group holds them, it doesn't own them.
- A plottable is in at most one group. `addMember` on a second group moves it.
- `addMember` sets the member's `group()` back-link. `removeMember` and the group's destructor clear it.
- `QCustomPlot::removePlottable` calls `p->group()->removeMember(p)` next to the existing `removeFromLegend()`, before the `delete`.
- A member's own legend row is removed while it's grouped, through the **virtual** `removeFromLegend()` (`plottable.h:199-202`). That covers `QCPMultiGraph` (`plottable-multigraph.cpp:1205-1255`).
- On leaving the group, the member gets its row back through the virtual `addToLegend()` **only if** `wantsLegend()`. The group checks the flag itself, so overrides of `addToLegend` don't need to know about it.
- **An empty group stays alive.** It has no legend row and is deleted only by `removePlottableGroup` (or with its `QCustomPlot`). Auto-deleting it would leave Python handles dangling, even inside the call that removed the last member.
- `~QCustomPlot` deletes its groups **before** its plottables and legend. As `QObject` children they would otherwise be destroyed after them, and their destructors would touch freed members and legend items.
- `removePlottableGroup` removes the group's legend item first, then gives members their rows back, then deletes the group.

### Legend row: QCPPlottableGroupLegendItem

- A `QCPAbstractLegendItem` subclass. It keeps a `QPointer<QCPPlottableGroup>`, never member pointers.
- `draw` and `minimumOuterSizeHint` build the row text with the same helper, like `QCPGroupLegendItem::headerRowText` does, so the legend never clips it.
- Null group or no members: `draw` returns early (same rule as `QCPPlottableLegendItem`, `layoutelement-legend.cpp:319-321`).
- **Icon:** the first *visible* member's icon. Plain plottable: its `drawLegendIcon`. `QCPMultiGraph`: `drawComponentLegendIcon` (public, `plottable-multigraph.h:155`) for its first visible component. All hidden: the first member's icon, dimmed.
- **Text:** the group name, prefixed with the busy symbol when any member is `visuallyBusy()`, like the other legend items.
- **Dimming:** read from `group->visible()` at draw time; the item applies the alpha itself. It never writes its text colour, because `QCPLegend::setTextColor` overwrites every item's colour (`layoutelement-legend.cpp:553-561`).
- **Double-click:** `QCustomPlot::legendDoubleClick` already fires for any legend item (`core.cpp:3211`). The group connects to it **once** and, when the item is its own row, emits `doubleClicked()`. The group doesn't change visibility itself on a double-click: its owner decides. (A plain NeoQCP user connects `doubleClicked` to `setVisible(!visible())`.)
- **Single click:** selects the row, through the inherited `QCPAbstractLegendItem::selectEvent` (`layoutelement-legend.cpp:209-221`). It doesn't select members. This is new: a `QCPGroupLegendItem` header click expands instead (`layoutelement-legend-group.cpp:134`). A group row has nothing to expand.

## 2. SciQLopPlots: SciQLopPlotGroup

```python
shue = proj.group("Shue 1998 (Dp=2 nPa)", [centre, low, high])
shue.visible = False         # hides all members, in every pane
shue.add(other); shue.remove(low)
shue.members                 # [centre, high, other]: the objects passed in
shue.delete()                # members get their rows back
```

### What it is

- `SciQLopPlotInterface::group(name, members)` returns a `SciQLopPlotGroup*`, a `QObject` parented to the plot.
- Bindings, on `SciQLopPlotInterface` (`bindings.xml:239`): return annotated `<define-ownership class="target" owner="c++"/>`; `members` gets no parent action, so members stay owned by their plot. `SciQLopPlotGroup` is an `object-type` with `parent-management="yes"`.
- Alive until `delete()` or its plot is destroyed. Empty is allowed.

### Members

- **User-level members** are what the caller passes: `SciQLopPlottableInterface`s (graphs, curves, colour maps, projection curves). `members` returns those.
- **NeoQCP members** are derived: the distinct QCP plottables behind each member's components, each added to the `QCPPlottableGroup` of the canvas that holds it.
  - Plain plot: one canvas, one `QCPPlottableGroup`.
  - Projection plot: one `QCPPlottableGroup` per pane, all with the group's name, created on demand. A `SciQLopNDProjectionCurves` member contributes its curve in every pane.
- **Validation**, raising `ValueError` (`std::invalid_argument`, mapped like `set_dimension_pairs`, `bindings.xml:520-528`):
  - the member must be a plottable of this plot, of one of its panes, or a projection curve of this projection. Checked by walking the parent chain (member → pane → projection): `SciQLopNDProjectionPlot::plottables()` only lists projection curves;
  - a pane curve that is a piece of a projection curve is refused. Group the projection curve instead. This keeps one granularity per curve, so `remove` is never ambiguous.
- A member already in another `SciQLopPlotGroup` moves to this one.
- The group listens to each member's `destroyed` and drops it. (Its QCP plottables have already left the NeoQCP groups through `removePlottable`.)
- Groups never appear in `plottables()`. `_place_time_markers` casts every entry to `SciQLopNDProjectionCurves` unchecked (`SciQLopNDProjectionPlot.cpp:424-450`).

### Visibility

- **All visibility goes through the member wrappers' `set_visible`.** Never through `QCPPlottableGroup::setVisible`: `QCPLayerable::setVisible` sends no signal (`layer.cpp:571`), so inspector checkboxes bound to `visible_changed` would go stale.
- `visible` is true when any member is visible. Setting it calls `set_visible` on every member.
- A double-click on any pane's group row arrives as `doubleClicked()`. The wrapper sets `visible = not visible`. That reaches every pane, because the members (not the per-pane groups) are toggled. No cross-pane fan-out, no re-entrancy.
- Every pane's row dims together, since each reads its own members' QCP visibility.
- Hiding a member by hand doesn't change the group. The row dims only when no member is visible. A later group toggle sets every member, including the one hidden by hand.

### Teardown

- `delete()` / destructor calls `removePlottableGroup` on each per-pane group that still exists (held by `QPointer`). Members get their rows back, if they want one.
- The wrapper is parented to the plot, not to the panes, so this teardown is explicit.

## 3. set_in_legend

- NeoQCP: `QCPAbstractPlottable::setWantsLegend(bool)` / `wantsLegend()`, true by default. The **single** source of truth.
  - `setWantsLegend(false)` calls the virtual `removeFromLegend()`; `true` calls `addToLegend()` unless the plottable is grouped.
  - The group checks it when a member leaves (§1).
- SciQLopPlots: `SciQLopPlottableInterface::set_in_legend(bool)` / `in_legend()`, forwarding to `wantsLegend` on every QCP plottable behind the wrapper. A projection curve forwards to every pane curve. No flag of its own.
- `in_legend()` is never derived from `itemWithPlottable`, which only finds `QCPPlottableLegendItem`s (`layoutelement-legend.cpp:755-766`).

## 4. Pane names

- `SciQLopNDProjectionPlot::set_pane_names(QStringList)`: one per pane, size must match `subplot_count()`, else `ValueError`. Empty list = automatic.
- Why an API and not a derived name: these are for the *pane* nodes. "XY" can't be derived reliably from "X MESO [R_M]", so the caller supplies it.
- Storage: `QStringList m_pane_names`. One public helper, `pane_name(i)`: the explicit name, or `x_label-y_label`.
- `_apply_axis_labels` (`SciQLopNDProjectionPlot.cpp:110-121`) uses `pane_name(i)` and is the only place pane names are applied. So `set_dimension_pairs` and `set_axis_labels` can't overwrite them. `set_pane_names` calls `_apply_axis_labels`.
- The pane tree node shows it, since the tree displays `objectName` (`Model.cpp:99-102`).

## 5. Projection curves in the tree

- Today: a `SciQLopNDProjectionCurves` node has one child per pane, all labelled the same. The tree shows `objectName`, and `set_name` sets it to the legend text (`SciQLopGraphComponent.cpp:155-176`, `SciQLopNDProjectionCurves.cpp:81-86`).
- Change: add an optional `std::function<QString(const QObject*)> label` to `TypeDescriptor`, next to `icon` and `tooltip`. `PlotsModel::data` uses it for the display role when it returns a non-empty string, else `objectName`.
- The `SciQLopNDProjectionCurves` descriptor gets its own `children` returning its components, and a component helper finds a component's pane: component → its graph wrapper → that wrapper's `parentWidget()` as `SciQLopNDProjectionPane` → index in the projection → `pane_name(i)`.
  - Because the shared `SciQLopGraphComponentInterface` descriptor can't tell a pane piece from a plain curve's component, the label lambda returns empty when the walk doesn't reach a projection pane.
- `set_name` and the legend text don't change. Renaming from the tree (if exposed) still writes `objectName`; the label wins for display.

## Later

- **Groups in the inspector tree.** The tree has no way to insert a node for a new kind of object (the plot descriptors list only axes, plottables and extensions, `Registrations.cpp:133-200`), no re-parent operation (`Model.cpp:172-255`), and listing members under both the group and the plot would duplicate them. Needs a "children changed" verb in `TypeDescriptor` and a `SciQLopPlotGroup` descriptor and delegate.
- **Items as members.** Needs items in the tree (none are listed, `Registrations.cpp:133-149`) and NeoQCP's group to accept `QCPAbstractItem`s.
- **Dragging a group.** Python-held item wrappers don't implement `move()`, only their NeoQCP-side objects do (`SciQLopPlotItem.hpp:168` vs `SciQLopShapesItems.hpp:78`). Straight lines bypass `move()` (`SciQLopStraightLines.cpp:25-51`). First step: generic item dragging in NeoQCP with a pixel-delta `move`.
- **`remove_group(g, remove_members=True)`.**

## Tests (written first)

NeoQCP:

- Group of 3 graphs: legend has 1 row; double-click emits `doubleClicked` once.
- `removePlottable(member)`, then a forced replot: no crash, group has 2 members.
- Last member removed: the group stays, with no row; adding a member brings the row back.
- `removePlottableGroup`: members get their rows back, except one with `wantsLegend() == false`.
- A `QCPMultiGraph` member with `setWantsLegend(false)`: no row while grouped, and still none after leaving the group.
- Group row icon: first visible member's; a multi-graph member draws its first visible component.
- Hidden group, then `QCPLegend::setTextColor`: the row is still dimmed.
- `addMember` from another `QCustomPlot` returns false.
- Deleting the `QCustomPlot` with a non-empty group: no crash (destruction order).
- A busy member shows the busy symbol on the group row.

SciQLopPlots:

- `set_in_legend(False)` on a curve, a line graph and a projection curve: no row in any pane; `in_legend()` is False.
- 3-pane projection group, shared legend: exactly 1 visible row. Double-click it: members hidden in all 3 panes, and each member's `visible_changed` fired.
- Member from another plot, or a pane piece of a projection curve: `ValueError`.
- `del` of the Python group object doesn't destroy the group; `g.remove(last)` then `g.members` returns `[]` without crashing.
- `remove_plottable(member)` while grouped: the group survives with one fewer member.
- A member hidden by hand: group still visible; all hidden by hand: rows dim; double-click shows all.
- `delete()`: every pane's members get their rows back.
- Tree: projection-curve children labelled by pane; labels follow `set_pane_names`; `set_pane_names` survives `set_dimension_pairs` and `set_axis_labels`; a component `set_name` doesn't change its tree label.

## Where v1 differs from this design

- **`set_in_legend` keeps a flag on the wrapper too.** Wrappers make their NeoQCP plottables late: a parametric curve gets one per column when its data is resampled. The wrapper flag is applied to each new component (`_register_component`), and NeoQCP's `wantsLegend` stays the source of truth for existing plottables.
- **Group icon:** `drawLegendIcon` of the first shown member, for every type. `QCPMultiGraph::drawLegendIcon` already draws its visible components, so there is no multigraph special case.
- **`QCPLayerable::visibleChanged(bool)`** was added so the group row repaints when a member hides. Without it the row kept its undimmed look until the legend repainted for another reason.
- **Deleting a group:** `plot.remove_group(group)`. `delete` can't be a C++ method name.
- **Late components:** the group listens to each member's `component_list_changed`. `SciQLopNDProjectionCurves` now forwards its pane curves' signal.
- **`legend.row_labels()` and `legend.row_rect(i)`:** added, so tests (here and in SciQLop) can read the legend and click its rows. A timeline's row reads as its categories, one per line, as drawn.
- **Tree labels refresh:** `TypeDescriptor.label_sources` names the objects whose renaming changes a node's label (a projection piece's pane).
- **`remove_group` with another plot's group raises `ValueError`**, checked before the call is queued. `set_pane_names` validates the same way.
- **Timelines** can be grouped and kept out of the legend.
- **`SciQLopPlotGroup` has no public constructor:** only `plot.group()` makes one, after checking the thread and the members.
