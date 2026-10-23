# Widgets Follow PySide's Ownership Rules Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A SciQLopPlots widget (or other non-drawn QObject) built with a parent belongs to that parent, as in PySide; three leftovers of the drawn-object review are fixed first.

**Architecture:** Shiboken skips its constructor-parent heuristic on classes with `parent-management="yes"`, so every bound constructor with a `parent` argument gets explicit `<modify-argument index="this"><parent index="N" action="add"/></modify-argument>` markup. A walk over the bound signatures keeps new classes from missing it. Removals and `setParent` need no markup today: the plan pins their behaviour with tests. Drawn objects (`SciQLopDrawnObject` and its subclasses: items, graphs, the legend, panel-wide objects) are part B and are left alone.

**Tech Stack:** C++20, Qt 6.11.2, Shiboken6/PySide6 6.11.2 (bindings.xml), Python 3.11+, pytest + pytest-qt.

**Spec:** `docs/plans/2026-10-23-widget-ownership-design.md` (revision 2 + notes). Tasks 1-3: `docs/plans/2026-10-22-drawn-object-base-design.md` (revision 6).

**Spec ambiguities resolved here (read before Task 4):**
1. *Change 2 has nothing to mark up.* No bound function takes a `destroy` flag: `SciQLopPlotContainer::remove_plot(plot, bool)` and `removeWidget(widget, bool)` are not bound (the container has no `object-type`), and every bound removal (`remove_plot`, `removeWidget`/`remove_widget`, `remove_panel`) destroys. Task 6 adds the walk as a guard that fails the day such a removal is bound, and pins the destroying removals with constructor-parented widgets. `remove_plot(destroy=False)` in the review list becomes "a plot moved to another panel by `add_plot`".
2. *"Every constructor with a parent" excludes drawn objects.* The spec's scope is widgets; it leaves "what a plot draws" to part B. The walk covers every bound `QObject` class with a `parent` constructor argument that is not a `SciQLopDrawnObject`: 12 widget classes and 15 non-widget QObject classes (models, axes, behaviours, providers). The drawn interfaces and `SciQLopNDProjectionCurves` are excluded by that one rule, named in the test.
3. *`set_bin_range`'s log-bins check stays after the hop.* `log_bins()` is set by `set_log_bins`, itself queued from a worker. Read before the hop, it would reject a valid `set_log_bins(False); set_bin_range(0, 10)` sequence. The observable contract off-thread is a warning on the GUI thread; Task 2 tests it. `set_bins` already validates before the hop (guard test only).
4. *Task 2 covers the siblings the brief did not name.* A scan for checks after `SCIQLOP_ON_OWNER_THREAD` also finds `set_colors` (count), the component's `set_fill_between`, the column `set_fill_between(int,int)` / `set_error_bars(int,int,int,double)` and `SciQLopTimeline::set_edit_modes`. Their argument-only or structural checks move before the hop. Checks that depend on state a queued call may change stay after it: a component's fill (`set_fill_color`, pinned by `test_a_queued_setter_that_throws_warns_instead_of_terminating`), a column's range (a pending batch adds columns).
5. *Task 1 also fixes the shared base.* `SciQLopDrawnObject::set_name` emits `name_changed` after `_named`, whose part renames run slots that may delete `this`. That is the same bug as the projection's `_set_visible`, in the function every family shares. It gets the same `QPointer` guard.
6. *Found in passing:* `PropertiesPanel::PropertiesPanel(QWidget* parent)` never passes `parent` to `QWidget`, so `PropertiesPanel(host).parent()` is `None` (verified on the current build). Task 4 fixes it with the markup; without the fix, the markup would make shiboken believe `host` owns a widget Qt never parented.

## Global Constraints

- Repo /home/jeandet/Documents/prog/SciQLopPlots, branch `feat/fills-histogram`; nothing is pushed. Commit with explicit paths; the untracked `qt.ini` is not ours. Messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Never push.
- Build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`. Called BUILD below.
- After any bindings.xml change (Tasks 4 and 5): `cd /home/jeandet/Documents/prog/SciQLopPlots && find build-venv/SciQLopPlots/bindings -name '*_wrapper.cpp' -delete -o -name '*_wrapper.h' -delete && .venv/bin/meson setup --reconfigure build-venv`, then BUILD. A stale wrapper segfaults on import. Called CLEAN below.
- After every bindings change, grep the build output for shiboken warnings: `.venv/bin/meson compile -C build-venv -j 4 2>&1 | grep -i "signature\|modify\|not found"` must print nothing about our classes. Shiboken only warns on a signature it can't match, and the entry then silently stops applying. When it warns, it lists the candidate signatures: copy the candidate it prints.
- Tests from /tmp: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`, with `<files>` as absolute paths. Called TEST below; `tests/...` paths below are relative to the repo.
- Full suite (FULL below): TEST `tests/unit tests/fuzzing tests/resampler tests/tracing` in one run; then `tests/integration` in two halves: `cd /home/jeandet/Documents/prog/SciQLopPlots && command ls tests/integration/test_*.py | sort | sed "s|^|$PWD/|" > /tmp/sqp-tests.txt && split -n l/2 -d /tmp/sqp-tests.txt /tmp/sqp-half-`, then TEST with `timeout 590` before the python and `$(command cat /tmp/sqp-half-00)` as `<files>`, then the same with `-01`. Never `tests/` (manual-tests hang on `app.exec`). Read the real pass/fail counts and exit codes.
- One build or test command at a time, foreground, waited out; timeout 600000 on the shell tool; never `run_in_background`, never `&`; `-j 4`. Nothing left running. `cat` is aliased to bat: use `command cat`.
- SciQLop (/home/jeandet/Documents/prog/SciQLop) and the plugins (/home/jeandet/Documents/prog/plugins_sciqlop) are read-only for this plan. SciQLop changes are written as a patch description in Task 8 for the user to approve; never applied.
- SciQLop's suite runs as `xvfb-run -a -s "-screen 0 2560x1440x24" env PYTHONPATH=... timeout 590 .venv/bin/python -m pytest ... --no-xvfb`: `xvfb-run` outside `timeout`, so a timeout kill never orphans Xvfb. After each run, `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` prints nothing.
- Ownership markup, used verbatim by Tasks 4 and 5, inside the class's `<object-type>`:

  ```xml
  <modify-function signature="<exact C++ signature>">
      <modify-argument index="this">
          <parent index="<1-based position of parent>" action="add"/>
      </modify-argument>
  </modify-function>
  ```
- Tests observe ownership three ways, never through the wrapper alone: `shiboken6.ownedByPython(obj)`; `host.findChild(QObject, "probe")` after the reference is dropped (Qt parented it); the `destroyed` signal (C++ really deleted it). `shiboken6.isValid` alone is not proof: shiboken invalidates a child wrapper when its parent dies even if Qt never parented the object.
- Style: small functions, data over code, comments only for "why".

## Review Focus

1. **A parent that is itself Python-owned and gets collected** (`host = QWidget(); SciQLopPlot(host); del host`): the plot dies with it, its wrapper raises `RuntimeError` on use, nothing crashes. Test in Task 4.
2. **A widget reparented after construction by a Qt layout** (or SciQLop's QtAds dock, `CDockWidget.setWidget`): the new parent owns it; the first parent dying leaves it alive. Test in Task 7; the dock path is checked by SciQLop's suite in Task 8.
3. **A Python subclass, TimeSyncPanel-style, parented then removed**: while parented, its Python state survives losing every Python reference; `remove_panel` frees both the C++ widget and the Python object (no leak, its `destroyed` handler runs). Test in Task 6.
4. **The parent deleted while a queued call targets the child** (a worker's `plot.set_title(...)` still pending): Qt drops the call with its context object, nothing crashes. Test in Task 4.
5. **A plot moved to another panel by `add_plot` without a removal** (the reachable form of "remove without destroying, then re-add"): the second panel owns it; deleting the first panel leaves it alive. Test in Task 6.

---

### Task 1: A slot deleting an aggregate mid-set (projection `_set_visible`, base `set_name`)

**Files:**
- Modify: `src/SciQLopNDProjectionCurves.cpp` (`_set_visible`, ~line 363)
- Modify: `src/SciQLopDrawnObject.cpp` (`set_name`)
- Test: `tests/integration/test_drawn_aggregates.py` (append)

**Interfaces:**
- Consumes: `_three_pane_projection(qtbot) -> (nd, projection)` and `record` already in `test_drawn_aggregates.py`; `process_events` from conftest.
- Produces: nothing new for later tasks.

- [ ] **Step 1: Failing tests** — append to `tests/integration/test_drawn_aggregates.py`:

```python
def _first_pane_curve(projection):
    return projection.components()[0].parent()


def test_a_slot_deleting_the_projection_mid_hide_does_not_crash(qtbot):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    _first_pane_curve(projection).visible_changed.connect(lambda _: shiboken6.delete(projection))
    projection.set_visible(False)  # _set_visible went on to update the colour scale of a dead self
    process_events()
    assert not shiboken6.isValid(projection)


def test_a_slot_deleting_the_projection_mid_select_does_not_crash(qtbot):  # guard
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    _first_pane_curve(projection).selection_changed.connect(lambda _: shiboken6.delete(projection))
    projection.set_selected(True)
    process_events()
    assert not shiboken6.isValid(projection)


def test_a_slot_deleting_the_projection_mid_rename_does_not_crash(qtbot):
    import shiboken6
    _nd, projection = _three_pane_projection(qtbot)
    projection.components()[0].name_changed.connect(lambda _: shiboken6.delete(projection))
    projection.name = "renamed"  # the base emitted name_changed on a dead self after _named
    process_events()
    assert not shiboken6.isValid(projection)
```

Add `from conftest import process_events, record` if `process_events` is not imported yet (it is today).

- [ ] **Step 2: Run** with freed memory poisoned, so the stale read crashes instead of reading leftovers: `cd /tmp && GLIBC_TUNABLES=glibc.malloc.tcache_count=0 MALLOC_PERTURB_=165 QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_drawn_aggregates.py -q -p no:cacheprovider -k "mid_hide"` — expected: the process dies (`Fatal Python error: Segmentation fault`, exit code 139 or -11) inside `SciQLopNDProjectionCurves::_set_visible`. Then the same with `-k "mid_rename"` — expected: the same crash, in `SciQLopDrawnObject::set_name`. Then `-k "mid_select"` — expected PASS (nothing runs on `this` after `_set_selected`'s loop; the test pins it).

- [ ] **Step 3: The projection** — `src/SciQLopNDProjectionCurves.cpp`:

```cpp
void SciQLopNDProjectionCurves::_set_visible(bool visible)
{
    // A pane curve's visible_changed slot may delete this projection mid-loop.
    const QPointer<SciQLopNDProjectionCurves> self(this);
    _for_each_curve([visible](SciQLopCurve* curve) { curve->set_visible(visible); });
    if (self)
        _update_color_scale();
}
```

(`_set_selected` stays as it is: its loop touches only a local list of `QPointer`s, and nothing follows it.)

- [ ] **Step 4: The base** — `src/SciQLopDrawnObject.cpp`, `set_name` (`QPointer` is already included):

```cpp
void SciQLopDrawnObject::set_name(const QString& name)
{
    if (SciQLopPlots::posted_to_owner_thread(this, [this, name] { set_name(name); }))
        return;
    if (objectName() == name)
        return;
    // objectNameChanged and the parts _named renames run slots, which may delete this.
    const QPointer<SciQLopDrawnObject> alive(this);
    setObjectName(name);
    if (alive)
        _named(name);
    if (alive)
        Q_EMIT name_changed(name);
}
```

- [ ] **Step 5: Build** — BUILD (no bound header changed).
- [ ] **Step 6: Run** the Step 2 command without `-k` (all of `test_drawn_aggregates.py`, poisoned memory kept) — PASS. Then TEST `tests/integration/test_drawn_aggregates.py tests/integration/test_drawn_object.py tests/integration/test_drawn_contract.py tests/integration/test_projection_pane_curve_delete.py tests/integration/test_projection_color_scale.py` — PASS.
- [ ] **Step 7: Commit**

```bash
git add src/SciQLopNDProjectionCurves.cpp src/SciQLopDrawnObject.cpp tests/integration/test_drawn_aggregates.py
git commit -m "fix(drawn): a slot deleting an aggregate mid-set no longer crashes

The projection updated the colour scale after hiding its pane curves, and
the base emitted name_changed after renaming the parts, both on a self a
part's slot may have deleted. Both check a QPointer first.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: A bad call from a worker thread raises in the caller

**Files:**
- Modify: `src/QCPAbstractPlottableWrapper.cpp:113-145` (`set_labels`, `set_colors`)
- Modify: `src/SciQLopGraphComponent.cpp:374-388` (`set_fill_between`), `:433-456` (`set_error_bars`)
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp:103` (declare `check_siblings` after `sibling_index`)
- Modify: `src/SciQLopMultiGraphBase.cpp:289-330` (`set_fill_between(int,int)`, `set_error_bars(int,int,int,double)`)
- Modify: `src/SciQLopTimeline.cpp:310-323` (`set_edit_modes`)
- Modify: `src/SciQLopHistogram.cpp:256-266` (a "why" comment only)
- Test: `tests/integration/test_offthread_validation.py` (create)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `SciQLopGraphComponent::check_siblings(const QList<SciQLopGraphComponentInterface*>& others, const char* message) const` (private). The rule, reused by Task 3's docs: an argument-only or structural check runs before `SCIQLOP_ON_OWNER_THREAD`; a check on state another queued call may change runs after it. The queued call re-enters the setter from the top, so a check placed before the hop also runs again on the owner thread.

- [ ] **Step 1: Failing tests** — `tests/integration/test_offthread_validation.py`:

```python
"""A setter called off the GUI thread validates its arguments before queueing itself.

Queued, it can only warn on the GUI thread: the caller's mistake would vanish into a log line.
Checks that depend on state another queued call may change (a fill, log bins, a column a
pending batch adds) still run on the GUI thread, and warn there.
"""
import threading

import numpy as np
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import SciQLopPlot, SciQLopTimeSeriesPlot

X = np.linspace(0.0, 10.0, 50)
THREE = np.column_stack([np.sin(X), np.sin(X) - 1, np.sin(X) + 1])


def _raised(call):
    try:
        call()
    except Exception as error:  # noqa: BLE001 - the test compares it
        return error
    return None


def _raised_off_thread(call):
    out = []
    worker = threading.Thread(target=lambda: out.append(_raised(call)))
    worker.start()
    worker.join(timeout=10)
    assert not worker.is_alive(), "the off-thread call blocked"
    return out[0]


def _plot(qtbot, cls=SciQLopPlot):
    plot = cls()
    qtbot.addWidget(plot)
    return plot


def _graph(qtbot):
    return _plot(qtbot).plot(X, THREE, labels=["a", "lo", "hi"])


def _components(qtbot):
    return _graph(qtbot).components()


def _two_graphs_components(qtbot):
    return _components(qtbot), _components(qtbot)


CASES = {
    "set_labels count": (_graph, lambda g: g.set_labels(["only one"])),
    "set_colors count": (_graph, lambda g: g.set_colors([QColor("red")])),
    "column fill against itself": (_graph, lambda g: g.set_fill_between(0, 0)),
    "column fill negative": (_graph, lambda g: g.set_fill_between(-1, 1)),
    "column error bars same bound twice": (_graph, lambda g: g.set_error_bars(0, 1, 1)),
    "component fill against itself": (_components, lambda cs: cs[0].set_fill_between(cs[0])),
    "component fill against another graph": (
        _two_graphs_components, lambda pair: pair[0][0].set_fill_between(pair[1][1])),
    "component error bars same bound twice": (
        _components, lambda cs: cs[0].set_error_bars(cs[1], cs[1])),
    "component error bars on itself": (_components, lambda cs: cs[0].set_error_bars(cs[0], cs[2])),
    "histogram bins": (lambda qtbot: _plot(qtbot).histogram(np.arange(10.0)),
                       lambda h: h.set_bins(0)),  # guard: already validated before the hop
    "timeline edit mode": (lambda qtbot: _plot(qtbot, SciQLopTimeSeriesPlot).add_timeline(),
                           lambda tl: tl.set_edit_modes(["teleport"])),
}


@pytest.mark.parametrize("name", sorted(CASES))
def test_a_bad_call_from_a_worker_raises_there_as_on_the_gui_thread(qtbot, name):
    build, call = CASES[name]
    target = build(qtbot)
    on_gui = _raised(lambda: call(target))
    assert on_gui is not None, "pick a call the GUI thread rejects"
    off = _raised_off_thread(lambda: call(target))
    assert (type(off), str(off)) == (type(on_gui), str(on_gui))


def test_a_log_bins_range_error_from_a_worker_warns_on_the_gui_thread(qtbot, qtlog):
    """log_bins is set by a queued setter too: only the GUI thread reads it in call order."""
    histogram = _plot(qtbot).histogram(np.arange(1.0, 100.0), log_bins=True)
    assert _raised_off_thread(lambda: histogram.set_bin_range(0.0, 10.0)) is None
    qtbot.wait(50)
    assert any("log bins need a positive lower bound" in r.message for r in qtlog.records)
```

- [ ] **Step 2: Run** TEST `tests/integration/test_offthread_validation.py` — expected FAIL on every case except "histogram bins": `assert (<class 'NoneType'>, 'None') == (<class '...Error'>, '...')` (the worker's call returned, the error became a warning). "histogram bins" and the log-bins test PASS (guards).

- [ ] **Step 3: Counts** — `src/QCPAbstractPlottableWrapper.cpp`; add before `set_labels`, in a new anonymous namespace at the top of the file's definitions:

```cpp
namespace
{
void require_one_per_component(std::size_t given, std::size_t components, const char* what)
{
    if (given != components)
        throw std::runtime_error(std::string("Invalid number of ") + what);
}
} // namespace
```

and rewrite the two setters:

```cpp
void SQPQCPAbstractPlottableWrapper::set_labels(const QStringList& labels)
{
    require_one_per_component(std::size(labels), std::size(m_components), "labels");
    SCIQLOP_ON_OWNER_THREAD(set_labels(labels));
    for (decltype(std::size(m_components)) i = 0UL; i < std::size(m_components); ++i)
        m_components[i]->set_name(labels[i]);
    Q_EMIT labels_changed(labels);
}

void SQPQCPAbstractPlottableWrapper::set_colors(const QList<QColor>& colors)
{
    require_one_per_component(std::size(colors), std::size(m_components), "colors");
    SCIQLOP_ON_OWNER_THREAD(set_colors(colors));
    for (decltype(std::size(m_components)) i = 0; i < std::size(m_components); ++i)
        m_components[i]->set_color(colors[i]);
    Q_EMIT colors_changed(colors);
}
```

(The queued call re-enters from the top, so the count is checked again on the GUI thread, where a callback graph's components may have changed meanwhile. Add `#include <string>` if the file lacks it.)

- [ ] **Step 4: Component siblings** — `SciQLopGraphComponent.hpp`, after `int sibling_index(...) const;`:

```cpp
    //! Throws \a message unless every one of \a others is a sibling and all differ from this
    //! and each other. Structural, so it runs on the calling thread, before the hop.
    void check_siblings(const QList<SciQLopGraphComponentInterface*>& others,
                        const char* message) const;
```

`src/SciQLopGraphComponent.cpp`, after `sibling_index` (add `#include <QSet>`):

```cpp
void SciQLopGraphComponent::check_siblings(const QList<SciQLopGraphComponentInterface*>& others,
                                           const char* message) const
{
    if (!multi_ref())
        return; // not a multigraph component: the setter warns unsupported, as before
    QSet<const QObject*> distinct { this };
    for (auto* other : others)
    {
        sibling_index(other);
        distinct.insert(other);
    }
    if (distinct.size() != others.size() + 1)
        throw std::invalid_argument(message);
}
```

In `set_fill_between`, first line: `check_siblings({ other }, "a component cannot be filled against itself");` (before `SCIQLOP_ON_OWNER_THREAD_WITH`). In `set_error_bars`, after the `cap_width` check: `check_siblings({ lower, upper }, "error bars need three distinct components");`. The bodies after the hop stay as they are (they re-check against NeoQCP's indices).

- [ ] **Step 5: Columns** — `src/SciQLopMultiGraphBase.cpp`:

```cpp
void SciQLopMultiGraphBase::set_fill_between(int column, int other)
{
    const auto invalid = "set_fill_between: invalid columns " + std::to_string(column) + ", "
        + std::to_string(other);
    require(column >= 0 && other >= 0 && column != other, invalid);
    SCIQLOP_ON_OWNER_THREAD(set_fill_between(column, other));
    // The column range waits for the GUI thread: a pending batch may add the columns.
    require(_multiGraph && _multiGraph->setComponentFillBetween(column, other), invalid);
    notify_component(*this, column, &SciQLopGraphComponentInterface::fill_changed);
    Q_EMIT replot();
}

void SciQLopMultiGraphBase::set_error_bars(int column, int lower, int upper, double cap_width)
{
    require(cap_width >= 0, "set_error_bars: cap_width must be >= 0");
    const auto invalid = "set_error_bars: invalid columns " + std::to_string(column) + ", "
        + std::to_string(lower) + ", " + std::to_string(upper);
    require(column >= 0 && lower >= 0 && upper >= 0 && lower != upper && column != lower
                && column != upper,
            invalid);
    SCIQLOP_ON_OWNER_THREAD(set_error_bars(column, lower, upper, cap_width));
    require(_multiGraph && _multiGraph->setComponentErrorBars(column, lower, upper, cap_width),
            invalid);
    notify_component(*this, column, &SciQLopGraphComponentInterface::error_bars_changed);
    Q_EMIT replot();
}
```

- [ ] **Step 6: Timeline and histogram** — `src/SciQLopTimeline.cpp`: move the parsing above the hop.

```cpp
void SciQLopTimeline::set_edit_modes(const QStringList& names)
{
    QCPIntervals::EditModes modes;
    for (const auto& name : names)
    {
        const auto it = std::ranges::find_if(edit_mode_names,
                                             [&](const auto& m) { return name == m.first; });
        if (it == std::end(edit_mode_names))
            throw std::invalid_argument("unknown edit mode: " + name.toStdString());
        modes |= it->second;
    }
    SCIQLOP_ON_OWNER_THREAD(set_edit_modes(names));
    _intervals->setEditModes(modes);
}
```

`src/SciQLopHistogram.cpp`, `set_bin_range`, above `if (log_bins() && lower <= 0)`: `// After the hop: set_log_bins is queued too, so only the owner thread reads log_bins() in call order.`

- [ ] **Step 7: Build** — BUILD (`SciQLopGraphComponent` has no `object-type`; if the import segfaults anyway, CLEAN).
- [ ] **Step 8: Run** TEST `tests/integration/test_offthread_validation.py tests/integration/test_thread_guards.py tests/integration/test_fills_errorbars.py tests/integration/test_histogram1d.py tests/integration/test_timeline_edit.py tests/integration/test_graph_api.py` — PASS. Then FULL.
- [ ] **Step 9: Commit**

```bash
git add src/QCPAbstractPlottableWrapper.cpp src/SciQLopGraphComponent.cpp include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp src/SciQLopMultiGraphBase.cpp src/SciQLopTimeline.cpp src/SciQLopHistogram.cpp tests/integration/test_offthread_validation.py
git commit -m "fix(threads): a bad call from a worker raises in the caller

set_labels, set_colors, the component and column fill/error-bar setters
and set_edit_modes validated after queueing themselves, so a worker's
mistake became a warning on the GUI thread. Argument and structural
checks now run first; checks on state a queued call may change (a fill,
log bins, a pending batch's columns) stay on the GUI thread.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Docs: what runs on the calling thread, hooks, dropped `noexcept`

**Files:**
- Modify: `docs/user-guide.md` (a new `## Threads` section just before `## Performance tips`, ~line 1308)
- Modify: `docs/api-review-2026-10-20.md` (Progress, end of the phase 4 paragraph, line 154)

**Interfaces:**
- Consumes: Task 2's rule (checks before the hop; state checks after).
- Produces: nothing for later tasks.

- [ ] **Step 1: User guide** — insert before `## Performance tips` (prose only: the guide test runs every `python` snippet, and this section has none):

```markdown
## Threads

Plots live on the GUI thread. You can still call them from another thread:

- **Setters queue themselves.** A setter called from another thread (`set_visible`, `set_title`,
  `set_range`, `add_plot`, ...) runs on the GUI thread a moment later and returns at once.
- **Bad arguments still raise in your thread.** A wrong number of labels, an unknown edit mode,
  a component filled against itself: the setter checks these before it queues itself.
- **Some mistakes can only be seen on the GUI thread.** They depend on what an earlier queued call
  will have done: a fill colour on a component whose fill is still queued, a bin range below zero
  with log bins, a column that a pending data batch adds. Those print a warning there.
- **Data setters run in your thread, on purpose.** `set_data`, `set_data_and_color`,
  `set_color_data`, `set_values`, `set_binned`, `set_time_values` and `set_color_values` read the
  arrays you pass while you still hold them, and `data_changed` fires before they return, in the
  order you called them.
- **Factories refuse other threads.** `plot()`, `create_plot()`, item constructors and the like
  hand an object back, so they raise `RuntimeError` off the GUI thread.
- **`set_x_axis` / `set_y_axis` run in the calling thread.** The plot calls them itself while it
  arranges its graphs, on the GUI thread; call them there too.
- **Protected hooks are for subclasses.** `_visible()`, `_set_color()` and the other `_x` hooks are
  where a subclass plugs in. Called from Python on a library object, they run the base default
  (most raise `NotImplementedError`), not the class's own code: call the public method instead.
```

- [ ] **Step 2: API review note** — append to the phase 4 paragraph (after `... Part B (factories, plot.remove, ownership) is next.`):

```markdown
Follow-ups from the part A review: a slot deleting a projection while it hid its panes, or any aggregate while it renamed its parts, used the dead object (fixed with a QPointer in the projection and in the base's set_name). Setters called from a worker validated after queueing themselves, so a bad argument became a warning on the GUI thread; argument and structural checks now run first (set_labels, set_colors, fills, error bars, set_edit_modes), and checks on state a queued call may change stay on the GUI thread (a component's fill, log bins, a pending batch's columns). Deliberately on the calling thread: the data setters (set_data, set_data_and_color, set_color_data, set_values, set_binned, set_time_values, set_color_values), because they read the caller's buffers while the caller holds them and emit data_changed in call order; and set_x_axis / set_y_axis, noexcept wiring the plot does on the GUI thread. Protected hooks called from Python run the base default: they are subclass mounting points. Setters that dropped noexcept: the four drawn accessors and every hook (the defaults throw sqp::unsupported_call, and a throw out of noexcept terminates); the component style setters set_pen, set_line_style, set_line_width, set_marker_shape, set_marker_pen, set_marker_size and their interface (they queue themselves off-thread, which allocates).
```

- [ ] **Step 3: Run** TEST `tests/integration/test_user_guide.py` — PASS (no new snippet; the screenshot check still finds every image).
- [ ] **Step 4: Commit**

```bash
git add docs/user-guide.md docs/api-review-2026-10-20.md
git commit -m "docs(threads): what queues, what raises where, what runs on the calling thread

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Widget constructors give the widget to its parent

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml` (12 widget `object-type`s: `SciQLopPlotInterface` ~:472, `SciQLopPlot` ~:850, `SciQLopTimeSeriesPlot` ~:887, `SciQLopNDProjectionPlot` ~:912, `SciQLopPlotPanelInterface` ~:1551, `SciQLopMultiPlotPanel` constructor entry ~:1656, `PlotsTreeView`, `InspectorView`, `PropertyDelegateBase`, `FontDelegate`, `PropertiesPanel` ~:1826-1831, `ProductsView` ~:1848)
- Modify: `src/PropertiesPanel.cpp:28` (pass `parent` to `QWidget`)
- Modify: `tests/integration/conftest.py` (two helpers)
- Test: `tests/integration/test_widget_ownership.py` (create)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: in conftest, `collect()` (gc + deferred deletes + events) and `on_destroyed(obj) -> list` (gets `True` when the C++ object is deleted). Tasks 5-7 use both.

- [ ] **Step 1: Helpers in conftest.py** (after `force_gc`):

```python
def collect():
    """Garbage-collects and delivers deferred deletes, which processEvents() alone does not."""
    from PySide6.QtCore import QCoreApplication, QEvent
    gc.collect()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    QApplication.processEvents()
    gc.collect()


def on_destroyed(obj):
    """A list that gets True once obj's C++ object is deleted; the slot holds no reference to obj."""
    gone = []
    obj.destroyed.connect(lambda *_: gone.append(True))
    return gone
```

- [ ] **Step 2: Failing tests** — `tests/integration/test_widget_ownership.py`:

```python
"""Widgets follow PySide's ownership rules: ownership follows the Qt parent.

docs/plans/2026-10-23-widget-ownership-design.md. Built with a parent, a widget lives as long
as the parent, whatever happens to the Python reference. Built without one, it belongs to its
Python object.
"""
import threading

import pytest
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtGui import QColor, QFont
from PySide6.QtWidgets import QWidget

from conftest import collect, on_destroyed
from SciQLopPlots import (FontDelegate, InspectorView, PlotsTreeView, ProductsView, PropertiesPanel,
                          PropertyDelegateBase, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopPlotInterface, SciQLopPlotPanelInterface,
                          SciQLopTimeSeriesPlot)

_KEEP = []  # objects a builder needs alive for as long as the test session


def _kept(obj):
    _KEEP.append(obj)
    return obj


WIDGETS = {
    "SciQLopPlot": lambda parent: SciQLopPlot(parent),
    "SciQLopTimeSeriesPlot": lambda parent: SciQLopTimeSeriesPlot(parent),
    "SciQLopNDProjectionPlot": lambda parent: SciQLopNDProjectionPlot(3, parent),
    "SciQLopMultiPlotPanel": lambda parent: SciQLopMultiPlotPanel(parent),
    "SciQLopMultiPlotPanel, keywords": lambda parent: SciQLopMultiPlotPanel(
        parent, synchronize_x=False, synchronize_time=True),
    "SciQLopPlotInterface": lambda parent: SciQLopPlotInterface(parent),
    "SciQLopPlotPanelInterface": lambda parent: SciQLopPlotPanelInterface(parent),
    "InspectorView": lambda parent: InspectorView(parent),
    "PlotsTreeView": lambda parent: PlotsTreeView(parent),
    "ProductsView": lambda parent: ProductsView(parent),
    "PropertiesPanel": lambda parent: PropertiesPanel(parent),
    "FontDelegate": lambda parent: FontDelegate(QFont(), QColor("red"), parent),
    "PropertyDelegateBase": lambda parent: PropertyDelegateBase(_kept(QObject()), parent),
}


def _probe(name, parent):
    widget = WIDGETS[name](parent)
    widget.setObjectName("probe")
    return widget, on_destroyed(widget)


@pytest.mark.parametrize("name", sorted(WIDGETS))
def test_a_widget_built_with_a_parent_lives_as_long_as_the_parent(qtbot, name):
    host = QWidget()
    widget, gone = _probe(name, host)
    assert (widget.parent() is host, shiboken6.ownedByPython(widget)) == (True, False)
    del widget
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]  # the parent owned it: no leak


@pytest.mark.parametrize("name", sorted(WIDGETS))
def test_a_widget_built_without_a_parent_belongs_to_python(qtbot, name):  # guard
    widget, gone = _probe(name, None)
    del widget
    collect()
    assert gone == [True]


def test_a_widget_dies_with_a_python_owned_parent_that_is_collected(qtbot):  # review focus 1
    host = QWidget()
    plot = SciQLopPlot(host)
    gone = on_destroyed(plot)
    del host
    collect()
    assert (gone, shiboken6.isValid(plot)) == ([True], False)
    with pytest.raises(RuntimeError):
        plot.objectName()


def test_a_queued_call_to_a_child_is_dropped_when_its_parent_dies_first(qtbot):  # review focus 4
    host = QWidget()
    plot = SciQLopPlot(host)
    worker = threading.Thread(target=lambda: plot.set_title("late"))
    worker.start()
    worker.join(timeout=10)
    assert not worker.is_alive()
    shiboken6.delete(host)
    collect()  # set_title would run here; Qt dropped it with its context object
    assert not shiboken6.isValid(plot)
```

- [ ] **Step 3: Run** TEST `tests/integration/test_widget_ownership.py` — expected FAIL: every row of `test_a_widget_built_with_a_parent_lives_as_long_as_the_parent` with `(True, True) == (True, False)` (Python owns it), and the `PropertiesPanel` row with `(False, True) == (True, False)` (the parent was never set). The other tests PASS (guards).

- [ ] **Step 4: PropertiesPanel** — `src/PropertiesPanel.cpp:28`: `PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent)`.

- [ ] **Step 5: bindings.xml** — add the Global Constraints markup as the first child of each `<object-type>` below; a self-closing `<object-type name="X" parent-management="yes"/>` becomes `<object-type name="X" parent-management="yes">` + the markup + `</object-type>`:

| object-type | signature | `parent index` |
|---|---|---|
| `SciQLopPlotInterface` | `SciQLopPlotInterface(QWidget*,const QString&amp;)` | 1 |
| `SciQLopPlot` | `SciQLopPlot(QWidget*)` | 1 |
| `SciQLopTimeSeriesPlot` | `SciQLopTimeSeriesPlot(QWidget*)` | 1 |
| `SciQLopNDProjectionPlot` | `SciQLopNDProjectionPlot(std::size_t,QWidget*)` | 2 |
| `SciQLopPlotPanelInterface` | `SciQLopPlotPanelInterface(QWidget*)` | 1 |
| `PlotsTreeView` | `PlotsTreeView(QWidget*)` | 1 |
| `InspectorView` | `InspectorView(QWidget*)` | 1 |
| `PropertiesPanel` | `PropertiesPanel(QWidget*)` | 1 |
| `ProductsView` | `ProductsView(QWidget*)` | 1 |
| `FontDelegate` | `FontDelegate(const QFont&amp;,const QColor&amp;,QWidget*)` | 3 |
| `PropertyDelegateBase` | `PropertyDelegateBase(QObject*,QWidget*)` | 2 |

For example, `SciQLopNDProjectionPlot`:

```xml
    <object-type name="SciQLopNDProjectionPlot" parent-management="yes">
        <modify-function signature="SciQLopNDProjectionPlot(std::size_t,QWidget*)">
            <modify-argument index="this">
                <parent index="2" action="add"/>
            </modify-argument>
        </modify-function>
        <modify-function signature="save_svg(const QString&amp;,int,int)">
```

and `PropertiesPanel`:

```xml
    <object-type name="PropertiesPanel" parent-management="yes">
        <modify-function signature="PropertiesPanel(QWidget*)">
            <modify-argument index="this">
                <parent index="1" action="add"/>
            </modify-argument>
        </modify-function>
    </object-type>
```

The panel already has a constructor entry; add the `this` argument at its top:

```xml
        <modify-function signature="SciQLopMultiPlotPanel(QWidget*,bool,bool,Qt::Orientation)">
            <modify-argument index="this">
                <parent index="1" action="add"/>
            </modify-argument>
            <modify-argument index="2">
                <rename to="synchronize_x"/>
            </modify-argument>
```

- [ ] **Step 6: Build** — CLEAN, BUILD, the warning grep (none for these classes; if `std::size_t` is reported, use the spelling shiboken lists).
- [ ] **Step 7: Run** TEST `tests/integration/test_widget_ownership.py tests/integration/test_projection_improvements.py tests/integration/test_sub_panel_time_sync.py tests/integration/test_inspector_insert_order.py tests/integration/test_panel_api.py tests/integration/test_widget_plot.py tests/integration/test_plot_lifecycle.py tests/integration/test_memory.py` — PASS. Then FULL.
- [ ] **Step 8: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml src/PropertiesPanel.cpp tests/integration/conftest.py tests/integration/test_widget_ownership.py
git commit -m "fix(bindings): a widget built with a parent belongs to the parent

Shiboken skips its constructor-parent heuristic on classes with
parent-management, so SciQLopPlot(host) and the other widgets stayed
Python-owned and died at the next collection. Each widget constructor
now gives the widget to its parent, as PySide does. PropertiesPanel
also ignored its parent argument.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: The constructor walk, and every other QObject constructor

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml` (15 `object-type`s: `SciQLopPlotAxisInterface` ~:815, `SciQLopPlotDummyAxis` ~:843, `SciQLopPlotCollectionBehavior` ~:1786, `VPlotsAlign` ~:1790, `DataProviderInterface` ~:1792, `RemoteDataPipeline` ~:1794, `DataProviderWorker` ~:1796, `PlotDragNDropCallback` ~:1803, `PlotsModel` ~:1825, `InspectorExtension` ~:1829, `ProductsTreeFilterModel` ~:1845, `ProductsFlatFilterModel` ~:1846, `ProductsModel` ~:1849, `ProductsModelNode` ~:1859, `Icons` ~:1869)
- Test: `tests/integration/test_constructor_ownership.py` (create)

**Interfaces:**
- Consumes: `collect()`, `on_destroyed(obj)` from conftest (Task 4); Task 4's widget markup.
- Produces: `BUILDERS: dict[str, list[Callable[[QObject | None], QObject]]]` in the test, one builder per bound constructor with a `parent` argument. A class bound later with such a constructor fails `test_every_parent_constructor_has_a_builder` until it gets a row and markup.

- [ ] **Step 1: Failing tests** — `tests/integration/test_constructor_ownership.py`:

```python
"""Every bound constructor with a `parent` argument gives the new object to that parent.

docs/plans/2026-10-23-widget-ownership-design.md, change 1. The walk reads the bound signatures
(`parent` is not always last), so a class bound later with a `parent` argument fails here until
it has a builder below and its <parent> markup in bindings.xml. Drawn objects (SciQLopDrawnObject
and its subclasses: items, graphs, the legend, panel-wide objects) are part B of the ownership
work and are left out.
"""
import pytest
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtGui import QColor, QFont
from PySide6.QtWidgets import QWidget

import SciQLopPlots as sqp
from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopDrawnObject, SciQLopPlotsBindings
# After SciQLopPlots: importing it loads shiboken's signature support.
from shibokensupport.signature import get_signature

_KEEP = []  # side objects a builder needs alive for the whole session


def _kept(obj):
    _KEEP.append(obj)
    return obj


def _subclass(cls):
    """A Python subclass, for a class shiboken refuses to build (C++ abstract)."""
    return type(f"{cls.__name__}ForTest", (cls,), {})


BUILDERS = {
    "SciQLopPlotInterface": [lambda parent: sqp.SciQLopPlotInterface(parent)],
    "SciQLopPlot": [lambda parent: sqp.SciQLopPlot(parent)],
    "SciQLopTimeSeriesPlot": [lambda parent: sqp.SciQLopTimeSeriesPlot(parent)],
    "SciQLopNDProjectionPlot": [lambda parent: sqp.SciQLopNDProjectionPlot(3, parent)],
    "SciQLopPlotPanelInterface": [lambda parent: sqp.SciQLopPlotPanelInterface(parent)],
    "SciQLopMultiPlotPanel": [lambda parent: sqp.SciQLopMultiPlotPanel(parent)],
    "PlotsTreeView": [lambda parent: sqp.PlotsTreeView(parent)],
    "InspectorView": [lambda parent: sqp.InspectorView(parent)],
    "PropertiesPanel": [lambda parent: sqp.PropertiesPanel(parent)],
    "ProductsView": [lambda parent: sqp.ProductsView(parent)],
    "FontDelegate": [lambda parent: sqp.FontDelegate(QFont(), QColor("red"), parent)],
    "PropertyDelegateBase": [lambda parent: sqp.PropertyDelegateBase(_kept(QObject()), parent)],
    "SciQLopPlotAxisInterface": [lambda parent: sqp.SciQLopPlotAxisInterface(parent)],
    "SciQLopPlotDummyAxis": [lambda parent: sqp.SciQLopPlotDummyAxis(parent)],
    "SciQLopPlotCollectionBehavior": [lambda parent: sqp.SciQLopPlotCollectionBehavior(parent)],
    "VPlotsAlign": [lambda parent: sqp.VPlotsAlign(parent)],
    "DataProviderInterface": [lambda parent: sqp.DataProviderInterface(parent)],
    "RemoteDataPipeline": [lambda parent: sqp.RemoteDataPipeline(parent)],
    "DataProviderWorker": [lambda parent: sqp.DataProviderWorker(parent)],
    "PlotDragNDropCallback": [lambda parent: sqp.PlotDragNDropCallback("text/plain", False, parent)],
    "PlotsModel": [lambda parent: sqp.PlotsModel(parent)],
    "InspectorExtension": [lambda parent: _subclass(sqp.InspectorExtension)(parent)],
    "ProductsTreeFilterModel": [lambda parent: sqp.ProductsTreeFilterModel(parent)],
    "ProductsFlatFilterModel": [
        lambda parent: sqp.ProductsFlatFilterModel(_kept(sqp.ProductsModel()), parent)],
    "ProductsModel": [lambda parent: sqp.ProductsModel(parent)],
    "ProductsModelNode": [
        lambda parent: sqp.ProductsModelNode("node", {}, "", parent),
        lambda parent: sqp.ProductsModelNode("node", "provider", {},
                                             sqp.ProductsModelNodeType.PARAMETER,
                                             sqp.ParameterType.Scalar, "", parent),
    ],
    "Icons": [lambda parent: sqp.Icons(parent)],
}

CASES = [(name, index) for name, builders in BUILDERS.items() for index in range(len(builders))]


def _signatures(cls):
    found = get_signature(cls)
    return found if isinstance(found, list) else [found] if found else []


def _parent_constructor_counts():
    counts = {}
    for name, cls in vars(SciQLopPlotsBindings).items():
        if isinstance(cls, type) and issubclass(cls, QObject) and not issubclass(cls, SciQLopDrawnObject):
            count = sum("parent" in s.parameters for s in _signatures(cls))
            if count:
                counts[name] = count
    return counts


def test_every_parent_constructor_has_a_builder():
    assert {name: len(b) for name, b in BUILDERS.items()} == _parent_constructor_counts()


@pytest.mark.parametrize("name,index", CASES, ids=[f"{n}-{i}" for n, i in CASES])
def test_an_object_built_with_a_parent_belongs_to_it(qtbot, name, index):
    host = QWidget()
    obj = BUILDERS[name][index](host)
    obj.setObjectName("probe")
    gone = on_destroyed(obj)
    assert (obj.parent() is host, shiboken6.ownedByPython(obj)) == (True, False)
    del obj
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]
```

- [ ] **Step 2: Run** TEST `tests/integration/test_constructor_ownership.py` — expected: `test_every_parent_constructor_has_a_builder` PASS (the table matches today's bindings: 27 classes, 28 constructors); every non-widget row FAIL with `(True, True) == (True, False)`; the widget rows PASS (Task 4). If the walk test fails, a class differs from the table: print `_parent_constructor_counts()` and reconcile (a new non-drawn class gets a builder here and markup in Step 3).

- [ ] **Step 3: bindings.xml** — the Global Constraints markup as the first child of each `<object-type>` (self-closing ones opened as in Task 4):

| object-type | signature | `parent index` |
|---|---|---|
| `SciQLopPlotAxisInterface` | `SciQLopPlotAxisInterface(QObject*,const QString&amp;)` | 1 |
| `SciQLopPlotDummyAxis` | `SciQLopPlotDummyAxis(QObject*)` | 1 |
| `SciQLopPlotCollectionBehavior` | `SciQLopPlotCollectionBehavior(QObject*)` | 1 |
| `VPlotsAlign` | `VPlotsAlign(QObject*)` | 1 |
| `DataProviderInterface` | `DataProviderInterface(QObject*)` | 1 |
| `RemoteDataPipeline` | `RemoteDataPipeline(QObject*)` | 1 |
| `DataProviderWorker` | `DataProviderWorker(QObject*)` | 1 |
| `PlotDragNDropCallback` | `PlotDragNDropCallback(const QString&amp;,bool,QObject*)` | 3 |
| `PlotsModel` | `PlotsModel(QObject*)` | 1 |
| `InspectorExtension` | `InspectorExtension(QObject*)` | 1 |
| `ProductsTreeFilterModel` | `ProductsTreeFilterModel(QObject*)` | 1 |
| `ProductsFlatFilterModel` | `ProductsFlatFilterModel(ProductsModel*,QObject*)` | 2 |
| `ProductsModel` | `ProductsModel(QObject*)` | 1 |
| `ProductsModelNode` | `ProductsModelNode(const QString&amp;,const QMap&lt;QString,QVariant&gt;&amp;,const QString&amp;,QObject*)` | 4 |
| `ProductsModelNode` | `ProductsModelNode(const QString&amp;,const QString&amp;,const QMap&lt;QString,QVariant&gt;&amp;,ProductsModelNodeType,ParameterType,const QString&amp;,QObject*)` | 7 |
| `Icons` | `Icons(QObject*)` | 1 |

For example, `ProductsModelNode` keeps its `add_child` entry after the two new ones:

```xml
    <object-type name="ProductsModelNode" parent-management="yes">
        <modify-function signature="ProductsModelNode(const QString&amp;,const QMap&lt;QString,QVariant&gt;&amp;,const QString&amp;,QObject*)">
            <modify-argument index="this">
                <parent index="4" action="add"/>
            </modify-argument>
        </modify-function>
        <modify-function signature="ProductsModelNode(const QString&amp;,const QString&amp;,const QMap&lt;QString,QVariant&gt;&amp;,ProductsModelNodeType,ParameterType,const QString&amp;,QObject*)">
            <modify-argument index="this">
                <parent index="7" action="add"/>
            </modify-argument>
        </modify-function>
        <modify-function signature="add_child(ProductsModelNode*)">
```

- [ ] **Step 4: Build** — CLEAN, BUILD, the warning grep (none for these classes; for a `QMap` mismatch, use the spelling shiboken lists, e.g. `QVariantMap`).
- [ ] **Step 5: Run** TEST `tests/integration/test_constructor_ownership.py tests/integration/test_widget_ownership.py tests/integration/test_products_model_consistency.py tests/integration/test_products_model_threading.py tests/integration/test_products_model_remove_node.py tests/integration/test_inspector_extensions.py tests/integration/test_remote_data_provider.py tests/integration/test_data_producer_worker_no_provider.py tests/integration/test_drop_target.py tests/integration/test_widget_plot.py` — PASS. Then FULL.
- [ ] **Step 6: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml tests/integration/test_constructor_ownership.py
git commit -m "fix(bindings): every non-drawn QObject built with a parent belongs to it

Models, axes, behaviours, providers and extensions got the same
constructor markup as the widgets. A walk over the bound signatures
fails when a class with a parent argument has no builder (and so no
markup); drawn objects are left to part B.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Removals keep destroying; moves and Python subclasses

**Files:**
- Test: `tests/integration/test_removal_ownership.py` (create)
- Modify (only if a test fails): `SciQLopPlots/bindings/bindings.xml`

**Interfaces:**
- Consumes: `collect()`, `on_destroyed(obj)`; the panel constructor markup (Task 4).
- Produces: `DESTROY_FLAG_REMOVALS` (empty set) in the test: the bound removals taking a `destroy` argument, each requiring `<parent index="1" action="remove"/>` (spec change 2).

These tests are expected to pass once Tasks 4-5 are in: no bound removal keeps a widget (ambiguity 1). A failure is a real bug: fix it in this task with its own commit.

- [ ] **Step 1: Tests** — `tests/integration/test_removal_ownership.py`:

```python
"""Removals and moves after constructor parenting (widget-ownership design, change 2).

No bound removal keeps the widget today: remove_plot, removeWidget / remove_widget and
remove_panel all destroy it. SciQLopPlotContainer's remove_plot(plot, destroy) and
removeWidget(widget, destroy) unparent without destroying, but are not bound.
"""
import weakref

import pytest
import shiboken6

from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotsBindings
from shibokensupport.signature import get_signature

# Binding a removal that keeps the widget? Mark it <parent index="1" action="remove"/> (the C++
# unparents it, so Python owns it again, as with setParent(None)), list it here, and test that the
# kept widget is Python-owned afterwards.
DESTROY_FLAG_REMOVALS = set()


def _bound_functions_with_a_destroy_argument():
    found = set()
    for class_name, cls in vars(SciQLopPlotsBindings).items():
        if not isinstance(cls, type):
            continue
        for name, function in vars(cls).items():
            try:
                signature = get_signature(function)
            except Exception:  # noqa: BLE001 - not every attribute has a signature
                continue
            signatures = signature if isinstance(signature, list) else [signature] if signature else []
            if any("destroy" in s.parameters for s in signatures):
                found.add(f"{class_name}.{name}")
    return found


def test_every_removal_that_keeps_the_widget_is_listed():
    assert _bound_functions_with_a_destroy_argument() == DESTROY_FLAG_REMOVALS


@pytest.mark.parametrize("remove", ["remove_plot", "removeWidget", "remove_widget"])
def test_a_removal_deletes_a_plot_built_with_the_panel_as_parent(qtbot, panel, remove):
    plot = SciQLopPlot(panel)
    panel.add_plot(plot)
    gone = on_destroyed(plot)
    getattr(panel, remove)(plot)
    collect()
    assert (gone, shiboken6.isValid(plot)) == ([True], False)


def test_remove_panel_deletes_a_sub_panel_built_with_the_panel_as_parent(qtbot, panel):
    sub = SciQLopMultiPlotPanel(panel)
    panel.add_panel(sub)
    gone = on_destroyed(sub)
    panel.remove_panel(sub)
    collect()
    assert gone == [True]


class _TimeSyncLike(SciQLopMultiPlotPanel):
    """SciQLop's TimeSyncPanel shape: Python state, released through destroyed, not the GC."""
    released = []

    def __init__(self, parent):
        super().__init__(parent, synchronize_x=False, synchronize_time=True)
        self.state = ["kept"]
        self.destroyed.connect(lambda *_: _TimeSyncLike.released.append(True))


def test_a_parented_python_subclass_keeps_its_state_until_removed(qtbot, panel):  # focus 3
    _TimeSyncLike.released.clear()
    alive = weakref.ref(_TimeSyncLike(panel))
    collect()
    assert alive() is not None and alive().state == ["kept"]
    panel.add_panel(alive())
    panel.remove_panel(alive())
    collect()
    assert (_TimeSyncLike.released, alive()) == ([True], None)


def test_a_plot_moved_to_another_panel_outlives_the_first(qtbot):  # review focus 5
    first, second = SciQLopMultiPlotPanel(), SciQLopMultiPlotPanel()
    qtbot.addWidget(second)
    plot = SciQLopPlot(first)
    first.add_plot(plot)
    second.add_plot(plot)
    gone = on_destroyed(plot)
    del plot
    shiboken6.delete(first)
    collect()
    assert (gone, len(second.plots())) == ([], 1)
```

- [ ] **Step 2: Run** TEST `tests/integration/test_removal_ownership.py` — expected PASS. If `test_every_removal_that_keeps_the_widget_is_listed` reports a function, it is a bound removal with a `destroy` flag: add `<modify-argument index="1"><parent index="1" action="remove"/></modify-argument>` to its `modify-function`, list it, add a test that the kept widget has no parent and `shiboken6.ownedByPython(widget)` is True, CLEAN, BUILD. If `test_a_parented_python_subclass_keeps_its_state_until_removed` fails on `alive() is not None`, the constructor markup does not reach Python subclasses: stop and report.
- [ ] **Step 3: Run** TEST `tests/integration/test_removal_ownership.py tests/integration/test_thread_guards.py tests/integration/test_panel_api.py tests/integration/test_sub_panel_time_sync.py` — PASS.
- [ ] **Step 4: Commit**

```bash
git add tests/integration/test_removal_ownership.py
git commit -m "test(ownership): removals, moves and Python subclasses after constructor parenting

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

(Add `SciQLopPlots/bindings/bindings.xml` to `git add` only if Step 2 changed it.)

---

### Task 7: `setParent` and layouts follow the same rule

**Files:**
- Test: `tests/integration/test_reparent_ownership.py` (create)

**Interfaces:**
- Consumes: `collect()`, `on_destroyed(obj)`; Tasks 4-5's markup.
- Produces: nothing.

PySide marks `QWidget.setParent` (typesystem_widgets_common.xml:2033-2046) and `QObject.setParent`; our classes do not redeclare `setParent`, so the inherited binding applies. A probe on the current build already showed `plot.setParent(host)` and `setParent(None)` flipping `shiboken6.ownedByPython`. These tests pin it.

- [ ] **Step 1: Tests** — `tests/integration/test_reparent_ownership.py`:

```python
"""setParent and Qt layouts move ownership with the Qt parent (widget-ownership design, change 3)."""
import shiboken6
from PySide6.QtCore import QObject
from PySide6.QtWidgets import QSplitter, QVBoxLayout, QWidget

from conftest import collect, on_destroyed
from SciQLopPlots import SciQLopPlot, VPlotsAlign


def test_set_parent_none_gives_a_parented_widget_back_to_python(qtbot):
    host = QWidget()
    plot = SciQLopPlot(host)
    plot.setParent(None)
    gone = on_destroyed(plot)
    assert shiboken6.ownedByPython(plot)
    del plot
    collect()
    assert gone == [True]


def test_set_parent_none_gives_a_parented_qobject_back_to_python(qtbot):
    host = QWidget()
    align = VPlotsAlign(host)
    align.setParent(None)
    gone = on_destroyed(align)
    del align
    collect()
    assert gone == [True]


def test_set_parent_gives_a_widget_to_its_new_parent(qtbot):
    host = QWidget()
    plot = SciQLopPlot()
    plot.setObjectName("probe")
    plot.setParent(host)
    gone = on_destroyed(plot)
    del plot
    collect()
    assert (gone, host.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(host)
    assert gone == [True]


def _moved_by(first, second, add):
    plot = SciQLopPlot(first)
    plot.setObjectName("probe")
    add(plot)
    gone = on_destroyed(plot)
    del plot
    shiboken6.delete(first)
    collect()
    assert (gone, second.findChild(QObject, "probe") is not None) == ([], True)
    shiboken6.delete(second)
    assert gone == [True]


def test_a_layout_moves_a_widget_to_its_host(qtbot):  # review focus 2
    first, second = QWidget(), QWidget()
    layout = QVBoxLayout(second)
    _moved_by(first, second, layout.addWidget)


def test_a_splitter_moves_a_widget_to_itself(qtbot):  # review focus 2
    first, second = QWidget(), QSplitter()
    _moved_by(first, second, second.addWidget)
```

- [ ] **Step 2: Run** TEST `tests/integration/test_reparent_ownership.py` — expected PASS. If a `setParent` test fails, PySide's markup does not reach our class: stop and report which class and test (a redeclared `setParent` would need its own markup; none exists today, `grep -rn "void setParent" include` is empty).
- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_reparent_ownership.py
git commit -m "test(ownership): setParent, layouts and splitters move ownership with the parent

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Docs, portability, SciQLop survey and suite gate

**Files:**
- Modify: `docs/user-guide.md` ("Put a plot in your own window", after `graph.set_data(x, y)` replaces the data at any time...", ~line 106)
- Modify: `docs/api-review-2026-10-20.md` (Progress, phase 4 paragraph, after Task 3's note)

**Interfaces:**
- Consumes: everything above.
- Produces: the SciQLop patch description (Step 6), for the user to approve.

- [ ] **Step 1: User guide** — after the `graph.set_data(x, y)` sentence (the guide test runs this snippet):

````markdown
### Who keeps a plot alive

Like any Qt widget, a plot belongs to its parent. Without a parent, it belongs to its Python object.

```python
from PySide6.QtWidgets import QWidget
from SciQLopPlots import SciQLopPlot

host = QWidget()
SciQLopPlot(host)        # kept: host owns it, no Python reference needed
plot = SciQLopPlot()     # no parent: dropping the last reference deletes it
plot.setParent(host)     # host owns it now
plot.setParent(None)     # Python owns it again
plot.deleteLater()       # delete a widget now, whoever owns it
```

Adding a plot to a panel or a layout gives it to that panel or layout's widget. The panel's
`remove_plot`, `remove_widget` and `remove_panel` delete what they remove.
````

- [ ] **Step 2: API review note** — append to the phase 4 paragraph: `Item 1.1 for widgets done: a widget or non-drawn QObject built with a parent belongs to the parent, as in PySide (shiboken skips its constructor heuristic on parent-management classes, so each constructor got explicit markup; a walk over the bound signatures catches a new class). Fixed on the way: PropertiesPanel ignored its parent. Break: code that dropped the last reference to delete a parented widget now keeps it until the parent dies; it calls deleteLater() or removes it. Drawn objects (items, graphs, legend) are part B.`
- [ ] **Step 3: Portability check** — clang's syntax check on the C++ this plan changed:

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots/build-venv && ../.venv/bin/python - <<'EOF'
import json, pathlib, shlex, subprocess
changed = {"SciQLopNDProjectionCurves.cpp", "SciQLopDrawnObject.cpp", "QCPAbstractPlottableWrapper.cpp",
           "SciQLopGraphComponent.cpp", "SciQLopMultiGraphBase.cpp", "SciQLopTimeline.cpp",
           "SciQLopHistogram.cpp", "PropertiesPanel.cpp"}
failed = []
for entry in json.load(open("compile_commands.json")):
    path = pathlib.Path(entry["file"])
    if path.name in changed and path.parent.name == "src":
        args = shlex.split(entry["command"])
        result = subprocess.run(["clang++", "-fsyntax-only", *args[1:]], cwd=entry["directory"],
                                capture_output=True, text=True)
        if result.returncode != 0:
            failed.append((path.name, result.stderr[-2000:]))
for name, err in failed:
    print(name, err)
print("clang: OK" if not failed else f"clang: {len(failed)} failed")
EOF
```

  Expected: `clang: OK` (warnings about GCC-only flags are fine; errors in our files are not: fix them).
- [ ] **Step 4: Full SciQLopPlots suite** — FULL.
- [ ] **Step 5: SciQLop and the plugins** against `build-venv`: from /home/jeandet/Documents/prog/SciQLop, `xvfb-run -a -s "-screen 0 2560x1440x24" env PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv timeout 590 .venv/bin/python -m pytest <files> -q -p no:cacheprovider -o addopts="" --no-xvfb`, in chunks under 590 s. Plugins `cdf_workbench`, `sciqlop_msa`, `sciqlop_radio`, `sciqlop_sismo` (under /home/jeandet/Documents/prog/plugins_sciqlop) with `PYTHONPATH=<build-venv>:<plugin dir>`. Known pre-existing failure: `test_onboarding_integration.py` drag step. Expected new failure: `tests/test_gui_leftover_sweep.py::test_release_covers_a_parented_python_subclass_widget` on its precondition `assert _has_ownership(panel)` (the parent owns a parented `TimeSyncPanel` now). Check especially the dock and panel tests (`test_panel_container.py`, `test_catalog_panel_removal.py`, `test_panel_close_busy.py`: QtAds `setWidget` reparenting, review focus 2) and `test_gui_leftover_sweep.py`. End with `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` empty. Do not edit SciQLop or the plugins.
- [ ] **Step 6: SciQLop patch description** — write it in the final report for the user to approve, never applied. Start from this survey (read-only, 2026-10-23), confirmed or corrected by Step 5:
  1. `tests/test_gui_leftover_sweep.py::test_release_covers_a_parented_python_subclass_widget`: replace `assert _has_ownership(panel), "expected a parented Python-subclass widget to be owned"` with `assert not _has_ownership(panel), "the parent owns a parented panel"`, and rename the test `test_a_parented_python_subclass_widget_belongs_to_its_parent`. The release calls after it still pass (releasing what Python doesn't own is harmless).
  2. `tests/conftest.py:313-314` (`_release_leftover_widget_ownership` docstring): drop "and widgets of a Python subclass, which keep Python ownership even when parented (a `TimeSyncPanel(parent=container)` is still force-destroyed)"; with SciQLopPlots >= this release, only unparented widgets stay Python-owned.
  3. No production change: `user_api/plot/_panel.py:201` hands the sub-panel to `add_panel` right away; `components/comparison/ui/inner_panel.py:16` (`TimeSyncPanel(parent=parent)`) returns the panel to callers that keep it; `core/ui/mainwindow.py:469,521` keep `ProductsView(self)` / `PropertiesPanel(self)` in attributes (`PropertiesPanel` is now really parented to the window until its dock takes it); the `InspectorExtension` subclasses built with `parent=graph` / `parent=renderer` / `host` were already handed to `add_inspector_extension`; `cdf_workbench/preview.py:75-78` keeps its four plots in a dict and a layout. Nothing relies on dropping a reference to delete a parented widget.
  4. SciQLop's SciQLopPlots pin moves to the release carrying this change.
- [ ] **Step 7: Reviews** — an opus review and an opencode review (started with `-- --auto`) of `git diff main...HEAD` restricted to this plan's commits. Fix every real finding with its own reproducer first; flag false positives.
- [ ] **Step 8: Commit docs**

```bash
git add docs/user-guide.md docs/api-review-2026-10-20.md
git commit -m "docs(ownership): a widget belongs to its parent, as in Qt

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

## Self-review

**Spec coverage.**
- The rule and change 1 (constructors, exact signatures, parent not always last): Task 4 (widgets, including the panel's existing entry and `SciQLopNDProjectionPlot` index 2), Task 5 (non-widget QObjects, `ProductsModelNode`'s two constructors, `PlotDragNDropCallback` index 3).
- The walk test reading bound signatures: Task 5, `test_every_parent_constructor_has_a_builder`.
- Change 2 (non-destroying removals, `action="remove"`, walk over `destroy` arguments): Task 6; nothing is bound with a `destroy` flag (ambiguity 1), so the walk guards the future and the destroying removals are pinned. `clear()` stays outside, as the spec says.
- Change 3 (`setParent`): Task 7 pins PySide's markup on our classes; no markup needed.
- Change 4 (nothing else changes for callers who keep references): the existing suite (FULL in Tasks 2, 4, 5, 8).
- Tests section: parented and unparented widgets, `findChild`, deletion with the host (Tasks 4, 5, through `destroyed`, a stronger proof than `isValid`), `setParent(None)` (Task 7), class walk (Task 5), removals (Task 6), item-ownership tests unchanged (FULL), SciQLop and plugins gate with a survey (Task 8).
- Who is affected / SciQLop survey: Task 8 Step 6; the survey found one SciQLop test the spec's survey did not mention (`test_gui_leftover_sweep.py`), and `inner_panel.py:16`, which the spec's "everywhere except `_panel.py:201`" missed (safe).
- Tasks A, B, C of the brief: Tasks 1, 2, 3.

**Placeholder scan.** Every code step has the code; every run step has the command and the expected result. The only conditional steps (Task 6 Step 2, Task 7 Step 2) say exactly what to add or to stop and report.

**Type consistency.** `collect()` and `on_destroyed(obj)` are defined in Task 4 Step 1 and used under those names in Tasks 5-7. `check_siblings(const QList<SciQLopGraphComponentInterface*>&, const char*) const` is declared and defined in Task 2 Step 4 with the same signature. `BUILDERS` keys in Task 5 match the bound class names the walk computes; the widget rows repeat Task 4's `WIDGETS` builders on purpose (each test file reads on its own). `DESTROY_FLAG_REMOVALS` is a `set` compared with a `set`.

**Review Focus.** Each of the five lines has its test in its owning task: 1 and 4 in Task 4, 3 and 5 in Task 6, 2 in Task 7 (the QtAds path in Task 8 Step 5).
