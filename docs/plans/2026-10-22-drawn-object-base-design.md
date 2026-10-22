# One base for everything drawn

Status: design for API review item 5.3 (docs/api-review-2026-10-20.md), Phase 4, part A. Revision 2, after an opencode review. Part B (item factories, `plot.remove(x)`, one ownership model: items 5.2 and phase-2 task 12) gets its own spec and builds on this one.

## Why

Every drawn family declares `visible`, `selected`, `name` and `color` on its own, with different spellings and signals:

- Plottables and components declare `visible_changed`, `selection_changed` and `name_changed` each on their own class. Items have no shared signals, and their visibility, selection and colour setters emit nothing. Spans emit the camelCase `selectionChanged`. The legend uses `is_visible` and `visibility_changed`.
- The inspector's `set_selected` callbacks cast to each class in turn (src/Registrations.cpp). Plot groups loop members through `SciQLopPlottableInterface` only, so items can't join them.
- Some setters are silent stubs: text and pixmap `set_tool_tip`, text and ellipse `set_movable` (`WARN_ABSTRACT_METHOD`), although the NeoQCP-side items store both (`impl::SciQLopPlotItem::setMovable`, `impl::SciQlopItemWithToolTip`).
- `SciQLopTimeline::set_visible` never emits `visible_changed` (src/SciQLopTimeline.cpp:213-217).
- The Python `name` property is `set="setObjectName"` (bindings.xml:359, :705, ...), so it bypasses `set_name()` and `m_user_named` (SciQLopGraphInterface.hpp:64) stays false.

## Scope

In:

1. A QObject base, `SciQLopDrawnObject`, under every drawn family.
2. `visible`, `selected`, `name` and `color` on it, as C++ methods and Qt signals, exposed to Python as methods and signals.
3. The inspector's selection callbacks call the base.
4. The three bugs above, each with its own reproducer.
5. Old spellings kept as aliases.

Out:

- New Python properties. Python keeps calling `x.visible()` / `x.set_visible(v)`; SciQLop does so in about 16 places (`_qt_property` in SciQLop/user_api/plot/_thread_safety.py:39 calls the getter). Turning these into properties is a later item, with a SciQLop change.
- `plot.remove(x)`, item factories, ownership: part B.
- Observing these through `.on`: item 5.1.
- Unifying the span classes: item 5.6.
- Axes (they keep `label`) and the panel (`SciQLopMultiPlotPanel.name` stays as it is).
- Legend rows: NeoQCP legend entries, not objects.
- `tooltip`, `movable`, `read_only` / `editable` on the base: item 5.9. Only the stubs are fixed here.

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
    virtual void _named(const QString&) { }
    virtual QColor _color() const;     virtual void _set_color(const QColor&);

    void _notify_visible(bool);  void _notify_selected(bool);  void _notify_color(const QColor&);
};
```

Rules:

1. **The public setter owns the signal, on the owner thread.** It first hops to the owner thread (`SCIQLOP_ON_OWNER_THREAD`, include/SciQLopPlots/ThreadGuard.hpp:76, which re-invokes the setter there). Then, on the owner thread: compare with the getter, return if unchanged, call `_set_x`, emit. The compare never runs on the calling thread.
2. **Aggregates always forward.** A group's `set_visible(v)` calls every member's `set_visible(v)` even when the group already reads `v` (one member hidden, group visible: showing the group must show that member). The group emits `visible_changed` when its own value changes, whether through its setter or a member's signal.
3. **State changed elsewhere still signals once.** When NeoQCP changes the state itself (a click selects an item: `QCPAbstractItem::selectionChanged`, NeoQCP items/item.h:224), the family calls `_notify_x(value)`, which emits if the value differs from the last one emitted. The setter path and the notify path share that last value, so one change gives one signal. NeoQCP emits on the GUI thread, which is the owner thread of every drawn object.
4. **Unsupported means raise.** The default `_x` / `_set_x` throw `sqp::unsupported_call` (include/SciQLopPlots/Debug.hpp), mapped to `NotImplementedError` naming the class. These virtuals are not `noexcept`, and every override drops `noexcept` (a throw from a `noexcept` function terminates the process).
5. **`name` is `objectName`.** `set_name` compares, sets `objectName`, calls `_named(name)` (plottables set `m_user_named` there), emits `name_changed` once. Every existing `objectNameChanged → name_changed` connection (src/SciQLopGraphInterface.cpp:40) and manual emit (src/SciQLopGraphComponent.cpp:156-176) is removed. `SciQLopPlotGroup` moves its `m_name` (SciQLopPlotGroup.hpp:45) onto `objectName`.
6. **No new storage on the base.** State stays where it is today (NeoQCP items, graph components). The exception is the text item's tooltip, which gets the `impl::SciQlopItemWithToolTip` mixin the ellipse and pixmap already use.
7. **The derived declarations go.** Each class that declares one of the four signals or accessors today loses that declaration and overrides the matching `_x` instead. A same-named signal on a derived class would be a different signal and silently never fire. Today's declarations: SciQLopGraphInterface.hpp:85-161, :225-232; SciQLopGraphComponentInterface.hpp:69-176; MultiPlotsVSpan.hpp:88-169; MultiPlotsVLine.hpp:80-81; SciQLopPlotGroup.hpp:62-72; SciQLopStraightLines.hpp:225-235; SciQLopPlotLegendInterface.hpp:47-123. Qualified base calls (`SciQLopPlottableInterface::set_name(...)`, SciQLopColorMapBase.hpp:78-80) are found with a grep and rewritten.

Families that derive `QObject` directly today derive `SciQLopDrawnObject` instead: `SciQLopPlottableInterface`, `SciQLopGraphComponentInterface`, `SciQLopItemInterface` (spans and shapes come through it), `SciQLopStraightLine`, `SciQLopMultiPlotObject`, `SciQLopPlotGroup`, `SciQLopPlotLegendInterface`. All are single-inheritance QObject chains (leaves hold the NeoQCP item by pointer), so no diamond appears.

## What each family supports

✓ supported; "raise" = `NotImplementedError` naming the class. Each row lists the bound classes it covers; the contract test reads this list.

| Row | Classes | visible | selected | name | color |
|---|---|---|---|---|---|
| graph | SciQLopLineGraph, SciQLopCurve, SciQLopWaterfallGraph, SciQLopNDProjectionCurves (+ `*Function` variants) | ✓ | ✓ | ✓ | raise ("use colors()") |
| component | SciQLopGraphComponent | ✓ | ✓ | ✓ | ✓ |
| colour map | SciQLopColorMap, SciQLopHistogram2D | ✓ | ✓ | ✓ | raise (gradient) |
| histogram | SciQLopHistogram | ✓ | ✓ | ✓ | raise ("use colors()") |
| timeline | SciQLopTimeline | ✓ (signal bug fixed) | ✓ | ✓ | raise (category colours) |
| group | SciQLopPlotGroup | ✓ (rule 2) | raise | ✓ | raise |
| text and lines | SciQLopTextItem, SciQLopCurvedLineItem, SciQLopStraightLine, SciQLopVerticalLine, SciQLopHorizontalLine | ✓ | ✓ | ✓ | ✓ |
| ellipse | SciQLopEllipseItem | ✓ | ✓ | ✓ | ✓ (outline colour) |
| pixmap | SciQLopPixmapItem | ✓ | ✓ | ✓ | raise |
| span | SciQLopVerticalSpan, SciQLopHorizontalSpan, SciQLopRectangularSpan | ✓ | ✓ | ✓ | ✓ |
| panel span | MultiPlotsVerticalSpan | ✓ | ✓ | ✓ | ✓ |
| panel line | MultiPlotsVerticalLine | ✓ | raise | ✓ | ✓ |
| span collection | MultiPlotsVSpanCollection | raise | raise | ✓ | raise |
| legend | SciQLopPlotLegend | ✓ | raise | ✓ | ✓ |

Abstract bound bases (`SciQLopPlottableInterface`, `SciQLopGraphInterface`, `SciQLopItemInterface`, `SciQLopMovableItemInterface`, `SciQLopColorMapInterface`, ...) are listed as abstract in the test and not instantiated.

Notes:

- Items forward selection to NeoQCP's `QCPAbstractItem::setSelected` / `selected`; whether a click can select an item is unchanged.
- A multi-line graph has no single colour: `color` raises and names `colors()`. A single line's colour is `graph.component(0).color()`.

## Compatibility

- **Spans:** `selection_changed` becomes the native signal and `selectionChanged` the alias. The Python alias at `SciQLopPlots/__init__.py:615-617` is inverted (it is a plain class attribute, not a wrapper, per the note at :602-603), so SciQLop's catalogue overlay keeps working: it connects `span.selection_changed` (overlay.py:226, :234) and sets `span.selected = True` (:164). C++ users of the camelCase signal (src/MultiPlotsVSpan.cpp:61-66) move to the new name now. The alias does not warn (a warning on connect needs a proxy object); it is documented as deprecated.
- **Legend:** `is_visible()` and `visibility_changed` stay as aliases of `visible()` and `visible_changed`. SciQLop calls `legend().is_visible()` (SciQLop/user_api/plot/_plots.py:501). The legend's existing `color_changed` becomes the base's.
- **Python properties unchanged:** `name` stays a property where it is today (plottables) and now goes through `set_name`; `MultiPlotsVerticalSpan` / `MultiPlotsVerticalLine` keep theirs. No property is added.
- **C++ subclasses outside the library:** overrides of these accessors must drop `noexcept` and move to `_x`. Listed in the changelog.
- **New behaviour:**
  - `color` / `selected` on a family that can't do them raise `NotImplementedError` where the method didn't exist before.
  - Text, line, ellipse and pixmap items gain `selected` and `name`.
  - The inspector can select a text item (its registration's `set_selected` was `nullptr`).
  - Text and pixmap `set_tool_tip`, text and ellipse `set_movable` work instead of doing nothing.

## Inspector

The `set_selected` lambdas of the five drawn registrations in src/Registrations.cpp (graph, component, colour map, 2-D histogram, timeline) become one call through the base; the text item gets one. Panel, plot, projection pane and axis registrations are not drawn objects and keep theirs.

## Bindings

- The base and its methods and signals are bound once. No `name` property on the base. The existing `name` properties switch to `set="set_name"`. The panel's (bindings.xml:1637) is untouched.
- The base's getters and setters carry the `NotImplementedError` mapping template, so `sqp::unsupported_call` reaches Python from every family.
- Changing the base of bound classes renumbers shiboken converters: the first build after the switch removes every generated `*_wrapper.cpp` and reconfigures (stale wrappers segfault).

## Order of work

The base lands first under the plottables (the families with the most callers and tests), then each other family moves in its own step with its own tests: components, items, spans, panel-wide objects, the group, the legend. The plan has one task per step.

## Tests

- **Contract test**, parametrised over the table: one row per family, with its classes and a factory that builds each. For each ✓ cell and each class: set → get round trip; the signal fires exactly once; setting the same value fires nothing; a set from a worker thread does nothing until events are processed (`qtbot.wait`), then applies and signals once. For each "raise" cell: `NotImplementedError` naming the class, for the getter and the setter. Every bound subclass of `SciQLopDrawnObject` must appear in exactly one row or in the abstract list, or the test fails.
- **Name emits once**, on a plottable and on a component (the double-emit path today), and `name = "x"` from Python sets `m_user_named` (checked through the behaviour that reads it; the plan names it).
- **NeoQCP-driven change signals once:** selecting an item through NeoQCP emits `selection_changed` once; hiding a group's last visible member emits the group's `visible_changed`; showing a group with one hidden member shows that member.
- **Reproducers first**, each failing before its fix: timeline `visible_changed`; text and pixmap tooltip round trip; text and ellipse `set_movable` round trip; the `name` property calls `set_name`.
- **Aliases:** the overlay's pattern on a span (`selection_changed` connect + `selected` set fires once, like tests/test_catalog_overlay.py:45-50 in SciQLop); `selectionChanged` still connects; `legend.is_visible()` and `legend.visibility_changed` work.
- **Unchanged:** the plot-group, inspector, item-ownership, projection and span tests pass as they are.
- **Gate:** the SciQLopPlots suite; SciQLop's full suite and the three plugins (msa, radio, sismo) against the build; an opus and an opencode review of the branch.
