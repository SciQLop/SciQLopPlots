# One span class, one line class (Plan A): implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One span class (`SciQLopSpan`) and one line class (`SciQLopStraightLine`), each at plot or panel scope, built by `span()` / `straight_line()` and their shorthands. The seven old names stay as empty subclasses, so SciQLop keeps working unchanged.

**Architecture:**
- `SciQLopSpan` derives from `SciQLopDrawnObject`. It holds a small target: `PlotSpanTarget` owns one NeoQCP span item; `PanelSpanTarget` holds the state and owns one plot-scope `SciQLopSpan` per `SciQLopPlot` of the panel. The public class never branches on scope.
- `SciQLopStraightLine` gets the same split: `PlotLineTarget` owns the `StraightLine` item, `PanelLineTarget` holds the state and the parts.
- The panel plumbing shared by both (parts follow `plot_list_changed`, `QPointer`s, `mark_panel_part`, snapshot loops, the echo flag) is one template, `impl::PanelParts<Part>`.
- The factories live on `SciQLopPlotInterface` (NotImplementedError by default), `SciQLopPlot` and `SciQLopMultiPlotPanel`. The shorthands are non-virtual and forward to `span()` / `straight_line()`.
- The old classes become subclasses that only fix kind and scope in their constructors. Their old Python properties stay in bindings.xml, where a property shadows the base method of the same name.

**Tech Stack:** C++20, Qt 6.11.2, NeoQCP, Shiboken6 6.11.2 (bindings.xml), pytest + pytest-qt.

**Spec:** docs/plans/2026-10-25-span-line-unification-design.md (revision 4). Implement **Plan A only**. Read the spec before your task. Where this plan and the spec disagree, the spec wins, except for the deviations listed at the end of this plan, which the controller approved.

## Global Constraints

- Build: one build at a time, in the foreground, `-j 4`, shell timeout 600000 ms. Never background a build or a test run. Never start a second one while one runs. A build longer than 600 s is normal: rerun the same command, it resumes.
- Build commands: run them from the repo root, with Qt on PATH and `LLVM_INSTALL_DIR=/usr`:
  ```bash
  export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH"
  export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
  export LLVM_INSTALL_DIR=/usr
  .venv/bin/meson compile -C build-venv -j 4
  ```
- When bindings.xml gains or loses an `<object-type>`, reconfigure first (the list of generated wrappers is computed at configure time, by `bindings/helper_scripts/src_list.py`), with the same environment, then compile:
  ```bash
  .venv/bin/meson setup --reconfigure build-venv
  ```
- After a header-only change that a binding uses, a wrapper may not regenerate. If so, `rm` that `build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/<class>_wrapper.cpp` (lower case) and compile again.
- Tests: run from /tmp, as their own command, never chained after the build. Use `timeout 540`:
  ```bash
  cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots timeout 540 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <paths> -q -p no:cacheprovider
  ```
  Test paths are absolute (`/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_span.py`); this plan writes them relative to the repo for short.
- Full suite: `tests/integration tests/unit tests/fuzzing tests/resampler tests/tracing`. Never plain `tests/`, because manual-tests hang. Read the real pass/fail count and exit code. If it takes more than 540 s, split it into two halves.
- Signature check after every bindings change: `tests/integration/test_binding_signatures.py` must show zero warnings.
- TDD: write the test first and watch it fail for the expected reason, then implement. A test marked **guard** pins existing behaviour that the task's change could break: it passes before the change and must still pass after.
- Commits:
  - Use explicit paths, never `git add -A`.
  - `qt.ini` is not ours: never stage it.
  - End every message with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
  - Never push.
- `cat` and `ls` are aliased in this shell. Use `command cat`, `command ls` and `test -f`.
- Never touch /home/jeandet/Documents/prog/SciQLop: no reads, no greps, no runs.
- Errors from C++ to Python:
  - `std::invalid_argument` → ValueError
  - `sqp::unsupported_call` → NotImplementedError
  - other `std::exception` → RuntimeError (off-thread calls)
  - a wrong argument type → TypeError (from shiboken overload resolution)

  A C++ function that throws needs a template in bindings.xml (`void_raises_valueerror`, `returning_raises_python_errors`), or shiboken turns every throw into a RuntimeError.
- A setter that can throw checks before `SCIQLOP_ON_OWNER_THREAD`: a throw from the queued call would escape the event loop.
- Deprecation message format, used verbatim by the tests and by the pyproject warning filter: `<old>(...) is deprecated: use <receiver>.<new>(...)`, for example `rectangular_span(...) is deprecated: use plot.span(...)`. Deprecations are emitted from Python wrappers with `stacklevel=2`.
- No Python wrapper ever replaces a Qt slot (tests/integration/test_slot_connections_survive_gc.py guards it). The wrappers this plan adds (`span` in branch F-py, the line shorthands, `rectangular_span`, `movable`, `set_movable`) are plain methods, not slots.
- The wasm side (`wasm/sciqlopplots_js.cpp`) is not compiled on this branch. Keep it compiling by keeping what it uses: `MultiPlotsVerticalSpan(panel, range, color, read_only, visible, tool_tip)`, `set_range`, `range()`, `range_changed`, `selection_changed`, `delete_requested`, `SciQLopMultiPlotPanel::remove`.
- Do not dispatch subagents.

## Review Focus

1. **A bool second argument to `plot.vertical_line` / `plot.horizontal_line`.** It meant `movable` before this plan. The spec says it "now fails on the colour slot", but it doesn't: PySide converts a bool to a `QColor` (`True` gives rgb (0, 0, 1)). It was checked against build-venv: `plot.vertical_span(R(1, 2), True)` is accepted today. So an old call would silently make a near-black, read-only line. It must raise TypeError, naming the colour and `read_only=`. Task 9 adds the guard and its test.
2. **Reordering plots in a panel (`panel.move_plot`).** `plot_list_changed` fires with the same plots. A panel span or line must keep exactly one part per plot, never a duplicate. Task 4 tests it for spans, Task 7 for lines.
3. **A panel holding a plot that isn't a `SciQLopPlot`** (an N-D projection plot next to time series). Panel spans and lines must skip it: no part on it, no crash, edits still reach the other plots. Task 4 tests it.
4. **A setter on an unbounded axis, called from a worker thread** (`span.set_value_range(...)` on a vertical span). It must raise ValueError in the worker, never post a call that throws inside the GUI event loop. Task 2 tests it.
5. **A slot on the panel span's `range_changed` that removes the panel span**, triggered by an edit on one of its parts. This deletes the part that is emitting. It must not crash, and nothing must emit afterwards. Task 4 tests it for spans, Task 7 for lines (`position_changed`).

---

## Spike ledger

Task 1 fills this in. Every later task reads it and follows the branch it names.

- **Outcome:** PENDING (one of `O`, `F`, `F-py`)
- **Evidence:** PENDING (the probe output lines, pasted)
- **Extra lines needed by the outcome:** PENDING (for `O`: any `mapping.type_map` entries the signature check needed; otherwise "none")

What is already known before the spike: the build-venv generator log has no type entry for `std::optional` at all. For example, `build-venv/SciQLopPlots/bindings/mjb_rejected_functions.log` lists `std::optional<QFont> QFontComboBox::displayFont(const QString&)const: unmatched return type 'std::optional<QFont>': Unable to translate type "std::optional<QFont>": Cannot find type entry for "std::optional"`. So bare `std::optional` is rejected. The question the spike answers is whether a `<container-type name="std::optional">` entry of ours makes it work (branch O), or whether we map None by hand (branch F, or F-py).

## Branch reference

The code that differs by spike outcome, defined once. Tasks name the snippet (B1 … B6) and use the variant the ledger names. F and F-py share everything except B4.

**Common to every branch:** C++ represents "this axis is unbounded" inside the class with the span's `kind()`. In the factory arguments, branch O uses `std::nullopt`; branches F and F-py use a null `SciQLopPlotRange()` (both bounds NaN, `is_null()` true).

### B1: `SciQLopSpan::key_range()` / `value_range()`

Branch O, header (public section of `SciQLopSpan`):
```cpp
    //! The range of the key (x) or value (y) axis; std::nullopt (None) on an axis the span does
    //! not bound.
    [[nodiscard]] std::optional<SciQLopPlotRange> key_range() const;
    [[nodiscard]] std::optional<SciQLopPlotRange> value_range() const;
```
Branch O, src/SciQLopSpan.cpp:
```cpp
std::optional<SciQLopPlotRange> SciQLopSpan::key_range() const
{
    if (!bounds(SpanAxis::Key))
        return std::nullopt;
    return m_target->range(SpanAxis::Key);
}

std::optional<SciQLopPlotRange> SciQLopSpan::value_range() const
{
    if (!bounds(SpanAxis::Value))
        return std::nullopt;
    return m_target->range(SpanAxis::Value);
}
```
Branches F and F-py, header:
```cpp
    //! The range of the key (x) or value (y) axis. Throws std::invalid_argument on an axis the
    //! span does not bound; Python reads None there (bindings.xml).
    [[nodiscard]] SciQLopPlotRange key_range() const;
    [[nodiscard]] SciQLopPlotRange value_range() const;
```
Branches F and F-py, src/SciQLopSpan.cpp:
```cpp
SciQLopPlotRange SciQLopSpan::key_range() const
{
    require_bounded(SpanAxis::Key, "key_range");
    return m_target->range(SpanAxis::Key);
}

SciQLopPlotRange SciQLopSpan::value_range() const
{
    require_bounded(SpanAxis::Value, "value_range");
    return m_target->range(SpanAxis::Value);
}
```

### B2: the `span()` factory's two range parameters, and how a body reads them

The rest of the signature is the same everywhere: `QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "", Coordinates coordinates = Coordinates::Data, const QString& id = ""`.

Branch O, parameters:
```cpp
std::optional<SciQLopPlotRange> key_range = std::nullopt,
std::optional<SciQLopPlotRange> value_range = std::nullopt,
```
Branch O, the kind and the ranges a body passes to a constructor:
```cpp
SciQLopPlots::span_kind(key_range.has_value(), value_range.has_value()),
key_range.value_or(SciQLopPlotRange()), value_range.value_or(SciQLopPlotRange()),
```
Branches F and F-py, parameters:
```cpp
const SciQLopPlotRange& key_range = SciQLopPlotRange(),
const SciQLopPlotRange& value_range = SciQLopPlotRange(),
```
Branches F and F-py, kind and ranges:
```cpp
SciQLopPlots::span_kind(!key_range.is_null(), !value_range.is_null()), key_range, value_range,
```
The bindings.xml signature string of `span` in each branch:
- O: `span(std::optional&lt;SciQLopPlotRange&gt;,std::optional&lt;SciQLopPlotRange&gt;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)`
- F and F-py: `span(const SciQLopPlotRange&amp;,const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)`

### B3: bindings for the range getters (inside `<object-type name="SciQLopSpan">`)

Branch O: nothing here. The `<container-type name="std::optional">` entry that Task 1 validated goes at the top level of bindings.xml, next to the `<smart-pointer-type>` line (~168), copied from the ledger.

Branches F and F-py:
```xml
        <modify-function signature="key_range()const">
            <inject-code class="target" position="beginning">
                if (%CPPSELF.kind() == SpanKind::Horizontal) {
                    Py_INCREF(Py_None);
                    %PYARG_0 = Py_None;
                } else {
                    <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
                }
            </inject-code>
        </modify-function>
        <modify-function signature="value_range()const">
            <inject-code class="target" position="beginning">
                if (%CPPSELF.kind() == SpanKind::Vertical) {
                    Py_INCREF(Py_None);
                    %PYARG_0 = Py_None;
                } else {
                    <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
                }
            </inject-code>
        </modify-function>
```

### B4: None in the `span()` arguments

Branch O: nothing. The container-type converts None.

Branch F: a template next to the others at the top of bindings.xml (after `void_raises_python_errors`):
```xml
    <!-- span(key_range=None, ...): None is an unbounded axis, a null SciQLopPlotRange() in C++
         (span/line unification plan, spike ledger: branch F). -->
    <template name="none_is_unbounded_range">
        SciQLopPlotRange %out;
        if (%in != Py_None) {
            if (!%ISCONVERTIBLE[SciQLopPlotRange](%in)) {
                PyErr_SetString(PyExc_TypeError, "a span range must be a SciQLopPlotRange or None");
                return {};
            }
            %out = %CONVERTTOCPP[SciQLopPlotRange](%in);
        }
    </template>
```
and in every `span(...)` modify-function (plot interface and panel), before the `return` argument:
```xml
            <modify-argument index="1">
                <replace-type modified-type="PyObject"/>
                <replace-default-expression with="Py_None"/>
                <conversion-rule class="native"><insert-template name="none_is_unbounded_range"/></conversion-rule>
            </modify-argument>
            <modify-argument index="2">
                <replace-type modified-type="PyObject"/>
                <replace-default-expression with="Py_None"/>
                <conversion-rule class="native"><insert-template name="none_is_unbounded_range"/></conversion-rule>
            </modify-argument>
```
If Task 1 had to adjust this markup to make the probe pass, use the ledger's version.

Branch F-py: no markup. SciQLopPlots/__init__.py maps None, in the block that starts with `import functools` (~line 352), after `_accept_ptr_handle`:
```python
# span(key_range=None, ...): shiboken 6.11.2 cannot take None for a SciQLopPlotRange argument
# (spike ledger, branch F-py), so it is mapped here. A null SciQLopPlotRange() is an unbounded axis.
def _none_is_unbounded(span):
    @functools.wraps(span)
    def wrapper(self, key_range=None, value_range=None, *args, **kwargs):
        unbounded = SciQLopPlotsBindings.SciQLopPlotRange()
        return span(self, unbounded if key_range is None else key_range,
                    unbounded if value_range is None else value_range, *args, **kwargs)
    return wrapper


for _cls in (SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlot, SciQLopMultiPlotPanel):
    if "span" in vars(_cls):
        _cls.span = _none_is_unbounded(vars(_cls)["span"])
```
Task 2 adds the loop with the first two classes; Task 4 adds `SciQLopMultiPlotPanel`.

### B5: `SciQLopStraightLine::min_value()` / `max_value()`

Branch O, header and src/SciQLopStraightLines.cpp:
```cpp
    //! The clamp bounds, std::nullopt (None) when unset.
    [[nodiscard]] std::optional<double> min_value() const;
    [[nodiscard]] std::optional<double> max_value() const;
```
```cpp
std::optional<double> SciQLopStraightLine::min_value() const
{
    return m_target->min_value();
}

std::optional<double> SciQLopStraightLine::max_value() const
{
    return m_target->max_value();
}
```
Branches F and F-py:
```cpp
    //! The clamp bounds, NaN when unset; Python reads None there (bindings.xml).
    [[nodiscard]] double min_value() const;
    [[nodiscard]] double max_value() const;
```
```cpp
double SciQLopStraightLine::min_value() const
{
    return m_target->min_value().value_or(std::numeric_limits<double>::quiet_NaN());
}

double SciQLopStraightLine::max_value() const
{
    return m_target->max_value().value_or(std::numeric_limits<double>::quiet_NaN());
}
```
plus, F and F-py only, a template at the top of bindings.xml:
```xml
    <!-- A getter returning NaN for "unset": Python reads None. -->
    <template name="nan_is_none">
        const double value = %CPPSELF.%FUNCTION_NAME();
        if (std::isnan(value)) {
            Py_INCREF(Py_None);
            %PYARG_0 = Py_None;
        } else {
            %PYARG_0 = %CONVERTTOPYTHON[double](value);
        }
    </template>
```
and inside `<object-type name="SciQLopStraightLine">`:
```xml
        <modify-function signature="min_value()const">
            <inject-code class="target" position="beginning"><insert-template name="nan_is_none"/></inject-code>
        </modify-function>
        <modify-function signature="max_value()const">
            <inject-code class="target" position="beginning"><insert-template name="nan_is_none"/></inject-code>
        </modify-function>
```

### B6: "unbounded" in a shorthand body

The shorthands call `span()` with one range unbounded. Branch O writes `std::nullopt`; branches F and F-py write `SciQLopPlotRange()`. Tasks 9 and 10 show the O spelling; under F and F-py, replace each `std::nullopt` in those bodies with `SciQLopPlotRange()`.

---

## File map

| File | Change | Tasks |
|---|---|---|
| `include/SciQLopPlots/enums.hpp` | `SpanKind` | 2 |
| `include/SciQLopPlots/Items/SciQLopSpan.hpp` (new) | the public span class, `SpanAxis`, `span_kind` | 2, 3, 4, 11 |
| `src/SciQLopSpan.cpp` (new) | its definitions | 2, 3, 4, 11 |
| `include/SciQLopPlots/Items/impl/SpanTargets.hpp` (new, internal) | `SpanTarget`, `PlotSpanTarget`, `SpanState`, `PanelSpanTarget` | 2, 3, 4, 11 |
| `src/SpanTargets.cpp` (new) | the targets | 2, 3, 4, 11 |
| `include/SciQLopPlots/Items/impl/PanelParts.hpp` (new, internal) | the parts template, `panel_host` | 4 |
| `include/SciQLopPlots/Items/impl/LineTargets.hpp` (new, internal), `src/LineTargets.cpp` (new) | `LineTarget`, `PlotLineTarget`, `LineState`, `PanelLineTarget` | 5, 7, 12 |
| `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, `src/SciQLopStraightLines.cpp` | line class on a target; `StraightLine::set_coordinates`; clamp getters; panel constructor | 5, 6, 7, 12 |
| `include/SciQLopPlots/SciQLopPlotInterface.hpp`, `src/SciQLopPlotInterface.cpp` | `span`, `straight_line`, shorthands, `remove(SciQLopSpan*)`, wrong-scope message | 2, 4, 5, 9 |
| `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp` | `span`, `straight_line` overrides; old factory overrides removed | 2, 5, 9 |
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp` | `span`, `straight_line`, shorthands, `remove` overloads | 4, 7, 10, 11, 12 |
| `include/SciQLopPlots/Items/SciQLop{Vertical,Horizontal,Rectangular}Span.hpp` | empty subclasses (+ legacy shims) | 11 |
| `src/SciQLop{Vertical,Horizontal,Rectangular}Span.cpp`, `src/SciQLopSpanBase.cpp`, `include/SciQLopPlots/Items/SciQLopSpanBase.hpp` | deleted | 11 |
| `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp`, `src/MultiPlotsVSpan.cpp` | the old panel span on `SciQLopSpan`; the collection unchanged | 11 |
| `include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp`; `src/MultiPlotsVLine.cpp` (deleted) | the old panel line on `SciQLopStraightLine` | 12 |
| `src/meson.build` | sources and moc headers | 2, 5, 11, 12 |
| `SciQLopPlots/bindings/bindings.xml`, `bindings.h` | types, ownership, errors, old properties | 1-7, 9-12 |
| `SciQLopPlots/__init__.py` | `.on` and alias tables, factory wrappers, deprecations | 2 (F-py), 4 (F-py), 5, 8, 9, 11, 12, 13 |
| `pyproject.toml` | the warning filter also catches `use line.` | 13 |
| tests (new) | `tests/integration/test_span.py`, `test_panel_span.py`, `test_straight_line.py`, `test_old_span_line_names.py` | 2-8, 11-13 |
| tests (changed) | `test_drawn_contract.py` (2, 4, 5, 7, 9, 10, 11, 12, 16), `test_item_factories.py` (5, 9, 10, 11), fallout of the factory switch (9, 10), the rest (14) | |
| `docs/user-guide.md`, `README.md`, `tests/manual-tests/` | the new API, the migration note | 10, 15 |
| `tests/sweeps/self_delete/common.py` | builders for every kind and scope | 16 |

---

### Task 1: Spike: how None crosses the bindings

**Files:**
- Create (throwaway, deleted in Step 8): `include/SciQLopPlots/OptionalSpike.hpp`
- Modify (reverted in Step 8): `SciQLopPlots/bindings/bindings.h`, `SciQLopPlots/bindings/bindings.xml`
- Modify (kept): this plan's **Spike ledger** section

**Interfaces:**
- Produces: the ledger's outcome, `O`, `F` or `F-py`, with the probe output as evidence. Tasks 2-12 read it.
- Leaves no code in the tree.

The question: can a getter return None and an argument take None, for `SciQLopPlotRange` and for `double`?
- Branch O: `std::optional<T>` both ways, through a `<container-type>` entry of ours.
- Branch F: plain C++ types, with None mapped by bindings markup (B3, B4, B5).
- Branch F-py: like F, except the `span()` arguments are mapped by a Python wrapper (B4).

**Exit criterion: the ledger is filled** (outcome, evidence, extra lines). The spike class compiling, or not, is evidence, not the goal.

The decision rule:
1. O if every O probe passes in Step 6 and the signature scan shows no warning (or only warnings that a `mapping.type_map` entry silences; record those entries).
2. Else F if every F probe passes in Step 4.
3. Else F-py if the F getter probes pass (`key_range`, `min_value`).
4. Else stop and report to the controller with the probe output.

- [ ] **Step 1: Write the throwaway class**

`include/SciQLopPlots/OptionalSpike.hpp`:

```cpp
#pragma once
// Throwaway: task 1 of docs/plans/2026-10-25-span-line-unification-plan.md, deleted by that task.
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include <cmath>
#include <optional>
#include <stdexcept>

class OptionalSpike
{
    bool m_bounded = true;

public:
    OptionalSpike() = default;

    // Branch O.
    std::optional<SciQLopPlotRange> echo_range(std::optional<SciQLopPlotRange> range = std::nullopt) const
    {
        return range;
    }
    std::optional<double> echo_value(std::optional<double> value = std::nullopt) const { return value; }

    // Branch F.
    void set_bounded(bool bounded) { m_bounded = bounded; }
    bool bounded() const { return m_bounded; }
    SciQLopPlotRange key_range() const
    {
        if (!m_bounded)
            throw std::invalid_argument("key_range: unbounded");
        return SciQLopPlotRange(1.0, 2.0);
    }
    double min_value() const { return m_bounded ? 3.0 : std::nan(""); }
    bool is_bounded_by(const SciQLopPlotRange& key_range = SciQLopPlotRange(), int tag = 0) const
    {
        return !key_range.is_null() && tag >= 0;
    }
};
```

In `SciQLopPlots/bindings/bindings.h`, after `#include <SciQLopPlots/Items/SciQLopStraightLines.hpp>` (line 34): `#include <SciQLopPlots/OptionalSpike.hpp>`.

- [ ] **Step 2: Add the F markup**

In bindings.xml, add the `nan_is_none` and `none_is_unbounded_range` templates exactly as in B4 and B5, after the `void_raises_python_errors` template (~line 120). Then, after `<object-type name="SciQLopRectangularSpan" parent-management="yes"/>` (~line 1313):

```xml
    <object-type name="OptionalSpike">
        <modify-function signature="key_range()const">
            <inject-code class="target" position="beginning">
                if (!%CPPSELF.bounded()) {
                    Py_INCREF(Py_None);
                    %PYARG_0 = Py_None;
                } else {
                    <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
                }
            </inject-code>
        </modify-function>
        <modify-function signature="min_value()const">
            <inject-code class="target" position="beginning"><insert-template name="nan_is_none"/></inject-code>
        </modify-function>
        <modify-function signature="is_bounded_by(const SciQLopPlotRange&amp;,int)const">
            <modify-argument index="1">
                <replace-type modified-type="PyObject"/>
                <replace-default-expression with="Py_None"/>
                <conversion-rule class="native"><insert-template name="none_is_unbounded_range"/></conversion-rule>
            </modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="bool"/></insert-template>
            </inject-code>
        </modify-function>
    </object-type>
```

No container-type yet: the O methods are expected to be rejected in this build.

- [ ] **Step 3: Reconfigure and build**

The object-type is new, so reconfigure first (Global Constraints), then compile. Each is one command, in the foreground. If the generator or the compiler rejects the F markup, try once with an adjusted spelling (for example `%PYARG_1` instead of `%in`, or a check with `Shiboken::Conversions::isPythonToCppConvertible`). Write each attempt and its error into the ledger evidence. If no spelling builds, the F argument mapping has failed: go on to Step 5 with the getter markup alone, keeping `is_bounded_by` without its `modify-argument`.

Then check what shiboken rejected:
```bash
grep -n "OptionalSpike" build-venv/SciQLopPlots/bindings/mjb_rejected_functions.log
```
Expected: `echo_range` and `echo_value` rejected with `Cannot find type entry for "std::optional"`. Paste the lines into the ledger.

- [ ] **Step 4: Probe branch F**

Write this probe to a scratch file outside the repo, for example `/tmp/optional_spike_probe.py`:

```python
import warnings
from PySide6.QtWidgets import QApplication
app = QApplication.instance() or QApplication([])
from SciQLopPlots import SciQLopPlotsBindings as B, SciQLopPlotRange as R
from shibokensupport.signature import get_signature

s = B.OptionalSpike()
results = {}


def check(name, call):
    try:
        results[name] = repr(call())
    except Exception as e:  # the probe records every outcome, errors included
        results[name] = f"{type(e).__name__}: {e}"


s.set_bounded(False)
check("F key_range unbounded (want None)", s.key_range)
check("F min_value unset (want None)", s.min_value)
s.set_bounded(True)
check("F key_range bounded (want the range)", s.key_range)
check("F min_value set (want 3.0)", s.min_value)
check("F arg omitted (want False)", lambda: s.is_bounded_by())
check("F arg None (want False)", lambda: s.is_bounded_by(None))
check("F arg keyword None (want False)", lambda: s.is_bounded_by(key_range=None, tag=1))
check("F arg range (want True)", lambda: s.is_bounded_by(R(1.0, 2.0)))
check("F arg wrong type (want TypeError)", lambda: s.is_bounded_by("x"))
check("O echo None (want None)", lambda: s.echo_range(None))
check("O echo omitted (want None)", lambda: s.echo_range())
check("O echo range (want the range)", lambda: s.echo_range(R(1.0, 2.0)))
check("O echo wrong type (want TypeError)", lambda: s.echo_range("x"))
check("O echo value (want 2.5)", lambda: s.echo_value(2.5))
check("O echo value None (want None)", lambda: s.echo_value(None))
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    for name in ("key_range", "min_value", "is_bounded_by", "echo_range", "echo_value"):
        if hasattr(B.OptionalSpike, name):
            check(f"signature {name}", lambda name=name: str(get_signature(getattr(B.OptionalSpike, name))))
results["signature warnings (want [])"] = [str(w.message) for w in caught]
for key, value in results.items():
    print(f"{key}: {value}")
```

Run it like a test, from /tmp, with the test environment:
```bash
cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots timeout 120 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python /tmp/optional_spike_probe.py
```
Expected: every `F ...` line shows the wanted result; the `O ...` lines show `AttributeError` (the methods were rejected). Paste the output into the ledger.

- [ ] **Step 5: Add the container-type for branch O**

At the top level of bindings.xml, after the `<smart-pointer-type name="QPointer" .../>` line (~168):

```xml
    <!-- std::optional<T> as T or None (span/line unification plan, spike ledger: branch O). -->
    <container-type name="std::optional" type="list">
        <include file-name="optional" location="global"/>
        <conversion-rule>
            <native-to-target>
                if (!%in.has_value())
                    Py_RETURN_NONE;
                return %CONVERTTOPYTHON[%INTYPE_0](*%in);
            </native-to-target>
            <target-to-native>
                <add-conversion type="Py_None">
                    %out = %OUTTYPE();
                </add-conversion>
                <add-conversion type="PyObject" check="%ISCONVERTIBLE[%OUTTYPE_0](%in)">
                    %out = %OUTTYPE(%CONVERTTOCPP[%OUTTYPE_0](%in));
                </add-conversion>
            </target-to-native>
        </conversion-rule>
    </container-type>
```

Build (no reconfigure: no object-type changed). If the generator rejects the `check` attribute, try once without it and rely on the wrong-type probe. If no version builds, branch O has failed: record the error and skip to Step 7.

- [ ] **Step 6: Probe branch O**

Run the same probe again. Branch O passes when every `O ...` line shows the wanted result, the `F ...` lines still do, and the signature line is `[]`. If the only failures are signature warnings naming an `std.optional[...]` or `std.nullopt` spelling, add the matching entries to `mapping.type_map` in a scratch copy of `_register_with_shiboken_signatures` (SciQLopPlots/__init__.py:60) and rerun. If that silences them, branch O passes: record the exact entries in the ledger's "Extra lines" field.

- [ ] **Step 7: Decide and fill in the ledger**

Apply the decision rule above. Replace the three PENDING fields of the **Spike ledger** section with the outcome, the pasted probe output of both runs, the rejected-log lines, and the extra lines (or "none"). If an F markup spelling had to change in Step 3, paste the version that worked into the ledger: Tasks 2-7 use it instead of B4's text.

- [ ] **Step 8: Remove the spike from the tree, rebuild, check**

```bash
rm include/SciQLopPlots/OptionalSpike.hpp
git checkout SciQLopPlots/bindings/bindings.h SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py
git status --short
```
Expected status: only `docs/plans/2026-10-25-span-line-unification-plan.md` modified (and the untracked `qt.ini`, which is not ours). Then reconfigure (the object-type is gone) and compile, as two commands. Then run `tests/integration/test_binding_signatures.py tests/integration/test_items_api.py`. Expected: all pass.

- [ ] **Step 9: Commit the ledger**

```bash
git add docs/plans/2026-10-25-span-line-unification-plan.md
git commit -m "docs(plans): span/line unification spike outcome (how None crosses the bindings)"
```

---

### Task 2: The `SciQLopSpan` core at plot scope, and `plot.span()`

This task builds the class, its plot target, the factory and the kind and unbounded-axis rules. Coordinates switching, pixel accessors, the border pen, click selection and the Delete key come in Task 3; the Python table rows in Task 8.

**Files:**
- Modify: `include/SciQLopPlots/enums.hpp` (after `Coordinates`, ~line 49)
- Create: `include/SciQLopPlots/Items/SciQLopSpan.hpp`, `src/SciQLopSpan.cpp`
- Create: `include/SciQLopPlots/Items/impl/SpanTargets.hpp`, `src/SpanTargets.cpp`
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (forward declaration ~line 53; `span` after `rectangular_span` ~line 558; `remove(SciQLopSpan*)` after line 572), `src/SciQLopPlotInterface.cpp` (after `remove(SciQLopStraightLine*)`, ~line 141)
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp` (after line 561), `src/SciQLopPlot.cpp` (after `rectangular_span`, ~line 1620; include at ~line 38)
- Modify: `src/meson.build` (moc header after line 70, sources after line 169)
- Modify: `SciQLopPlots/bindings/bindings.h` (after line 30), `SciQLopPlots/bindings/bindings.xml`
- Modify, branch F-py only: `SciQLopPlots/__init__.py` (B4)
- Test: `tests/integration/test_span.py` (create), `tests/integration/test_drawn_contract.py` (one row), `tests/integration/conftest.py` (three helpers, shared with Tasks 3-7)

**Interfaces:**
- Consumes: the ledger's branch (B1, B2, B3, B4).
- Produces, tests: `plots_of(host)`, `canvas_of(plot)`, `scoped_host(scope, qtbot)` in `tests/integration/conftest.py`.
- Produces, C++:
  - `enum class SpanKind { Vertical, Horizontal, Rectangular };` (global, enums.hpp), Python `SpanKind`.
  - `namespace impl { enum class SpanAxis { Key, Value }; }` in SciQLopSpan.hpp.
  - `SpanKind SciQLopPlots::span_kind(bool bounds_key, bool bounds_value)`: throws `std::invalid_argument("span: give key_range, value_range or both")` for neither.
  - `class SciQLopSpan : public SciQLopDrawnObject` with the plot constructor `SciQLopSpan(SciQLopPlot* plot, SpanKind kind, const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range, const QColor& color, bool read_only, bool visible, const QString& tool_tip, Coordinates coordinates, const QString& id = "")`. The range of an unbounded axis is ignored. Not constructible from Python.
  - Its methods: `kind()`, `scope()` (the parent: plot or panel), `is_panel_part()`, `id()`, B1's `key_range()` / `value_range()`, `set_key_range`, `set_value_range`, `range()`, `set_range`, `coordinates()` (the getter; the constructor takes the mode), `read_only()`, `set_read_only`, `tool_tip()`, `set_tool_tip`, and the drawn hooks (visible, selected, colour). Signals: `range_changed`, `key_range_changed`, `value_range_changed` (each `SciQLopPlotRange`), `read_only_changed(bool)`, `tool_tip_changed(const QString&)`.
  - Private, for the targets (friends): `void notify_range(impl::SpanAxis, const SciQLopPlotRange&)`.
  - `impl::SpanTarget` (abstract), `impl::PlotSpanTarget`, `impl::bounded_axes(SpanKind)`, in `Items/impl/SpanTargets.hpp`. Task 3 adds members to both.
  - `SciQLopPlotInterface::span(...)` (virtual, NotImplementedError by default; B2 signature), overridden by `SciQLopPlot`. `SciQLopPlotInterface::remove(SciQLopSpan*)`.
- Produces, Python: `plot.span(key_range=None, value_range=None, color=QColor(100, 100, 100, 100), read_only=False, visible=True, tool_tip="", coordinates=Coordinates.Data, id="")`, C++-owned result.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/conftest.py` (it already imports `numpy as np` and `QApplication`):

```python
def plots_of(host):
    """The plots of a plot (itself) or of a panel, in order."""
    from SciQLopPlots import SciQLopPlot
    if isinstance(host, SciQLopPlot):
        return [host]
    return [host.plot_at(i) for i in range(host.size())]


def canvas_of(plot):
    """The QCustomPlot widget of a plot: where mouse and key events go."""
    from PySide6.QtWidgets import QWidget
    return next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))


def scoped_host(scope, qtbot):
    """A plot ("plot") or a two-plot panel ("panel"), x over 0..10 and y over -1..1."""
    from SciQLopPlots import SciQLopMultiPlotPanel, SciQLopPlot, SciQLopPlotRange
    x = np.linspace(0.0, 10.0, 200)
    if scope == "plot":
        host = SciQLopPlot()
        host.plot(x, np.sin(x))
        host.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    else:
        host = SciQLopMultiPlotPanel()
        host.plot(x, np.sin(x))
        host.plot(x, np.cos(x))
        host.set_time_axis_range(SciQLopPlotRange(0.0, 10.0))
    qtbot.addWidget(host)
    QApplication.processEvents()
    for plot in plots_of(host):
        plot.y_axis().set_range(SciQLopPlotRange(-1.0, 1.0))
    return host
```

`tests/integration/test_span.py`. `SCOPES` has only `"plot"` here; Task 4 adds `"panel"`, and every test then runs at both scopes.

```python
"""SciQLopSpan: one span class for every kind, at plot and panel scope (API review 5.6, Plan A)."""
import pytest
import shiboken6
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import Coordinates, SciQLopPlot, SciQLopPlotRange as R, SciQLopSpan, SpanKind
from conftest import canvas_of, off_thread, plots_of, process_events, record, scoped_host

SCOPES = ["plot"]
KINDS = {
    "vertical": (SpanKind.Vertical, dict(key_range=R(2.0, 8.0))),
    "horizontal": (SpanKind.Horizontal, dict(value_range=R(-0.5, 0.5))),
    "rectangular": (SpanKind.Rectangular, dict(key_range=R(2.0, 8.0), value_range=R(-0.5, 0.5))),
}
ONE_AXIS = {"vertical": "key", "horizontal": "value"}
OTHER = {"key": "value", "value": "key"}
GREY = QColor(100, 100, 100, 100)


@pytest.fixture(params=SCOPES)
def host(request, qtbot):
    return scoped_host(request.param, qtbot)


@pytest.fixture
def shown_host(host, qtbot):
    host.resize(800, 600)
    host.show()
    qtbot.waitExposed(host)
    process_events()
    return host


@pytest.fixture(params=list(KINDS))
def kind(request):
    return request.param


def make(host, kind, **kwargs):
    return host.span(**KINDS[kind][1], **kwargs)


def bounded(kind):
    return [axis for axis in ("key", "value") if f"{axis}_range" in KINDS[kind][1]]


def _close(a, b, tol):
    return abs(a[0] - b[0]) <= tol and abs(a[1] - b[1]) <= tol


def test_the_factory_returns_the_plain_class(host, kind):
    span = make(host, kind)
    assert type(span) is SciQLopSpan
    assert span.kind() == KINDS[kind][0]


def test_the_class_has_no_python_constructor(plot, panel):
    """The full C++ argument list, so only a removed constructor can raise here."""
    for host in (plot, panel):
        with pytest.raises(TypeError):
            SciQLopSpan(host, SpanKind.Vertical, R(1.0, 2.0), R(1.0, 2.0), QColor("red"), False,
                        True, "tip", Coordinates.Data, "id")


def test_the_defaults(host, kind):
    span = make(host, kind)
    assert span.color() == GREY
    assert (span.read_only(), span.visible(), span.selected(), span.tool_tip()) == (False, True, False, "")
    assert span.coordinates() == Coordinates.Data
    assert span.id() and span.id() != make(host, kind).id()


def test_the_factory_arguments(host, kind):
    span = make(host, kind, color=QColor("red"), read_only=True, visible=False, tool_tip="tip",
                id="event-1")
    assert (span.color(), span.read_only(), span.visible(), span.tool_tip(), span.id()) == \
        (QColor("red"), True, False, "tip", "event-1")


def test_scope_and_part_flag(host, kind):
    span = make(host, kind)
    assert span.scope() is host
    assert span.is_panel_part() is False


def test_each_bounded_axis_reads_its_range_and_the_other_reads_none(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        assert getattr(span, f"{axis}_range")() == KINDS[kind][1].get(f"{axis}_range")


def test_setting_an_unbounded_axis_raises(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        if axis not in bounded(kind):
            with pytest.raises(ValueError, match=f"{axis} axis"):
                getattr(span, f"set_{axis}_range")(R(0.0, 1.0))


def test_a_bounded_axis_round_trips(host, kind):
    span = make(host, kind)
    for axis in bounded(kind):
        getattr(span, f"set_{axis}_range")(R(0.1, 0.3))
        assert getattr(span, f"{axis}_range")() == R(0.1, 0.3)


@pytest.mark.parametrize("kind", ["vertical", "horizontal"])
def test_range_is_the_bounded_axis(host, kind):
    span = make(host, kind)
    axis = ONE_AXIS[kind]
    assert span.range() == getattr(span, f"{axis}_range")()
    span.set_range(R(0.2, 0.4))
    assert getattr(span, f"{axis}_range")() == R(0.2, 0.4)


def test_range_on_a_rectangular_span_raises(host):
    span = make(host, "rectangular")
    for call in (span.range, lambda: span.set_range(R(0.0, 1.0))):
        with pytest.raises(ValueError, match="rectangular"):
            call()


def test_a_span_without_a_range_raises(host):
    with pytest.raises(ValueError, match="key_range, value_range or both"):
        host.span()
    with pytest.raises(ValueError, match="key_range, value_range or both"):
        host.span(key_range=None, value_range=None)


def test_an_explicit_none_is_an_unbounded_axis(host):
    span = host.span(key_range=None, value_range=R(-0.5, 0.5))
    assert (span.kind(), span.key_range(), span.value_range()) == \
        (SpanKind.Horizontal, None, R(-0.5, 0.5))


def test_a_wrong_range_type_is_a_type_error(host):
    with pytest.raises(TypeError):
        host.span(key_range="2.0..8.0")


@pytest.mark.parametrize("kind", ["vertical", "horizontal"])
def test_one_set_range_emits_once_on_each_signal(host, kind):
    span = make(host, kind)
    axis = ONE_AXIS[kind]
    ranges = record(span.range_changed)
    mine = record(getattr(span, f"{axis}_range_changed"))
    others = record(getattr(span, f"{OTHER[axis]}_range_changed"))
    span.set_range(R(0.2, 0.4))
    span.set_range(R(0.2, 0.4))
    assert (ranges, mine, others) == ([R(0.2, 0.4)], [R(0.2, 0.4)], [])


def test_a_rectangular_span_never_emits_range_changed(host):
    span = make(host, "rectangular")
    ranges, keys, values = record(span.range_changed), record(span.key_range_changed), \
        record(span.value_range_changed)
    span.set_key_range(R(0.2, 0.4))
    span.set_value_range(R(0.1, 0.3))
    assert (ranges, keys, values) == ([], [R(0.2, 0.4)], [R(0.1, 0.3)])


SIGNALLED = {"read_only": (True, "read_only_changed"), "tool_tip": ("tip", "tool_tip_changed")}


@pytest.mark.parametrize("name", SIGNALLED)
def test_signalled_accessors_round_trip_and_emit_once(host, kind, name):
    value, signal = SIGNALLED[name]
    span = make(host, kind)
    seen = record(getattr(span, signal))
    getattr(span, f"set_{name}")(value)
    getattr(span, f"set_{name}")(value)
    assert (getattr(span, name)(), seen) == (value, [value])


def test_remove_deletes_it(host, kind):
    span = make(host, kind)
    host.remove(span)
    assert not shiboken6.isValid(span)


def test_the_factory_raises_off_thread(host):
    out = {}

    def call():
        try:
            host.span(key_range=R(1.0, 2.0))
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


def test_a_set_off_thread_lands_on_the_gui_thread(host, kind):
    span = make(host, kind)
    axis = bounded(kind)[0]
    seen = record(getattr(span, f"{axis}_range_changed"))
    off_thread(lambda: getattr(span, f"set_{axis}_range")(R(0.1, 0.2)))
    assert seen == []
    process_events()
    assert seen == [R(0.1, 0.2)]


def test_an_unbounded_axis_set_off_thread_raises_in_the_caller(host):
    """Review Focus 4: the kind check runs before the call is queued to the GUI thread."""
    span = make(host, "vertical")
    out = {}

    def call():
        try:
            span.set_value_range(R(0.0, 1.0))
        except ValueError as e:
            out["error"] = e
    off_thread(call)
    process_events()
    assert "value axis" in str(out.get("error", ""))


@pytest.mark.parametrize("name", ["read_only", "tool_tip", "range", "key_range", "value_range"])
def test_on_resolves_with_a_signal(host, name):
    assert getattr(make(host, "vertical").on, name).signal is not None


def test_on_range_raises_on_use_for_a_rectangular_span(host):
    prop = make(host, "rectangular").on.range
    with pytest.raises(ValueError, match="rectangular"):
        prop.value
    with pytest.raises(ValueError, match="rectangular"):
        prop.value = R(0.0, 1.0)


def test_on_reads_none_and_refuses_writes_on_an_unbounded_axis(host):
    span = make(host, "vertical")
    assert span.on.value_range.value is None
    with pytest.raises(ValueError, match="value axis"):
        span.on.value_range.value = R(0.0, 1.0)


def test_on_position_is_absent(host, kind):
    span = make(host, kind)
    assert not hasattr(span, "position")
    with pytest.raises(AttributeError):
        span.on.position
```

In `tests/integration/test_drawn_contract.py`, after the `Row("span", ...)` entry (~line 222), add:

```python
    Row("new span", ("SciQLopSpan",), {
        "SciQLopSpan": lambda h: h.plot.span(key_range=R(1.0, 2.0)),
    }, ALL),
```

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_span.py tests/integration/test_drawn_contract.py`.
Expected: test_span.py fails at import (`cannot import name 'SciQLopSpan'`); test_drawn_contract.py fails the same way at collection of the new row's `h.plot.span`, or with `AttributeError: ... has no attribute 'span'`.

- [ ] **Step 3: `SpanKind`**

In `include/SciQLopPlots/enums.hpp`, after `Q_DECLARE_METATYPE(Coordinates);`:

```cpp
//! The axes a span bounds, fixed when it is made: the key axis (x), the value axis (y), or both.
enum class SpanKind
{
    Vertical,
    Horizontal,
    Rectangular
};
Q_DECLARE_METATYPE(SpanKind);
```

- [ ] **Step 4: `include/SciQLopPlots/Items/SciQLopSpan.hpp`**

Start with the GPL header of `include/SciQLopPlots/SciQLopDrawnObject.hpp` (Copyright 2026). Then:

```cpp
#pragma once

#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include "SciQLopPlots/enums.hpp"

#include <QColor>
#include <QString>
#include <memory>
#include <optional>

class SciQLopPlot;
class SciQLopMultiPlotPanel;

namespace impl
{
//! The axis a span range lies on: the key axis (x) or the value axis (y).
enum class SpanAxis
{
    Key,
    Value
};
class SpanTarget;
class PlotSpanTarget;
class PanelSpanTarget;
}

namespace SciQLopPlots
{
//! The kind of a span bounding the key axis, the value axis or both; std::invalid_argument for
//! neither.
SpanKind span_kind(bool bounds_key, bool bounds_value);
}

/*!
 * \brief A band over the key axis (Vertical), the value axis (Horizontal) or both (Rectangular),
 * on one plot or on every plot of a panel.
 *
 * Kind and scope are fixed at creation; plot.span(...), panel.span(...) and their shorthands make
 * them (docs/plans/2026-10-25-span-line-unification-design.md). Inside, a target holds the state:
 * one NeoQCP item on a plot, or one plot-scope part per plot on a panel.
 */
class SciQLopSpan : public SciQLopDrawnObject
{
    Q_OBJECT
    friend class impl::PlotSpanTarget;
    friend class impl::PanelSpanTarget;

    SpanKind m_kind;
    QString m_id;
    std::unique_ptr<impl::SpanTarget> m_target;

    [[nodiscard]] bool bounds(impl::SpanAxis axis) const noexcept;
    void require_bounded(impl::SpanAxis axis, const char* what) const;
    [[nodiscard]] impl::SpanAxis one_axis(const char* what) const;
    //! The axis's signal, then range_changed on a one-axis span: one change, one of each.
    void notify_range(impl::SpanAxis axis, const SciQLopPlotRange& range);

protected:
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;
    QColor _color() const override;
    void _set_color(const QColor& color) override;

public:
    SciQLopSpan(SciQLopPlot* plot, SpanKind kind, const SciQLopPlotRange& key_range,
                const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                bool visible, const QString& tool_tip, Coordinates coordinates,
                const QString& id = "");
    ~SciQLopSpan() override;

    [[nodiscard]] SpanKind kind() const noexcept;
    //! The plot or the panel the span belongs to.
    [[nodiscard]] QObject* scope() const;
    //! True for the per-plot parts of a panel-scope span.
    [[nodiscard]] bool is_panel_part() const;
    [[nodiscard]] QString id() const;

    // B1 (the ledger's branch): key_range() and value_range().

    void set_key_range(const SciQLopPlotRange& range);
    void set_value_range(const SciQLopPlotRange& range);
    //! The bounded axis of a vertical or horizontal span; std::invalid_argument on a rectangular one.
    [[nodiscard]] SciQLopPlotRange range() const;
    void set_range(const SciQLopPlotRange& range);

    //! Data, or Pixels from the plot area's top-left, as made.
    [[nodiscard]] Coordinates coordinates() const;

    [[nodiscard]] bool read_only() const;
    void set_read_only(bool read_only);
    [[nodiscard]] QString tool_tip() const;
    void set_tool_tip(const QString& tool_tip);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void range_changed(SciQLopPlotRange range);
    Q_SIGNAL void key_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void value_range_changed(SciQLopPlotRange range);
    Q_SIGNAL void read_only_changed(bool read_only);
    Q_SIGNAL void tool_tip_changed(const QString& tool_tip);
};
```

Replace the `// B1` comment with B1's header lines for the ledger's branch.

- [ ] **Step 5: `include/SciQLopPlots/Items/impl/SpanTargets.hpp`**

Internal: not included by bindings.h, so shiboken never sees it. Same GPL header. Then:

```cpp
#pragma once

#include "SciQLopPlots/Items/SciQLopSpan.hpp"
#include "SciQLopPlots/Items/impl/SpanBase.hpp"

#include <QPointer>
#include <qcustomplot.h>
#include <vector>

namespace impl
{
//! The axes a span of \a kind bounds, key first.
inline std::vector<SpanAxis> bounded_axes(SpanKind kind)
{
    switch (kind)
    {
        case SpanKind::Vertical:
            return { SpanAxis::Key };
        case SpanKind::Horizontal:
            return { SpanAxis::Value };
        case SpanKind::Rectangular:
            return { SpanAxis::Key, SpanAxis::Value };
    }
    return {};
}

//! What a SciQLopSpan reads and writes: one plot's item, or a panel's parts.
class SpanTarget
{
public:
    virtual ~SpanTarget() = default;

    virtual SciQLopPlotRange range(SpanAxis axis) const = 0;
    //! Applies \a range, and the span notifies once; nothing for the current range.
    virtual void set_range(SpanAxis axis, const SciQLopPlotRange& range) = 0;
    virtual Coordinates coordinates() const = 0;
    virtual bool read_only() const = 0;
    virtual void set_read_only(bool read_only) = 0;
    virtual QString tool_tip() const = 0;
    virtual void set_tool_tip(const QString& tool_tip) = 0;
    virtual QColor color() const = 0;
    virtual void set_color(const QColor& color) = 0;
    virtual bool visible() const = 0;
    virtual void set_visible(bool visible) = 0;
    virtual bool selected() const = 0;
    virtual void set_selected(bool selected) = 0;
};

//! A span on one plot: one NeoQCP span item, which holds the state.
class PlotSpanTarget final : public SpanTarget
{
    SpanKind m_kind;
    QPointer<QCPAbstractSpanItem> m_item;
    SpanBase* m_base; // m_item seen as a SpanBase: use it only while m_item is alive
    Coordinates m_coordinates;

public:
    PlotSpanTarget(SciQLopSpan* span, SciQLopPlot* plot, SpanKind kind,
                   const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range,
                   Coordinates coordinates);
    ~PlotSpanTarget() override;

    SciQLopPlotRange range(SpanAxis axis) const override;
    void set_range(SpanAxis axis, const SciQLopPlotRange& range) override;
    Coordinates coordinates() const override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
};
} // namespace impl
```

- [ ] **Step 6: `src/SpanTargets.cpp`**

Same GPL header. Note: `SciQLopPlotRange(double, double)` sorts its bounds, so every range built here is sorted.

```cpp
#include "SciQLopPlots/Items/impl/SpanTargets.hpp"

#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/constants.hpp"
#include "SciQLopPlots/helpers.hpp"

#include <QSignalBlocker>

namespace impl
{
namespace
{
//! A NeoQCP span that SciQLopPlot's tooltip lookup finds: SpanBase is a SciQlopItemWithToolTip.
template <typename QCPSpan>
class SpanItem final : public QCPSpan, public SpanBase
{
public:
    explicit SpanItem(QCustomPlot* plot) : QCPSpan(plot), SpanBase(this)
    {
        this->setLayer(Constants::LayersNames::Spans);
        this->setSelectable(true);
        this->setBorderPen(QPen(QBrush(Qt::black), 3));
    }
};

QCPAbstractSpanItem* make_item(QCustomPlot* plot, SpanKind kind)
{
    switch (kind)
    {
        case SpanKind::Vertical:
            return new SpanItem<QCPItemVSpan>(plot);
        case SpanKind::Horizontal:
            return new SpanItem<QCPItemHSpan>(plot);
        case SpanKind::Rectangular:
            return new SpanItem<QCPItemRSpan>(plot);
    }
    return nullptr;
}

//! The two edges that bound one axis of a span, and the pixel direction they move along.
struct Edges
{
    QCPItemPosition* lower;
    QCPItemPosition* upper;
    Qt::Orientation along;
};

Edges edges_of(QCPAbstractSpanItem* item, SpanAxis axis)
{
    if (auto* v = qobject_cast<QCPItemVSpan*>(item))
        return { v->lowerEdge, v->upperEdge, Qt::Horizontal };
    if (auto* h = qobject_cast<QCPItemHSpan*>(item))
        return { h->lowerEdge, h->upperEdge, Qt::Vertical };
    auto* r = qobject_cast<QCPItemRSpan*>(item);
    return axis == SpanAxis::Key ? Edges { r->leftEdge, r->rightEdge, Qt::Horizontal }
                                 : Edges { r->bottomEdge, r->topEdge, Qt::Vertical };
}

double along(const QPointF& point, Qt::Orientation orientation)
{
    return orientation == Qt::Horizontal ? point.x() : point.y();
}

// Pixels are measured from the plot area, not the widget: ptAbsolute's origin sits inside the
// axis margins, where clipToAxisRect() hides it.
QCPItemPosition::PositionType position_type(Coordinates coordinates)
{
    return coordinates == Coordinates::Pixels ? QCPItemPosition::ptAxisRectAbsolute
                                              : QCPItemPosition::ptPlotCoords;
}

void set_type(const Edges& edges, QCPItemPosition::PositionType type)
{
    for (auto* edge : { edges.lower, edges.upper })
    {
        if (edges.along == Qt::Horizontal)
            edge->setTypeX(type);
        else
            edge->setTypeY(type);
    }
}

SciQLopPlotRange coords_of(const Edges& edges)
{
    return SciQLopPlotRange(along(edges.lower->coords(), edges.along),
                            along(edges.upper->coords(), edges.along));
}

//! Through NeoQCP's setters: they mark the GPU layer dirty, queue a replot and emit last.
void apply_range(QCPAbstractSpanItem* item, SpanAxis axis, const SciQLopPlotRange& range)
{
    const QCPRange r(range.first, range.second);
    if (auto* v = qobject_cast<QCPItemVSpan*>(item))
        v->setRange(r);
    else if (auto* h = qobject_cast<QCPItemHSpan*>(item))
        h->setRange(r);
    else if (auto* rect = qobject_cast<QCPItemRSpan*>(item))
    {
        if (axis == SpanAxis::Key)
            rect->setKeyRange(r);
        else
            rect->setValueRange(r);
    }
}
} // namespace

PlotSpanTarget::PlotSpanTarget(SciQLopSpan* span, SciQLopPlot* plot, SpanKind kind,
                               const SciQLopPlotRange& key_range,
                               const SciQLopPlotRange& value_range, Coordinates coordinates)
        : m_kind { kind }
        , m_item { make_item(plot->qcp_plot(), kind) }
        , m_base { dynamic_cast<SpanBase*>(m_item.data()) }
        , m_coordinates { coordinates }
{
    {
        const QSignalBlocker blocker(m_item.data());
        for (const auto axis : bounded_axes(kind))
        {
            // The type before the coordinates: setType keeps the pixel position it had.
            set_type(edges_of(m_item.data(), axis), position_type(coordinates));
            apply_range(m_item.data(), axis, axis == SpanAxis::Key ? key_range : value_range);
        }
    }
    // NeoQCP emits these for a drag and for our own setters alike: the one path to notify_range.
    const auto notify = [span](SpanAxis axis)
    {
        return [span, axis](const QCPRange& r)
        { span->notify_range(axis, SciQLopPlotRange(r.lower, r.upper)); };
    };
    if (auto* v = qobject_cast<QCPItemVSpan*>(m_item.data()))
        QObject::connect(v, &QCPItemVSpan::rangeChanged, span, notify(SpanAxis::Key));
    else if (auto* h = qobject_cast<QCPItemHSpan*>(m_item.data()))
        QObject::connect(h, &QCPItemHSpan::rangeChanged, span, notify(SpanAxis::Value));
    else if (auto* r = qobject_cast<QCPItemRSpan*>(m_item.data()))
    {
        QObject::connect(r, &QCPItemRSpan::keyRangeChanged, span, notify(SpanAxis::Key));
        QObject::connect(r, &QCPItemRSpan::valueRangeChanged, span, notify(SpanAxis::Value));
    }
}

PlotSpanTarget::~PlotSpanTarget()
{
    qcp_item_remove(m_item);
}

SciQLopPlotRange PlotSpanTarget::range(SpanAxis axis) const
{
    return m_item ? coords_of(edges_of(m_item.data(), axis)) : SciQLopPlotRange();
}

void PlotSpanTarget::set_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    if (m_item && this->range(axis) != range)
        apply_range(m_item.data(), axis, range);
}

Coordinates PlotSpanTarget::coordinates() const
{
    return m_coordinates;
}

bool PlotSpanTarget::read_only() const
{
    return !m_item || !m_item->movable();
}

void PlotSpanTarget::set_read_only(bool read_only)
{
    if (m_item)
        m_item->setMovable(!read_only);
}

QString PlotSpanTarget::tool_tip() const
{
    return m_item ? m_base->tooltip() : QString();
}

void PlotSpanTarget::set_tool_tip(const QString& tool_tip)
{
    if (m_item)
        m_base->setToolTip(tool_tip);
}

QColor PlotSpanTarget::color() const
{
    return m_item ? m_base->color() : QColor();
}

void PlotSpanTarget::set_color(const QColor& color)
{
    if (!m_item)
        return;
    m_base->set_color(color);
    m_base->replot();
}

bool PlotSpanTarget::visible() const
{
    return m_item && m_item->visible();
}

void PlotSpanTarget::set_visible(bool visible)
{
    if (!m_item)
        return;
    m_item->setVisible(visible);
    m_base->replot();
}

bool PlotSpanTarget::selected() const
{
    return m_item && m_item->selected();
}

void PlotSpanTarget::set_selected(bool selected)
{
    if (!m_item)
        return;
    m_item->setSelected(selected);
    m_base->replot();
}
} // namespace impl
```

The lambdas reach `notify_range` and `_notify_selected` because `PlotSpanTarget` is a friend of `SciQLopSpan`: a lambda in a member function has that function's access.

- [ ] **Step 7: `src/SciQLopSpan.cpp`**

Same GPL header.

```cpp
#include "SciQLopPlots/Items/SciQLopSpan.hpp"

#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"
#include "SciQLopPlots/Items/impl/SpanTargets.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QPointer>
#include <QUuid>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using impl::SpanAxis;

namespace
{
const char* axis_name(SpanAxis axis)
{
    return axis == SpanAxis::Key ? "key" : "value";
}

QString id_or_uuid(const QString& id)
{
    return id.isEmpty() ? QUuid::createUuid().toString() : id;
}
} // namespace

SpanKind SciQLopPlots::span_kind(bool bounds_key, bool bounds_value)
{
    if (bounds_key && bounds_value)
        return SpanKind::Rectangular;
    if (bounds_key)
        return SpanKind::Vertical;
    if (bounds_value)
        return SpanKind::Horizontal;
    throw std::invalid_argument("span: give key_range, value_range or both");
}

SciQLopSpan::SciQLopSpan(SciQLopPlot* plot, SpanKind kind, const SciQLopPlotRange& key_range,
                         const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                         bool visible, const QString& tool_tip, Coordinates coordinates,
                         const QString& id)
        : SciQLopDrawnObject { SciQLopPlots::item_plot(plot, "SciQLopSpan") }
        , m_kind { kind }
        , m_id { id_or_uuid(id) }
        , m_target { std::make_unique<impl::PlotSpanTarget>(this, plot, kind, key_range,
                                                            value_range, coordinates) }
{
    m_target->set_color(color);
    m_target->set_read_only(read_only);
    m_target->set_visible(visible);
    m_target->set_tool_tip(tool_tip);
}

SciQLopSpan::~SciQLopSpan() = default;

bool SciQLopSpan::bounds(SpanAxis axis) const noexcept
{
    return m_kind == SpanKind::Rectangular
        || (m_kind == SpanKind::Vertical) == (axis == SpanAxis::Key);
}

void SciQLopSpan::require_bounded(SpanAxis axis, const char* what) const
{
    if (!bounds(axis))
        throw std::invalid_argument(std::string(what) + ": this span does not bound the "
                                    + axis_name(axis) + " axis");
}

SpanAxis SciQLopSpan::one_axis(const char* what) const
{
    if (m_kind == SpanKind::Rectangular)
        throw std::invalid_argument(std::string(what)
                                    + ": a rectangular span has two ranges; use key_range() "
                                      "and value_range()");
    return m_kind == SpanKind::Vertical ? SpanAxis::Key : SpanAxis::Value;
}

void SciQLopSpan::notify_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    const QPointer<SciQLopSpan> alive(this);
    if (axis == SpanAxis::Key)
        Q_EMIT key_range_changed(range);
    else
        Q_EMIT value_range_changed(range);
    if (alive && m_kind != SpanKind::Rectangular)
        Q_EMIT range_changed(range);
}

SpanKind SciQLopSpan::kind() const noexcept
{
    return m_kind;
}

QObject* SciQLopSpan::scope() const
{
    return parent();
}

bool SciQLopSpan::is_panel_part() const
{
    return SciQLopPlots::is_panel_part(this);
}

QString SciQLopSpan::id() const
{
    return m_id;
}

// B1 (the ledger's branch): SciQLopSpan::key_range() and SciQLopSpan::value_range().

// The kind is checked before the hop: a throw from the queued call would escape the event loop.
void SciQLopSpan::set_key_range(const SciQLopPlotRange& range)
{
    require_bounded(SpanAxis::Key, "set_key_range");
    SCIQLOP_ON_OWNER_THREAD(set_key_range(range));
    m_target->set_range(SpanAxis::Key, range.sorted());
}

void SciQLopSpan::set_value_range(const SciQLopPlotRange& range)
{
    require_bounded(SpanAxis::Value, "set_value_range");
    SCIQLOP_ON_OWNER_THREAD(set_value_range(range));
    m_target->set_range(SpanAxis::Value, range.sorted());
}

SciQLopPlotRange SciQLopSpan::range() const
{
    return m_target->range(one_axis("range"));
}

void SciQLopSpan::set_range(const SciQLopPlotRange& range)
{
    const auto axis = one_axis("set_range");
    SCIQLOP_ON_OWNER_THREAD(set_range(range));
    m_target->set_range(axis, range.sorted());
}

Coordinates SciQLopSpan::coordinates() const
{
    return m_target->coordinates();
}

bool SciQLopSpan::read_only() const
{
    return m_target->read_only();
}

void SciQLopSpan::set_read_only(bool read_only)
{
    SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
    if (m_target->read_only() == read_only)
        return;
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_read_only(read_only);
    if (alive)
        Q_EMIT read_only_changed(read_only);
}

QString SciQLopSpan::tool_tip() const
{
    return m_target->tool_tip();
}

void SciQLopSpan::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    if (m_target->tool_tip() == tool_tip)
        return;
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_tool_tip(tool_tip);
    if (alive)
        Q_EMIT tool_tip_changed(tool_tip);
}

bool SciQLopSpan::_visible() const
{
    return m_target->visible();
}

void SciQLopSpan::_set_visible(bool visible)
{
    m_target->set_visible(visible);
}

bool SciQLopSpan::_selected() const
{
    return m_target->selected();
}

void SciQLopSpan::_set_selected(bool selected)
{
    m_target->set_selected(selected);
}

QColor SciQLopSpan::_color() const
{
    return m_target->color();
}

void SciQLopSpan::_set_color(const QColor& color)
{
    m_target->set_color(color);
}
```

Replace the `// B1` comment with B1's definitions for the ledger's branch.

- [ ] **Step 8: The plot factory and `remove`**

`include/SciQLopPlots/SciQLopPlotInterface.hpp`:
- After `class SciQLopStraightLine;` (line 52): `class SciQLopSpan;`. Branch O also needs `#include <optional>` next to `<memory>`.
- After the `rectangular_span` virtual (ends ~line 558):

  ```cpp
    //! A span over the key axis, the value axis or both: an axis given None is unbounded.
    //! Raises ValueError when both are None.
    inline virtual SciQLopSpan* span(/* B2 parameters, with defaults */
                                     QColor color = QColor::fromRgb(100, 100, 100, 100),
                                     bool read_only = false, bool visible = true,
                                     const QString& tool_tip = "",
                                     Coordinates coordinates = Coordinates::Data,
                                     const QString& id = "")
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }
  ```
  Replace the comment with B2's two parameter lines for the ledger's branch.
- After `void remove(SciQLopStraightLine* line);` (line 572): `void remove(SciQLopSpan* span);`.

`src/SciQLopPlotInterface.cpp`: include `"SciQLopPlots/Items/SciQLopSpan.hpp"`, and after `remove(SciQLopStraightLine*)`:

```cpp
void SciQLopPlotInterface::remove(SciQLopSpan* span)
{
    check_removable(span);
    SCIQLOP_ON_OWNER_THREAD_WITH(span, remove(span));
    delete span;
}
```

`include/SciQLopPlots/SciQLopPlot.hpp`, after line 561:

```cpp
    SciQLopSpan* span(/* B2 parameters, with defaults */
                      QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                      bool visible = true, const QString& tool_tip = "",
                      Coordinates coordinates = Coordinates::Data,
                      const QString& id = "") override;
```

`src/SciQLopPlot.cpp`: `#include "SciQLopPlots/Items/SciQLopSpan.hpp"` after line 38, and after `rectangular_span` (~line 1620):

```cpp
SciQLopSpan* SciQLopPlot::span(/* B2 parameters, without defaults */ QColor color,
                               bool read_only, bool visible, const QString& tool_tip,
                               Coordinates coordinates, const QString& id)
{
    return new SciQLopSpan(this, /* B2 kind and ranges */ color, read_only, visible, tool_tip,
                           coordinates, id);
}
```
Replace both comments with B2's lines for the ledger's branch (drop the `= ...` defaults in the parameters here).

- [ ] **Step 9: Build files and bindings**

`src/meson.build`: after `'../include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp',` (line 70) add `'../include/SciQLopPlots/Items/SciQLopSpan.hpp',`. After `'SciQLopRectangularSpan.cpp',` (line 169) add `'SciQLopSpan.cpp',` and `'SpanTargets.cpp',`.

`SciQLopPlots/bindings/bindings.h`: after `#include <SciQLopPlots/Items/SciQLopRectangularSpan.hpp>` (line 30): `#include <SciQLopPlots/Items/SciQLopSpan.hpp>`.

`SciQLopPlots/bindings/bindings.xml`:
- After `<enum-type name="Coordinates" python-type="IntEnum"/>` (line 139): `<enum-type name="SpanKind" python-type="IntEnum"/>`.
- Branch O: the ledger's container-type, after the smart-pointer line (~168). Branch F: B4's template.
- Inside `<object-type name="SciQLopPlotInterface" ...>`, after the `rectangular_span` modify-function (~line 552):

  ```xml
        <modify-function signature="span(... B2 signature string ...)">
            <!-- branch F only: B4's two modify-argument elements here -->
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="remove(SciQLopSpan*)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_valueerror"/>
            </inject-code>
        </modify-function>
  ```
- After `<object-type name="SciQLopRectangularSpan" parent-management="yes"/>` (line 1313):

  ```xml
    <object-type name="SciQLopSpan" parent-management="yes">
        <!-- Made by the factories only (plot.span, panel.span and their shorthands). -->
        <modify-function signature="SciQLopSpan(SciQLopPlot*,SpanKind,const SciQLopPlotRange&amp;,const SciQLopPlotRange&amp;,const QColor&amp;,bool,bool,const QString&amp;,Coordinates,const QString&amp;)" remove="all"/>
        <modify-function signature="set_key_range(const SciQLopPlotRange&amp;)">
            <inject-code class="target" position="beginning"><insert-template name="void_raises_valueerror"/></inject-code>
        </modify-function>
        <modify-function signature="set_value_range(const SciQLopPlotRange&amp;)">
            <inject-code class="target" position="beginning"><insert-template name="void_raises_valueerror"/></inject-code>
        </modify-function>
        <modify-function signature="set_range(const SciQLopPlotRange&amp;)">
            <inject-code class="target" position="beginning"><insert-template name="void_raises_valueerror"/></inject-code>
        </modify-function>
        <modify-function signature="range()const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
            </inject-code>
        </modify-function>
        <!-- B3: branches F and F-py add the key_range / value_range modify-functions here. -->
    </object-type>
  ```

If shiboken warns that the `remove="all"` signature was not found, read the exact spelling it lists in `build-venv/SciQLopPlots/bindings/mjb_shiboken.log` and copy it. `test_the_class_has_no_python_constructor` is the proof.

- [ ] **Step 10: Python, by branch**

`SciQLopPlots/__init__.py` (the `.on` and alias rows come in Task 8):
- Branch F-py: B4's block, with the loop over `(SciQLopPlotsBindings.SciQLopPlotInterface, SciQLopPlot)`.
- Branch O: the ledger's `mapping.type_map` entries, if any, in `_register_with_shiboken_signatures` (line 60).

- [ ] **Step 11: Reconfigure, build, run**

Reconfigure (new object-type), then compile: two commands. Then run `tests/integration/test_span.py tests/integration/test_drawn_contract.py tests/integration/test_binding_signatures.py`. Expected: all pass. Then the full suite.

- [ ] **Step 12: Commit**

```bash
git add include/SciQLopPlots/enums.hpp include/SciQLopPlots/Items/SciQLopSpan.hpp include/SciQLopPlots/Items/impl/SpanTargets.hpp src/SciQLopSpan.cpp src/SpanTargets.cpp include/SciQLopPlots/SciQLopPlotInterface.hpp src/SciQLopPlotInterface.cpp include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp src/meson.build SciQLopPlots/bindings/bindings.h SciQLopPlots/bindings/bindings.xml tests/integration/conftest.py tests/integration/test_span.py tests/integration/test_drawn_contract.py
git commit -m "feat(span): SciQLopSpan core, one span class for every kind, and plot.span()"
```

Branch F-py also stages `SciQLopPlots/__init__.py`.

---

### Task 3: Span coordinates, pixel accessors, the border pen, click selection, the Delete key

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopSpan.hpp`, `src/SciQLopSpan.cpp`
- Modify: `include/SciQLopPlots/Items/impl/SpanTargets.hpp`, `src/SpanTargets.cpp`
- Modify: `SciQLopPlots/bindings/bindings.xml` (inside `<object-type name="SciQLopSpan">`)
- Test: `tests/integration/test_span.py` (append)

**Interfaces:**
- Consumes: Task 2 (`SciQLopSpan`, `SpanTarget`, `PlotSpanTarget`, the `Edges` helpers in `src/SpanTargets.cpp`).
- Produces, on `SciQLopSpan`: `set_coordinates(Coordinates)`, `pixel_range()`, `pixel_key_range()`, `pixel_value_range()`, `borders_color()` / `set_borders_color`, `line_width()` / `set_line_width`, `line_style()` / `set_line_style`; signals `coordinates_changed(Coordinates)` and `delete_requested()`. The constructor sets the borders to `color.lighter()`. NeoQCP's click selection reaches `selection_changed`.
- Produces, on `SpanTarget` (pure virtual) and `PlotSpanTarget`: `pixel_range(SpanAxis)`, `set_coordinates(Coordinates)`, `borders_color`, `set_borders_color`, `line_width`, `set_line_width`, `line_style`, `set_line_style`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_span.py`:

```python
def test_the_borders_default_to_the_lighter_colour(host, kind):
    assert make(host, kind).borders_color() == GREY.lighter()


def test_pixel_accessors_follow_the_bounded_axes(host, kind):
    span = make(host, kind)
    for axis in ("key", "value"):
        if axis not in bounded(kind):
            with pytest.raises(ValueError, match=f"{axis} axis"):
                getattr(span, f"pixel_{axis}_range")()
    if kind == "rectangular":
        with pytest.raises(ValueError, match="rectangular"):
            span.pixel_range()


def test_on_coordinates_has_a_signal(host):
    assert make(host, "vertical").on.coordinates.signal is not None


def test_clicking_a_span_selects_it_and_signals_once(shown_host):
    span = make(shown_host, "vertical")
    seen = record(span.selection_changed)
    canvas = canvas_of(plots_of(shown_host)[0])
    QTest.mouseClick(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier,
                     QPoint(canvas.width() // 2, canvas.height() // 2))
    process_events()
    assert (span.selected(), seen) == (True, [True])


STYLE = {"borders_color": QColor("red"), "line_width": 5.0, "line_style": Qt.PenStyle.DashLine}


@pytest.mark.parametrize("name", STYLE)
def test_the_border_style_round_trips(host, kind, name):
    span = make(host, kind)
    getattr(span, f"set_{name}")(STYLE[name])
    assert getattr(span, name)() == STYLE[name]


def test_switching_coordinates_keeps_the_span_in_place(shown_host, kind):
    span = make(shown_host, kind)
    process_events()
    data = {axis: getattr(span, f"{axis}_range")() for axis in bounded(kind)}
    pixels = {axis: getattr(span, f"pixel_{axis}_range")() for axis in bounded(kind)}
    seen = record(span.coordinates_changed)
    span.set_coordinates(Coordinates.Pixels)
    process_events()
    assert seen == [Coordinates.Pixels]
    for axis in bounded(kind):
        assert _close(getattr(span, f"{axis}_range")(), pixels[axis], 1.0)
        assert _close(getattr(span, f"pixel_{axis}_range")(), pixels[axis], 1.0)
    span.set_coordinates(Coordinates.Data)
    process_events()
    for axis in bounded(kind):
        assert _close(getattr(span, f"{axis}_range")(), data[axis], 0.05)


def test_switching_coordinates_signals_the_new_numbers(shown_host):
    span = make(shown_host, "vertical")
    process_events()
    pixels = span.pixel_range()
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    span.set_coordinates(Coordinates.Pixels)
    assert len(ranges) == 1 and len(keys) == 1 and _close(ranges[0], pixels, 1.0)


def test_a_pixels_span_stays_put_on_zoom(shown_host, kind):
    span = make(shown_host, kind)
    process_events()
    span.set_coordinates(Coordinates.Pixels)
    before = {axis: getattr(span, f"pixel_{axis}_range")() for axis in bounded(kind)}
    for plot in plots_of(shown_host):
        plot.y_axis().set_range(R(-5.0, 5.0))
    if isinstance(shown_host, SciQLopPlot):
        shown_host.x_axis().set_range(R(0.0, 100.0))
    else:
        shown_host.set_time_axis_range(R(0.0, 100.0))
    process_events()
    for axis in bounded(kind):
        assert _close(getattr(span, f"pixel_{axis}_range")(), before[axis], 1.0)


def test_the_delete_key_asks_once(shown_host, kind):
    span = make(shown_host, kind)
    asked = []
    span.delete_requested.connect(lambda: asked.append(True))
    span.set_selected(True)
    process_events()
    QTest.keyClick(canvas_of(plots_of(shown_host)[0]), Qt.Key.Key_Delete)
    process_events()
    assert asked == [True]
```

Add `QPoint` to the `PySide6.QtCore` import of the file.

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_span.py`. Expected: `AttributeError` for `borders_color`, `pixel_key_range`, `set_coordinates`, `delete_requested`; `coordinates_changed` is missing so `.on.coordinates.signal` is None; the click selects the item but `selection_changed` never fires.

- [ ] **Step 3: The declarations**

`include/SciQLopPlots/Items/SciQLopSpan.hpp`: replace the `coordinates()` getter's two lines with

```cpp
    [[nodiscard]] Coordinates coordinates() const;
    //! Data, or Pixels from the plot area's top-left, for every bounded axis; the span stays put.
    void set_coordinates(Coordinates coordinates);
    [[nodiscard]] SciQLopPlotRange pixel_range() const;
    [[nodiscard]] SciQLopPlotRange pixel_key_range() const;
    [[nodiscard]] SciQLopPlotRange pixel_value_range() const;
```

after `void set_tool_tip(const QString& tool_tip);` add

```cpp

    //! The border pen: colour, width and style.
    [[nodiscard]] QColor borders_color() const;
    void set_borders_color(const QColor& color);
    [[nodiscard]] double line_width() const;
    void set_line_width(double width);
    [[nodiscard]] Qt::PenStyle line_style() const;
    void set_line_style(Qt::PenStyle style);
```

and in the signals: `Q_SIGNAL void coordinates_changed(Coordinates coordinates);` after `value_range_changed`, and `Q_SIGNAL void delete_requested();` last.

`include/SciQLopPlots/Items/impl/SpanTargets.hpp`, in `SpanTarget`:

```cpp
    virtual SciQLopPlotRange pixel_range(SpanAxis axis) const = 0;
    //! Re-expresses every bounded axis in \a coordinates, keeping the span in place. Silent: the
    //! span reports what changed.
    virtual void set_coordinates(Coordinates coordinates) = 0;
    virtual QColor borders_color() const = 0;
    virtual void set_borders_color(const QColor& color) = 0;
    virtual double line_width() const = 0;
    virtual void set_line_width(double width) = 0;
    virtual Qt::PenStyle line_style() const = 0;
    virtual void set_line_style(Qt::PenStyle style) = 0;
```

and the matching `override` declarations in `PlotSpanTarget`.

- [ ] **Step 4: `src/SpanTargets.cpp`**

In the anonymous namespace, after `set_type`:

```cpp
double area_origin(const Edges& edges)
{
    const auto* rect = edges.lower->axisRect();
    return rect ? along(rect->topLeft(), edges.along) : 0.;
}
```

after `coords_of`:

```cpp
// The resolved pixel positions are right in both modes; coordToPixel() would read pixel
// coordinates as data values.
SciQLopPlotRange pixels_of(const Edges& edges)
{
    const double origin = area_origin(edges);
    return SciQLopPlotRange(along(edges.lower->pixelPosition(), edges.along) - origin,
                            along(edges.upper->pixelPosition(), edges.along) - origin);
}

//! \a pixels (from the plot area) expressed in \a coordinates, through the edges' axis.
SciQLopPlotRange from_pixels(const Edges& edges, Coordinates coordinates,
                             const SciQLopPlotRange& pixels)
{
    if (coordinates == Coordinates::Pixels)
        return pixels;
    auto* axis = edges.along == Qt::Horizontal ? edges.lower->keyAxis() : edges.lower->valueAxis();
    if (!axis)
        return coords_of(edges);
    const double origin = area_origin(edges);
    return SciQLopPlotRange(axis->pixelToCoord(pixels.first + origin),
                            axis->pixelToCoord(pixels.second + origin));
}
```

At the end of `PlotSpanTarget`'s constructor:

```cpp
    QObject::connect(m_item.data(), &QCPAbstractItem::selectionChanged, span,
                     [span](bool selected) { span->_notify_selected(selected); });
    QObject::connect(m_item.data(), &QCPAbstractItem::deleteRequested, span,
                     &SciQLopSpan::delete_requested);
```

The lambda reaches `_notify_selected` because `PlotSpanTarget` is a friend of `SciQLopSpan`. And the new members:

```cpp
SciQLopPlotRange PlotSpanTarget::pixel_range(SpanAxis axis) const
{
    return m_item ? pixels_of(edges_of(m_item.data(), axis)) : SciQLopPlotRange();
}

void PlotSpanTarget::set_coordinates(Coordinates coordinates)
{
    if (!m_item || coordinates == m_coordinates)
        return;
    const QSignalBlocker blocker(m_item.data());
    for (const auto axis : bounded_axes(m_kind))
    {
        const auto edges = edges_of(m_item.data(), axis);
        const auto pixels = pixels_of(edges);
        set_type(edges, position_type(coordinates));
        apply_range(m_item.data(), axis, from_pixels(edges, coordinates, pixels));
    }
    m_coordinates = coordinates;
}

QColor PlotSpanTarget::borders_color() const
{
    return m_item ? m_item->borderPen().color() : QColor();
}

void PlotSpanTarget::set_borders_color(const QColor& color)
{
    if (!m_item)
        return;
    auto pen = m_item->borderPen();
    pen.setColor(color);
    m_item->setBorderPen(pen);
    m_base->replot();
}

double PlotSpanTarget::line_width() const
{
    return m_item ? m_base->line_width() : 0.;
}

void PlotSpanTarget::set_line_width(double width)
{
    if (!m_item)
        return;
    m_base->set_line_width(width);
    m_base->replot();
}

Qt::PenStyle PlotSpanTarget::line_style() const
{
    return m_item ? m_base->line_style() : Qt::SolidLine;
}

void PlotSpanTarget::set_line_style(Qt::PenStyle style)
{
    if (!m_item)
        return;
    m_base->set_line_style(style);
    m_base->replot();
}
```

- [ ] **Step 5: `src/SciQLopSpan.cpp`**

In the plot constructor, after `m_target->set_color(color);`: `m_target->set_borders_color(color.lighter());`. Then:

```cpp
void SciQLopSpan::set_coordinates(Coordinates coordinates)
{
    SCIQLOP_ON_OWNER_THREAD(set_coordinates(coordinates));
    if (m_target->coordinates() == coordinates)
        return;
    std::vector<std::pair<SpanAxis, SciQLopPlotRange>> before;
    for (const auto axis : impl::bounded_axes(m_kind))
        before.emplace_back(axis, m_target->range(axis));
    // A part's slot may delete a panel span while its target writes.
    const QPointer<SciQLopSpan> alive(this);
    m_target->set_coordinates(coordinates);
    if (!alive)
        return;
    Q_EMIT coordinates_changed(coordinates);
    for (const auto& [axis, old] : before)
    {
        if (!alive)
            return;
        if (const auto now = m_target->range(axis); now != old)
            notify_range(axis, now);
    }
}

SciQLopPlotRange SciQLopSpan::pixel_range() const
{
    return m_target->pixel_range(one_axis("pixel_range"));
}

SciQLopPlotRange SciQLopSpan::pixel_key_range() const
{
    require_bounded(SpanAxis::Key, "pixel_key_range");
    return m_target->pixel_range(SpanAxis::Key);
}

SciQLopPlotRange SciQLopSpan::pixel_value_range() const
{
    require_bounded(SpanAxis::Value, "pixel_value_range");
    return m_target->pixel_range(SpanAxis::Value);
}

QColor SciQLopSpan::borders_color() const
{
    return m_target->borders_color();
}

void SciQLopSpan::set_borders_color(const QColor& color)
{
    SCIQLOP_ON_OWNER_THREAD(set_borders_color(color));
    m_target->set_borders_color(color);
}

double SciQLopSpan::line_width() const
{
    return m_target->line_width();
}

void SciQLopSpan::set_line_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
    m_target->set_line_width(width);
}

Qt::PenStyle SciQLopSpan::line_style() const
{
    return m_target->line_style();
}

void SciQLopSpan::set_line_style(Qt::PenStyle style)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_style(style));
    m_target->set_line_style(style);
}
```

- [ ] **Step 6: Bindings**

Inside `<object-type name="SciQLopSpan">`, before the B3 comment:

```xml
        <modify-function signature="pixel_range()const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="pixel_key_range()const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="pixel_value_range()const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopPlotRange"/></insert-template>
            </inject-code>
        </modify-function>
```

- [ ] **Step 7: Build, run, full suite**

Compile (`rm build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/sciqlopspan_wrapper.cpp` first if the new methods are missing from Python). Run `tests/integration/test_span.py tests/integration/test_drawn_contract.py tests/integration/test_binding_signatures.py`, then the full suite.

- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopSpan.hpp src/SciQLopSpan.cpp include/SciQLopPlots/Items/impl/SpanTargets.hpp src/SpanTargets.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_span.py
git commit -m "feat(span): coordinates switching, pixel accessors, the border pen, click selection and delete_requested"
```

---

### Task 4: Spans at panel scope, and `panel.span()`

**Files:**
- Create: `include/SciQLopPlots/Items/impl/PanelParts.hpp`
- Modify: `include/SciQLopPlots/Items/impl/SpanTargets.hpp`, `src/SpanTargets.cpp` (add `SpanState`, `PanelSpanTarget`)
- Modify: `include/SciQLopPlots/Items/SciQLopSpan.hpp`, `src/SciQLopSpan.cpp` (the panel constructor)
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` (forward declaration ~line 48; `span` and `remove` after line 271), `src/SciQLopMultiPlotPanel.cpp` (`check_removable_here` ~line 162-174; `span` and `remove` after `remove(SciQLopPlotInterface*)` ~line 213)
- Modify: `src/SciQLopPlotInterface.cpp` (`check_removable`, ~line 99-112)
- Modify: `SciQLopPlots/bindings/bindings.xml`; branch F-py: `SciQLopPlots/__init__.py` (B4 loop)
- Test: `tests/integration/test_span.py` (`SCOPES`), `tests/integration/test_panel_span.py` (create), `tests/integration/test_drawn_contract.py` (builder variants)

**Interfaces:**
- Consumes: Task 2 (`SciQLopSpan`, `SpanTarget`, `PlotSpanTarget`, `bounded_axes`, `notify_range`), Task 3 (the rest of `SpanTarget`, which `PanelSpanTarget` implements).
- Produces:
  - `template <typename Part> class impl::PanelParts` (PanelParts.hpp): `PanelParts(QObject* owner, std::function<Part*(SciQLopPlot*)> make)`, `void follow(SciQLopMultiPlotPanel*)`, `bool writing() const`, `Part* first() const`, `void for_each(F) const`, `[[nodiscard]] bool write(F)` (false when a slot deleted the owner). Its destructor deletes the parts. Task 7 uses it for lines.
  - `SciQLopMultiPlotPanel* impl::panel_host(SciQLopMultiPlotPanel* panel, const char* what)`: throws `std::invalid_argument("<what>: panel cannot be None")`, then checks the thread.
  - `struct impl::SpanState`, `class impl::PanelSpanTarget`.
  - `SciQLopSpan(SciQLopMultiPlotPanel* panel, SpanKind kind, const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range, const QColor& color, bool read_only, bool visible, const QString& tool_tip, Coordinates coordinates, const QString& id = "")`. Its parts are plain plot-scope `SciQLopSpan`s, marked with `mark_panel_part`, sharing its `id()`.
  - `SciQLopMultiPlotPanel::span(...)` (B2 signature, defaults as on the plot), `SciQLopMultiPlotPanel::remove(SciQLopSpan*)`.
  - Wrong-scope messages: `plot.remove(x)` of a panel object says `remove: this <Class> belongs to a panel; remove it with panel.remove`; `panel.remove(x)` of a plot object says `remove: this <Class> belongs to a plot; remove it with plot.remove`; `panel.remove(part)` says `... is part of a panel-wide object ...`.

- [ ] **Step 1: Write the failing tests**

In `tests/integration/test_span.py`: `SCOPES = ["plot", "panel"]`. Every test of that file now also runs on a two-plot panel.

Most of them mean the same thing at both scopes. These change meaning at panel scope, and this is what they now expect:
- `test_scope_and_part_flag`: `scope()` is the panel, and the panel object is not a part.
- The signal-count tests (`test_one_set_range_emits_once_on_each_signal`, `test_a_rectangular_span_never_emits_range_changed`, `test_signalled_accessors_round_trip_and_emit_once`, `test_a_set_off_thread_lands_on_the_gui_thread`): one emission from the panel object per change, never one per part.
- The coordinate tests (`test_switching_coordinates_keeps_the_span_in_place`, `test_switching_coordinates_signals_the_new_numbers`, `test_a_pixels_span_stays_put_on_zoom`): the pixel accessors read plot 0's part; after `set_coordinates`, every part holds plot 0's numbers (the `simplify:` rule of Step 4). Zoom goes through `set_time_axis_range` and each plot's y axis.
- `test_clicking_a_span_selects_it_and_signals_once` and `test_the_delete_key_asks_once`: the click or the key lands on plot 0's canvas, so on part 0; the panel object then signals `selection_changed` once, or `delete_requested` once.
- `test_remove_deletes_it`: `panel.remove(span)`, which deletes the parts too.
- `test_the_class_has_no_python_constructor` already covers the panel constructor.

`tests/integration/test_panel_span.py`:

```python
"""Panel-scope spans: parts follow the plots, user edits flow up once (API review 5.6, Plan A)."""
import numpy as np
import pytest
import shiboken6
from PySide6.QtCore import QPoint, Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import (SciQLopMultiPlotPanel, SciQLopNDProjectionPlot, SciQLopPlotRange as R,
                          SciQLopSpan)
from conftest import canvas_of, collect, process_events, record

X = np.linspace(0.0, 10.0, 200)


def _panel(qtbot, n=2):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    for i in range(n):
        panel.plot(X, np.sin(X + i))
    panel.set_time_axis_range(R(0.0, 10.0))
    process_events()
    return panel


def _parts(panel, span):
    """The plot-scope parts `span` built, in plot order."""
    return [part for i in range(panel.size())
            for part in panel.plot_at(i).findChildren(SciQLopSpan)
            if part.is_panel_part() and part.id() == span.id()]


def test_one_plain_marked_part_per_plot(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    parts = _parts(panel, span)
    assert len(parts) == 2
    assert all(type(part) is SciQLopSpan and part.is_panel_part() for part in parts)
    assert [part.scope() for part in parts] == [panel.plot_at(0), panel.plot_at(1)]
    assert (span.scope(), span.is_panel_part()) == (panel, False)


def test_the_span_is_the_panels_child_and_outlives_its_python_reference(qtbot):
    panel = _panel(qtbot)
    panel.span(key_range=R(2.0, 8.0))
    collect()
    [span] = [child for child in panel.children() if isinstance(child, SciQLopSpan)]
    assert len(_parts(panel, span)) == 2


def test_parts_follow_plots_joining_and_leaving(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    panel.plot(X, np.cos(X))
    process_events()
    assert len(_parts(panel, span)) == 3
    panel.remove_plot(panel.plot_at(0))
    collect()
    assert len(_parts(panel, span)) == 2


def test_parts_built_later_carry_the_current_state(qtbot):
    panel = SciQLopMultiPlotPanel()
    qtbot.addWidget(panel)
    span = panel.span(key_range=R(2.0, 8.0), color=QColor("red"))
    span.set_key_range(R(3.0, 4.0))
    span.set_read_only(True)
    span.set_tool_tip("tip")
    span.set_visible(False)
    span.set_line_width(5.0)
    panel.plot(X, np.sin(X))
    process_events()
    [part] = _parts(panel, span)
    assert (part.key_range(), part.color(), part.read_only(), part.tool_tip(), part.visible(),
            part.line_width()) == (R(3.0, 4.0), QColor("red"), True, "tip", False, 5.0)


def test_a_plot_destroyed_under_the_span_is_survived(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    shiboken6.delete(panel.plot_at(0))
    collect()
    span.set_key_range(R(3.0, 4.0))
    span.set_color(QColor("red"))
    assert [(part.key_range(), part.color()) for part in _parts(panel, span)] == \
        [(R(3.0, 4.0), QColor("red"))]


def test_an_edit_on_a_part_reaches_the_span_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    first.set_key_range(R(3.0, 4.0))
    assert (ranges, keys) == ([R(3.0, 4.0)], [R(3.0, 4.0)])
    assert span.key_range() == second.key_range() == R(3.0, 4.0)


def test_a_rectangular_part_edit_reaches_both_axes(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0), value_range=R(-0.5, 0.5))
    first, second = _parts(panel, span)
    values = record(span.value_range_changed)
    first.set_value_range(R(0.1, 0.2))
    assert values == [R(0.1, 0.2)]
    assert second.value_range() == R(0.1, 0.2)


def test_selecting_a_part_selects_the_span_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    seen = record(span.selection_changed)
    first.set_selected(True)
    assert (span.selected(), second.selected(), seen) == (True, True, [True])


def test_one_drag_on_a_part_emits_once_and_moves_the_other_part(qtbot):
    panel = _panel(qtbot)
    panel.resize(800, 600)
    panel.show()
    qtbot.waitExposed(panel)
    process_events()
    span = panel.span(key_range=R(2.0, 8.0))
    first, second = _parts(panel, span)
    panel.replot(True)
    process_events()
    ranges, keys = record(span.range_changed), record(span.key_range_changed)
    canvas = canvas_of(panel.plot_at(0))
    start = QPoint(canvas.width() // 2, canvas.height() // 2)
    end = start + QPoint(40, 0)
    QTest.mousePress(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, start)
    QTest.mouseMove(canvas, end)
    QTest.mouseRelease(canvas, Qt.MouseButton.LeftButton, Qt.KeyboardModifier.NoModifier, end)
    process_events()
    assert (len(ranges), len(keys)) == (1, 1)
    assert ranges[0] != R(2.0, 8.0)
    assert span.key_range() == first.key_range() == second.key_range() == ranges[0]


def test_reordering_plots_keeps_one_part_per_plot(qtbot):
    """Review Focus 2."""
    panel = _panel(qtbot, 3)
    span = panel.span(key_range=R(2.0, 8.0))
    panel.move_plot(0, 2)
    process_events()
    assert len(_parts(panel, span)) == 3


def test_a_projection_plot_in_the_panel_gets_no_part(qtbot):
    """Review Focus 3."""
    panel = _panel(qtbot)
    projection = SciQLopNDProjectionPlot(3)
    panel.add_plot(projection)
    process_events()
    span = panel.span(key_range=R(2.0, 8.0))
    span.set_key_range(R(3.0, 4.0))
    assert [part.key_range() for part in _parts(panel, span)] == [R(3.0, 4.0)] * 2
    assert projection.findChildren(SciQLopSpan) == []


def test_a_slot_removing_the_span_during_a_part_edit(qtbot):
    """Review Focus 5: the emitting part dies with the span."""
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    first, _ = _parts(panel, span)
    span.range_changed.connect(lambda *_: panel.remove(span))
    first.set_key_range(R(3.0, 4.0))
    process_events()
    assert not shiboken6.isValid(span)
    assert [p for i in range(panel.size()) for p in panel.plot_at(i).findChildren(SciQLopSpan)] == []


def test_panel_remove_deletes_the_span_and_its_parts(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    parts = _parts(panel, span)
    panel.remove(span)
    assert not shiboken6.isValid(span)
    assert not any(shiboken6.isValid(part) for part in parts)


def test_plot_remove_refuses_a_part(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.plot_at(0).remove(_parts(panel, span)[0])


def test_plot_remove_refuses_a_panel_span_naming_panel_remove(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match=r"panel\.remove"):
        panel.plot_at(0).remove(span)


def test_panel_remove_refuses_a_plot_span_naming_plot_remove(qtbot):
    panel = _panel(qtbot)
    plot_span = panel.plot_at(0).span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match=r"plot\.remove"):
        panel.remove(plot_span)


def test_panel_remove_refuses_a_part(qtbot):
    panel = _panel(qtbot)
    span = panel.span(key_range=R(2.0, 8.0))
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.remove(_parts(panel, span)[0])
```

In `tests/integration/test_drawn_contract.py`, let a class have one builder per scope, and let a builder read its attributes as properties on its own:
- `from dataclasses import dataclass, field`.
- In `Row`, after `properties`: `builder_properties: dict = field(default_factory=dict)  # per builder, over properties`.
- Right after the `Row` class, these two helpers:

  ```python
  def _class_of(builder):
      """'SciQLopSpan[panel]' builds a SciQLopSpan: one class may have a builder per scope."""
      return builder.split("[")[0]


  def _properties(row, builder):
      return row.builder_properties.get(builder, row.properties)
  ```
- In `test_every_drawn_class_is_in_exactly_one_row_or_abstract`, the last check compares `{_class_of(b) for b in row.builders}` instead of `set(row.builders)`.
- `_get(obj, row, attr)` and `_set(obj, row, attr, value)` take the builder name second: `_get(obj, row, name, attr)` and `_set(obj, row, name, attr, value)`, and test `attr in _properties(row, name)`. Update their calls in the two round-trip tests.
- In the `Row("new span", ...)` builders: `"SciQLopSpan[panel]": lambda h: h.panel.span(key_range=R(1.0, 2.0)),`.

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_span.py tests/integration/test_panel_span.py tests/integration/test_drawn_contract.py`.
Expected: every panel case fails with `AttributeError: 'SciQLopMultiPlotPanel' object has no attribute 'span'`; the plot cases still pass.

- [ ] **Step 3: `include/SciQLopPlots/Items/impl/PanelParts.hpp`**

GPL header, then:

```cpp
#pragma once

#include "SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"
#include "SciQLopPlots/SciQLopPlotInterface.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QList>
#include <QPointer>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace impl
{
//! \a panel, checked, for a panel-scope object's first base-class initializer.
inline SciQLopMultiPlotPanel* panel_host(SciQLopMultiPlotPanel* panel, const char* what)
{
    if (panel == nullptr)
        throw std::invalid_argument(std::string(what) + ": panel cannot be None");
    return SciQLopPlots::on_owner_thread(panel, what);
}

/*!
 * The per-plot parts of a panel-wide object: one per SciQLopPlot of the panel (projection and
 * widget plots get none), built when the plot joins and deleted when it leaves. A plot deleted
 * directly takes its part with it, which leaves a null QPointer here.
 *
 * Every loop runs over a snapshot, so a slot that deletes a part, or the owner, is safe. While
 * write() runs, writing() is true: the owner ignores the parts' echoes of its own change, so one
 * change emits once.
 */
template <typename Part>
class PanelParts
{
    QObject* m_owner;
    std::function<Part*(SciQLopPlot*)> m_make;
    QList<QPointer<SciQLopPlotInterface>> m_plots;
    QList<QPointer<Part>> m_parts;
    bool m_writing = false;

    void update(const QList<QPointer<SciQLopPlotInterface>>& plots)
    {
        m_parts.removeIf([](const QPointer<Part>& part) { return part.isNull(); });
        for (const auto& plot : plots)
        {
            if (!plot || m_plots.contains(plot))
                continue;
            m_plots.append(plot);
            if (auto* scp = qobject_cast<SciQLopPlot*>(plot.data()))
                add(scp);
        }
        const auto known = m_plots;
        for (const auto& plot : known)
        {
            if (plots.contains(plot))
                continue;
            m_plots.removeOne(plot);
            remove(plot.data());
        }
    }

    void add(SciQLopPlot* plot)
    {
        auto* part = m_make(plot);
        SciQLopPlots::mark_panel_part(part);
        m_parts.append(part);
    }

    void remove(SciQLopPlotInterface* plot)
    {
        if (plot == nullptr)
            return;
        const auto parts = m_parts;
        for (const auto& part : parts)
        {
            if (part && part->parent() == plot)
            {
                m_parts.removeAll(part);
                delete part.data();
            }
        }
    }

public:
    PanelParts(QObject* owner, std::function<Part*(SciQLopPlot*)> make)
            : m_owner(owner), m_make(std::move(make))
    {
    }

    // Deletion needs no writing() guard, unlike write(): a dying part emits none of the signals
    // its owner follows (only QObject::destroyed, which nobody here connects), so nothing re-enters.
    ~PanelParts()
    {
        for_each([](Part* part) { delete part; });
    }

    PanelParts(const PanelParts&) = delete;
    PanelParts& operator=(const PanelParts&) = delete;

    //! Builds a part for each plot \a panel has, then follows its plot list.
    void follow(SciQLopMultiPlotPanel* panel)
    {
        QObject::connect(panel, &SciQLopPlotPanelInterface::plot_list_changed, m_owner,
                         [this](const QList<QPointer<SciQLopPlotInterface>>& plots)
                         { update(plots); });
        update(panel->plots());
    }

    [[nodiscard]] bool writing() const noexcept { return m_writing; }

    [[nodiscard]] Part* first() const
    {
        for (const auto& part : m_parts)
            if (part)
                return part.data();
        return nullptr;
    }

    template <typename F>
    void for_each(F&& f) const
    {
        const auto parts = m_parts;
        for (const auto& part : parts)
            if (part)
                f(part.data());
    }

    //! for_each with writing() raised. False when a slot deleted the owner: touch nothing then.
    template <typename F>
    [[nodiscard]] bool write(F&& f)
    {
        const QPointer<QObject> alive(m_owner);
        const bool was = std::exchange(m_writing, true);
        for_each(std::forward<F>(f));
        if (!alive)
            return false;
        m_writing = was;
        return true;
    }
};
} // namespace impl
```

`m_plots.contains(plot)` compares the pointers, so `move_plot` (same plots, new order) adds nothing: Review Focus 2.

- [ ] **Step 4: `SpanState` and `PanelSpanTarget`**

In `include/SciQLopPlots/Items/impl/SpanTargets.hpp`: `#include "SciQLopPlots/Items/impl/PanelParts.hpp"`, then after `PlotSpanTarget`:

```cpp
//! Everything a panel span keeps and writes to its parts.
struct SpanState
{
    SciQLopPlotRange key_range;
    SciQLopPlotRange value_range;
    Coordinates coordinates = Coordinates::Data;
    bool read_only = false;
    QString tool_tip;
    QColor color;
    QColor borders_color;
    double line_width = 3.0;
    Qt::PenStyle line_style = Qt::SolidLine;
    bool visible = true;
    bool selected = false;
};

//! A span on every SciQLopPlot of a panel: the state, and one plot-scope part per plot.
class PanelSpanTarget final : public SpanTarget
{
    SciQLopSpan* m_span;
    SpanKind m_kind;
    SpanState m_state;
    PanelParts<SciQLopSpan> m_parts; // last: destroyed first, while the state still exists

    SciQLopSpan* make_part(SciQLopPlot* plot);
    void follow(SciQLopSpan* part);
    //! Stores \a range and writes it to every part; false when a slot deleted the span meanwhile.
    bool store_range(SpanAxis axis, const SciQLopPlotRange& range);
    template <typename T, typename Setter>
    void write(T& field, const T& value, Setter setter);

public:
    PanelSpanTarget(SciQLopSpan* span, SciQLopMultiPlotPanel* panel, SpanKind kind,
                    SpanState state);

    SciQLopPlotRange range(SpanAxis axis) const override;
    void set_range(SpanAxis axis, const SciQLopPlotRange& range) override;
    SciQLopPlotRange pixel_range(SpanAxis axis) const override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    QColor borders_color() const override;
    void set_borders_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
};
```

In `src/SpanTargets.cpp`, add `#include <utility>` and `#include <vector>`, and after the `PlotSpanTarget` definitions:

```cpp
PanelSpanTarget::PanelSpanTarget(SciQLopSpan* span, SciQLopMultiPlotPanel* panel, SpanKind kind,
                                 SpanState state)
        : m_span { span }
        , m_kind { kind }
        , m_state { std::move(state) }
        , m_parts { span, [this](SciQLopPlot* plot) { return make_part(plot); } }
{
    m_parts.follow(panel);
}

SciQLopSpan* PanelSpanTarget::make_part(SciQLopPlot* plot)
{
    auto* part = new SciQLopSpan(plot, m_kind, m_state.key_range, m_state.value_range,
                                 m_state.color, m_state.read_only, m_state.visible,
                                 m_state.tool_tip, m_state.coordinates, m_span->id());
    part->set_borders_color(m_state.borders_color);
    part->set_line_width(m_state.line_width);
    part->set_line_style(m_state.line_style);
    part->set_selected(m_state.selected);
    follow(part);
    return part;
}

// A part's own edits (a drag, a click, the Delete key) flow up; the span writes them back out to
// every part, whose echoes writing() swallows.
void PanelSpanTarget::follow(SciQLopSpan* part)
{
    QObject::connect(part, &SciQLopSpan::key_range_changed, m_span,
                     [this](const SciQLopPlotRange& range)
                     {
                         if (!m_parts.writing())
                             set_range(SpanAxis::Key, range);
                     });
    QObject::connect(part, &SciQLopSpan::value_range_changed, m_span,
                     [this](const SciQLopPlotRange& range)
                     {
                         if (!m_parts.writing())
                             set_range(SpanAxis::Value, range);
                     });
    QObject::connect(part, &SciQLopDrawnObject::selection_changed, m_span,
                     [this](bool selected)
                     {
                         if (!m_parts.writing())
                             m_span->set_selected(selected);
                     });
    QObject::connect(part, &SciQLopSpan::delete_requested, m_span, &SciQLopSpan::delete_requested);
}

bool PanelSpanTarget::store_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    (axis == SpanAxis::Key ? m_state.key_range : m_state.value_range) = range;
    return m_parts.write(
        [axis, range](SciQLopSpan* part)
        {
            if (axis == SpanAxis::Key)
                part->set_key_range(range);
            else
                part->set_value_range(range);
        });
}

template <typename T, typename Setter>
void PanelSpanTarget::write(T& field, const T& value, Setter setter)
{
    field = value;
    (void)m_parts.write([value, setter](SciQLopSpan* part) { (part->*setter)(value); });
}

SciQLopPlotRange PanelSpanTarget::range(SpanAxis axis) const
{
    return axis == SpanAxis::Key ? m_state.key_range : m_state.value_range;
}

void PanelSpanTarget::set_range(SpanAxis axis, const SciQLopPlotRange& range)
{
    if (this->range(axis) == range)
        return;
    const SciQLopPlotRange stored = range; // \a range may live in a part a slot deletes
    if (store_range(axis, stored))
        m_span->notify_range(axis, stored);
}

// simplify: the first plot's pixels stand for the panel's; a per-plot answer needs a plot argument.
SciQLopPlotRange PanelSpanTarget::pixel_range(SpanAxis axis) const
{
    auto* part = m_parts.first();
    if (!part)
        return SciQLopPlotRange();
    return part->m_target->pixel_range(axis);
}

Coordinates PanelSpanTarget::coordinates() const
{
    return m_state.coordinates;
}

// simplify: each part converts on its own plot, then the first part's numbers become the
// panel's and every part takes them, so plots of different sizes share one pixel range; with no
// plot yet the numbers stay as they are. A range per plot would lift both limits.
void PanelSpanTarget::set_coordinates(Coordinates coordinates)
{
    m_state.coordinates = coordinates;
    if (!m_parts.write([coordinates](SciQLopSpan* part) { part->set_coordinates(coordinates); }))
        return;
    auto* first = m_parts.first();
    if (!first)
        return;
    std::vector<std::pair<SpanAxis, SciQLopPlotRange>> converted;
    for (const auto axis : bounded_axes(m_kind))
        converted.emplace_back(axis, first->m_target->range(axis));
    for (const auto& [axis, range] : converted)
        if (!store_range(axis, range))
            return;
}

bool PanelSpanTarget::read_only() const
{
    return m_state.read_only;
}

void PanelSpanTarget::set_read_only(bool read_only)
{
    write(m_state.read_only, read_only, &SciQLopSpan::set_read_only);
}

QString PanelSpanTarget::tool_tip() const
{
    return m_state.tool_tip;
}

void PanelSpanTarget::set_tool_tip(const QString& tool_tip)
{
    write(m_state.tool_tip, tool_tip, &SciQLopSpan::set_tool_tip);
}

QColor PanelSpanTarget::color() const
{
    return m_state.color;
}

void PanelSpanTarget::set_color(const QColor& color)
{
    write(m_state.color, color, &SciQLopDrawnObject::set_color);
}

QColor PanelSpanTarget::borders_color() const
{
    return m_state.borders_color;
}

void PanelSpanTarget::set_borders_color(const QColor& color)
{
    write(m_state.borders_color, color, &SciQLopSpan::set_borders_color);
}

double PanelSpanTarget::line_width() const
{
    return m_state.line_width;
}

void PanelSpanTarget::set_line_width(double width)
{
    write(m_state.line_width, width, &SciQLopSpan::set_line_width);
}

Qt::PenStyle PanelSpanTarget::line_style() const
{
    return m_state.line_style;
}

void PanelSpanTarget::set_line_style(Qt::PenStyle style)
{
    write(m_state.line_style, style, &SciQLopSpan::set_line_style);
}

bool PanelSpanTarget::visible() const
{
    return m_state.visible;
}

void PanelSpanTarget::set_visible(bool visible)
{
    write(m_state.visible, visible, &SciQLopDrawnObject::set_visible);
}

bool PanelSpanTarget::selected() const
{
    return m_state.selected;
}

void PanelSpanTarget::set_selected(bool selected)
{
    write(m_state.selected, selected, &SciQLopDrawnObject::set_selected);
}
```

The state is stored before the parts are written: their echoes are ignored by `writing()`, so the order cannot double an emission, and a part that comes later is built from the new value.

- [ ] **Step 5: The panel constructor**

`include/SciQLopPlots/Items/SciQLopSpan.hpp`, after the plot constructor:

```cpp
    SciQLopSpan(SciQLopMultiPlotPanel* panel, SpanKind kind, const SciQLopPlotRange& key_range,
                const SciQLopPlotRange& value_range, const QColor& color, bool read_only,
                bool visible, const QString& tool_tip, Coordinates coordinates,
                const QString& id = "");
```

`src/SciQLopSpan.cpp`, after the plot constructor (designated initializers, C++20, in `SpanState`'s order):

```cpp
SciQLopSpan::SciQLopSpan(SciQLopMultiPlotPanel* panel, SpanKind kind,
                         const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range,
                         const QColor& color, bool read_only, bool visible,
                         const QString& tool_tip, Coordinates coordinates, const QString& id)
        : SciQLopDrawnObject { impl::panel_host(panel, "SciQLopSpan") }
        , m_kind { kind }
        , m_id { id_or_uuid(id) }
        , m_target { std::make_unique<impl::PanelSpanTarget>(
              this, panel, kind,
              impl::SpanState { .key_range = key_range.sorted(),
                                .value_range = value_range.sorted(),
                                .coordinates = coordinates,
                                .read_only = read_only,
                                .tool_tip = tool_tip,
                                .color = color,
                                .borders_color = color.lighter(),
                                .visible = visible }) }
{
}
```

`m_id` is declared before `m_target`, so the parts built inside the target's constructor read the right `id()`.

- [ ] **Step 6: The panel factory and the `remove` checks**

`include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`: `class SciQLopSpan;` after line 48 (branch O: `#include <optional>`), and after `void remove(SciQLopPlotInterface* plot);` (line 271):

```cpp
    //! A span on every plot of the panel, plot.span's arguments and defaults.
    SciQLopSpan* span(/* B2 parameters, with defaults */
                      QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                      bool visible = true, const QString& tool_tip = "",
                      Coordinates coordinates = Coordinates::Data, const QString& id = "");
    void remove(SciQLopSpan* span);
```

`src/SciQLopMultiPlotPanel.cpp`: `#include "SciQLopPlots/Items/SciQLopSpan.hpp"`. Replace `check_removable_here` (~line 164-173) with:

```cpp
template <typename T>
void check_removable_here(const SciQLopMultiPlotPanel* panel, const T* x)
{
    if (x == nullptr)
        throw std::invalid_argument("remove: got None");
    const std::string what = x->metaObject()->className();
    if (SciQLopPlots::is_panel_part(x))
        throw std::invalid_argument("remove: this " + what
                                    + " is part of a panel-wide object; remove that object from "
                                      "the panel (panel.remove)");
    if (qobject_cast<const SciQLopPlotInterface*>(x->parent()))
        throw std::invalid_argument("remove: this " + what
                                    + " belongs to a plot; remove it with plot.remove");
    if (x->parent() != panel)
        throw std::invalid_argument("remove: this " + what + " belongs to another panel");
}
```

After `remove(SciQLopPlotInterface*)`:

```cpp
SciQLopSpan* SciQLopMultiPlotPanel::span(/* B2 parameters, without defaults */ QColor color,
                                         bool read_only, bool visible, const QString& tool_tip,
                                         Coordinates coordinates, const QString& id)
{
    SciQLopPlots::require_owner_thread(this, "SciQLopMultiPlotPanel::span");
    return new SciQLopSpan(this, /* B2 kind and ranges */ color, read_only, visible, tool_tip,
                           coordinates, id);
}

void SciQLopMultiPlotPanel::remove(SciQLopSpan* span)
{
    check_removable_here(this, span);
    SCIQLOP_ON_OWNER_THREAD_WITH(span, remove(span));
    delete span;
}
```

`src/SciQLopPlotInterface.cpp`: `#include "SciQLopPlots/MultiPlots/SciQLopPlotPanelInterface.hpp"`, and in `check_removable`, between the `is_panel_part` check and the owner check:

```cpp
    if (qobject_cast<const SciQLopPlotPanelInterface*>(x->parent()))
        throw std::invalid_argument("remove: this " + what
                                    + " belongs to a panel; remove it with panel.remove");
```

- [ ] **Step 7: Bindings**

In `<object-type name="SciQLopSpan">`, after the plot constructor's `remove="all"`:

```xml
        <modify-function signature="SciQLopSpan(SciQLopMultiPlotPanel*,SpanKind,const SciQLopPlotRange&amp;,const SciQLopPlotRange&amp;,const QColor&amp;,bool,bool,const QString&amp;,Coordinates,const QString&amp;)" remove="all"/>
```

In `<object-type name="SciQLopMultiPlotPanel" ...>`, after the `remove(SciQLopPlotInterface*)` modify-function (~line 1757):

```xml
        <modify-function signature="span(... B2 signature string ...)">
            <!-- branch F only: B4's two modify-argument elements here -->
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="remove(SciQLopSpan*)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_valueerror"/>
            </inject-code>
        </modify-function>
```

Branch F-py: add `SciQLopMultiPlotPanel` to the tuple of the B4 loop that Task 2 added to `__init__.py`.

- [ ] **Step 8: Build, run, full suite**

Compile (no new object-type). Run `tests/integration/test_span.py tests/integration/test_panel_span.py tests/integration/test_drawn_contract.py tests/integration/test_plot_remove.py tests/integration/test_binding_signatures.py`, then the full suite. `test_plot_remove.py` pins the old `remove` messages: a test whose expected message changed with the new wrong-scope wording contradicts the spec (it names the scope to use now); update its `match=` and name it in the report.

- [ ] **Step 9: Commit**

```bash
git add include/SciQLopPlots/Items/impl/PanelParts.hpp include/SciQLopPlots/Items/impl/SpanTargets.hpp src/SpanTargets.cpp include/SciQLopPlots/Items/SciQLopSpan.hpp src/SciQLopSpan.cpp include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp src/SciQLopPlotInterface.cpp SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_span.py tests/integration/test_panel_span.py tests/integration/test_drawn_contract.py
git commit -m "feat(span): panel-scope spans (panel.span): one part per plot, user edits flow up once"
```

---

### Task 5: The line at plot scope on a target: position, coordinates, read-only, Delete; `plot.straight_line()`

The class moves onto a target, keeping every existing method; the new behaviour here is the coordinates setter, `read_only`, `delete_requested` and the factory. The clamp getters and `tool_tip_changed` come in Task 6; the `.on.tooltip` row in Task 8.

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopStraightLines.hpp` (`StraightLine` ~line 34-149: a clamp function, `coordinates`, `set_coordinates`, `place`; `SciQLopStraightLine` ~line 151-305: rewritten on a target)
- Modify: `src/SciQLopStraightLines.cpp`
- Create: `include/SciQLopPlots/Items/impl/LineTargets.hpp`, `src/LineTargets.cpp`
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (`straight_line` after `span`), `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp`
- Modify: `src/meson.build` (sources, after `'SciQLopStraightLines.cpp',` line 185)
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopStraightLine`, `SciQLopVerticalLine`, `SciQLopHorizontalLine` ~line 1315-1321; `SciQLopPlotInterface`)
- Modify: `SciQLopPlots/__init__.py` (`_ITEM_FACTORIES` line 379)
- Test: `tests/integration/test_straight_line.py` (create), `tests/integration/test_item_factories.py`, `tests/integration/test_drawn_contract.py`

**Interfaces:**
- Consumes: `plots_of`, `canvas_of`, `scoped_host` (Task 2).
- Produces:
  - `inline double clamp_line_position(double pos, std::optional<double> min, std::optional<double> max)` (global, SciQLopStraightLines.hpp): `StraightLine::_clamp`'s rule, used by Task 7's panel line too.
  - `StraightLine::coordinates()`, `StraightLine::set_coordinates(Coordinates)` (silent: no `moved`).
  - `impl::LineTarget` (abstract), `impl::PlotLineTarget`.
  - On `SciQLopStraightLine`: `orientation()`, `scope()`, `is_panel_part()`, `position()` / `set_position`, `coordinates()` / `set_coordinates`, `read_only()` / `set_read_only`, `movable()` / `set_movable` (inverse aliases), `tool_tip()` / `set_tool_tip`, `line_width`, `line_style`, `set_min_value`, `clear_min_value`, `set_max_value`, `clear_max_value` (as before, now through the target). Signals: `position_changed(double)`, `coordinates_changed(Coordinates)`, `read_only_changed(bool)`, `delete_requested()`. The plot constructor keeps its signature.
  - `SciQLopPlotInterface::straight_line(double position, Qt::Orientation orientation, QColor color = QColor::fromRgb(0, 0, 0), bool read_only = true, bool visible = true, const QString& tool_tip = "", Coordinates coordinates = Coordinates::Data)` (virtual, NotImplementedError by default), overridden by `SciQLopPlot`.
  - Python: `position` is a method on `SciQLopStraightLine` and stays a property on `SciQLopVerticalLine` / `SciQLopHorizontalLine`; `SciQLopStraightLine(plot, ...)` warns `... use plot.straight_line(...)`.

- [ ] **Step 1: Write the failing tests**

`tests/integration/test_straight_line.py` (`SCOPES` gets `"panel"` in Task 7):

```python
"""SciQLopStraightLine: one line class, vertical or horizontal, at plot and panel scope (5.6)."""
import pytest
import shiboken6
from PySide6.QtCore import Qt
from PySide6.QtGui import QColor
from PySide6.QtTest import QTest

from SciQLopPlots import (Coordinates, SciQLopHorizontalLine, SciQLopPlot, SciQLopPlotRange as R,
                          SciQLopStraightLine, SciQLopVerticalLine)
from conftest import canvas_of, off_thread, plots_of, process_events, record, scoped_host

SCOPES = ["plot"]
ORIENTATIONS = {"vertical": (Qt.Orientation.Vertical, 5.0),
                "horizontal": (Qt.Orientation.Horizontal, 0.5)}


@pytest.fixture(params=SCOPES)
def host(request, qtbot):
    return scoped_host(request.param, qtbot)


@pytest.fixture
def shown_host(host, qtbot):
    host.resize(800, 600)
    host.show()
    qtbot.waitExposed(host)
    process_events()
    return host


@pytest.fixture(params=list(ORIENTATIONS))
def orientation(request):
    return request.param


def make(host, orientation, **kwargs):
    orient, position = ORIENTATIONS[orientation]
    return host.straight_line(position, orient, **kwargs)


def test_the_factory_returns_the_plain_class(host, orientation):
    line = make(host, orientation)
    assert type(line) is SciQLopStraightLine
    assert (line.orientation(), line.position()) == ORIENTATIONS[orientation]


def test_the_defaults(host, orientation):
    line = make(host, orientation)
    assert (line.color(), line.read_only(), line.visible(), line.selected(), line.tool_tip()) == \
        (QColor(0, 0, 0), True, True, False, "")
    assert line.coordinates() == Coordinates.Data
    assert (line.scope(), line.is_panel_part()) == (host, False)


def test_the_factory_arguments(host, orientation):
    line = make(host, orientation, color=QColor("red"), read_only=False, visible=False,
                tool_tip="tip")
    assert (line.color(), line.read_only(), line.visible(), line.tool_tip()) == \
        (QColor("red"), False, False, "tip")


SIGNALLED = {"position": (0.25, "position_changed"), "read_only": (False, "read_only_changed")}


@pytest.mark.parametrize("name", SIGNALLED)
def test_signalled_accessors_round_trip_and_emit_once(host, orientation, name):
    value, signal = SIGNALLED[name]
    line = make(host, orientation)
    seen = record(getattr(line, signal))
    getattr(line, f"set_{name}")(value)
    getattr(line, f"set_{name}")(value)
    assert (getattr(line, name)(), seen) == (value, [value])


STYLE = {"line_width": 3.0, "line_style": Qt.PenStyle.DashLine}


@pytest.mark.parametrize("name", STYLE)
def test_the_style_round_trips(host, orientation, name):
    line = make(host, orientation)
    getattr(line, f"set_{name}")(STYLE[name])
    assert getattr(line, name)() == STYLE[name]


def test_switching_coordinates_keeps_the_line_in_place(shown_host, orientation):
    line = make(shown_host, orientation)
    process_events()
    data = line.position()
    coordinates, positions = record(line.coordinates_changed), record(line.position_changed)
    line.set_coordinates(Coordinates.Pixels)
    process_events()
    pixels = line.position()
    canvas = canvas_of(plots_of(shown_host)[0])
    assert 0 < pixels < (canvas.width() if orientation == "vertical" else canvas.height())
    assert (coordinates, positions) == ([Coordinates.Pixels], [pixels])
    line.set_coordinates(Coordinates.Data)
    process_events()
    assert line.position() == pytest.approx(data, abs=0.05)


def test_a_pixels_line_stays_put_on_zoom(shown_host, orientation):
    line = make(shown_host, orientation)
    process_events()
    line.set_coordinates(Coordinates.Pixels)
    before = line.position()
    for plot in plots_of(shown_host):
        plot.y_axis().set_range(R(-5.0, 5.0))
    if isinstance(shown_host, SciQLopPlot):
        shown_host.x_axis().set_range(R(0.0, 100.0))
    else:
        shown_host.set_time_axis_range(R(0.0, 100.0))
    process_events()
    assert line.position() == before


def test_the_delete_key_asks_once(shown_host, orientation):
    line = make(shown_host, orientation)
    asked = []
    line.delete_requested.connect(lambda: asked.append(True))
    line.set_selected(True)
    process_events()
    QTest.keyClick(canvas_of(plots_of(shown_host)[0]), Qt.Key.Key_Delete)
    process_events()
    assert asked == [True]


def test_remove_deletes_it(host, orientation):
    line = make(host, orientation)
    host.remove(line)
    assert not shiboken6.isValid(line)


def test_the_factory_raises_off_thread(host):
    out = {}

    def call():
        try:
            host.straight_line(1.0, Qt.Orientation.Vertical)
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


@pytest.mark.parametrize("name", ["position", "read_only", "coordinates"])
def test_on_resolves_with_a_signal(host, name):
    assert getattr(make(host, "vertical").on, name).signal is not None


def test_the_old_line_classes_keep_position_as_a_property(plot):
    """Guard: SciQLop reads line.position on the old classes."""
    with pytest.warns(DeprecationWarning):
        vertical = SciQLopVerticalLine(plot, 1.0)
    with pytest.warns(DeprecationWarning):
        horizontal = SciQLopHorizontalLine(plot, 0.5)
    vertical.position = 2.0
    assert (vertical.position, horizontal.position) == (2.0, 0.5)
```

In `tests/integration/test_item_factories.py`:
- `from PySide6.QtCore import QObject, QPointF, QRectF, SIGNAL, Qt`.
- In `FACTORIES`: `"straight_line": (lambda p: p.straight_line(1.0, Qt.Orientation.Vertical), SciQLopStraightLine),`.
- In `NO_PLOT_FACTORY`: delete the `"SciQLopStraightLine"` entry.
- In `DEPRECATED`: `"straight_line": (CONSTRUCTORS["straight_line"], "use plot.straight_line"),`.

In `tests/integration/test_drawn_contract.py`: the `"text and lines"` row's `SciQLopStraightLine` builder becomes `lambda h: h.plot.straight_line(1.0, Qt.Orientation.Vertical)`; delete the `_straight_line` helper; `from PySide6.QtCore import QPointF, QRectF, Qt`.

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_straight_line.py tests/integration/test_item_factories.py tests/integration/test_drawn_contract.py`.
Expected: `AttributeError: ... has no attribute 'straight_line'` almost everywhere; `test_the_old_line_classes_keep_position_as_a_property` passes (it is a guard).

- [ ] **Step 3: `StraightLine`: a shared clamp, coordinates that can change**

In `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, before `class StraightLine` (line 34), the old `_clamp` body as a free function, with its comments:

```cpp
//! \a pos inside the optional bounds. NaN goes to the lower bound, else the upper, else 0.
inline double clamp_line_position(double pos, std::optional<double> min, std::optional<double> max)
{
    if (std::isnan(pos))
    {
        if (min)
            return *min;
        if (max)
            return *max;
        return 0.0;
    }
    // Inverted bounds (min > max): max wins, because hitting the upper limit is the more common
    // UI intent; the unconditional ladder below would silently violate max.
    if (min && max && *min > *max)
        return *max;
    if (min && pos < *min)
        return *min;
    if (max && pos > *max)
        return *max;
    return pos;
}
```

In `StraightLine`, `_clamp` becomes `inline double _clamp(double pos) const { return clamp_line_position(pos, m_min_value, m_max_value); }`, and add, private: `void place(double pos);`; public, next to `position()`:

```cpp
    [[nodiscard]] Coordinates coordinates() const noexcept { return m_coordinates; }
    //! Re-derives the position in \a coordinates so the line stays where it is on screen. Silent:
    //! no moved(), the caller reports the change. The min/max clamps keep their numbers.
    void set_coordinates(Coordinates coordinates);
```

In `src/SciQLopStraightLines.cpp`, `set_position` writes through `place`, and `set_coordinates` is new:

```cpp
void StraightLine::place(double pos)
{
    if (m_orientation == Qt::Orientation::Vertical)
    {
        this->point1->setCoords(pos, 0);
        this->point2->setCoords(pos, 1);
    }
    else
    {
        this->point1->setCoords(0, pos);
        this->point2->setCoords(1, pos);
    }
}

void StraightLine::set_position(double pos)
{
    pos = _clamp(pos);
    if (position() == pos)
        return;
    place(pos);
    const QPointer<StraightLine> self(this);
    emit moved(pos);
    if (self)
        this->replot();
}

void StraightLine::set_coordinates(Coordinates coordinates)
{
    if (coordinates == m_coordinates)
        return;
    const bool vertical = m_orientation == Qt::Orientation::Vertical;
    const QPointF pixel = point1->pixelPosition();
    double position = 0.;
    if (coordinates == Coordinates::Pixels)
    {
        const auto* rect = point1->axisRect();
        if (!rect)
            return;
        position = vertical ? pixel.x() - rect->left() : pixel.y() - rect->top();
    }
    else
    {
        const auto* axis = vertical ? point1->keyAxis() : point1->valueAxis();
        if (!axis)
            return;
        position = axis->pixelToCoord(vertical ? pixel.x() : pixel.y());
    }
    // Pixels are measured from the plot area: ptAbsolute's origin sits in the axis margins.
    const auto type = coordinates == Coordinates::Pixels ? QCPItemPosition::ptAxisRectAbsolute
                                                         : QCPItemPosition::ptPlotCoords;
    point1->setType(type);
    point2->setType(type);
    m_coordinates = coordinates;
    place(position);
    replot();
}
```

- [ ] **Step 4: `include/SciQLopPlots/Items/impl/LineTargets.hpp` and `src/LineTargets.cpp`**

Header (GPL header first):

```cpp
#pragma once

#include "SciQLopPlots/Items/SciQLopStraightLines.hpp"

#include <QPointer>
#include <optional>

namespace impl
{
//! What a SciQLopStraightLine reads and writes: one plot's item, or a panel's parts.
class LineTarget
{
public:
    virtual ~LineTarget() = default;

    virtual double position() const = 0;
    //! Applies \a position, clamped; position_changed fires once, never for the current one.
    virtual void set_position(double position) = 0;
    virtual Coordinates coordinates() const = 0;
    //! Re-derives the position in \a coordinates, keeping the line in place. Silent.
    virtual void set_coordinates(Coordinates coordinates) = 0;
    virtual bool read_only() const = 0;
    virtual void set_read_only(bool read_only) = 0;
    virtual QString tool_tip() const = 0;
    virtual void set_tool_tip(const QString& tool_tip) = 0;
    virtual QColor color() const = 0;
    virtual void set_color(const QColor& color) = 0;
    virtual double line_width() const = 0;
    virtual void set_line_width(double width) = 0;
    virtual Qt::PenStyle line_style() const = 0;
    virtual void set_line_style(Qt::PenStyle style) = 0;
    virtual bool visible() const = 0;
    virtual void set_visible(bool visible) = 0;
    virtual bool selected() const = 0;
    virtual void set_selected(bool selected) = 0;
    virtual std::optional<double> min_value() const = 0;
    virtual void set_min_value(std::optional<double> min) = 0;
    virtual std::optional<double> max_value() const = 0;
    virtual void set_max_value(std::optional<double> max) = 0;
};

//! A line on one plot: one StraightLine item, which holds the state.
class PlotLineTarget final : public LineTarget
{
    QPointer<StraightLine> m_line;

public:
    PlotLineTarget(SciQLopStraightLine* line, SciQLopPlot* plot, double position, bool movable,
                   Coordinates coordinates, Qt::Orientation orientation);
    ~PlotLineTarget() override;

    double position() const override;
    void set_position(double position) override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
    std::optional<double> min_value() const override;
    void set_min_value(std::optional<double> min) override;
    std::optional<double> max_value() const override;
    void set_max_value(std::optional<double> max) override;
};
} // namespace impl
```

`src/LineTargets.cpp`:

```cpp
#include "SciQLopPlots/Items/impl/LineTargets.hpp"

#include "SciQLopPlots/helpers.hpp"

#include <limits>

namespace impl
{
PlotLineTarget::PlotLineTarget(SciQLopStraightLine* line, SciQLopPlot* plot, double position,
                               bool movable, Coordinates coordinates, Qt::Orientation orientation)
        : m_line { new StraightLine(plot->qcp_plot(), position, movable, coordinates, orientation) }
{
    QObject::connect(m_line.data(), &StraightLine::moved, line,
                     &SciQLopStraightLine::position_changed);
    QObject::connect(m_line.data(), &QCPAbstractItem::selectionChanged, line,
                     [line](bool selected) { line->_notify_selected(selected); });
    QObject::connect(m_line.data(), &QCPAbstractItem::deleteRequested, line,
                     &SciQLopStraightLine::delete_requested);
}

PlotLineTarget::~PlotLineTarget()
{
    qcp_item_remove(m_line);
}

double PlotLineTarget::position() const
{
    return m_line ? m_line->position() : std::numeric_limits<double>::quiet_NaN();
}

void PlotLineTarget::set_position(double position)
{
    if (m_line)
        m_line->set_position(position);
}

Coordinates PlotLineTarget::coordinates() const
{
    return m_line ? m_line->coordinates() : Coordinates::Data;
}

void PlotLineTarget::set_coordinates(Coordinates coordinates)
{
    if (m_line)
        m_line->set_coordinates(coordinates);
}

bool PlotLineTarget::read_only() const
{
    return !m_line || !m_line->movable();
}

void PlotLineTarget::set_read_only(bool read_only)
{
    if (m_line)
        m_line->setMovable(!read_only);
}

QString PlotLineTarget::tool_tip() const
{
    return m_line ? m_line->tooltip() : QString();
}

void PlotLineTarget::set_tool_tip(const QString& tool_tip)
{
    if (m_line)
        m_line->setToolTip(tool_tip);
}

QColor PlotLineTarget::color() const
{
    return m_line ? m_line->color() : QColor();
}

void PlotLineTarget::set_color(const QColor& color)
{
    if (m_line)
        m_line->set_color(color);
}

double PlotLineTarget::line_width() const
{
    return m_line ? m_line->line_width() : std::numeric_limits<double>::quiet_NaN();
}

void PlotLineTarget::set_line_width(double width)
{
    if (m_line)
        m_line->set_line_width(width);
}

Qt::PenStyle PlotLineTarget::line_style() const
{
    return m_line ? m_line->line_style() : Qt::SolidLine;
}

void PlotLineTarget::set_line_style(Qt::PenStyle style)
{
    if (m_line)
        m_line->set_line_style(style);
}

bool PlotLineTarget::visible() const
{
    return m_line && m_line->visible();
}

void PlotLineTarget::set_visible(bool visible)
{
    if (!m_line)
        return;
    m_line->setVisible(visible);
    m_line->replot();
}

bool PlotLineTarget::selected() const
{
    return m_line && m_line->selected();
}

void PlotLineTarget::set_selected(bool selected)
{
    if (!m_line)
        return;
    m_line->setSelected(selected);
    m_line->replot();
}

std::optional<double> PlotLineTarget::min_value() const
{
    return m_line ? m_line->min_value() : std::nullopt;
}

void PlotLineTarget::set_min_value(std::optional<double> min)
{
    if (!m_line)
        return;
    if (min)
        m_line->set_min_value(*min);
    else
        m_line->clear_min_value();
}

std::optional<double> PlotLineTarget::max_value() const
{
    return m_line ? m_line->max_value() : std::nullopt;
}

void PlotLineTarget::set_max_value(std::optional<double> max)
{
    if (!m_line)
        return;
    if (max)
        m_line->set_max_value(*max);
    else
        m_line->clear_max_value();
}
} // namespace impl
```

- [ ] **Step 5: `SciQLopStraightLine` on its target**

In `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, add `#include <memory>`, and before `class SciQLopStraightLine`:

```cpp
namespace impl
{
class LineTarget;
class PlotLineTarget;
class PanelLineTarget;
}
```

Replace the whole `SciQLopStraightLine` class (line 151-305) with:

```cpp
/*! A straight line across a plot, or across every plot of a panel: vertical (at a key) or
    horizontal (at a value), fixed when it is made. The factories make it read-only by default. */
class SciQLopStraightLine : public SciQLopDrawnObject
{
    Q_OBJECT
    friend class impl::PlotLineTarget;
    friend class impl::PanelLineTarget;

    Qt::Orientation m_orientation;
    std::unique_ptr<impl::LineTarget> m_target;

protected:
    void _set_visible(bool visible) override;
    bool _visible() const override;
    void _set_selected(bool selected) override;
    bool _selected() const override;
    void _set_color(const QColor& color) override;
    QColor _color() const override;

public:
    /*!
     * \param plot The plot the line is drawn on
     * \param position Its key (vertical line) or value (horizontal line)
     * \param movable If the user can drag it: the inverse of read_only
     * \param coordinates Data, or Pixels from the plot area's top-left
     * \param orientation Vertical or Horizontal
     */
    SciQLopStraightLine(SciQLopPlot* plot, const double position, const bool movable = false,
                        Coordinates coordinates = Coordinates::Data,
                        Qt::Orientation orientation = Qt::Orientation::Vertical);
    ~SciQLopStraightLine() override;

    [[nodiscard]] Qt::Orientation orientation() const noexcept;
    //! The plot or the panel the line belongs to.
    [[nodiscard]] QObject* scope() const;
    //! True for the per-plot parts of a panel-scope line.
    [[nodiscard]] bool is_panel_part() const;

    void set_position(double pos);
    [[nodiscard]] double position() const;
    [[nodiscard]] Coordinates coordinates() const;
    //! Re-derives the position in \a coordinates: the line stays where it is on screen. The
    //! min/max clamps keep their numbers.
    void set_coordinates(Coordinates coordinates);

    [[nodiscard]] bool read_only() const;
    void set_read_only(bool read_only);
    //! The inverse of read_only (deprecated in Python).
    [[nodiscard]] bool movable() const;
    void set_movable(bool movable);

    [[nodiscard]] QString tool_tip() const;
    void set_tool_tip(const QString& tool_tip);
    [[nodiscard]] double line_width() const;
    void set_line_width(double width);
    [[nodiscard]] Qt::PenStyle line_style() const;
    void set_line_style(Qt::PenStyle style);

    void set_min_value(double min);
    void clear_min_value();
    void set_max_value(double max);
    void clear_max_value();

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void position_changed(double new_position);
    Q_SIGNAL void coordinates_changed(Coordinates coordinates);
    Q_SIGNAL void read_only_changed(bool read_only);
    Q_SIGNAL void delete_requested();
};
```

`SciQLopVerticalLine` and `SciQLopHorizontalLine` stay as they are.

In `src/SciQLopStraightLines.cpp`, add `#include "SciQLopPlots/Items/impl/LineTargets.hpp"`, `#include "SciQLopPlots/SciQLopPlotInterface.hpp"`, `#include <QPointer>`, and replace the old `SciQLopStraightLine::set_position` (line 138-143) with:

```cpp
SciQLopStraightLine::SciQLopStraightLine(SciQLopPlot* plot, const double position,
                                         const bool movable, Coordinates coordinates,
                                         Qt::Orientation orientation)
        : SciQLopDrawnObject { SciQLopPlots::item_plot(plot, "SciQLopStraightLine") }
        , m_orientation { orientation }
        , m_target { std::make_unique<impl::PlotLineTarget>(this, plot, position, movable,
                                                            coordinates, orientation) }
{
}

SciQLopStraightLine::~SciQLopStraightLine() = default;

Qt::Orientation SciQLopStraightLine::orientation() const noexcept
{
    return m_orientation;
}

QObject* SciQLopStraightLine::scope() const
{
    return parent();
}

bool SciQLopStraightLine::is_panel_part() const
{
    return SciQLopPlots::is_panel_part(this);
}

void SciQLopStraightLine::set_position(double pos)
{
    SCIQLOP_ON_OWNER_THREAD(set_position(pos));
    m_target->set_position(pos);
}

double SciQLopStraightLine::position() const
{
    return m_target->position();
}

Coordinates SciQLopStraightLine::coordinates() const
{
    return m_target->coordinates();
}

void SciQLopStraightLine::set_coordinates(Coordinates coordinates)
{
    SCIQLOP_ON_OWNER_THREAD(set_coordinates(coordinates));
    if (m_target->coordinates() == coordinates)
        return;
    const double before = m_target->position();
    // A part's slot may delete a panel line while its target writes.
    const QPointer<SciQLopStraightLine> alive(this);
    m_target->set_coordinates(coordinates);
    if (!alive)
        return;
    Q_EMIT coordinates_changed(coordinates);
    if (!alive)
        return;
    if (const double now = m_target->position(); now != before)
        Q_EMIT position_changed(now);
}

bool SciQLopStraightLine::read_only() const
{
    return m_target->read_only();
}

void SciQLopStraightLine::set_read_only(bool read_only)
{
    SCIQLOP_ON_OWNER_THREAD(set_read_only(read_only));
    if (m_target->read_only() == read_only)
        return;
    const QPointer<SciQLopStraightLine> alive(this);
    m_target->set_read_only(read_only);
    if (alive)
        Q_EMIT read_only_changed(read_only);
}

bool SciQLopStraightLine::movable() const
{
    return !read_only();
}

void SciQLopStraightLine::set_movable(bool movable)
{
    set_read_only(!movable);
}

QString SciQLopStraightLine::tool_tip() const
{
    return m_target->tool_tip();
}

void SciQLopStraightLine::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    m_target->set_tool_tip(tool_tip);
}

double SciQLopStraightLine::line_width() const
{
    return m_target->line_width();
}

void SciQLopStraightLine::set_line_width(double width)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_width(width));
    m_target->set_line_width(width);
}

Qt::PenStyle SciQLopStraightLine::line_style() const
{
    return m_target->line_style();
}

void SciQLopStraightLine::set_line_style(Qt::PenStyle style)
{
    SCIQLOP_ON_OWNER_THREAD(set_line_style(style));
    m_target->set_line_style(style);
}

void SciQLopStraightLine::set_min_value(double min)
{
    SCIQLOP_ON_OWNER_THREAD(set_min_value(min));
    m_target->set_min_value(min);
}

void SciQLopStraightLine::clear_min_value()
{
    SCIQLOP_ON_OWNER_THREAD(clear_min_value());
    m_target->set_min_value(std::nullopt);
}

void SciQLopStraightLine::set_max_value(double max)
{
    SCIQLOP_ON_OWNER_THREAD(set_max_value(max));
    m_target->set_max_value(max);
}

void SciQLopStraightLine::clear_max_value()
{
    SCIQLOP_ON_OWNER_THREAD(clear_max_value());
    m_target->set_max_value(std::nullopt);
}

void SciQLopStraightLine::_set_visible(bool visible)
{
    m_target->set_visible(visible);
}

bool SciQLopStraightLine::_visible() const
{
    return m_target->visible();
}

void SciQLopStraightLine::_set_selected(bool selected)
{
    m_target->set_selected(selected);
}

bool SciQLopStraightLine::_selected() const
{
    return m_target->selected();
}

void SciQLopStraightLine::_set_color(const QColor& color)
{
    m_target->set_color(color);
}

QColor SciQLopStraightLine::_color() const
{
    return m_target->color();
}
```

- [ ] **Step 6: The plot factory**

`include/SciQLopPlots/SciQLopPlotInterface.hpp`, after `span`:

```cpp
    //! A straight line at \a position: vertical (at a key) or horizontal (at a value).
    //! Read-only unless read_only=False.
    inline virtual SciQLopStraightLine* straight_line(double position, Qt::Orientation orientation,
                                                      QColor color = QColor::fromRgb(0, 0, 0),
                                                      bool read_only = true, bool visible = true,
                                                      const QString& tool_tip = "",
                                                      Coordinates coordinates = Coordinates::Data)
    {
        throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
    }
```

`include/SciQLopPlots/SciQLopPlot.hpp`, after `span`:

```cpp
    SciQLopStraightLine* straight_line(double position, Qt::Orientation orientation,
                                       QColor color = QColor::fromRgb(0, 0, 0),
                                       bool read_only = true, bool visible = true,
                                       const QString& tool_tip = "",
                                       Coordinates coordinates = Coordinates::Data) override;
```

`src/SciQLopPlot.cpp`, after `span`:

```cpp
SciQLopStraightLine* SciQLopPlot::straight_line(double position, Qt::Orientation orientation,
                                                QColor color, bool read_only, bool visible,
                                                const QString& tool_tip, Coordinates coordinates)
{
    auto* line = new SciQLopStraightLine(this, position, !read_only, coordinates, orientation);
    line->set_color(color);
    line->set_visible(visible);
    line->set_tool_tip(tool_tip);
    return line;
}
```

- [ ] **Step 7: Build files, bindings, Python**

`src/meson.build`: `'LineTargets.cpp',` after `'SciQLopStraightLines.cpp',`.

bindings.xml:
- In `SciQLopPlotInterface`, after the `span` modify-function:

  ```xml
        <modify-function signature="straight_line(double,Qt::Orientation,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
  ```
- Replace lines 1315-1321 (the `SciQLopStraightLine` object-type with its `position` property, the comment, and the two bare line object-types) with:

  ```xml
    <object-type name="SciQLopStraightLine" parent-management="yes">
    </object-type>
    <!-- The old line classes keep position as a property during Plan A (SciQLop reads it);
         SciQLopStraightLine itself has position() / set_position(). -->
    <object-type name="SciQLopVerticalLine" parent-management="yes">
        <property name="position" type="double" get="position" set="set_position" generate-getsetdef="yes"/>
    </object-type>
    <object-type name="SciQLopHorizontalLine" parent-management="yes">
        <property name="position" type="double" get="position" set="set_position" generate-getsetdef="yes"/>
    </object-type>
  ```

`SciQLopPlots/__init__.py`, `_ITEM_FACTORIES` line 379: `SciQLopPlotsBindings.SciQLopStraightLine: ("plot.straight_line", True),`.

- [ ] **Step 8: Build, run, full suite**

The header change touches a binding: if `sciqlopstraightline_wrapper.cpp` keeps the old `position` property, `rm` it and the two line wrappers, then compile again (Global Constraints). Run `tests/integration/test_straight_line.py tests/integration/test_item_factories.py tests/integration/test_drawn_contract.py tests/integration/test_line_drag_pixels.py tests/integration/test_multiplots_vertical_line.py tests/integration/test_binding_signatures.py`, then the full suite. A test reading `.position` as an attribute on a `SciQLopStraightLine` built by the deprecated constructor now gets a method: change it to `position()` and name it in the report.

- [ ] **Step 9: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopStraightLines.hpp src/SciQLopStraightLines.cpp include/SciQLopPlots/Items/impl/LineTargets.hpp src/LineTargets.cpp include/SciQLopPlots/SciQLopPlotInterface.hpp include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp src/meson.build SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_straight_line.py tests/integration/test_item_factories.py tests/integration/test_drawn_contract.py
git commit -m "feat(lines): SciQLopStraightLine on a target: read_only, coordinates, delete_requested; plot.straight_line()"
```

---

### Task 6: Line clamp getters and `tool_tip_changed`

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, `src/SciQLopStraightLines.cpp`
- Modify, branches F and F-py: `SciQLopPlots/bindings/bindings.xml`
- Test: `tests/integration/test_straight_line.py` (append)

**Interfaces:**
- Consumes: Task 5 (`SciQLopStraightLine` on `LineTarget`, whose `min_value()` / `max_value()` already return `std::optional<double>`); B5 (the ledger's branch).
- Produces, on `SciQLopStraightLine`: B5's `min_value()` / `max_value()` (None when unset in Python) and the signal `tool_tip_changed(const QString&)`, emitted once per change.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_straight_line.py`:

```python
def test_the_clamps_and_their_getters(host, orientation):
    line = make(host, orientation)
    assert (line.min_value(), line.max_value()) == (None, None)
    line.set_min_value(0.2)
    line.set_max_value(0.8)
    assert (line.min_value(), line.max_value()) == (0.2, 0.8)
    line.set_position(5.0)
    assert line.position() == 0.8
    line.clear_min_value()
    line.clear_max_value()
    assert (line.min_value(), line.max_value()) == (None, None)


def test_the_tool_tip_round_trips_and_signals_once(host, orientation):
    line = make(host, orientation)
    seen = record(line.tool_tip_changed)
    line.set_tool_tip("tip")
    line.set_tool_tip("tip")
    assert (line.tool_tip(), seen) == ("tip", ["tip"])


def test_on_tool_tip_has_a_signal(host):
    assert make(host, "vertical").on.tool_tip.signal is not None
```

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_straight_line.py`. Expected: `AttributeError` for `min_value` and `tool_tip_changed`; `.on.tool_tip.signal` is None.

- [ ] **Step 3: Implement**

`include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, in `SciQLopStraightLine`: B5's header lines before `void set_min_value(double min);`, and `Q_SIGNAL void tool_tip_changed(const QString& tool_tip);` after `read_only_changed`.

`src/SciQLopStraightLines.cpp`: B5's definitions before `set_min_value`, and `set_tool_tip` becomes:

```cpp
void SciQLopStraightLine::set_tool_tip(const QString& tool_tip)
{
    SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
    if (m_target->tool_tip() == tool_tip)
        return;
    // A part's slot may delete a panel line while its target writes.
    const QPointer<SciQLopStraightLine> alive(this);
    m_target->set_tool_tip(tool_tip);
    if (alive)
        Q_EMIT tool_tip_changed(tool_tip);
}
```

Branches F and F-py, bindings.xml: B5's `nan_is_none` template at the top, and B5's two modify-functions inside `<object-type name="SciQLopStraightLine">`.

- [ ] **Step 4: Build, run, full suite**

Compile (`rm` `sciqlopstraightline_wrapper.cpp` first if `min_value` is missing from Python). Run `tests/integration/test_straight_line.py tests/integration/test_binding_signatures.py`, then the full suite.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopStraightLines.hpp src/SciQLopStraightLines.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_straight_line.py
git commit -m "feat(lines): min_value()/max_value() getters (None when unset) and tool_tip_changed"
```

---

### Task 7: The line at panel scope, and `panel.straight_line()`

**Files:**
- Modify: `include/SciQLopPlots/Items/impl/LineTargets.hpp`, `src/LineTargets.cpp` (`LineState`, `PanelLineTarget`)
- Modify: `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, `src/SciQLopStraightLines.cpp` (the panel constructor)
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Test: `tests/integration/test_straight_line.py`, `tests/integration/test_drawn_contract.py`

**Interfaces:**
- Consumes: Task 4 (`PanelParts`, `panel_host`, `check_removable_here`), Task 5 (`LineTarget`, `clamp_line_position`), Task 6 (the clamp getters the tests read on parts).
- Produces:
  - `struct impl::LineState`, `class impl::PanelLineTarget`.
  - `SciQLopStraightLine(SciQLopMultiPlotPanel* panel, double position, Qt::Orientation orientation, const QColor& color, bool read_only, bool visible, const QString& tool_tip, Coordinates coordinates)`. Its parts are plain plot-scope `SciQLopStraightLine`s, marked.
  - `SciQLopMultiPlotPanel::straight_line(...)` (the plot's signature and defaults), `SciQLopMultiPlotPanel::remove(SciQLopStraightLine*)`.

- [ ] **Step 1: Write the failing tests**

In `tests/integration/test_straight_line.py`: `SCOPES = ["plot", "panel"]`, and append:

```python
def _panel(qtbot, n=2):
    panel = scoped_host("panel", qtbot)
    for _ in range(n - 2):
        panel.plot(*_xy())
    process_events()
    return panel


def _xy():
    import numpy as np
    x = np.linspace(0.0, 10.0, 200)
    return x, np.sin(x)


def _parts(panel):
    return [part for plot in plots_of(panel) for part in plot.findChildren(SciQLopStraightLine)
            if part.is_panel_part()]


def test_a_panel_line_has_one_plain_marked_part_per_plot(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    parts = _parts(panel)
    assert [type(part) for part in parts] == [SciQLopStraightLine] * 2
    assert [part.scope() for part in parts] == plots_of(panel)
    assert (line.scope(), line.is_panel_part()) == (panel, False)


def test_the_line_is_the_panels_child_and_outlives_its_python_reference(qtbot):
    from conftest import collect
    panel = _panel(qtbot)
    panel.straight_line(5.0, Qt.Orientation.Vertical)
    collect()
    assert len([c for c in panel.children() if isinstance(c, SciQLopStraightLine)]) == 1
    assert len(_parts(panel)) == 2


def test_an_edit_on_a_part_moves_the_line_and_the_other_part_once(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical, read_only=False)
    first, second = _parts(panel)
    seen = record(line.position_changed)
    first.set_position(3.0)
    assert (seen, line.position(), second.position()) == ([3.0], 3.0, 3.0)


def test_selecting_a_part_selects_the_line_once(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    first, second = _parts(panel)
    seen = record(line.selection_changed)
    first.set_selected(True)
    assert (line.selected(), second.selected(), seen) == (True, True, [True])


def test_the_panel_line_clamps_like_its_parts(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(3.0, Qt.Orientation.Vertical)
    line.set_min_value(2.0)
    line.set_max_value(4.0)
    line.set_position(9.0)
    assert [line.position()] + [part.position() for part in _parts(panel)] == [4.0] * 3
    assert [(part.min_value(), part.max_value()) for part in _parts(panel)] == [(2.0, 4.0)] * 2


def test_reordering_plots_keeps_one_line_part_per_plot(qtbot):
    """Review Focus 2, for lines."""
    panel = _panel(qtbot, 3)
    panel.straight_line(5.0, Qt.Orientation.Vertical)
    panel.move_plot(0, 2)
    process_events()
    assert len(_parts(panel)) == 3


def test_a_slot_removing_the_line_during_a_part_edit(qtbot):
    """Review Focus 5, for lines: the emitting part dies with the line."""
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical, read_only=False)
    first, _ = _parts(panel)
    line.position_changed.connect(lambda *_: panel.remove(line))
    first.set_position(3.0)
    process_events()
    assert not shiboken6.isValid(line)
    assert _parts(panel) == []


def test_panel_remove_deletes_the_line_and_its_parts(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    parts = _parts(panel)
    panel.remove(line)
    assert not shiboken6.isValid(line)
    assert not any(shiboken6.isValid(part) for part in parts)


def test_plot_remove_refuses_a_part_and_the_panel_line(qtbot):
    panel = _panel(qtbot)
    line = panel.straight_line(5.0, Qt.Orientation.Vertical)
    with pytest.raises(ValueError, match="part of a panel-wide object"):
        panel.plot_at(0).remove(_parts(panel)[0])
    with pytest.raises(ValueError, match=r"panel\.remove"):
        panel.plot_at(0).remove(line)
```

In `tests/integration/test_drawn_contract.py`, the `"text and lines"` row gets `"SciQLopStraightLine[panel]": lambda h: h.panel.straight_line(2.0, Qt.Orientation.Vertical),`.

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_straight_line.py tests/integration/test_drawn_contract.py`. Expected: the panel cases fail with `AttributeError: 'SciQLopMultiPlotPanel' object has no attribute 'straight_line'`.

- [ ] **Step 3: `LineState` and `PanelLineTarget`**

`include/SciQLopPlots/Items/impl/LineTargets.hpp`: `#include "SciQLopPlots/Items/impl/PanelParts.hpp"`, and after `PlotLineTarget`:

```cpp
//! Everything a panel line keeps and writes to its parts.
struct LineState
{
    double position = 0.;
    Qt::Orientation orientation = Qt::Vertical;
    Coordinates coordinates = Coordinates::Data;
    bool read_only = true;
    QString tool_tip;
    QColor color = QColor(0, 0, 0);
    double line_width = 1.0;
    Qt::PenStyle line_style = Qt::SolidLine;
    bool visible = true;
    bool selected = false;
    std::optional<double> min_value;
    std::optional<double> max_value;
};

//! A line on every SciQLopPlot of a panel: the state, and one plot-scope part per plot.
class PanelLineTarget final : public LineTarget
{
    SciQLopStraightLine* m_line;
    LineState m_state;
    PanelParts<SciQLopStraightLine> m_parts; // last: destroyed first, while the state still exists

    SciQLopStraightLine* make_part(SciQLopPlot* plot);
    void follow(SciQLopStraightLine* part);
    template <typename T, typename Setter>
    void write(T& field, const T& value, Setter setter);

public:
    PanelLineTarget(SciQLopStraightLine* line, SciQLopMultiPlotPanel* panel, LineState state);

    double position() const override;
    void set_position(double position) override;
    Coordinates coordinates() const override;
    void set_coordinates(Coordinates coordinates) override;
    bool read_only() const override;
    void set_read_only(bool read_only) override;
    QString tool_tip() const override;
    void set_tool_tip(const QString& tool_tip) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
    double line_width() const override;
    void set_line_width(double width) override;
    Qt::PenStyle line_style() const override;
    void set_line_style(Qt::PenStyle style) override;
    bool visible() const override;
    void set_visible(bool visible) override;
    bool selected() const override;
    void set_selected(bool selected) override;
    std::optional<double> min_value() const override;
    void set_min_value(std::optional<double> min) override;
    std::optional<double> max_value() const override;
    void set_max_value(std::optional<double> max) override;
};
```

`src/LineTargets.cpp`, add `#include <utility>` and, after the `PlotLineTarget` definitions:

```cpp
PanelLineTarget::PanelLineTarget(SciQLopStraightLine* line, SciQLopMultiPlotPanel* panel,
                                 LineState state)
        : m_line { line }
        , m_state { std::move(state) }
        , m_parts { line, [this](SciQLopPlot* plot) { return make_part(plot); } }
{
    m_parts.follow(panel);
}

SciQLopStraightLine* PanelLineTarget::make_part(SciQLopPlot* plot)
{
    auto* part = new SciQLopStraightLine(plot, m_state.position, !m_state.read_only,
                                         m_state.coordinates, m_state.orientation);
    if (m_state.min_value)
        part->set_min_value(*m_state.min_value);
    if (m_state.max_value)
        part->set_max_value(*m_state.max_value);
    part->set_color(m_state.color);
    part->set_line_width(m_state.line_width);
    part->set_line_style(m_state.line_style);
    part->set_tool_tip(m_state.tool_tip);
    part->set_visible(m_state.visible);
    part->set_selected(m_state.selected);
    follow(part);
    return part;
}

// A part's own edits (a drag, a click, the Delete key) flow up; the line writes them back out to
// every part, whose echoes writing() swallows.
void PanelLineTarget::follow(SciQLopStraightLine* part)
{
    QObject::connect(part, &SciQLopStraightLine::position_changed, m_line,
                     [this](double position)
                     {
                         if (!m_parts.writing())
                             set_position(position);
                     });
    QObject::connect(part, &SciQLopDrawnObject::selection_changed, m_line,
                     [this](bool selected)
                     {
                         if (!m_parts.writing())
                             m_line->set_selected(selected);
                     });
    QObject::connect(part, &SciQLopStraightLine::delete_requested, m_line,
                     &SciQLopStraightLine::delete_requested);
}

template <typename T, typename Setter>
void PanelLineTarget::write(T& field, const T& value, Setter setter)
{
    field = value;
    (void)m_parts.write([value, setter](SciQLopStraightLine* part) { (part->*setter)(value); });
}

double PanelLineTarget::position() const
{
    return m_state.position;
}

void PanelLineTarget::set_position(double position)
{
    position = clamp_line_position(position, m_state.min_value, m_state.max_value);
    if (position == m_state.position)
        return;
    m_state.position = position;
    if (m_parts.write([position](SciQLopStraightLine* part) { part->set_position(position); }))
        Q_EMIT m_line->position_changed(position);
}

Coordinates PanelLineTarget::coordinates() const
{
    return m_state.coordinates;
}

// simplify: each part converts on its own plot, then the first part's number becomes the line's
// and every part takes it; with no plot yet the number stays. A position per plot would lift it.
void PanelLineTarget::set_coordinates(Coordinates coordinates)
{
    m_state.coordinates = coordinates;
    if (!m_parts.write([coordinates](SciQLopStraightLine* part)
                       { part->set_coordinates(coordinates); }))
        return;
    auto* first = m_parts.first();
    if (!first)
        return;
    m_state.position = first->position();
    (void)m_parts.write([position = m_state.position](SciQLopStraightLine* part)
                        { part->set_position(position); });
}

bool PanelLineTarget::read_only() const
{
    return m_state.read_only;
}

void PanelLineTarget::set_read_only(bool read_only)
{
    write(m_state.read_only, read_only, &SciQLopStraightLine::set_read_only);
}

QString PanelLineTarget::tool_tip() const
{
    return m_state.tool_tip;
}

void PanelLineTarget::set_tool_tip(const QString& tool_tip)
{
    write(m_state.tool_tip, tool_tip, &SciQLopStraightLine::set_tool_tip);
}

QColor PanelLineTarget::color() const
{
    return m_state.color;
}

void PanelLineTarget::set_color(const QColor& color)
{
    write(m_state.color, color, &SciQLopDrawnObject::set_color);
}

double PanelLineTarget::line_width() const
{
    return m_state.line_width;
}

void PanelLineTarget::set_line_width(double width)
{
    write(m_state.line_width, width, &SciQLopStraightLine::set_line_width);
}

Qt::PenStyle PanelLineTarget::line_style() const
{
    return m_state.line_style;
}

void PanelLineTarget::set_line_style(Qt::PenStyle style)
{
    write(m_state.line_style, style, &SciQLopStraightLine::set_line_style);
}

bool PanelLineTarget::visible() const
{
    return m_state.visible;
}

void PanelLineTarget::set_visible(bool visible)
{
    write(m_state.visible, visible, &SciQLopDrawnObject::set_visible);
}

bool PanelLineTarget::selected() const
{
    return m_state.selected;
}

void PanelLineTarget::set_selected(bool selected)
{
    write(m_state.selected, selected, &SciQLopDrawnObject::set_selected);
}

std::optional<double> PanelLineTarget::min_value() const
{
    return m_state.min_value;
}

void PanelLineTarget::set_min_value(std::optional<double> min)
{
    m_state.min_value = min;
    (void)m_parts.write(
        [min](SciQLopStraightLine* part)
        {
            if (min)
                part->set_min_value(*min);
            else
                part->clear_min_value();
        });
}

std::optional<double> PanelLineTarget::max_value() const
{
    return m_state.max_value;
}

void PanelLineTarget::set_max_value(std::optional<double> max)
{
    m_state.max_value = max;
    (void)m_parts.write(
        [max](SciQLopStraightLine* part)
        {
            if (max)
                part->set_max_value(*max);
            else
                part->clear_max_value();
        });
}
```

`position_changed` is a public signal: emitting it from the target is a plain call.

- [ ] **Step 4: The panel constructor**

`include/SciQLopPlots/Items/SciQLopStraightLines.hpp`: `class SciQLopMultiPlotPanel;` next to the `impl` forward declarations, and after the plot constructor:

```cpp
    //! At panel scope: one plot-scope part on each SciQLopPlot of \a panel.
    SciQLopStraightLine(SciQLopMultiPlotPanel* panel, double position, Qt::Orientation orientation,
                        const QColor& color, bool read_only, bool visible,
                        const QString& tool_tip, Coordinates coordinates);
```

`src/SciQLopStraightLines.cpp`, after the plot constructor:

```cpp
SciQLopStraightLine::SciQLopStraightLine(SciQLopMultiPlotPanel* panel, double position,
                                         Qt::Orientation orientation, const QColor& color,
                                         bool read_only, bool visible, const QString& tool_tip,
                                         Coordinates coordinates)
        : SciQLopDrawnObject { impl::panel_host(panel, "SciQLopStraightLine") }
        , m_orientation { orientation }
        , m_target { std::make_unique<impl::PanelLineTarget>(
              this, panel,
              impl::LineState { .position = position,
                                .orientation = orientation,
                                .coordinates = coordinates,
                                .read_only = read_only,
                                .tool_tip = tool_tip,
                                .color = color,
                                .visible = visible }) }
{
}
```

- [ ] **Step 5: The panel factory and `remove`**

`include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`: `class SciQLopStraightLine;` with the other forward declarations, and after `remove(SciQLopSpan*)`:

```cpp
    //! A straight line on every plot of the panel, plot.straight_line's arguments and defaults.
    SciQLopStraightLine* straight_line(double position, Qt::Orientation orientation,
                                       QColor color = QColor::fromRgb(0, 0, 0),
                                       bool read_only = true, bool visible = true,
                                       const QString& tool_tip = "",
                                       Coordinates coordinates = Coordinates::Data);
    void remove(SciQLopStraightLine* line);
```

`src/SciQLopMultiPlotPanel.cpp`, after `remove(SciQLopSpan*)` (the file already includes SciQLopStraightLines.hpp through MultiPlotsVLine.hpp):

```cpp
SciQLopStraightLine* SciQLopMultiPlotPanel::straight_line(double position,
                                                          Qt::Orientation orientation,
                                                          QColor color, bool read_only,
                                                          bool visible, const QString& tool_tip,
                                                          Coordinates coordinates)
{
    SciQLopPlots::require_owner_thread(this, "SciQLopMultiPlotPanel::straight_line");
    return new SciQLopStraightLine(this, position, orientation, color, read_only, visible,
                                   tool_tip, coordinates);
}

void SciQLopMultiPlotPanel::remove(SciQLopStraightLine* line)
{
    check_removable_here(this, line);
    SCIQLOP_ON_OWNER_THREAD_WITH(line, remove(line));
    delete line;
}
```

- [ ] **Step 6: Bindings**

In `<object-type name="SciQLopStraightLine">`:

```xml
        <!-- The panel constructor is for the factory (panel.straight_line) only. -->
        <modify-function signature="SciQLopStraightLine(SciQLopMultiPlotPanel*,double,Qt::Orientation,const QColor&amp;,bool,bool,const QString&amp;,Coordinates)" remove="all"/>
```

In `<object-type name="SciQLopMultiPlotPanel">`, after `remove(SciQLopSpan*)`:

```xml
        <modify-function signature="straight_line(double,Qt::Orientation,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="remove(SciQLopStraightLine*)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_valueerror"/>
            </inject-code>
        </modify-function>
```

- [ ] **Step 7: Build, run, full suite**

Compile. Run `tests/integration/test_straight_line.py tests/integration/test_drawn_contract.py tests/integration/test_panel_span.py tests/integration/test_binding_signatures.py`, then the full suite.

- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/Items/impl/LineTargets.hpp src/LineTargets.cpp include/SciQLopPlots/Items/SciQLopStraightLines.hpp src/SciQLopStraightLines.cpp include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_straight_line.py tests/integration/test_drawn_contract.py
git commit -m "feat(lines): panel-scope lines (panel.straight_line): one part per plot, edits flow up once"
```

---

### Task 8: The Python rows of the new classes: `.on.tooltip`, `selectionChanged`

They must exist before Task 9 switches the factories, or `test_on_convention.py` and `test_drawn_spans.py` lose them on the objects the factories return.

**Files:**
- Modify: `SciQLopPlots/__init__.py` (`_ON_EXCEPTIONS` ~line 304-320, `_DEPRECATED_ALIASES` ~line 629-635)
- Test: `tests/integration/test_span.py`, `tests/integration/test_straight_line.py` (append)

**Interfaces:**
- Consumes: `tool_tip_changed` on `SciQLopSpan` (Task 2) and on `SciQLopStraightLine` (Task 6).
- Produces: `.on.tooltip` on every `SciQLopSpan` and `SciQLopStraightLine` (signal `tool_tip_changed`); `SciQLopSpan.selectionChanged`, the same signal as `selection_changed`. Task 13 removes the rows these make redundant.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_span.py`:

```python
def test_on_tooltip_is_observable(host, kind):
    span = make(host, kind)
    seen = []
    pipeline = span.on.tooltip >> (lambda event: seen.append(event.value))
    span.set_tool_tip("tip")
    assert seen == ["tip"]
    pipeline.disconnect()


def test_selection_changed_keeps_its_old_alias():
    assert SciQLopSpan.selectionChanged is SciQLopSpan.selection_changed
```

Append to `tests/integration/test_straight_line.py`:

```python
def test_on_tooltip_is_observable(host, orientation):
    line = make(host, orientation)
    seen = []
    pipeline = line.on.tooltip >> (lambda event: seen.append(event.value))
    line.set_tool_tip("tip")
    assert seen == ["tip"]
    pipeline.disconnect()
```

- [ ] **Step 2: Run them and watch them fail**

Run both files. Expected: `AttributeError: ... has no observable property 'tooltip'` (the name has no getter of its own) and `AttributeError: ... selectionChanged`.

- [ ] **Step 3: Implement**

`SciQLopPlots/__init__.py`, in `_ON_EXCEPTIONS`, after the `SciQLopVerticalSpan` row (line 317-319):

```python
    (SciQLopPlotsBindings.SciQLopSpan, "tooltip", dict(
        signal_name="tool_tip_changed", getter_name="tool_tip", setter_name="set_tool_tip",
        property_type="value")),
    (SciQLopPlotsBindings.SciQLopStraightLine, "tooltip", dict(
        signal_name="tool_tip_changed", getter_name="tool_tip", setter_name="set_tool_tip",
        property_type="value")),
```

In `_DEPRECATED_ALIASES` (line 629): `SciQLopPlotsBindings.SciQLopSpan: {"selectionChanged": "selection_changed"},`.

- [ ] **Step 4: Run, full suite, commit**

Run the two files, then the full suite.

```bash
git add SciQLopPlots/__init__.py tests/integration/test_span.py tests/integration/test_straight_line.py
git commit -m "feat(on): .on.tooltip on spans and lines, selectionChanged on SciQLopSpan"
```

---

### Fallout rules (Tasks 9 and 10)

Tasks 9 and 10 change what existing factories return, so existing call sites break. Each task fixes the call sites its own change broke, by these rules, and leaves the full suite green. Task 14 sweeps what is left.
- A test **about an old class** (its isinstance, its properties, `set_pixel_range`, the border selection, `MultiPlotsVSpanCollection`, `selectionChanged` on V/H/R) keeps the old class, built through its old entry point: the deprecated constructor inside `with pytest.warns(DeprecationWarning):`, or `MultiPlotsVSpanCollection(panel).create_span(...)` (`.data()` for the object). Everything else moves to the new classes.
- `plot.vertical_line(p, <bool>, <coords>)` → `plot.vertical_line(p, read_only=not <bool>, coordinates=<coords>)`; the same for `horizontal_line`. A `True` that meant "draggable" becomes `read_only=False`.
- A line from a factory: `line.position` → `line.position()`; `line.position = v` → `line.set_position(v)`.
- A span from a factory: `span.pixel_range` → `span.pixel_range()`.
- `plot.rectangular_span(k, v, ...)` → `plot.span(key_range=k, value_range=v, ...)`, unless the test is about `rectangular_span`.
- The old constructors named their range `horizontal_range` / `vertical_range`; the shorthands call it `range`, and it is positional-only. A call moved from a constructor to a shorthand passes the range positionally: `SciQLopVerticalSpan(plot, horizontal_range=r)` → `plot.vertical_span(r)`. `grep -rn "horizontal_range=\|vertical_range=" <files>` finds them.
- Type checks on a factory result: `SciQLopVerticalSpan` / `SciQLopHorizontalSpan` / `SciQLopRectangularSpan` / `MultiPlotsVerticalSpan` → `SciQLopSpan` (plus `kind()` where the kind mattered); `SciQLopVerticalLine` / `SciQLopHorizontalLine` / `MultiPlotsVerticalLine` → `SciQLopStraightLine` (plus `orientation()`).
- Panel properties on a factory panel object → methods: `span.visible = x` → `span.set_visible(x)`, `span.selected = True` → `span.set_selected(True)`, `span.color = c` → `span.set_color(c)`, `span.range` → `span.range()`, `span.read_only` → `span.read_only()`, `span.tooltip` → `span.tool_tip()`, `span.id` → `span.id()`; for lines also `line.position`, `line.line_width`.
- The parts on a plot: `plot.findChildren(SciQLopVerticalSpan)` → `[s for s in plot.findChildren(SciQLopSpan) if s.is_panel_part()]`, and `SciQLopVerticalLine` → `SciQLopStraightLine` the same way. Keep the `[0]` / unpacking the test had.
- A panel line a test drags: the factory makes it read-only now, so pass `read_only=False`.
- Find the sites with `grep -rnE "(vertical|horizontal)_line\(|(vertical|horizontal|rectangular)_span\(|\.position\b|pixel_range\b|findChildren\((SciQLop|MultiPlots)" <files>`, then let the full suite name the rest.
- List every file you changed in the report.

---

### Task 9: The plot factories return the new classes

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (the five factory virtuals ~line 535-558 become shorthands; forward declarations line 45-49)
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp` (delete the overrides, line 557-561), `src/SciQLopPlot.cpp` (delete their definitions, ~line 1597-1620)
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopPlotInterface`, line 524-552)
- Modify: `SciQLopPlots/__init__.py` (after the `_ITEM_FACTORIES` loop, ~line 409)
- Test: `tests/integration/test_item_factories.py`, `tests/integration/test_drawn_contract.py`, and the fallout (rules above)

**Interfaces:**
- Consumes: `span` (Task 2), `straight_line` (Task 5), B6.
- Produces, non-virtual on `SciQLopPlotInterface`:
  ```cpp
  SciQLopSpan* vertical_span(const SciQLopPlotRange& range, QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "", Coordinates coordinates = Coordinates::Data, const QString& id = "");
  SciQLopSpan* horizontal_span(/* the same */);
  SciQLopSpan* rectangular_span(const SciQLopPlotRange& key_range, const SciQLopPlotRange& value_range, /* the rest as vertical_span */);
  SciQLopStraightLine* vertical_line(double position, QColor color = QColor::fromRgb(0, 0, 0), bool read_only = true, bool visible = true, const QString& tool_tip = "", Coordinates coordinates = Coordinates::Data);
  SciQLopStraightLine* horizontal_line(/* the same */);
  ```
- Produces, Python: `rectangular_span(...)` warns `rectangular_span(...) is deprecated: use plot.span(...)`; `vertical_line` / `horizontal_line` take `movable=` with `movable=... is deprecated: use plot.vertical_line(..., read_only=...)` (and `horizontal_line`), and raise TypeError for a bool second argument; `_DEPRECATED_CALLS` (a table of `(class, method) -> message`) and `_deprecated_call(message, method)`, which Task 13 reuses.

- [ ] **Step 1: Write the failing tests**

In `tests/integration/test_item_factories.py`:
- Imports: `import inspect`, `from shibokensupport.signature import get_signature`, `from SciQLopPlots import SciQLopPlotsBindings, SciQLopSpan, SpanKind`.
- `FACTORIES`: the line and span rows become

  ```python
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopStraightLine),
    "horizontal_line": (lambda p: p.horizontal_line(1.0), SciQLopStraightLine),
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopSpan),
    "span": (lambda p: p.span(key_range=R(1.0, 2.0), value_range=R(1.0, 2.0)), SciQLopSpan),
  ```
  and the `"rectangular_span"` row goes (it warns now).
- `NO_PLOT_FACTORY` gains, with one reason string for the five: `"old name (Plan A): only its deprecated constructor builds it"` for `SciQLopVerticalLine`, `SciQLopHorizontalLine`, `SciQLopVerticalSpan`, `SciQLopHorizontalSpan`, `SciQLopRectangularSpan`.
- Append:

```python
SPAN_SHORTHANDS = {
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SpanKind.Vertical),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SpanKind.Horizontal),
}


@pytest.mark.parametrize("name", SPAN_SHORTHANDS)
def test_span_shorthands_return_plain_spans(plot, name):
    make, kind = SPAN_SHORTHANDS[name]
    span = make(plot)
    assert (type(span), span.kind(), span.range()) == (SciQLopSpan, kind, R(1.0, 2.0))


def test_rectangular_span_warns_and_returns_a_plain_span(plot):
    with pytest.warns(DeprecationWarning,
                      match=r"rectangular_span\(\.\.\.\) is deprecated: use plot\.span\(\.\.\.\)") as record:
        span = plot.rectangular_span(R(1.0, 2.0), R(3.0, 4.0))
    assert record[0].filename == __file__
    assert (type(span), span.kind(), span.key_range(), span.value_range()) == \
        (SciQLopSpan, SpanKind.Rectangular, R(1.0, 2.0), R(3.0, 4.0))


@pytest.mark.parametrize("name, orientation", [("vertical_line", Qt.Orientation.Vertical),
                                               ("horizontal_line", Qt.Orientation.Horizontal)])
def test_line_shorthands_return_plain_read_only_black_lines(plot, name, orientation):
    line = getattr(plot, name)(1.0)
    assert (type(line), line.orientation(), line.read_only(), line.color()) == \
        (SciQLopStraightLine, orientation, True, QColor(0, 0, 0))


@pytest.mark.parametrize("name", ["vertical_line", "horizontal_line"])
@pytest.mark.parametrize("movable", [True, False])
def test_the_movable_keyword_warns_and_inverts(plot, name, movable):
    with pytest.warns(DeprecationWarning,
                      match=rf"movable=\.\.\. is deprecated: use plot\.{name}\(\.\.\., read_only=\.\.\.\)") as record:
        line = getattr(plot, name)(1.0, movable=movable)
    assert record[0].filename == __file__
    assert line.read_only() is (not movable)


@pytest.mark.parametrize("name", ["vertical_line", "horizontal_line"])
def test_a_bool_second_argument_raises_instead_of_becoming_a_colour(plot, name):
    """Review Focus 1: QColor(True) is a valid colour, so the old movable slot would pass silently."""
    with pytest.raises(TypeError, match="read_only"):
        getattr(plot, name)(1.0, True)


def _defaults(func):
    signature = get_signature(inspect.unwrap(func))
    return {name: p.default for name, p in signature.parameters.items() if p.default is not p.empty}


@pytest.mark.parametrize("base, shorthands", [
    ("span", ("vertical_span", "horizontal_span", "rectangular_span")),
    ("straight_line", ("vertical_line", "horizontal_line")),
])
def test_the_plot_shorthands_take_the_base_factory_defaults(base, shorthands):
    cls = SciQLopPlotsBindings.SciQLopPlotInterface
    defaults = _defaults(getattr(cls, base))
    for name in shorthands:
        own = _defaults(getattr(cls, name))
        assert own == {key: defaults[key] for key in own}, name
```

In `tests/integration/test_drawn_contract.py`: a helper next to `_ranged`, and the old-class builders of the `"span"` and `"text and lines"` rows switch to it (the factories no longer build those classes):

```python
def _old(cls, *args):
    """An old-name object, through its deprecated constructor: the factories no longer make one."""
    with pytest.warns(DeprecationWarning):
        return cls(*args)
```
```python
        "SciQLopVerticalLine": lambda h: _old(SciQLopVerticalLine, h.plot, 1.0),
        "SciQLopHorizontalLine": lambda h: _old(SciQLopHorizontalLine, h.plot, 0.5),
```
```python
        "SciQLopVerticalSpan": lambda h: _old(SciQLopVerticalSpan, h.plot, R(1.0, 2.0)),
        "SciQLopHorizontalSpan": lambda h: _old(SciQLopHorizontalSpan, h.plot, R(1.0, 2.0)),
        "SciQLopRectangularSpan": lambda h: _old(SciQLopRectangularSpan, h.plot, R(1.0, 2.0), R(1.0, 2.0)),
```

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_item_factories.py`. Expected: the shorthand tests fail on the type (`SciQLopVerticalSpan` is not `SciQLopSpan`), the movable and bool tests fail (no warning; `vertical_line(1.0, True)` is accepted), and the defaults test fails (`vertical_span` has no `id`, the lines have `movable`).

- [ ] **Step 3: The shorthands**

In `include/SciQLopPlots/SciQLopPlotInterface.hpp`, delete the forward declarations of `SciQLopVerticalLine`, `SciQLopHorizontalLine`, `SciQLopVerticalSpan`, `SciQLopHorizontalSpan`, `SciQLopRectangularSpan` (line 45-49), and replace the five virtuals `vertical_line` … `rectangular_span` (~line 535-558) with these non-virtual members (branch O spelling; F and F-py: B6):

```cpp
    //! span(key_range=range, ...): the defaults are span()'s.
    SciQLopSpan* vertical_span(const SciQLopPlotRange& range,
                               QColor color = QColor::fromRgb(100, 100, 100, 100),
                               bool read_only = false, bool visible = true,
                               const QString& tool_tip = "",
                               Coordinates coordinates = Coordinates::Data, const QString& id = "")
    {
        return span(range, std::nullopt, color, read_only, visible, tool_tip, coordinates, id);
    }

    //! span(value_range=range, ...): the defaults are span()'s.
    SciQLopSpan* horizontal_span(const SciQLopPlotRange& range,
                                 QColor color = QColor::fromRgb(100, 100, 100, 100),
                                 bool read_only = false, bool visible = true,
                                 const QString& tool_tip = "",
                                 Coordinates coordinates = Coordinates::Data,
                                 const QString& id = "")
    {
        return span(std::nullopt, range, color, read_only, visible, tool_tip, coordinates, id);
    }

    //! Deprecated in Python: span(key_range=..., value_range=...).
    SciQLopSpan* rectangular_span(const SciQLopPlotRange& key_range,
                                  const SciQLopPlotRange& value_range,
                                  QColor color = QColor::fromRgb(100, 100, 100, 100),
                                  bool read_only = false, bool visible = true,
                                  const QString& tool_tip = "",
                                  Coordinates coordinates = Coordinates::Data,
                                  const QString& id = "")
    {
        return span(key_range, value_range, color, read_only, visible, tool_tip, coordinates, id);
    }

    //! straight_line(position, Qt::Vertical, ...): the defaults are straight_line()'s.
    SciQLopStraightLine* vertical_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                       bool read_only = true, bool visible = true,
                                       const QString& tool_tip = "",
                                       Coordinates coordinates = Coordinates::Data)
    {
        return straight_line(position, Qt::Vertical, color, read_only, visible, tool_tip,
                             coordinates);
    }

    //! straight_line(position, Qt::Horizontal, ...): the defaults are straight_line()'s.
    SciQLopStraightLine* horizontal_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                         bool read_only = true, bool visible = true,
                                         const QString& tool_tip = "",
                                         Coordinates coordinates = Coordinates::Data)
    {
        return straight_line(position, Qt::Horizontal, color, read_only, visible, tool_tip,
                             coordinates);
    }
```

C++ needs the defaults written again; `test_the_plot_shorthands_take_the_base_factory_defaults` keeps them equal to `span()`'s and `straight_line()`'s, which is what "defined once" means for Python callers.

Delete the five overrides from `include/SciQLopPlots/SciQLopPlot.hpp` (line 557-561) and their definitions from `src/SciQLopPlot.cpp` (~line 1597-1620).

- [ ] **Step 4: Bindings**

In `<object-type name="SciQLopPlotInterface">`, replace the five modify-functions `vertical_line(double,bool,Coordinates)` … `rectangular_span(SciQLopPlotRange,SciQLopPlotRange,QColor,bool,bool,const QString&amp;)` (line 524-552) with:

```xml
        <modify-function signature="vertical_line(double,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="horizontal_line(double,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="vertical_span(const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="horizontal_span(const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="rectangular_span(const SciQLopPlotRange&amp;,const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
```

- [ ] **Step 5: Python wrappers**

In `SciQLopPlots/__init__.py`, after the `for _item_cls, (_factory, _needs_plot) in _ITEM_FACTORIES.items():` loop:

```python
# The plot line shorthands took movable second (5.6); color and read_only replace it. A bool there
# would silently become a QColor (QColor(int)), so it raises; movable= still works, inverted.
def _line_shorthand(name, method):
    message = f"movable=... is deprecated: use plot.{name}(..., read_only=...)"

    @functools.wraps(method)
    def wrapper(self, position, *args, **kwargs):
        if args and isinstance(args[0], bool):
            raise TypeError(f"{name}(position, color, ...): the second argument is the colour; "
                            "pass read_only=... to choose whether the user can drag the line")
        if "movable" in kwargs:
            warnings.warn(message, DeprecationWarning, stacklevel=2)
            kwargs["read_only"] = not kwargs.pop("movable")
        return method(self, position, *args, **kwargs)
    return wrapper


for _name in ("vertical_line", "horizontal_line"):
    setattr(SciQLopPlotsBindings.SciQLopPlotInterface, _name,
            _line_shorthand(_name, vars(SciQLopPlotsBindings.SciQLopPlotInterface)[_name]))


def _deprecated_call(message, method):
    @functools.wraps(method)
    def wrapper(self, *args, **kwargs):
        warnings.warn(message, DeprecationWarning, stacklevel=2)
        return method(self, *args, **kwargs)
    return wrapper


# (class, method) -> message. Plain methods only: never a Qt slot (a wrapper cuts its connections).
_DEPRECATED_CALLS = {
    (SciQLopPlotsBindings.SciQLopPlotInterface, "rectangular_span"):
        "rectangular_span(...) is deprecated: use plot.span(...)",
}
for (_cls, _name), _message in _DEPRECATED_CALLS.items():
    setattr(_cls, _name, _deprecated_call(_message, vars(_cls)[_name]))
```

- [ ] **Step 6: Build, run the new tests**

Compile. If `sciqlopplot_wrapper.cpp` still has the old overrides, `rm` it and `sciqlopplotinterface_wrapper.cpp`, then compile again. Run `tests/integration/test_item_factories.py tests/integration/test_drawn_contract.py tests/integration/test_binding_signatures.py`. Expected: all pass.

- [ ] **Step 7: Fix the fallout, full suite**

Run the full suite. Fix every failure the plot factories caused, by the **Fallout rules**. Expect at least: `test_line_drag_pixels.py` (positional `True`, `.position`), `test_items_api.py`, `test_item_signals_and_pixel.py` (`.position`; the `set_pixel_range` round trip is about the old class: build `SciQLopVerticalSpan(plot, ...)` under `pytest.warns`), `test_items_style_api.py`, `test_audit_2026_04_28_reproducers.py`, `test_plot_remove.py:139` (`vertical_line(1.0, True)`), `test_on_convention.py` (`test_exception_tooltip_has_no_signal`: the span's tooltip has a signal now, rename it `test_exception_tooltip_has_a_signal` and assert `span.on.tooltip.signal is not None`), `test_drawn_spans.py` (its `SPAN_FACTORIES` build the old classes through their constructors now), `test_thread_guards.py`, `tests/fuzzing/item_actions.py`. Then the full suite again, until green.

- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_item_factories.py tests/integration/test_drawn_contract.py <every fallout file, by path>
git commit -m "feat(factories): plot span and line shorthands return SciQLopSpan / SciQLopStraightLine; rectangular_span and movable= deprecated"
```

---

### Task 10: The panel factories return the new classes

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` (line 261-268), `src/SciQLopMultiPlotPanel.cpp` (line 176-191)
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopMultiPlotPanel`, line 1733-1743)
- Modify: `docs/user-guide.md` (the panel span bullet ~line 831, the panel cursor snippet and the paragraph after it ~line 837-857): guide snippets run in the suite
- Test: `tests/integration/test_item_factories.py`, `tests/integration/test_binding_signatures.py` (`test_default_panel_item_color`), `tests/integration/test_drawn_contract.py`, and the fallout

**Interfaces:**
- Consumes: `SciQLopMultiPlotPanel::span` (Task 4), `straight_line` (Task 7), B6.
- Produces on `SciQLopMultiPlotPanel`: `vertical_span`, `horizontal_span` (→ `SciQLopSpan*`) and `vertical_line`, `horizontal_line` (→ `SciQLopStraightLine*`), with the plot shorthands' signatures. No `rectangular_span` on panels. The old `MultiPlotsVerticalSpan*` / `MultiPlotsVerticalLine*` returning factories are gone. `remove(MultiPlotsVerticalSpan*)` and `remove(MultiPlotsVerticalLine*)` stay until Tasks 11 and 12.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_item_factories.py`:

```python
PANEL_SHORTHANDS = {
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopSpan),
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopStraightLine),
    "horizontal_line": (lambda p: p.horizontal_line(0.5), SciQLopStraightLine),
}


@pytest.mark.parametrize("name", PANEL_SHORTHANDS)
def test_panel_shorthands_return_the_plain_classes_at_panel_scope(panel, name):
    make, cls = PANEL_SHORTHANDS[name]
    item = make(panel)
    assert type(item) is cls and item.scope() is panel


def test_the_panel_defaults_are_the_plot_ones(panel):
    span, line = panel.vertical_span(R(1.0, 2.0)), panel.vertical_line(1.0)
    assert (span.color(), span.read_only()) == (QColor(100, 100, 100, 100), False)
    assert (line.color(), line.read_only()) == (QColor(0, 0, 0), True)


@pytest.mark.parametrize("base, shorthands", [
    ("span", ("vertical_span", "horizontal_span")),
    ("straight_line", ("vertical_line", "horizontal_line")),
])
def test_the_panel_shorthands_take_the_base_factory_defaults(base, shorthands):
    from SciQLopPlots import SciQLopMultiPlotPanel
    defaults = _defaults(getattr(SciQLopMultiPlotPanel, base))
    for name in shorthands:
        own = _defaults(getattr(SciQLopMultiPlotPanel, name))
        assert own == {key: defaults[key] for key in own}, name


def test_panels_have_no_rectangular_span(panel):
    assert not hasattr(panel, "rectangular_span")
```

In `tests/integration/test_binding_signatures.py`, `test_default_panel_item_color` becomes two tests (the factory defaults changed by design; the collection keeps the old ones):

```python
@pytest.mark.parametrize("make, expected", [
    (lambda panel: panel.vertical_span(_RANGE).color(), _SPAN_GREY),
    (lambda panel: panel.vertical_line(0.5).color(), QColor(0, 0, 0)),
], ids=["span", "line"])
def test_default_panel_item_color(panel, make, expected):
    assert make(panel) == expected


def test_default_collection_span_color(panel):
    from SciQLopPlots import MultiPlotsVSpanCollection
    collection = MultiPlotsVSpanCollection(panel)
    assert collection.create_span(_RANGE).color == _LINE_GREY
```

In `tests/integration/test_drawn_contract.py`, the `"panel span"` and `"panel line"` rows build the old classes through `_old` (Task 9):

```python
        "MultiPlotsVerticalSpan": lambda h: _old(MultiPlotsVerticalSpan, h.panel, R(1.0, 2.0)),
```
```python
        "MultiPlotsVerticalLine": lambda h: _old(MultiPlotsVerticalLine, h.panel, 2.0),
```

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_item_factories.py tests/integration/test_binding_signatures.py`. Expected: the panel tests fail on the types (`MultiPlotsVerticalSpan`), on `horizontal_span` (missing) and on the defaults.

- [ ] **Step 3: The panel shorthands**

In `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, replace the `vertical_span` and `vertical_line` declarations (line 261-268) with:

```cpp
    //! span(key_range=range, ...) on every plot: the defaults are span()'s.
    SciQLopSpan* vertical_span(const SciQLopPlotRange& range,
                               QColor color = QColor::fromRgb(100, 100, 100, 100),
                               bool read_only = false, bool visible = true,
                               const QString& tool_tip = "",
                               Coordinates coordinates = Coordinates::Data, const QString& id = "");
    //! span(value_range=range, ...) on every plot: the defaults are span()'s.
    SciQLopSpan* horizontal_span(const SciQLopPlotRange& range,
                                 QColor color = QColor::fromRgb(100, 100, 100, 100),
                                 bool read_only = false, bool visible = true,
                                 const QString& tool_tip = "",
                                 Coordinates coordinates = Coordinates::Data,
                                 const QString& id = "");
    //! straight_line(position, Qt::Vertical, ...) on every plot: the defaults are straight_line()'s.
    SciQLopStraightLine* vertical_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                       bool read_only = true, bool visible = true,
                                       const QString& tool_tip = "",
                                       Coordinates coordinates = Coordinates::Data);
    //! straight_line(position, Qt::Horizontal, ...) on every plot.
    SciQLopStraightLine* horizontal_line(double position, QColor color = QColor::fromRgb(0, 0, 0),
                                         bool read_only = true, bool visible = true,
                                         const QString& tool_tip = "",
                                         Coordinates coordinates = Coordinates::Data);
```

In `src/SciQLopMultiPlotPanel.cpp`, replace the two old definitions (line 176-191) with (branch O spelling; F and F-py: B6):

```cpp
SciQLopSpan* SciQLopMultiPlotPanel::vertical_span(const SciQLopPlotRange& range, QColor color,
                                                  bool read_only, bool visible,
                                                  const QString& tool_tip,
                                                  Coordinates coordinates, const QString& id)
{
    return span(range, std::nullopt, color, read_only, visible, tool_tip, coordinates, id);
}

SciQLopSpan* SciQLopMultiPlotPanel::horizontal_span(const SciQLopPlotRange& range, QColor color,
                                                    bool read_only, bool visible,
                                                    const QString& tool_tip,
                                                    Coordinates coordinates, const QString& id)
{
    return span(std::nullopt, range, color, read_only, visible, tool_tip, coordinates, id);
}

SciQLopStraightLine* SciQLopMultiPlotPanel::vertical_line(double position, QColor color,
                                                          bool read_only, bool visible,
                                                          const QString& tool_tip,
                                                          Coordinates coordinates)
{
    return straight_line(position, Qt::Vertical, color, read_only, visible, tool_tip,
                         coordinates);
}

SciQLopStraightLine* SciQLopMultiPlotPanel::horizontal_line(double position, QColor color,
                                                            bool read_only, bool visible,
                                                            const QString& tool_tip,
                                                            Coordinates coordinates)
{
    return straight_line(position, Qt::Horizontal, color, read_only, visible, tool_tip,
                         coordinates);
}
```

The thread check is in `span` and `straight_line`.

- [ ] **Step 4: Bindings**

In `<object-type name="SciQLopMultiPlotPanel">`, replace the `vertical_span(SciQLopPlotRange,QColor,bool,bool,const QString&amp;,const QString&amp;)` and `vertical_line(double,QColor,bool,bool,const QString&amp;)` modify-functions (line 1733-1743) with:

```xml
        <modify-function signature="vertical_span(const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="horizontal_span(const SciQLopPlotRange&amp;,QColor,bool,bool,const QString&amp;,Coordinates,const QString&amp;)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopSpan*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="vertical_line(double,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="horizontal_line(double,QColor,bool,bool,const QString&amp;,Coordinates)">
            <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopStraightLine*"/></insert-template>
            </inject-code>
        </modify-function>
```

- [ ] **Step 5: The guide's panel examples**

They run in `tests/integration/test_user_guide.py`. In `docs/user-guide.md`:
- The bullet at ~line 831 becomes: ``- On a panel, `panel.vertical_span(range, color, ...)` draws one span across every plot. That's handy for marking an event in time. Its state is read and set with methods: `span.set_selected(True)`, `span.visible()`, `span.color()`, `span.read_only()`, `span.id()`.``
- The cursor snippet's last three lines (~line 849-852) become:

  ```python
  cursor = panel.vertical_line(2.0, read_only=False, tool_tip="cursor")
  cursor.position_changed.connect(lambda t: print("cursor at", t))
  cursor.set_position(5.0)          # moves it on every plot
  cursor.set_color(QColor("red"))   # also set_line_width, set_visible, set_read_only, set_tool_tip
  ```
- The paragraph after the image (~line 857) becomes: ``Dragging the line on any plot moves it on all of them, and `position_changed` fires once per move. Lines are read-only by default: `read_only=False` lets the user drag one.``

- [ ] **Step 6: Build, run, fallout, full suite**

Compile (`rm` `sciqlopmultiplotpanel_wrapper.cpp` first if it keeps the old signatures). Run `tests/integration/test_item_factories.py tests/integration/test_binding_signatures.py tests/integration/test_drawn_contract.py tests/integration/test_user_guide.py`. Then the full suite, and fix the fallout by the **Fallout rules**. Expect at least: `test_panel_api.py`, `test_drawn_panel_objects.py` (`test_panel_span_state_stays_attributes` and `test_a_panel_line_colour_signals_once` are about the old classes' properties: build them with `_old`-style deprecated constructors; the others move to methods and to `SciQLopSpan` parts), `test_multiplots_vertical_line.py` (about the old class: build it with its deprecated constructor), `test_plot_remove.py` (panel parts via `SciQLopSpan` / `SciQLopStraightLine` with `is_panel_part()`), `test_item_self_delete.py`, `test_api_hygiene.py` (`test_a_multi_plot_span_keeps_a_time_range`: `span.range()`), `test_thread_guards.py`, `test_pipeline_integration.py`, `tests/fuzzing/item_actions.py`, `tests/fuzzing/fuzzer_actions.py`, `tests/fuzzing/test_sciqlop_workflows.py`. Then the full suite again, until green.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp SciQLopPlots/bindings/bindings.xml docs/user-guide.md tests/integration/test_item_factories.py tests/integration/test_binding_signatures.py tests/integration/test_drawn_contract.py <every fallout file, by path>
git commit -m "feat(factories): panel span and line shorthands return SciQLopSpan / SciQLopStraightLine; panel.horizontal_span, panel.horizontal_line"
```

---

### Task 11: The old span names become subclasses of `SciQLopSpan`

**Files:**
- Modify (rewrite): `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`, `SciQLopHorizontalSpan.hpp`, `SciQLopRectangularSpan.hpp`
- Delete: `src/SciQLopVerticalSpan.cpp`, `src/SciQLopHorizontalSpan.cpp`, `src/SciQLopRectangularSpan.cpp`, `src/SciQLopSpanBase.cpp`, `include/SciQLopPlots/Items/SciQLopSpanBase.hpp` (`impl::SpanBase` stays: `PlotSpanTarget` uses it)
- Modify: `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp` (the `MultiPlotsVerticalSpan` class, line 32-157), `src/MultiPlotsVSpan.cpp` (line 24-140)
- Modify: `include/SciQLopPlots/Items/SciQLopSpan.hpp`, `src/SciQLopSpan.cpp`, `include/SciQLopPlots/Items/impl/SpanTargets.hpp`, `src/SpanTargets.cpp` (legacy parts, `_set_pixel_range`, `to_range`)
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp` (delete `remove(MultiPlotsVerticalSpan*)`)
- Modify: `src/meson.build` (delete the four sources, line 166-169)
- Modify: `SciQLopPlots/bindings/bindings.xml`, `SciQLopPlots/__init__.py` (`_SNAKE_CASE_ALIASES`, line 612-621)
- Test: `tests/integration/test_old_span_line_names.py` (create), `tests/integration/test_drawn_contract.py`, `tests/integration/test_api_hygiene.py`, `tests/integration/test_item_factories.py`

**Interfaces:**
- Consumes: Tasks 2, 3, 4.
- Produces:
  - `SciQLopVerticalSpan(plot, horizontal_range, color, read_only, visible, tool_tip, coordinates)`, `SciQLopHorizontalSpan(plot, vertical_range, color, read_only, visible, tool_tip)`, `SciQLopRectangularSpan(plot, key_range, value_range, color, read_only, visible, tool_tip)`: the old signatures and defaults, subclasses of `SciQLopSpan` that fix the kind. `SciQLopVerticalSpan` keeps `set_pixel_range` and the `pixel_range` property; V and H keep `select_lower_border` / `select_upper_border` public, as they are bound and public today, and their signals. None is a `SciQLopItemInterface` any more.
  - `MultiPlotsVerticalSpan(panel, horizontal_range, color = QColor::fromRgb(100, 100, 100), read_only = false, visible = true, tool_tip = "", id = "")`: a panel-scope `SciQLopSpan` whose parts are `SciQLopVerticalSpan`s; it keeps `show()`, `hide()`, the collection's culling (`set_in_range`, friend `MultiPlotsVSpanCollection`) and its Python properties. It loses `addObject` / `removeObject` / `replotAll` / `updatePlotList` and their snake_case aliases.
  - On `SciQLopSpan`: the panel constructor gains a last parameter `bool legacy_parts = false`; protected `void _set_pixel_range(impl::SpanAxis axis, const SciQLopPlotRange& pixels)`.
  - On `SpanTarget`: `virtual SciQLopPlotRange to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const = 0` (pixels from the plot area, in the span's coordinates).

The border-selection mirror of the old panel span (its private `select_lower_border` / `select_upper_border` slots, which pushed one part's border selection to the others) is not carried over: the old panel class can have no behaviour of its own, and nothing fires a part's border selection from user input. The parts keep the API, so `test_drawn_panel_objects.py`'s border test still runs.

- [ ] **Step 1: Write the failing tests**

`tests/integration/test_old_span_line_names.py`:

```python
"""The old span and line names during Plan A: subclasses that only fix kind and scope (5.6)."""
import numpy as np
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalSpan, MultiPlotsVSpanCollection, SciQLopHorizontalSpan,
                          SciQLopPlotRange as R, SciQLopRangeItemInterface, SciQLopRectangularSpan,
                          SciQLopSpan, SciQLopVerticalSpan, SpanKind)
from conftest import plots_of, process_events

X = np.linspace(0.0, 10.0, 50)
OLD_PLOT_SPANS = {
    SciQLopVerticalSpan: (lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)), SpanKind.Vertical),
    SciQLopHorizontalSpan: (lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)), SpanKind.Horizontal),
    SciQLopRectangularSpan: (lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
                             SpanKind.Rectangular),
}


def _old(make, host):
    with pytest.warns(DeprecationWarning):
        return make(host)


def _with_plots(panel):
    panel.plot(X, np.sin(X))
    panel.plot(X, np.cos(X))
    process_events()
    return panel


@pytest.mark.parametrize("cls", OLD_PLOT_SPANS, ids=lambda cls: cls.__name__)
def test_an_old_plot_span_is_a_span_of_its_kind_and_no_other_class(plot, cls):
    make, kind = OLD_PLOT_SPANS[cls]
    span = _old(make, plot)
    assert isinstance(span, SciQLopSpan) and isinstance(span, cls)
    assert (span.kind(), span.scope()) == (kind, plot)
    others = (set(OLD_PLOT_SPANS) - {cls}) | {MultiPlotsVerticalSpan, SciQLopRangeItemInterface}
    assert not any(isinstance(span, other) for other in others)


def test_factory_spans_are_never_an_old_class(plot, panel):
    spans = [plot.vertical_span(R(1.0, 2.0)), plot.horizontal_span(R(1.0, 2.0)),
             panel.vertical_span(R(1.0, 2.0))]
    old = tuple(OLD_PLOT_SPANS) + (MultiPlotsVerticalSpan,)
    assert [type(span) for span in spans] == [SciQLopSpan] * 3
    assert not any(isinstance(span, old) for span in spans)


def test_an_old_panel_span_has_old_class_parts(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0)), panel)
    assert isinstance(span, SciQLopSpan) and (span.kind(), span.scope()) == (SpanKind.Vertical, panel)
    parts = [p for plot in plots_of(panel) for p in plot.findChildren(SciQLopVerticalSpan)]
    assert len(parts) == 2
    assert all(p.is_panel_part() and not isinstance(p, MultiPlotsVerticalSpan) for p in parts)


def test_the_old_panel_span_keeps_its_properties(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0), id="event"), panel)
    span.range = R(3.0, 4.0)
    span.selected = True
    span.color = QColor("red")
    span.visible = False
    span.tooltip = "tip"
    span.read_only = True
    assert (span.range, span.selected, span.color, span.visible, span.tooltip, span.read_only,
            span.id) == (R(3.0, 4.0), True, QColor("red"), False, "tip", True, "event")
    with pytest.raises(TypeError):
        span.range()


def test_the_old_panel_span_keeps_show_and_hide(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0)), panel)
    span.hide()
    assert span.visible is False
    span.show()
    assert span.visible is True


def test_the_old_panel_span_lost_its_plumbing():
    assert not any(hasattr(MultiPlotsVerticalSpan, name) for name in
                   ("addObject", "add_object", "removeObject", "replotAll", "update_plot_list"))


def test_the_collection_still_makes_old_panel_spans(panel):
    _with_plots(panel)
    collection = MultiPlotsVSpanCollection(panel)
    assert isinstance(collection.create_span(R(1.0, 2.0)).data(), MultiPlotsVerticalSpan)


def test_the_old_vertical_span_keeps_pixel_range_and_border_selection(qtbot, plot):
    plot.resize(800, 600)
    plot.show()
    qtbot.waitExposed(plot)
    plot.x_axis().set_range(R(0.0, 100.0))
    process_events()
    span = _old(lambda p: SciQLopVerticalSpan(p, R(20.0, 80.0)), plot)
    process_events()
    span.set_pixel_range(span.pixel_range)
    assert span.range()[0] == pytest.approx(20.0, abs=0.1)
    seen = []
    span.lower_border_selection_changed.connect(seen.append)
    span.select_lower_border(True)
    assert (seen, span.selected()) == ([True], True)
    for cls in (SciQLopVerticalSpan, SciQLopHorizontalSpan):
        assert all(callable(getattr(cls, name, None))
                   for name in ("select_lower_border", "select_upper_border"))


@pytest.mark.parametrize("cls", OLD_PLOT_SPANS, ids=lambda cls: cls.__name__)
def test_on_position_no_longer_resolves_on_an_old_span(plot, cls):
    span = _old(OLD_PLOT_SPANS[cls][0], plot)
    with pytest.raises(AttributeError):
        span.on.position
```

In `tests/integration/test_drawn_contract.py`, the `"span"`, `"new span"` and `"panel span"` rows become one row (the old classes are subclasses of `SciQLopSpan` now, so its closure reaches them):

```python
    Row("span", ("SciQLopSpan",), {
        "SciQLopSpan": lambda h: h.plot.span(key_range=R(1.0, 2.0)),
        "SciQLopSpan[panel]": lambda h: h.panel.span(key_range=R(1.0, 2.0)),
        "SciQLopVerticalSpan": lambda h: _old(SciQLopVerticalSpan, h.plot, R(1.0, 2.0)),
        "SciQLopHorizontalSpan": lambda h: _old(SciQLopHorizontalSpan, h.plot, R(1.0, 2.0)),
        "SciQLopRectangularSpan": lambda h: _old(SciQLopRectangularSpan, h.plot, R(1.0, 2.0), R(1.0, 2.0)),
        "MultiPlotsVerticalSpan": lambda h: _old(MultiPlotsVerticalSpan, h.panel, R(1.0, 2.0)),
    }, ALL, builder_properties={"MultiPlotsVerticalSpan": frozenset({"visible", "selected", "color"})}),
```

In `tests/integration/test_api_hygiene.py`, delete the two `MultiPlotsVerticalSpan` rows of `TestSnakeCaseAliases.test_methods_have_both_names` (line ~74-75): the new test file pins that they are gone.

In `tests/integration/test_item_factories.py`, delete the three span entries from `NO_PLOT_FACTORY` (they are no longer `SciQLopItemInterface` subclasses, so the walk does not reach them).

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py`. Expected: the isinstance tests fail (`SciQLopVerticalSpan` is not a `SciQLopSpan`), the plumbing test fails (`addObject` exists), `.on.position` resolves, and the contract walk reports V/H/R and `MultiPlotsVerticalSpan` in two rows or none.

- [ ] **Step 3: `to_range`, `_set_pixel_range`, legacy parts**

`include/SciQLopPlots/Items/impl/SpanTargets.hpp`:
- In `SpanTarget`, after `pixel_range`: `//! \a pixels (from the plot area) in the span's coordinates.` / `virtual SciQLopPlotRange to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const = 0;`, and the matching `override` declaration in both targets.
- `PanelSpanTarget`: a member `bool m_legacy_parts;` declared before `m_state`, and a last constructor parameter `bool legacy_parts`.

`src/SpanTargets.cpp`: `#include "SciQLopPlots/Items/SciQLopVerticalSpan.hpp"`, and:

```cpp
SciQLopPlotRange PlotSpanTarget::to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const
{
    return m_item ? from_pixels(edges_of(m_item.data(), axis), m_coordinates, pixels) : pixels;
}

SciQLopPlotRange PanelSpanTarget::to_range(SpanAxis axis, const SciQLopPlotRange& pixels) const
{
    auto* part = m_parts.first();
    return part ? part->m_target->to_range(axis, pixels) : pixels;
}
```

`PanelSpanTarget`'s constructor initialises `m_legacy_parts { legacy_parts }`, and `make_part` starts with:

```cpp
    // The old panel class keeps old-class parts (Plan A): SciQLop finds them by that class.
    auto* part = m_legacy_parts
        ? static_cast<SciQLopSpan*>(new SciQLopVerticalSpan(plot, m_state.key_range, m_state.color,
                                                            m_state.read_only, m_state.visible,
                                                            m_state.tool_tip, m_state.coordinates))
        : new SciQLopSpan(plot, m_kind, m_state.key_range, m_state.value_range, m_state.color,
                          m_state.read_only, m_state.visible, m_state.tool_tip,
                          m_state.coordinates, m_span->id());
```
(the rest of `make_part` unchanged).

`include/SciQLopPlots/Items/SciQLopSpan.hpp`:
- The panel constructor's last parameter: `const QString& id = "", bool legacy_parts = false);` with the doc line `//! \a legacy_parts: parts are SciQLopVerticalSpans, for the old MultiPlotsVerticalSpan (Plan A).`
- In the `protected:` section:

  ```cpp
    //! The old SciQLopVerticalSpan's set_pixel_range: \a pixels from the plot area (Plan B removes it).
    void _set_pixel_range(impl::SpanAxis axis, const SciQLopPlotRange& pixels);
  ```

`src/SciQLopSpan.cpp`: the panel constructor takes `bool legacy_parts` and passes it as `PanelSpanTarget`'s last argument; and:

```cpp
void SciQLopSpan::_set_pixel_range(SpanAxis axis, const SciQLopPlotRange& pixels)
{
    SCIQLOP_ON_OWNER_THREAD(_set_pixel_range(axis, pixels));
    m_target->set_range(axis, m_target->to_range(axis, pixels));
}
```

- [ ] **Step 4: The three plot-scope classes**

`include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`, everything after the licence header:

```cpp
#pragma once

#include "SciQLopPlots/Items/SciQLopSpan.hpp"

#include <QPointer>

/*!
 * \brief The old name of a plot's vertical span: a SciQLopSpan bounding the key axis.
 *
 * Built by its deprecated constructor and as the parts of MultiPlotsVerticalSpan; the factories
 * return plain SciQLopSpans. It keeps the old API SciQLop may call: set_pixel_range and the
 * border selection. Plan B removes it (docs/plans/2026-10-25-span-line-unification-design.md).
 */
class SciQLopVerticalSpan : public SciQLopSpan
{
    Q_OBJECT

public:
    SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
                        QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                        bool visible = true, const QString& tool_tip = "",
                        Coordinates coordinates = Coordinates::Data)
            : SciQLopSpan(plot, SpanKind::Vertical, horizontal_range, SciQLopPlotRange(), color,
                          read_only, visible, tool_tip, coordinates)
    {
    }

    //! Old API: the range in pixels from the plot area's left edge (Plan B removes it).
    void set_pixel_range(const SciQLopPlotRange& pixels)
    {
        _set_pixel_range(impl::SpanAxis::Key, pixels);
    }

    //! Old API, public and bound today; never fired by user input (NeoQCP does not call it).
    //! Plan B removes it.
    void select_lower_border(bool selected)
    {
        const QPointer<SciQLopVerticalSpan> alive(this);
        set_selected(selected);
        if (alive)
            Q_EMIT lower_border_selection_changed(selected);
    }

    void select_upper_border(bool selected)
    {
        const QPointer<SciQLopVerticalSpan> alive(this);
        set_selected(selected);
        if (alive)
            Q_EMIT upper_border_selection_changed(selected);
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void lower_border_selection_changed(bool);
    Q_SIGNAL void upper_border_selection_changed(bool);
};
```

`include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp`, the same shape: class `SciQLopHorizontalSpan`, doc "bounding the value axis", the old constructor below first under `public:`, then the two **public** border methods and the two signals (with `QPointer<SciQLopHorizontalSpan>`), no `set_pixel_range`:

```cpp
    SciQLopHorizontalSpan(SciQLopPlot* plot, SciQLopPlotRange vertical_range,
                          QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                          bool visible = true, const QString& tool_tip = "")
            : SciQLopSpan(plot, SpanKind::Horizontal, SciQLopPlotRange(), vertical_range, color,
                          read_only, visible, tool_tip, Coordinates::Data)
    {
    }
```

`include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp`:

```cpp
#pragma once

#include "SciQLopPlots/Items/SciQLopSpan.hpp"

/*! The old name of a plot's rectangular span: a SciQLopSpan bounding both axes. Built by its
    deprecated constructor; the factories return plain SciQLopSpans. Plan B removes it. */
class SciQLopRectangularSpan : public SciQLopSpan
{
    Q_OBJECT

public:
    SciQLopRectangularSpan(SciQLopPlot* plot, SciQLopPlotRange key_range,
                           SciQLopPlotRange value_range,
                           QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false,
                           bool visible = true, const QString& tool_tip = "")
            : SciQLopSpan(plot, SpanKind::Rectangular, key_range, value_range, color, read_only,
                          visible, tool_tip, Coordinates::Data)
    {
    }
};
```

Delete the five files listed under **Files** (`git rm`), and the four `.cpp` lines from `src/meson.build` (`'SciQLopSpanBase.cpp'`, `'SciQLopVerticalSpan.cpp'`, `'SciQLopHorizontalSpan.cpp'`, `'SciQLopRectangularSpan.cpp'`). The three headers stay in the moc list. `grep -rn "SciQLopSpanBase" include src` must print nothing.

- [ ] **Step 5: `MultiPlotsVerticalSpan`**

In `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp`, replace the class (line 32-157) with:

```cpp
/*!
 * \brief The old name of a panel-wide vertical span: a SciQLopSpan at panel scope whose parts are
 * SciQLopVerticalSpans.
 *
 * Built by its deprecated constructor, MultiPlotsVSpanCollection::create_span and interactive
 * creation (span_created); the factories return plain SciQLopSpans. It keeps the collection's
 * range culling and show()/hide(); its Python properties are in bindings.xml. Plan B removes it.
 */
class MultiPlotsVerticalSpan : public SciQLopSpan
{
    Q_OBJECT
    friend class MultiPlotsVSpanCollection;

    bool m_shown = true; // the user's choice; the collection's culling hides on top of it
    bool m_in_range = true;

    //! Out-of-range culling, kept apart from set_visible: a span the user hid stays hidden in range.
    void set_in_range(bool in_range);

protected:
    bool _visible() const override { return m_shown && m_in_range; }
    void _set_visible(bool visible) override;
    // Needed by set_visible: a hide while culled reads as unchanged (the span is already hidden)
    // but must be remembered. It is class-wide because apply_change has no per-setter switch;
    // colour and selection re-applying when unchanged is harmless.
    bool _always_apply() const override { return true; }

public:
    MultiPlotsVerticalSpan(SciQLopMultiPlotPanel* panel, SciQLopPlotRange horizontal_range,
                           QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "", QString id = "")
            : SciQLopSpan(panel, SpanKind::Vertical, horizontal_range, SciQLopPlotRange(), color,
                          read_only, visible, tool_tip, Coordinates::Data, id, true)
            , m_shown(visible)
    {
    }

    inline void show() { set_visible(true); }

    inline void hide() { set_visible(false); }
};
```

`#include "SciQLopMultiPlotObject.hpp"` stays: the collection uses it.

In `src/MultiPlotsVSpan.cpp`, replace every `MultiPlotsVerticalSpan::` definition (line 24-140) with:

```cpp
void MultiPlotsVerticalSpan::_set_visible(bool visible)
{
    const QPointer<MultiPlotsVerticalSpan> alive(this);
    SciQLopSpan::_set_visible(visible && m_in_range);
    if (alive)
        m_shown = visible;
}

void MultiPlotsVerticalSpan::set_in_range(bool in_range)
{
    SCIQLOP_ON_OWNER_THREAD(set_in_range(in_range));
    m_in_range = in_range;
    const bool effective = _visible();
    const QPointer<MultiPlotsVerticalSpan> alive(this);
    SciQLopSpan::_set_visible(effective);
    if (alive)
        _notify_visible(effective);
}
```

The collection's code (from `MultiPlotsVSpanCollection::panel()` on) does not change: it uses `range()`, `id()`, `set_in_range` and the constructor, which all still exist.

In `SciQLopMultiPlotPanel`, delete `void remove(MultiPlotsVerticalSpan* span);` and its definition: `remove(SciQLopSpan*)` takes it now. The forward declaration of `MultiPlotsVerticalSpan` stays (`span_created`).

- [ ] **Step 6: Bindings and Python**

bindings.xml:
- `SciQLopVerticalSpan` (line 1301-1311): keep the constructor's `modify-function` with its renames; delete `<modify-function signature="pixel_range() const" rename="_pixel_range"/>`; keep the `pixel_range` property, with the comment `<!-- Old API (Plan A): a property on this class; SciQLopSpan has pixel_range(). -->`.
- `SciQLopHorizontalSpan`, `SciQLopRectangularSpan`: unchanged (bare).
- `SciQLopSpan`: the panel constructor's `remove="all"` signature ends with `,const QString&amp;,bool)`.
- `MultiPlotsVerticalSpan` (line 1895-1922): delete the three `rename` modify-functions (`range() const`, `read_only() const`, `id() const`: they are not declared in this class any more, and must not be restored); keep the constructor renames and the seven properties. Each property now names a base-class method and shadows it, as `color` already did.
- `SciQLopMultiPlotPanel`: delete the `remove(MultiPlotsVerticalSpan*)` modify-function (line 1745-1749).

`SciQLopPlots/__init__.py`, `_SNAKE_CASE_ALIASES` (line 612-621): drop `SciQLopPlotsBindings.MultiPlotsVerticalSpan` from the tuple, and make the comment say `# MultiPlotsVerticalLine overrides addObject/removeObject: it needs its own alias.`

- [ ] **Step 7: Reconfigure, build, run, full suite**

Sources were removed: reconfigure, then compile. `rm` the wrappers of the four span classes and of `MultiPlotsVerticalSpan` first if they keep stale code. Run `tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py tests/integration/test_panel_api.py tests/integration/test_drawn_panel_objects.py tests/integration/test_drawn_spans.py tests/integration/test_api_hygiene.py tests/integration/test_binding_signatures.py`, then the full suite. Read `wasm/sciqlopplots_js.cpp:815-862` once: it must still compile against the new `MultiPlotsVerticalSpan` (constructor, `set_range`, `range()`, the three signals). Say so in the report; it is not built here.

- [ ] **Step 8: Commit**

```bash
git rm src/SciQLopVerticalSpan.cpp src/SciQLopHorizontalSpan.cpp src/SciQLopRectangularSpan.cpp src/SciQLopSpanBase.cpp include/SciQLopPlots/Items/SciQLopSpanBase.hpp
git add include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp src/MultiPlotsVSpan.cpp include/SciQLopPlots/Items/SciQLopSpan.hpp src/SciQLopSpan.cpp include/SciQLopPlots/Items/impl/SpanTargets.hpp src/SpanTargets.cpp include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp src/meson.build SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py tests/integration/test_api_hygiene.py tests/integration/test_item_factories.py
git commit -m "refactor(span): the old span classes are SciQLopSpan subclasses that fix kind and scope"
```

---

### Task 12: `MultiPlotsVerticalLine` becomes a subclass of `SciQLopStraightLine`

**Files:**
- Modify (rewrite): `include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp`
- Delete: `src/MultiPlotsVLine.cpp`
- Modify: `include/SciQLopPlots/Items/SciQLopStraightLines.hpp`, `src/SciQLopStraightLines.cpp`, `include/SciQLopPlots/Items/impl/LineTargets.hpp`, `src/LineTargets.cpp` (legacy parts)
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp` (delete `remove(MultiPlotsVerticalLine*)`)
- Modify: `src/meson.build` (delete `'MultiPlotsVLine.cpp',` line 156), `SciQLopPlots/bindings/bindings.xml`, `SciQLopPlots/__init__.py`
- Test: `tests/integration/test_old_span_line_names.py`, `tests/integration/test_drawn_contract.py`, `tests/integration/test_api_hygiene.py`

**Interfaces:**
- Consumes: Tasks 5, 6, 7.
- Produces:
  - `MultiPlotsVerticalLine(panel, position, color = QColor::fromRgb(100, 100, 100), read_only = false, visible = true, tool_tip = "")`: a panel-scope vertical `SciQLopStraightLine` whose parts are `SciQLopVerticalLine`s. Grey and draggable by default, as before; its Python properties stay. It loses the plumbing methods.
  - The `SciQLopStraightLine` panel constructor gains a last parameter `bool legacy_parts = false`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_old_span_line_names.py` (add `from PySide6.QtCore import Qt` and `MultiPlotsVerticalLine, SciQLopStraightLine, SciQLopVerticalLine` to the imports):

```python
def test_an_old_panel_line_has_old_class_parts(panel):
    _with_plots(panel)
    line = _old(lambda p: MultiPlotsVerticalLine(p, 2.0), panel)
    assert isinstance(line, SciQLopStraightLine)
    assert (line.orientation(), line.scope()) == (Qt.Orientation.Vertical, panel)
    parts = [p for plot in plots_of(panel) for p in plot.findChildren(SciQLopVerticalLine)]
    assert len(parts) == 2
    assert all(p.is_panel_part() and not isinstance(p, MultiPlotsVerticalLine) for p in parts)


def test_the_old_panel_line_keeps_its_defaults_and_properties(panel):
    _with_plots(panel)
    line = _old(lambda p: MultiPlotsVerticalLine(p, 2.0), panel)
    assert (line.color, line.read_only) == (QColor(100, 100, 100), False)
    line.position = 4.0
    line.color = QColor("red")
    line.line_width = 3.0
    line.visible = False
    line.selected = True
    line.tooltip = "tip"
    line.read_only = True
    assert (line.position, line.color, line.line_width, line.visible, line.selected, line.tooltip,
            line.read_only) == (4.0, QColor("red"), 3.0, False, True, "tip", True)
    with pytest.raises(TypeError):
        line.position()


def test_the_old_panel_line_lost_its_plumbing():
    assert not any(hasattr(MultiPlotsVerticalLine, name) for name in
                   ("addObject", "add_object", "removeObject", "replotAll", "update_plot_list"))
```

`tests/integration/test_drawn_contract.py`: the `"panel line"` row merges into `"text and lines"`, whose closure now reaches `MultiPlotsVerticalLine`:

```python
        "MultiPlotsVerticalLine": lambda h: _old(MultiPlotsVerticalLine, h.panel, 2.0),
```
added to the `"text and lines"` builders, with `builder_properties={"MultiPlotsVerticalLine": frozenset({"visible", "selected", "color"})}`; delete the `"panel line"` row.

`tests/integration/test_api_hygiene.py`: delete the two `MultiPlotsVerticalLine` rows of `test_methods_have_both_names`.

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py`. Expected: `MultiPlotsVerticalLine` is not a `SciQLopStraightLine`; it still has `addObject`; the contract walk complains about the row.

- [ ] **Step 3: Legacy parts for lines**

`LineTargets.hpp`: `PanelLineTarget` gets `bool m_legacy_parts;` (before `m_state`) and a last constructor parameter `bool legacy_parts`. `LineTargets.cpp`: initialise it, and `make_part` starts with:

```cpp
    // The old panel line keeps old-class parts (Plan A): SciQLop finds them by that class.
    auto* part = m_legacy_parts
        ? static_cast<SciQLopStraightLine*>(new SciQLopVerticalLine(
              plot, m_state.position, !m_state.read_only, m_state.coordinates))
        : new SciQLopStraightLine(plot, m_state.position, !m_state.read_only, m_state.coordinates,
                                  m_state.orientation);
```

`SciQLopStraightLines.hpp`: the panel constructor's last parameter `Coordinates coordinates, bool legacy_parts = false);`, doc `//! \a legacy_parts: parts are SciQLopVerticalLines, for the old MultiPlotsVerticalLine (Plan A).`; `.cpp` passes it on.

- [ ] **Step 4: `MultiPlotsVerticalLine`**

`include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp`, everything after the licence header:

```cpp
#pragma once

#include "../Items/SciQLopStraightLines.hpp"
#include "SciQLopMultiPlotPanel.hpp"

/*! The old name of a panel-wide vertical line: a SciQLopStraightLine at panel scope whose parts
    are SciQLopVerticalLines. Grey and draggable by default, as before; its Python properties are
    in bindings.xml. Built by its deprecated constructor; Plan B removes it. */
class MultiPlotsVerticalLine : public SciQLopStraightLine
{
    Q_OBJECT

public:
    MultiPlotsVerticalLine(SciQLopMultiPlotPanel* panel, double position,
                           QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false,
                           bool visible = true, const QString tool_tip = "")
            : SciQLopStraightLine(panel, position, Qt::Orientation::Vertical, color, read_only,
                                  visible, tool_tip, Coordinates::Data, true)
    {
    }
};
```

`git rm src/MultiPlotsVLine.cpp`; delete its line from `src/meson.build`. In `SciQLopMultiPlotPanel`, delete `void remove(MultiPlotsVerticalLine* line);` and its definition; `remove(SciQLopStraightLine*)` takes it. Delete the `class MultiPlotsVerticalLine;` forward declaration (line 48) if nothing else in the header uses it.

- [ ] **Step 5: Bindings and Python**

bindings.xml:
- `SciQLopStraightLine`: the panel constructor's `remove="all"` signature ends with `,Coordinates,bool)`.
- `MultiPlotsVerticalLine` (line 1924-1948): delete the three `rename` modify-functions (`position() const`, `line_width() const`, `read_only() const`); keep the constructor renames and the seven properties. Do not restore the renames if a property looks odd: the getters are inherited now, so each property shadows an inherited method, as `color` always did, and a `rename` of a method the class does not declare is an error.
- `SciQLopMultiPlotPanel`: delete the `remove(MultiPlotsVerticalLine*)` modify-function.

`SciQLopPlots/__init__.py`, `_SNAKE_CASE_ALIASES` becomes:

```python
_SNAKE_CASE_ALIASES = {
    SciQLopMultiPlotPanel: {"set_selected": "setSelected", "add_widget": "addWidget",
                            "remove_widget": "removeWidget", "index_of": "indexOf",
                            "selection_changed": "selectionChanged"},
    SciQLopPlotsBindings.SciQLopMultiPlotObject: {"add_object": "addObject",
                                                  "remove_object": "removeObject",
                                                  "replot_all": "replotAll",
                                                  "update_plot_list": "updatePlotList"},
}
```

- [ ] **Step 6: Reconfigure, build, run, full suite**

A source was removed: reconfigure, then compile. Run `tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py tests/integration/test_multiplots_vertical_line.py tests/integration/test_drawn_panel_objects.py tests/integration/test_api_hygiene.py tests/integration/test_binding_signatures.py`, then the full suite.

- [ ] **Step 7: Commit**

```bash
git rm src/MultiPlotsVLine.cpp
git add include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp include/SciQLopPlots/Items/SciQLopStraightLines.hpp src/SciQLopStraightLines.cpp include/SciQLopPlots/Items/impl/LineTargets.hpp src/LineTargets.cpp include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp src/meson.build SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_old_span_line_names.py tests/integration/test_drawn_contract.py tests/integration/test_api_hygiene.py
git commit -m "refactor(lines): MultiPlotsVerticalLine is a panel-scope SciQLopStraightLine with old-class parts"
```

---

### Task 13: The Python surface: one `.on` row, one alias row, `movable` deprecated

**Files:**
- Modify: `SciQLopPlots/__init__.py` (`_ON_EXCEPTIONS` ~line 304-320, `_DEPRECATED_ALIASES` ~line 629-635, `_DEPRECATED_CALLS` from Task 9)
- Modify: `pyproject.toml` (`filterwarnings`, line 56-58)
- Test: `tests/integration/test_straight_line.py`, `tests/integration/test_old_span_line_names.py`, and every test that calls `movable()` / `set_movable()` on a line

**Interfaces:**
- Consumes: Task 8 (the `SciQLopSpan` and `SciQLopStraightLine` rows), Task 9 (`_DEPRECATED_CALLS`), Tasks 11 and 12 (the old classes are subclasses now, so the rows on the base classes cover them).
- Produces: `line.movable()` warns `movable() is deprecated: use line.read_only()`, `line.set_movable(x)` warns `set_movable(...) is deprecated: use line.set_read_only(...)`, both with `stacklevel=2`. The suite's filter also catches `use line.`.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_straight_line.py`:

```python
@pytest.mark.parametrize("movable", [True, False])
def test_set_movable_warns_and_inverts(host, movable):
    line = make(host, "vertical")
    with pytest.warns(DeprecationWarning,
                      match=r"set_movable\(\.\.\.\) is deprecated: use line\.set_read_only\(\.\.\.\)") as record:
        line.set_movable(movable)
    assert record[0].filename == __file__
    assert line.read_only() is (not movable)


def test_movable_warns_and_reads_the_inverse(host):
    line = make(host, "vertical", read_only=False)
    with pytest.warns(DeprecationWarning, match=r"movable\(\) is deprecated: use line\.read_only\(\)"):
        assert line.movable() is True


def test_the_suite_filter_turns_a_bare_set_movable_into_an_error(host):
    """Pins the filterwarnings entry in pyproject.toml: no pytest.warns wrapper here."""
    with pytest.raises(DeprecationWarning, match=r"use line\."):
        make(host, "vertical").set_movable(True)
```

Append to `tests/integration/test_old_span_line_names.py` (guards: they pass before the table rows go, and must still pass after):

```python
@pytest.mark.parametrize("cls", OLD_PLOT_SPANS, ids=lambda cls: cls.__name__)
def test_the_old_spans_keep_on_tooltip_and_selection_changed(plot, cls):
    span = _old(OLD_PLOT_SPANS[cls][0], plot)
    span.on.tooltip.value = "tip"
    assert span.tool_tip() == "tip"
    assert cls.selectionChanged is cls.selection_changed


def test_the_old_panel_objects_keep_on_tooltip(panel):
    _with_plots(panel)
    span = _old(lambda p: MultiPlotsVerticalSpan(p, R(1.0, 2.0)), panel)
    line = _old(lambda p: MultiPlotsVerticalLine(p, 2.0), panel)
    for item in (span, line):
        item.on.tooltip.value = "tip"
        assert item.tool_tip() == "tip"
```

- [ ] **Step 2: Run them and watch them fail**

Run: `tests/integration/test_straight_line.py tests/integration/test_old_span_line_names.py`. Expected: the three `movable` tests fail (no warning); the guards pass.

- [ ] **Step 3: Implement**

`SciQLopPlots/__init__.py`:
- `_ON_EXCEPTIONS`: delete the `SciQLopVerticalSpan` `"tooltip"` row. The `SciQLopSpan` and `SciQLopStraightLine` rows reach every span and line through the MRO.
- `_DEPRECATED_ALIASES`: delete the `SciQLopVerticalSpan`, `SciQLopHorizontalSpan` and `SciQLopRectangularSpan` rows; the `SciQLopSpan` row covers them.
- `_DEPRECATED_CALLS` gains:

  ```python
    (SciQLopPlotsBindings.SciQLopStraightLine, "movable"):
        "movable() is deprecated: use line.read_only()",
    (SciQLopPlotsBindings.SciQLopStraightLine, "set_movable"):
        "set_movable(...) is deprecated: use line.set_read_only(...)",
  ```

`pyproject.toml`, `filterwarnings`:

```toml
filterwarnings = [
    "error:.*is deprecated. use (plot|panel|line)\\.:DeprecationWarning",
]
```

- [ ] **Step 4: Run, migrate, full suite**

Run the two files, then the full suite. Every `set_movable(x)` / `movable()` on a line that the filter now turns into an error becomes `set_read_only(not x)` / `not line.read_only()`. Find them with `grep -rnE "set_movable\(|\.movable\(\)" tests`. `set_movable` on text, ellipse and pixmap items is another class's method and stays. List the changed files in the report.

- [ ] **Step 5: Commit**

```bash
git add SciQLopPlots/__init__.py pyproject.toml tests/integration/test_straight_line.py tests/integration/test_old_span_line_names.py <every migrated file, by path>
git commit -m "feat(lines): movable()/set_movable() deprecated for read_only; one .on and alias row per new class"
```

---

### Task 14: Move the remaining tests to the new API

The suite is green after Tasks 9-13; this task removes what still uses the old API without being about it.

**Files:** every test file the greps below name, under `tests/integration`, `tests/unit`, `tests/fuzzing`.

**Rules:** the **Fallout rules** (before Task 9), plus:
- A test that builds a span or line through a deprecated constructor, and is not about that constructor or class, uses the factory: `SciQLopVerticalSpan(plot, r, ...)` → `plot.vertical_span(r, ...)`, `SciQLopRectangularSpan(plot, k, v)` → `plot.span(key_range=k, value_range=v)`, `SciQLopStraightLine(plot, p, m, c, o)` → `plot.straight_line(p, o, read_only=not m, coordinates=c)`, `MultiPlotsVerticalSpan(panel, ...)` → `panel.vertical_span(...)`, `MultiPlotsVerticalLine(panel, ...)` → `panel.vertical_line(...)` (add `color=QColor(100, 100, 100), read_only=False` where the test relied on the old defaults).
- `span.selectionChanged` → `span.selection_changed`, except in the alias tests.
- The tests **about** the old names keep them: `test_old_span_line_names.py`, `test_item_factories.py` (constructor deprecations), `test_item_ownership.py` (constructor-built ownership), `test_drawn_contract.py` (the `_old` builders), `test_drawn_spans.py` (the `selectionChanged` alias), `test_api_hygiene.py`, `test_binding_signatures.py` (the type map and the collection), `test_multiplots_vertical_line.py`, `test_drawn_panel_objects.py` (the catalogue overlay pattern and the old properties), the `TestMultiPlotsVerticalSpanProperties` and `TestSpanCollectionCulling` classes of `test_panel_api.py`, the constructor guards of `test_thread_guards.py`, and the property guard in `test_straight_line.py`.

- [ ] **Step 1: Find what is left**

```bash
grep -rnE "SciQLop(Vertical|Horizontal|Rectangular)Span\b|MultiPlotsVertical(Span|Line)\b|SciQLop(Vertical|Horizontal)Line\b|SciQLopStraightLine\(" tests/integration tests/unit tests/fuzzing
grep -rnE "rectangular_span\(|selectionChanged|movable=" tests/integration tests/unit tests/fuzzing
```

Every hit outside the files the rules keep is migrated. Inside those files, a hit that is not about the old name is migrated too.

- [ ] **Step 2: Rewrite, then run the touched files, then the full suite**

Expected after the rewrite: the first grep hits only the kept files; `rectangular_span(` only in `test_item_factories.py`; `selectionChanged` only in `test_drawn_spans.py`, `test_api_hygiene.py` and `test_old_span_line_names.py`; `movable=` only in `test_item_factories.py`.

- [ ] **Step 3: Commit**

```bash
git add <every changed test file, by path>
git commit -m "test: spans and lines through span()/straight_line() and their shorthands, methods instead of panel properties"
```

---

### Task 15: Docs, README, manual tests

**Files:**
- Modify: `docs/user-guide.md` (overlays ~line 805-857, `.on` ~line 959-990, camelCase ~line 1422, deprecations ~line 1424)
- Check: `README.md` (line 22, 88-98)
- Modify: `tests/manual-tests/gallery.py` (spans tab ~153-179), `advanced/pipeline.py` (~59-70), `basics/vertical_spans.py`, `advanced/stacked_plots.py` (~42), `stress/stress_model.py` (~42), `real_data/micro_sciqlop.py`, `advanced/projections.py`

- [ ] **Step 1: The overlays section**

In `docs/user-guide.md`, the snippet at ~line 807-827 stays as it is (`plot.vertical_span(...)` and `plot.horizontal_line(0.5)` are the new factories). Replace the bullets after its image (~line 831-835; Task 10 already reworded the first one) with:

```markdown
- A span bounds the key axis, the value axis or both: `plot.span(key_range=..., value_range=...)`. An axis left at `None` is unbounded, and `span.key_range()` / `span.value_range()` read `None` there. `plot.vertical_span(range)` and `plot.horizontal_span(range)` are the one-axis shorthands, with `span.range()`, `set_range()` and `range_changed` for the bounded axis. On a span bounding both axes, `range()` raises `ValueError`: use `key_range()` and `value_range()`.
- A line is vertical or horizontal: `plot.straight_line(position, Qt.Orientation.Vertical)`, or the shorthands `plot.vertical_line(position)` and `plot.horizontal_line(position)`. Lines are read-only, so the user can't drag them, unless you pass `read_only=False`. Spans are editable unless you pass `read_only=True`.
- `coordinates=Coordinates.Pixels` keeps a span or a line in place on screen, measured from the plot area's top-left corner. `set_coordinates()` switches later and leaves the object where it is. `span.pixel_range()` (`pixel_key_range()`, `pixel_value_range()`) reads a span's position in pixels in either mode.
- On a panel, the same factories draw one span or line across every plot, including plots added later: `panel.span(...)`, `panel.vertical_span(...)`, `panel.straight_line(...)`, `panel.vertical_line(...)` and the horizontal ones, with the same arguments. (Only the plot line factories raise `TypeError` for a bool second argument: the panel ones always took the colour there.) That's handy for marking an event in time. A drag, a click or the Delete key on one plot reaches all of them, and the panel object signals once.
- Every span and line has `selected()`, `visible()`, `color()`, `tool_tip()` and `read_only()`, with their `set_` methods and `_changed` signals, and `delete_requested`, which fires when the user presses Delete on it. Nothing is deleted for you: connect `delete_requested` to `plot.remove(span)` or `panel.remove(span)` if that is what you want.
- A span created through a `MultiPlotsVSpanCollection` is hidden while its range is outside the panel's time range, so `span.visible` reads `False` until the range comes back. A span you hid yourself stays hidden when it returns.
- The other factories are `plot.ellipse(rect)`, `plot.pixmap(pixmap, rect)` and `plot.curved_line(start, stop)`.
- The plot owns what its factories return. `plot.remove(item)` deletes one now. It raises `ValueError` for an object of another plot, for a panel-wide span or line (that one goes through `panel.remove`), for one plot's part of a panel-wide span or line, and for `None`, and `TypeError` for the legend or a graph component. `panel.remove(span_or_line)` deletes a panel-wide span or line and its parts now. `panel.remove(plot)` deletes the plot later, like `remove_plot`.
- Text, ellipse and pixmap items have `selected()` / `set_selected()` and `name()` / `set_name()` too. Text, pixmap and curved-line tooltips work, and so does `set_movable` on text, ellipse and pixmap.
```

(The collection bullet stays as it was: `MultiPlotsVSpanCollection` builds old-class spans, where `visible` is an attribute.)

- [ ] **Step 2: The `.on` section**

- The bullet `- `span.on.range`, `line.on.position`` (~line 979) becomes ``- `span.on.range`, `span.on.key_range`, `span.on.read_only`, `span.on.coordinates`, `line.on.position` ``.
- After the paragraph ending "…for example `color` on a multi-column graph or `selected` on the legend." add: ``On spans, `span.on.range` resolves on every span and raises `ValueError` when used on one that bounds both axes; `span.on.key_range` and `span.on.value_range` read `None` on an axis the span does not bound, and raise when written there.``
- The last exception bullet (~line 990) becomes: ``- `tooltip` on spans and lines: the accessors are `tool_tip`, and the signal is `tool_tip_changed`.``

- [ ] **Step 3: The camelCase and deprecation paragraphs**

- ~line 1422: "on panel-wide lines and spans `add_object`, `remove_object`" becomes "on `MultiPlotsVSpanCollection` `add_object`, `remove_object`".
- After the **Deprecated calls.** paragraph (~line 1424), add:

```markdown
**Spans and lines.** The factories return two classes, `SciQLopSpan` and `SciQLopStraightLine`, for every kind of span and line and on plots and panels alike. Their accessors are methods.
- The old classes stay for code that uses them: `SciQLopVerticalSpan`, `SciQLopHorizontalSpan`, `SciQLopRectangularSpan`, `MultiPlotsVerticalSpan`, `SciQLopVerticalLine`, `SciQLopHorizontalLine` and `MultiPlotsVerticalLine`. Their constructors, `MultiPlotsVSpanCollection.create_span` and interactive span creation (`span_created`) still build them, with their old defaults.
- On the old panel classes the state is still read and written as attributes: `span.range`, `span.selected = True`, `line.position = 3.0`. On what the factories return, the same names are methods: `span.range()`, `span.set_selected(True)`, `line.set_position(3.0)`. So a pipeline written against an old panel object (`span.on.range`) does not carry over to a new one until its calls move to the methods.
- `plot.rectangular_span(k, v)` warns: use `plot.span(key_range=k, value_range=v)`. `SciQLopStraightLine(plot, ...)` warns: use `plot.straight_line(...)`.
- The plot line factories take the colour second and `read_only` third. `movable=` still works and warns. A bool in second place raises `TypeError` instead of becoming a colour. `line.movable()` and `line.set_movable()` warn: use `read_only()` and `set_read_only()`, which mean the opposite.
- The factory defaults are the same on plots and panels: spans are grey with alpha 100 and editable, lines are black and read-only. `panel.vertical_line` used to give a grey, draggable line: pass `color=QColor(100, 100, 100), read_only=False` to keep that.
```

- [ ] **Step 4: README and manual tests**

`README.md`: the snippet at line 88-98 already uses the factories and `.on.tooltip`; leave it. Line 22 stays.

`tests/manual-tests/`: rewrite each listed file by the **Fallout rules** and Task 14's rules. Keep `span_created` and `set_span_creation_enabled` in `gallery.py` (interactive creation is unchanged). Manual tests block on `app.exec`, so they are not run: check each with `.venv/bin/python -m py_compile <file>`.

- [ ] **Step 5: Run the guide test, then the full suite**

Run `tests/integration/test_user_guide.py`, then the full suite.

- [ ] **Step 6: Commit**

```bash
git add docs/user-guide.md <each changed manual test, by path>
git commit -m "docs: one span class and one line class, the factories, the migration note"
```

---

### Task 16: Self-delete sweep, ASan, and the last checks

**Files:**
- Modify: `tests/sweeps/self_delete/common.py` (`BUILDERS`, line 123-130)
- Test: the sweep (not collected by pytest), the ASan run, the full suite

- [ ] **Step 1: The contract walk covers both scopes**

Check, by reading `tests/integration/test_drawn_contract.py`, that the `"span"` row has `SciQLopSpan`, `SciQLopSpan[panel]` and the four old classes, and the `"text and lines"` row has `SciQLopStraightLine`, `SciQLopStraightLine[panel]`, the two old line classes and `MultiPlotsVerticalLine`. Run it once. Expected: all pass.

- [ ] **Step 2: Sweep builders for every kind and scope**

In `tests/sweeps/self_delete/common.py`, after `_decorated`:

```python
def _quiet(cls, *args):
    """An old-name object, through its deprecated constructor, without the warning."""
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        return cls(*args)
```

and replace the `BUILDERS` entries from `"vertical_line"` to `"panel_line"` (line 123-129) with:

```python
    "vertical_line": lambda h: h.plot.vertical_line(1.0),
    "horizontal_line": lambda h: h.plot.horizontal_line(0.5),
    "vspan": lambda h: h.plot.vertical_span(R(1.0, 2.0)),
    "hspan": lambda h: h.plot.horizontal_span(R(1.0, 2.0)),
    "rspan": lambda h: h.plot.span(key_range=R(1.0, 2.0), value_range=R(1.0, 2.0)),
    "panel_vspan": lambda h: h.panel.vertical_span(R(1.0, 2.0)),
    "panel_hspan": lambda h: h.panel.horizontal_span(R(1.0, 2.0)),
    "panel_rspan": lambda h: h.panel.span(key_range=R(1.0, 2.0), value_range=R(1.0, 2.0)),
    "panel_vline": lambda h: h.panel.vertical_line(2.0),
    "panel_hline": lambda h: h.panel.horizontal_line(0.5),
    "old_vspan": lambda h: _quiet(S.SciQLopVerticalSpan, h.plot, R(1.0, 2.0)),
    "old_panel_span": lambda h: _quiet(S.MultiPlotsVerticalSpan, h.panel, R(1.0, 2.0)),
    "old_panel_line": lambda h: _quiet(S.MultiPlotsVerticalLine, h.panel, 2.0),
```

- [ ] **Step 3: Run the sweep**

`cd tests/sweeps/self_delete`, delete `discovery.jsonl` and `pairs.jsonl`, then `../../../.venv/bin/python driver.py discover`, `../../../.venv/bin/python driver.py pairs`, `../../../.venv/bin/python driver.py summary`. Each is one command, waited out in the foreground with timeout 600000; the sweep resumes, so rerun the same phase if one passes 540 s. Expected: 0 crashed and 0 hung pairs. A crash is a bug: reproduce it with a pytest test (one setter, one signal, a slot that deletes the object), fix it at its root, and commit that fix on its own before going on. `./bt.sh <kind> <setter> ...` gives a backtrace.

- [ ] **Step 4: ASan**

Is `build-asan` configured with `-Db_sanitize=address`? Rebuild it, alone, in the foreground: `.venv/bin/meson compile -C build-asan -j 4` (same environment as the main build; reconfigure it first, because Tasks 2-12 added and removed object-types and sources). A rebuild longer than 600 s is normal: rerun the same command. Then:

```bash
cd /tmp && LD_PRELOAD=/usr/lib64/libasan.so.8 ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-asan:/home/jeandet/Documents/prog/SciQLopPlots timeout 540 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_span.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_panel_span.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_straight_line.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_old_span_line_names.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_drawn_contract.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_plot_remove.py -q -p no:cacheprovider
```

Expected: passes with no ASan report. Put the summary line in the report. A report is a bug: same handling as a sweep crash.

- [ ] **Step 5: Full suite, signature test, commit**

Run the full suite and `tests/integration/test_binding_signatures.py` on build-venv. Then:

```bash
git add tests/sweeps/self_delete/common.py
git commit -m "test(sweep): self-delete builders for every span and line kind at both scopes"
```

Do not commit `discovery.jsonl` or `pairs.jsonl` unless they were tracked before (`git ls-files tests/sweeps/self_delete`).

---

## Deviations from the spec, and their reasons

Spec revision 4 folds in 1 to 4 below; 5 stays a plan-level choice. Each is named where it happens.

1. **A bool second argument raises TypeError** (Task 9). The spec says a positional bool "now fails on the colour slot"; PySide accepts it as a colour, so the plan adds the failure.
2. **The old panel span's border-selection mirror goes in Plan A** (Task 11), not Plan B: a subclass with no behaviour of its own cannot keep it, and no user input ever fires it. The parts keep `select_lower_border` / `select_upper_border` and their signals (Plan B removes those).
3. **Legacy shims on the old subclasses** beyond "only fix kind and scope", each kept because SciQLop may call it: `SciQLopVerticalSpan.set_pixel_range` and its `pixel_range` property; V/H border selection; `MultiPlotsVerticalSpan.show()` / `hide()` and the collection's culling hooks (the spec asks for the bug round's visibility rules); `position` stays a property on `SciQLopVerticalLine` / `SciQLopHorizontalLine` (the spec lists only the panel properties, but the old plot line classes had `position` as a property too). Lost from the old plot spans: `replot()`, `parentPlot()` and the raising `movable` / `position` / `move` stubs (the spec drops the stubs).
4. **The suite filter also catches `use line.`** (Task 13), so a missed `set_movable` call fails the suite like a missed factory call.
5. **Panel-scope pixels** (Tasks 4 and 7, `simplify:` comments): `pixel_range()` reads the first plot; `set_coordinates` converts on each plot, then gives every part the first plot's numbers; on a panel without plots the numbers stay as they were.

## Self-review against the spec

Each spec requirement, and the task that implements and tests it.

| Spec | Task |
|---|---|
| `SciQLopSpan` from `SciQLopDrawnObject`, not an item interface; kind fixed; `SpanKind` | 2 |
| `key_range()` / `value_range()` None on an unbounded axis; their setters raise; their signals | 1 (mechanism), 2 |
| `range()` shortcut, raises on rectangular, `range_changed` paired once with the axis signal | 2 |
| `coordinates` (setter, signal), `pixel_range` / `pixel_key_range` / `pixel_value_range` | 3, 4 |
| `read_only` (default False), `tool_tip`, `id()`, `scope()`, `is_panel_part()` | 2, 4 |
| Borders and pen style, click selection, `delete_requested` | 3, 4 |
| `SciQLopStraightLine`: `position`, `orientation`, `coordinates` with a new setter, `read_only` (default True), `delete_requested`, scope | 5, 7 |
| Line clamp getters, `tool_tip_changed`; the deprecated `movable` aliases | 6, 13 |
| One class, two targets; `PanelTarget` plumbing: join/leave, QPointers, `mark_panel_part`, snapshots, edits flow up, one emission per edit, C++-owned panel children | 4, 7 |
| Old names as subclasses fixing kind and scope, with the named shims; built by the old entry points only; old panel properties kept; plumbing methods gone; not `SciQLopRangeItemInterface` | 11, 12 |
| Factory defaults, the same at both scopes | 2, 5, 9, 10 |
| `span` and shorthands with the defaults defined once; `straight_line` and shorthands; `rectangular_span` (plots only) deprecated; `movable=` deprecated and the bool guard; the message format; the `_ITEM_FACTORIES` hint | 2, 5, 9, 10 |
| `remove` overloads at both scopes, wrong-scope messages, part refusal | 2, 4, 7 |
| `.on`: `range` / `key_range` / `value_range` rules, observable `coordinates` / `read_only` / `tool_tip`, no `.on.position` on spans | 2, 3, 11 |
| `.on.tooltip` on every span and line; `selectionChanged` on `SciQLopSpan` | 8, 13; guide in 15 |
| Collection legacy-only, `span_created` unchanged, wasm unchanged | 11 (read check) |
| Migration of tests, guide, README, manual tests; the migration note | 9, 10, 13, 14, 15 |
| Self-delete sweep, ASan, contract walk with both scopes | 2, 4, 7, 11, 12, 16 |

Gaps found while writing this, and fixed:
- An explicit `span(key_range=None, ...)` had no test: only omitted arguments did, and branch F maps the two differently. Task 2 now tests it.
- "Panel-scope objects are the panel's QObject children, C++-owned" had no test. Tasks 4 and 7 now drop the Python reference and find the object among the panel's children.
- The old plot line classes would have lost their `position` property when `SciQLopStraightLine` moved to methods; the spec only kept the panel properties. Task 5 keeps it on `SciQLopVerticalLine` / `SciQLopHorizontalLine`, with a guard test.
- `SciQLopVerticalSpan.set_pixel_range` and V/H border selection had no home once the subclasses lost their behaviour; existing tests use both. Task 11 keeps them as named shims (Deviation 3).
- The `.on` exception and the `selectionChanged` alias had to exist on `SciQLopSpan` before Task 9, or the switch of the factories would break `test_on_convention.py` and `test_drawn_spans.py`. Task 8 adds them, before the switch; Task 13 only removes the redundant rows.
- The spec's claim that a bool second argument fails on the colour slot is false (Review Focus 1, Deviation 1).

Not tested, on purpose: interactive span creation (`span_created`) has no automated test today and this plan does not change it; the wasm C++ is read, not compiled.
