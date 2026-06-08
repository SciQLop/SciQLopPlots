# Hide / show curves & graph components — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the ability to hide/show any curve, graph component, or color map from the inspector (a "Visible" checkbox) and from the legend (double-click a legend entry, component-by-component for multi-component graphs), kept in sync via the existing `visible_changed` signal.

**Architecture:** The visibility *plumbing* (`set_visible`/`visible`/`visible_changed`) already exists on `SciQLopGraphComponent` and `SciQLopColorMapBase`. We only add UI affordances that drive it: (1) a `BooleanDelegate` checkbox in the two property delegates, and (2) move the legend double-click handling out of `SciQLopPlot` (which pokes QCP directly and never synced) down into the component / colormap, where each object owns its own legend entry — matching the existing `selectionChanged`/`componentClicked` pattern. Legend dimming is driven off `visible_changed`, so it stays correct no matter which entry point flips visibility.

**Tech Stack:** C++20, Qt6, QCustomPlot/NeoQCP, Meson `build-venv`, pytest integration tests.

---

## Build & Test reference (used by every task)

**Build** (Qt must be on PATH per CLAUDE.md):

```bash
export PROJ=/var/home/jeandet/Documents/prog/SciQLopPlots
export VENV=$PROJ/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C $PROJ/build-venv
```

**Run the new tests** (from /tmp so the source pkg doesn't shadow the build):

```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py -q
```

> These delegate/component changes are C++-only and add no new Python-exposed API
> (`set_visible`/`visible`/`visible_changed` are already bound). No `bindings.xml`
> change is needed. If a stale shiboken wrapper misbehaves after the build, run
> `$VENV/bin/meson setup --reconfigure $PROJ/build-venv` and rebuild.

## File structure

- `tests/integration/test_visibility.py` — **new** — all tests for this feature.
- `src/SciQLopGraphComponentDelegate.cpp` — add Visible checkbox + reverse wiring.
- `src/SciQLopColorMapBaseDelegate.cpp` — add Visible checkbox + reverse wiring.
- `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` — declare legend slot + dim helper + connect helper.
- `src/SciQLopGraphComponent.cpp` — implement them; call connect helper from both ctors.
- `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp` — declare legend slot + dim helper.
- `src/SciQLopColorMapBase.cpp` — implement them; connect in ctor.
- `include/SciQLopPlots/SciQLopPlot.hpp` — remove `_legend_double_clicked` declaration.
- `src/SciQLopPlot.cpp` — remove handler + its `connect`.

---

## Task 1: Visible checkbox on the graph-component inspector delegate

**Files:**
- Test: `tests/integration/test_visibility.py` (create)
- Modify: `src/SciQLopGraphComponentDelegate.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/integration/test_visibility.py`:

```python
"""Hide/show visibility from the inspector delegates and component API."""
import numpy as np
import pytest
from PySide6.QtWidgets import QCheckBox

from SciQLopPlots import DelegateRegistry
from conftest import process_events


def _make_delegate(target, qtbot):
    d = DelegateRegistry.instance().create_delegate(target, None)
    assert d is not None
    qtbot.addWidget(d)
    return d


def _visible_checkbox(delegate):
    boxes = [c for c in delegate.findChildren(QCheckBox) if c.text() == "Visible"]
    assert len(boxes) == 1, f"expected one 'Visible' checkbox, got {len(boxes)}"
    return boxes[0]


class TestComponentDelegateVisible:
    def test_checkbox_reflects_initial_state(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        assert _visible_checkbox(d).isChecked() is True

    def test_checkbox_toggles_component(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        box = _visible_checkbox(d)
        box.setChecked(False)
        process_events()
        assert comp.visible() is False
        box.setChecked(True)
        process_events()
        assert comp.visible() is True

    def test_external_change_updates_checkbox(self, plot, sample_data, qtbot):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        d = _make_delegate(comp, qtbot)
        box = _visible_checkbox(d)
        comp.set_visible(False)
        process_events()
        assert box.isChecked() is False
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py::TestComponentDelegateVisible -q
```
Expected: FAIL — `_visible_checkbox` raises `AssertionError: expected one 'Visible' checkbox, got 0` (the delegate has no checkbox yet).

- [ ] **Step 3: Add the Visible checkbox to the component delegate**

In `src/SciQLopGraphComponentDelegate.cpp`, add the include near the other delegate includes (after line 26):

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
```

Then, at the **start** of the constructor body (immediately after the opening brace on the line after `: PropertyDelegateBase(object, parent)`), insert:

```cpp
    auto* visibleCheck = new BooleanDelegate("Visible", object->visible(), this);
    m_layout->addRow(visibleCheck);
    connect(visibleCheck, &BooleanDelegate::value_changed, object,
            &SciQLopGraphComponentInterface::set_visible);
    connect(object, &SciQLopGraphComponentInterface::visible_changed, visibleCheck,
            [visibleCheck](bool v)
            {
                QSignalBlocker blocker(visibleCheck);
                visibleCheck->set_value(v);
            });
```

(`QSignalBlocker` is already included in this file.)

- [ ] **Step 4: Build**

Run: `$VENV/bin/meson compile -C $PROJ/build-venv`
Expected: builds with no errors.

- [ ] **Step 5: Run test to verify it passes**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py::TestComponentDelegateVisible -q
```
Expected: 3 passed.

- [ ] **Step 6: Commit**

```bash
cd $PROJ
git add tests/integration/test_visibility.py src/SciQLopGraphComponentDelegate.cpp
git commit -m "feat(inspector): Visible checkbox on graph-component delegate

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Visible checkbox on the color-map inspector delegate

**Files:**
- Test: `tests/integration/test_visibility.py` (append)
- Modify: `src/SciQLopColorMapBaseDelegate.cpp`

- [ ] **Step 1: Write the failing test**

Append to `tests/integration/test_visibility.py`:

```python
class TestColorMapDelegateVisible:
    def test_checkbox_toggles_colormap(self, plot, sample_colormap_data, qtbot):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        d = _make_delegate(cmap, qtbot)
        box = _visible_checkbox(d)
        assert box.isChecked() is True
        box.setChecked(False)
        process_events()
        assert cmap.visible() is False

    def test_external_change_updates_checkbox(self, plot, sample_colormap_data, qtbot):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        d = _make_delegate(cmap, qtbot)
        box = _visible_checkbox(d)
        cmap.set_visible(False)
        process_events()
        assert box.isChecked() is False
```

- [ ] **Step 2: Run test to verify it fails**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py::TestColorMapDelegateVisible -q
```
Expected: FAIL — `AssertionError: expected one 'Visible' checkbox, got 0`.

- [ ] **Step 3: Add the Visible checkbox to the colormap base delegate**

In `src/SciQLopColorMapBaseDelegate.cpp`, add includes after line 23 (`#include ".../SciQLopColorMapBase.hpp"`):

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"

#include <QSignalBlocker>
```

Replace the constructor body — delete the `Q_UNUSED(object);` line and the explanatory comment block, and insert:

```cpp
        : PropertyDelegateBase(object, parent)
{
    auto* visibleCheck = new BooleanDelegate("Visible", object->visible(), this);
    m_layout->addRow(visibleCheck);
    connect(visibleCheck, &BooleanDelegate::value_changed, object,
            &SciQLopPlottableInterface::set_visible);
    connect(object, &SciQLopPlottableInterface::visible_changed, visibleCheck,
            [visibleCheck](bool v)
            {
                QSignalBlocker blocker(visibleCheck);
                visibleCheck->set_value(v);
            });
    // Color-scale controls (gradient, autoscale percentile) live on the
    // "Color Scale" (z) axis node. Product-specific knobs are added by derived
    // delegates (Contours, Binning/normalization).
}
```

(`SciQLopPlottableInterface` is the shared base of `SciQLopColorMapInterface`; it declares `set_visible`/`visible`/`visible_changed`. It is reached via the `SciQLopGraphInterface.hpp` include.)

- [ ] **Step 4: Build**

Run: `$VENV/bin/meson compile -C $PROJ/build-venv`
Expected: builds with no errors.

- [ ] **Step 5: Run test to verify it passes**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py::TestColorMapDelegateVisible -q
```
Expected: 2 passed.

- [ ] **Step 6: Commit**

```bash
cd $PROJ
git add tests/integration/test_visibility.py src/SciQLopColorMapBaseDelegate.cpp
git commit -m "feat(inspector): Visible checkbox on color-map delegate

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: Safety-net tests for the visibility contract (pre-refactor guard)

These lock the `set_visible`/`visible`/`visible_changed` behavior that both the
inspector checkbox and the upcoming legend refactor depend on. They pass now and
must keep passing after Task 4/5 (they guard against API regressions while the
legend handler moves).

**Files:**
- Test: `tests/integration/test_visibility.py` (append)

- [ ] **Step 1: Write the tests**

Append to `tests/integration/test_visibility.py`:

```python
class TestVisibilityContract:
    def test_component_round_trip(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        assert comp.visible() is True
        comp.set_visible(False)
        assert comp.visible() is False
        comp.set_visible(True)
        assert comp.visible() is True

    def test_component_emits_signal(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y, labels=["a"])
        process_events()
        comp = g.components()[0]
        seen = []
        comp.visible_changed.connect(lambda v: seen.append(v))
        comp.set_visible(False)
        assert seen == [False]

    def test_multicomponent_independent(self, plot, sample_multicomponent_data):
        x, y = sample_multicomponent_data
        g = plot.line(x, y, labels=["a", "b", "c"])
        process_events()
        comps = g.components()
        assert len(comps) == 3
        comps[1].set_visible(False)
        assert comps[0].visible() is True
        assert comps[1].visible() is False
        assert comps[2].visible() is True

    def test_colormap_round_trip(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        cmap = plot.colormap(x, y, z)
        process_events()
        assert cmap.visible() is True
        cmap.set_visible(False)
        assert cmap.visible() is False
        cmap.set_visible(True)
        assert cmap.visible() is True
```

- [ ] **Step 2: Run to verify they pass (contract holds today)**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py::TestVisibilityContract -q
```
Expected: 4 passed. (If `test_multicomponent_independent` fails, stop — the
per-component model is broken and must be fixed before the legend refactor.)

- [ ] **Step 3: Commit**

```bash
cd $PROJ
git add tests/integration/test_visibility.py
git commit -m "test(visibility): lock component/colormap visibility contract

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: Move legend double-click into SciQLopGraphComponent

Each component connects to the parent plot's `legendDoubleClick` and toggles its
own visibility; legend dimming is driven off `visible_changed`. The old
`SciQLopPlot` handler is removed in Task 6.

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`
- Modify: `src/SciQLopGraphComponent.cpp`

- [ ] **Step 1: Declare the slot, dim helper, and connect helper in the header**

In `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`, find the private
section near `bool m_selected = false;` (around line 99) and add **above** it:

```cpp
    void _connect_legend_visibility();
    void _apply_legend_visibility_style(bool visible);
```

These are private members (the class already has `Q_OBJECT`). Then add a forward
declaration for `QMouseEvent` near the top of the file, just after the include
block (before `class SciQLopGraphComponent`):

```cpp
class QMouseEvent;
```

Add the private slot. Find the `private Q_SLOTS:` section if present; if there is
none, add one just before the existing `bool m_selected = false;` line:

```cpp
private Q_SLOTS:
    void _on_legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item,
                                   QMouseEvent* event);

private:
```

(The trailing `private:` keeps `m_selected` private as before.)

- [ ] **Step 2: Implement in the .cpp and wire both constructors**

In `src/SciQLopGraphComponent.cpp`, add includes after the existing include
(line 22):

```cpp
#include <QCustomPlot.h>
#include <QMouseEvent>
#include <layoutelements/layoutelement-legend.h>
#include <layoutelements/layoutelement-legend-group.h>
```

> If any of those headers is already transitively included via
> `SciQLopGraphComponent.hpp`, the duplicate `#include` is harmless (header guards).

Add the three new methods at the end of the file (after `QColor SciQLopGraphComponent::color()`):

```cpp
void SciQLopGraphComponent::_connect_legend_visibility()
{
    if (auto* plot = _plot())
        connect(plot, &QCustomPlot::legendDoubleClick, this,
                &SciQLopGraphComponent::_on_legend_double_clicked);
    connect(this, &SciQLopGraphComponentInterface::visible_changed, this,
            &SciQLopGraphComponent::_apply_legend_visibility_style);
}

void SciQLopGraphComponent::_on_legend_double_clicked(QCPLegend* legend,
                                                      QCPAbstractLegendItem* item,
                                                      QMouseEvent* event)
{
    Q_UNUSED(legend);
    Q_UNUSED(event);
    bool mine = false;
    if (m_componentIndex >= 0)
    {
        if (auto* gi = dynamic_cast<QCPGroupLegendItem*>(item);
            gi != nullptr && gi->multiGraph() == qobject_cast<QCPMultiGraph*>(m_plottable.data())
            && gi->selectedComponent() == m_componentIndex)
            mine = true;
    }
    else if (auto* li = dynamic_cast<QCPPlottableLegendItem*>(item);
             li != nullptr && li == _legend_item())
    {
        mine = true;
    }
    if (mine)
        set_visible(!visible());
}

void SciQLopGraphComponent::_apply_legend_visibility_style(bool visible)
{
    // Only single-plottable components have a per-entry legend item to dim;
    // multi-component group items have no per-row text colour (matches prior
    // behaviour — the curve simply disappears).
    if (auto* li = _legend_item(); li != nullptr)
    {
        const QColor c = visible ? Qt::black : Qt::gray;
        li->setTextColor(c);
        li->setSelectedTextColor(c);
    }
}
```

Then call `_connect_legend_visibility();` at the **end** of *both* constructors:

In the `SciQLopGraphComponent(QCPAbstractPlottable* plottable, ...)` ctor, add it
just before the closing brace (after the `connect(plottable, ...selectionChanged...)`
block, inside the `if (m_plottable)` guard is fine but place it after the guard so
it always runs):

```cpp
    }
    _connect_legend_visibility();
}
```

In the `SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex, ...)`
ctor, add it just before the closing brace (after the
`if (auto* gi = _group_legend_item(); gi) { ... }` block):

```cpp
    _connect_legend_visibility();
}
```

- [ ] **Step 3: Build**

Run: `$VENV/bin/meson compile -C $PROJ/build-venv`
Expected: builds with no errors.

- [ ] **Step 4: Run the safety-net + delegate tests (no API regression)**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py -q
```
Expected: all passed (9 from Tasks 1-3). The double-click path is wired but not
yet exercised by an automated test — that is verified manually in Task 7.

- [ ] **Step 5: Commit**

```bash
cd $PROJ
git add include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp src/SciQLopGraphComponent.cpp
git commit -m "feat(legend): component owns its legend double-click hide/show

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Same legend wiring for SciQLopColorMapBase

So color maps keep legend-double-click hide/show after the plot handler is removed.

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp`
- Modify: `src/SciQLopColorMapBase.cpp`

- [ ] **Step 1: Declare slot + dim helper in the header**

In `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp`, add a forward
declaration for `QMouseEvent` near the top (after the existing include block,
before `class SciQLopColorMapBase`):

```cpp
class QMouseEvent;
```

In the `protected:` section (near `_legend_item()`), add:

```cpp
    void _connect_legend_visibility();
    void _apply_legend_visibility_style(bool visible);
```

Add a private slot section before the closing `};` of the class (after the public
API, alongside the other members):

```cpp
private Q_SLOTS:
    void _on_legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item,
                                   QMouseEvent* event);
```

- [ ] **Step 2: Implement and connect in the constructor**

In `src/SciQLopColorMapBase.cpp`, add includes after the existing includes at the
top of the file:

```cpp
#include <QCustomPlot.h>
#include <QMouseEvent>
#include <layoutelements/layoutelement-legend.h>
```

Call the connect helper at the end of the constructor body:

```cpp
    , _colorScaleAxis{colorScaleAxis}
{
    _connect_legend_visibility();
}
```

Add the method implementations after the constructor:

```cpp
void SciQLopColorMapBase::_connect_legend_visibility()
{
    if (auto* plot = _plot())
        connect(plot, &QCustomPlot::legendDoubleClick, this,
                &SciQLopColorMapBase::_on_legend_double_clicked);
    connect(this, &SciQLopPlottableInterface::visible_changed, this,
            &SciQLopColorMapBase::_apply_legend_visibility_style);
}

void SciQLopColorMapBase::_on_legend_double_clicked(QCPLegend* legend,
                                                    QCPAbstractLegendItem* item,
                                                    QMouseEvent* event)
{
    Q_UNUSED(legend);
    Q_UNUSED(event);
    if (auto* li = dynamic_cast<QCPPlottableLegendItem*>(item);
        li != nullptr && li == _legend_item())
        set_visible(!visible());
}

void SciQLopColorMapBase::_apply_legend_visibility_style(bool visible)
{
    if (auto* li = _legend_item(); li != nullptr)
    {
        const QColor c = visible ? Qt::black : Qt::gray;
        li->setTextColor(c);
        li->setSelectedTextColor(c);
    }
}
```

- [ ] **Step 3: Build**

Run: `$VENV/bin/meson compile -C $PROJ/build-venv`
Expected: builds with no errors.

- [ ] **Step 4: Run tests (no regression)**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py -q
```
Expected: all passed.

- [ ] **Step 5: Commit**

```bash
cd $PROJ
git add include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp src/SciQLopColorMapBase.cpp
git commit -m "feat(legend): color map owns its legend double-click hide/show

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Remove the old SciQLopPlot legend handler

**Files:**
- Modify: `src/SciQLopPlot.cpp` (remove handler body + connect at line ~64)
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp` (remove declaration at line ~211)

- [ ] **Step 1: Remove the connect**

In `src/SciQLopPlot.cpp`, delete the line (around line 64):

```cpp
    connect(this, &QCustomPlot::legendDoubleClick, this, &SciQLopPlot::_legend_double_clicked);
```

- [ ] **Step 2: Remove the handler body**

In `src/SciQLopPlot.cpp`, delete the entire `void SciQLopPlot::_legend_double_clicked(...)
{ ... }` function (lines ~545-584, the whole body shown in the design doc).

- [ ] **Step 3: Remove the declaration**

In `include/SciQLopPlots/SciQLopPlot.hpp`, delete the line (around line 211):

```cpp
    void _legend_double_clicked(QCPLegend* legend, QCPAbstractLegendItem* item, QMouseEvent* event);
```

- [ ] **Step 4: Verify no other references**

Run:
```bash
grep -rn "_legend_double_clicked" $PROJ/src $PROJ/include
```
Expected: no output.

- [ ] **Step 5: Build**

Run: `$VENV/bin/meson compile -C $PROJ/build-venv`
Expected: builds with no errors.

- [ ] **Step 6: Run full visibility tests**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration/test_visibility.py -q
```
Expected: all passed.

- [ ] **Step 7: Commit**

```bash
cd $PROJ
git add src/SciQLopPlot.cpp include/SciQLopPlots/SciQLopPlot.hpp
git commit -m "refactor(legend): drop SciQLopPlot double-click handler (moved to components)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: Full regression run + manual legend verification

**Files:** none (verification only)

- [ ] **Step 1: Run the whole integration suite**

Run:
```bash
cd /tmp && PYTHONPATH=$PROJ/build-venv $VENV/bin/python -m pytest \
  $PROJ/tests/integration -q
```
Expected: the full suite passes (no regressions). Read the final `N passed`
line and the exit code — do not infer success from a partial scroll.

- [ ] **Step 2: Manual legend double-click check**

Deploy the built `.so`/`.py` into SciQLop's venv (per CLAUDE.md: `cp build-venv/SciQLopPlots/*.so ... build-venv/SciQLopPlots/*.py ...` into SciQLop's `.venv`), launch SciQLop, then verify:
  - Double-click a single-curve legend entry → curve hides, legend text greys; double-click again → shows, text black.
  - Open the inspector for that component → the "Visible" checkbox tracks the legend state and vice-versa.
  - For a multi-component graph, expand the group legend, double-click one
    component row → only that component hides; others unchanged.
  - Color map: inspector "Visible" checkbox hides/shows the map.

- [ ] **Step 3: Final confirmation**

Report the integration suite result (`N passed`) and the manual-check outcome.
This branch is then ready for the finishing-a-development-branch flow (PR).

---

## Notes / out of scope

- **Group-header double-click is now a no-op** (the old "toggle all components" on
  the group header is intentionally dropped; each legend row toggles exactly one
  thing). Out of scope: a dedicated "toggle all" affordance.
- Out of scope: persisting visibility across save/load; changing how hidden curves
  participate in autoscale (unchanged).
