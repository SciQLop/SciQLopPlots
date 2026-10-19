# Plot groups, legend exclusion and pane names

Status: design, not implemented. 2026-10-19, revision 2 (after the first opencode review).

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
2. **SciQLopPlots:** `SciQLopPlotGroup`, the Python-facing group. One NeoQCP group per pane.
3. **SciQLopPlots:** `set_in_legend(False)` for decorations (the Earth circle).
4. **SciQLopPlots:** `set_pane_names([...])` for short pane node names.
5. **SciQLopPlots:** projection-curve tree children labelled by pane.

Later, each needing its own groundwork first (see "Later"):

- Items (ellipse, rectangle, text, pixmap) as group members.
- Dragging a group.
- `remove_group(..., remove_members=True)`.

The scene legend only needs plottables, so v1 fully fixes it.

## 1. NeoQCP: QCPPlottableGroup

### Why NeoQCP

- The legend row, its icon, double-click and dimming are `QCPLegend` concerns. `QCPGroupLegendItem` (multi-component rows) already lives there.
- `QCustomPlot::removePlottable` is the one exit for plottables, and it already calls `removeFromLegend()` (`core.cpp:1297-1315`). The group drops members at the same point, so no legend item ever holds a freed member.
- The JS/wasm API is built on NeoQCP, so it gets groups too.

### Shape

```cpp
class QCPPlottableGroup : public QObject {
    // created by and parented to its QCustomPlot
    QString name() const; void setName(const QString&);
    QList<QCPAbstractPlottable*> members() const;
    void addMember(QCPAbstractPlottable*);    // must belong to the same QCustomPlot
    void removeMember(QCPAbstractPlottable*);
    bool visible() const;                      // any member visible
    void setVisible(bool);                     // sets every member
signals:
    void membersChanged();
    void visibilityChanged(bool);
    void doubleClicked();                      // from its legend row
};
QCPPlottableGroup* QCustomPlot::addPlottableGroup(const QString& name);
bool QCustomPlot::removePlottableGroup(QCPPlottableGroup*);  // members stay, rows come back
QList<QCPPlottableGroup*> QCustomPlot::plottableGroups() const;
QCPPlottableGroup* QCPAbstractPlottable::group() const;      // back-link, null if none
```

- Members are tracked with `QPointer`. The group holds them, it doesn't own them.
- A plottable is in at most one group. `addMember` on a second group moves it.
- `QCustomPlot::removePlottable` calls `group()->removeMember(p)` before deleting it. The group also listens to `destroyed`, for plottables deleted another way.
- A member's own legend row is removed while it's grouped, through the **virtual** `removeFromLegend()` / `addToLegend()` (`plottable.h:199-202`). That handles `QCPMultiGraph` too (`plottable-multigraph.h:149-152`).
- Leaving the group restores the member's row only if the member wants one (see `set_in_legend` below: NeoQCP needs a per-plottable "wants legend" flag for this).
- An empty group has no legend row, and is deleted automatically when its last member leaves. That way nobody is left with a group they can't see or reach.

### Legend row: QCPPlottableGroupLegendItem

- A new `QCPAbstractLegendItem` subclass. It keeps a `QPointer<QCPPlottableGroup>`, never member pointers.
- At draw time it asks the group for its first live member and draws that member's icon:
  - plain plottable: the member's own `drawLegendIcon`;
  - `QCPMultiGraph`: the first component's icon, through the same code `QCPGroupLegendItem` uses.
- Text: the group name.
- **Dimming:** the item reads `group->visible()` at draw time and applies the dim alpha itself. It never writes its text colour, because `QCPLegend::setTextColor` overwrites every item's colour (`layoutelement-legend.cpp:553-561`) and would un-dim a hidden row on the next theme change.
- **Double-click:** `QCustomPlot::legendDoubleClick` already fires for any legend item (`core.cpp:3200-3215`). The group connects to it **once**. If the item is its own row, it toggles `setVisible(!visible())` and emits `doubleClicked`. Per-member handlers never see a group row, so there is no fan-out.
- **Single click:** selects the row only, like a `QCPGroupLegendItem` header today (`layoutelement-legend-group.cpp:119-147`). It doesn't select members. That means no replot per member, and it's consistent with multi-component rows. *Decision, easy to flip.*

### Not in NeoQCP

- Projection panes, shared legends, Python, the inspector. Those are SciQLopPlots concepts.

## 2. SciQLopPlots: SciQLopPlotGroup

```python
shue = proj.group("Shue 1998 (Dp=2 nPa)", [centre, low, high])
shue.visible = False         # hides all members, in every pane
shue.add(other); shue.remove(low)
shue.members
shue.delete()                # members get their rows back
```

- `SciQLopPlotInterface::group(name, members)` returns a `SciQLopPlotGroup*`. A `QObject` parented to the plot.
- Bindings: return annotated `<define-ownership class="target" owner="c++"/>`, like `subplot()` and `add_reference_curve()` (`bindings.xml:530-547`). `members` gets no parent action: members stay owned by their plot. `SciQLopPlotGroup` is `parent-management="yes"`.
- **Plain plot:** wraps one `QCPPlottableGroup`.
- **Projection plot:** one `QCPPlottableGroup` per pane, all with the same name. A projection-level member (`SciQLopNDProjectionCurves`) adds each of its pane curves to its pane's group. With `set_shared_legend(True)` only pane 1's legend is shown, so the group shows once.
- **Membership check:** a member must be a plottable of this plot, of one of its panes, or a projection-level curve of this projection. Otherwise the factory and `add` throw `std::invalid_argument`, mapped to `ValueError` the same way as `set_dimension_pairs` (`bindings.xml:520-528`).
  - Checked by walking the parent chain (member → pane → projection). `SciQLopNDProjectionPlot::plottables()` alone isn't enough: it only lists projection-level curves.
  - Groups never appear in `plottables()`. `_place_time_markers` casts every entry of it to `SciQLopNDProjectionCurves` unchecked (`SciQLopNDProjectionPlot.cpp:424-450`).
- `visible` mirrors the NeoQCP groups. Hiding a member by hand doesn't change the group: the row only dims when no member is visible. A double-click on a dimmed row shows all members; on a lit row it hides all.
- Deleted when its NeoQCP groups are (last member gone) or by `delete()`.

### Inspector

- A group is a tree node under its plot. Its children are its members.
- v1 doesn't move members' existing nodes. The tree model can add nodes but has no re-parent operation (`Model.cpp:172-255`), so grouped members are listed under the group **and** stay where they are. Re-parenting is "Later".
- Deleting the group node deletes the group only.

## 3. set_in_legend

- `SciQLopPlottableInterface::set_in_legend(bool)` / `in_legend()`. True by default.
- Stored as a flag on the wrapper, mirrored to NeoQCP's per-plottable "wants legend" flag. `in_legend()` reads the flag. It is never derived from `itemWithPlottable`, which only finds `QCPPlottableLegendItem`s and misses multi-component rows (`layoutelement-legend.h:755-807`).
- Applied through the virtual `removeFromLegend()` / `addToLegend()`. A projection curve forwards it to every pane curve.
- A grouped plottable has no row anyway. Its flag applies again when it leaves the group.

## 4. Pane names

- `SciQLopNDProjectionPlot::set_pane_names(QStringList)`. One per pane, size must match `subplot_count()`, else `ValueError`. Empty list = automatic.
- Storage: `QStringList m_pane_names`. One helper, `_pane_name(i)`, returns the explicit name or `x_label-y_label`.
- `_apply_axis_labels` (`SciQLopNDProjectionPlot.cpp:110-121`) uses `_pane_name(i)` and is the only place pane names are applied. So `set_dimension_pairs` and `set_axis_labels` can't overwrite them.
- The pane's tree node shows it, since the tree displays `objectName` (`Model.cpp:99-102`).
- No automatic short name: "XY" can't be derived reliably from "X MESO [R_M]".

## 5. Projection curves in the tree

- Today: a `SciQLopNDProjectionCurves` node has one child per pane, all labelled the same, because the tree shows `objectName` and `set_name` sets it to the legend text (`SciQLopGraphComponent.cpp:155-176`, `SciQLopNDProjectionCurves.cpp:81-86`).
- Change: add an optional `label` to `TypeDescriptor`, used by the tree instead of `objectName` when set. The projection-curve component descriptor returns the name of the component's pane.
- `set_name` and the legend text don't change.

## Later

- **Items as members.** Needs items in the inspector tree (the plot descriptor lists none, `Registrations.cpp:133-149`) and a plot-level item list in NeoQCP's group (`QCPAbstractItem` is also a `QCPLayerable`).
- **Dragging a group.** Python-held item wrappers don't implement `move()`: only their internal NeoQCP-side objects do (`SciQLopPlotItem.hpp:168` vs `SciQLopShapesItems.hpp:78`). Straight lines also bypass `move()` in their own `mouseMoveEvent` (`SciQLopStraightLines.cpp:25-51`). First step: generic item dragging in NeoQCP with a pixel-delta `move`, then a group forwards one delta to every member in its pane.
- **Re-parenting grouped members in the tree.** Needs a "children changed" verb in `TypeDescriptor`.
- **`remove_group(g, remove_members=True)`.**

## Tests (written first)

NeoQCP:

- Group of 3 graphs: legend has 1 row; double-click hides all 3, again shows all 3.
- `removePlottable(member)`, then a forced replot: no crash, group has 2 members.
- Last member removed: group deleted, no row left.
- `removePlottableGroup`: members get their rows back, except a member that doesn't want one.
- A `QCPMultiGraph` member: no own row while grouped, group row draws its first component's icon.
- Hidden group, then `QCPLegend::setTextColor`: the row is still dimmed.
- `addMember` from another `QCustomPlot`: refused.

SciQLopPlots:

- `set_in_legend(False)` on a curve, a line graph and a projection curve: no row in any pane; `in_legend()` is False.
- Projection group across 3 panes with a shared legend: exactly 1 visible row.
- Member from another plot: `ValueError`.
- `del` of the Python group object doesn't destroy the group.
- `remove_plottable(member)` while grouped: the group survives with one fewer member.
- A member hidden by hand: group still visible; hide all by hand: row dims; double-click shows all.
- Tree: group node with members as children; projection-curve children labelled by pane; `set_pane_names` survives `set_dimension_pairs` and `set_axis_labels`.
