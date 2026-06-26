# Function Line-Graph Auto-Sizing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make callback-driven line/scatter graphs size their components from the returned data instead of the label count, so a multi-component callback with no `labels` renders correctly instead of silently collapsing to one line.

**Architecture:** In `SciQLopPlot::plot_impl()` the `GraphType::Line`/`Scatter` callable branch chooses `SciQLopSingleLineGraphFunction` vs `SciQLopLineGraphFunction` by `labels.size()`. The multi-line class already creates one component per data column on first callback result (verified: `line_count()==0` at creation, populated on data). Always using it fixes the defect. A second, independent change auto-names unlabeled components from the graph's base name.

**Tech Stack:** C++ (Qt6, QCustomPlot fork), Shiboken6 Python bindings, meson build, pytest integration tests under `tests/integration/`.

**Design doc:** `docs/superpowers/specs/2026-06-26-function-graph-auto-size-design.md` (self-contained context).

**Two independent deliverables:**
- **Change 1 (Tasks 1–3)** — the critical rendering fix. Ship this even if Change 2 is deferred.
- **Change 2 (Tasks 4–5)** — cosmetic auto-naming of unlabeled components. Can ship in a later release.

---

## File Structure

- `src/SciQLopPlot.cpp` — `plot_impl()` graph-class selection (Change 1).
- `src/SciQLopLineGraph.cpp` / `src/SciQLopMultiGraphBase.*` / `SciQLopFunctionGraph` — component-creation path where unlabeled components get auto-named (Change 2).
- `tests/integration/test_function_graph_autosize.py` — new pytest module (mirrors `tests/integration/test_graph_api.py` and `test_line_graph_regression.py` conventions: module-scoped `app` fixture, function-scoped `plot` fixture, `qtbot.waitUntil` to let the worker-thread callback deliver data).

**Build note:** rebuild the bindings after each C++ change before running pytest (the maintainer's normal `meson`/`ninja` flow). These tests import the compiled `SciQLopPlots` module.

---

### Task 1: Reproducer — multi-component callback without labels must size from data

**Files:**
- Test: `tests/integration/test_function_graph_autosize.py` (create)

- [ ] **Step 1: Write the failing test**

```python
"""Callback line/scatter graphs size components from data, not label count."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot, GraphType


@pytest.fixture(scope="module")
def app():
    inst = QApplication.instance()
    return inst if inst else QApplication([])


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def _three_component(start, stop):
    x = np.linspace(start, stop, 100)
    return x, np.column_stack([np.sin(x), np.cos(x), np.sin(2 * x)])


def test_multicomponent_callback_without_labels_sizes_from_data(plot, qtbot):
    g = plot.line(_three_component)            # NO labels
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=3000)
    assert g.line_count() == 3
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/integration/test_function_graph_autosize.py::test_multicomponent_callback_without_labels_sizes_from_data -v`
Expected: FAIL — graph is `SciQLopSingleLineGraphFunction`, `line_count()` stays 1, `waitUntil` times out.

- [ ] **Step 3: Commit the red reproducer**

```bash
git add tests/integration/test_function_graph_autosize.py
git commit -m "test: reproduce multi-component callback collapsing to one line"
```

---

### Task 2: Guard — scalar callback without labels must stay one line

**Files:**
- Test: `tests/integration/test_function_graph_autosize.py` (append)

- [ ] **Step 1: Write the test**

```python
def _scalar(start, stop):
    x = np.linspace(start, stop, 100)
    return x, np.sin(x)


def test_scalar_callback_without_labels_is_one_line(plot, qtbot):
    g = plot.line(_scalar)                     # NO labels
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=3000)
    assert g.line_count() == 1
```

- [ ] **Step 2: Run test to verify it passes today**

Run: `pytest tests/integration/test_function_graph_autosize.py::test_scalar_callback_without_labels_is_one_line -v`
Expected: PASS already (this is the regression guard — it must keep passing after Change 1).

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_function_graph_autosize.py
git commit -m "test: guard scalar callback stays single line"
```

---

### Task 3: Change 1 — always use the data-sized class for line/scatter callables

**Files:**
- Modify: `src/SciQLopPlot.cpp:966-974` (`plot_impl`, the `GraphType::Line`/`Scatter` branch)

- [ ] **Step 1: Make the edit**

Replace the `if (labels.size() <= 1) … else …` branch. Before:

```cpp
        case GraphType::Line:
        case GraphType::Scatter:
            if (labels.size() <= 1)
                plottable = m_impl->add_plottable<SciQLopSingleLineGraphFunction>(
                    std::move(callable), labels, metaData);
            else
                plottable = m_impl->add_plottable<SciQLopLineGraphFunction>(
                    std::move(callable), labels, metaData);
            break;
```

After:

```cpp
        case GraphType::Line:
        case GraphType::Scatter:
            // Size components from the callback's data, not the label count:
            // SciQLopLineGraphFunction creates one component per data column on
            // first result, so a multi-component callback no longer silently
            // collapses to a single line when labels are omitted.
            plottable = m_impl->add_plottable<SciQLopLineGraphFunction>(
                std::move(callable), labels, metaData);
            break;
```

- [ ] **Step 2: Rebuild the bindings**

Run the maintainer's normal build (e.g. `ninja -C build` / `meson compile -C build`).
Expected: builds clean. If `SciQLopSingleLineGraphFunction` becomes unused and the compiler warns, that is expected — leave the class defined (still used elsewhere / for the opt-in option below); do not delete it in this task.

- [ ] **Step 3: Run both tests to verify the reproducer passes and the guard holds**

Run: `pytest tests/integration/test_function_graph_autosize.py -v`
Expected: both PASS — multi-component now `line_count()==3`, scalar still `line_count()==1`.

- [ ] **Step 4: Run the existing line-graph + graph-api suites for regressions**

Run: `pytest tests/integration/test_line_graph_regression.py tests/integration/test_graph_api.py -v`
Expected: PASS — static-data sizing and label-based callable graphs are unchanged.

- [ ] **Step 5: Commit**

```bash
git add src/SciQLopPlot.cpp
git commit -m "fix(plot): size line/scatter callback graphs from data, not labels"
```

**Open author decision (record in the commit body or a follow-up issue):** whether to keep `SciQLopSingleLineGraphFunction` as an explicit opt-in fast-path for single-component callbacks. This plan drops it from the default `Line`/`Scatter` callable path; the class remains defined for that potential opt-in.

---

### Task 4: Auto-naming test — unlabeled components named from the graph base name

**Files:**
- Test: `tests/integration/test_function_graph_autosize.py` (append)

- [ ] **Step 1: Write the failing test**

```python
def test_unlabeled_components_autonamed_from_graph_name(plot, qtbot):
    g = plot.line(_three_component, name="b_gse")    # name, no labels
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=3000)
    assert g.labels() == ["b_gse[0]", "b_gse[1]", "b_gse[2]"]


def test_unlabeled_scalar_named_from_graph_name(plot, qtbot):
    g = plot.line(_scalar, name="amplitude")
    qtbot.waitUntil(lambda: g.line_count() == 1, timeout=3000)
    assert g.labels() == ["amplitude"]
```

- [ ] **Step 2: Run test to verify it fails**

Run: `pytest tests/integration/test_function_graph_autosize.py -k autonamed -v`
Run: `pytest tests/integration/test_function_graph_autosize.py -k named_from_graph_name -v`
Expected: FAIL — components are created with default/empty names, not `base[i]`.

- [ ] **Step 3: Commit the red tests**

```bash
git add tests/integration/test_function_graph_autosize.py
git commit -m "test: auto-name unlabeled callback components from graph name"
```

---

### Task 5: Change 2 — auto-name unlabeled components from the base name

**Files:**
- Modify: the function-graph component-creation path. Start at
  `SciQLopLineGraphFunction` (`src/SciQLopLineGraph.cpp`) and its bases
  `SciQLopMultiGraphBase` / `SciQLopFunctionGraph`. Trace where a component is
  added when a callback result with N columns first arrives (the lazy
  `create_graphs` / resize-on-data path — `line_count()` goes 0 → N there).

- [ ] **Step 1: Implement the naming rule**

At the point where components are (re)created from data, when there is **no
explicit label** for a component index, derive the component name from the
plottable's base name (`this->name()`):

- if the final component count is 1 → name the single component `base`
- if it is N>1 → name component `i` as `base[i]`

Requirements / cautions to handle while implementing:
- The base name may be set *after* the graph is constructed (callers may call
  `set_name()` later). Ensure naming is applied when components are created
  **and** re-applied if the base name changes while labels are still absent.
  Wire it to the existing name-change signal (`name_changed`) if needed.
- An empty base name must not crash and must not produce names like `[0]`;
  fall back to the pre-existing default component naming.
- Explicit `labels` (count matching components) must still take precedence —
  do not overwrite caller-provided labels.

Keep the change minimal and localized to the component-creation/naming path; do
not alter data buffers or the rendering pipeline.

- [ ] **Step 2: Rebuild the bindings**

Run the maintainer's normal build.
Expected: builds clean.

- [ ] **Step 3: Run the auto-naming tests**

Run: `pytest tests/integration/test_function_graph_autosize.py -v`
Expected: all PASS — `["b_gse[0]", "b_gse[1]", "b_gse[2]"]` and `["amplitude"]`.

- [ ] **Step 4: Run the graph-api suite (labels still honored)**

Run: `pytest tests/integration/test_graph_api.py -k "label or component or multicomponent" -v`
Expected: PASS — explicit labels are unchanged (e.g. `test_multicomponent_data`, `test_set_labels`).

- [ ] **Step 5: Commit**

```bash
git add src/SciQLopLineGraph.cpp src/SciQLopMultiGraphBase.cpp
git commit -m "feat(plot): auto-name unlabeled callback components from graph name"
```

(Adjust the staged file list to whatever files the naming change actually touched.)

---

### Task 6: Scatter parity + full sweep

**Files:**
- Test: `tests/integration/test_function_graph_autosize.py` (append)

- [ ] **Step 1: Write the scatter parity test**

```python
def test_multicomponent_scatter_callback_without_labels(plot, qtbot):
    g = plot.scatter(_three_component)         # scatter shares the Line branch
    qtbot.waitUntil(lambda: g.line_count() == 3, timeout=3000)
    assert g.line_count() == 3
```

- [ ] **Step 2: Run the new module + regression suites**

Run:
```bash
pytest tests/integration/test_function_graph_autosize.py \
       tests/integration/test_graph_api.py \
       tests/integration/test_line_graph_regression.py -v
```
Expected: all PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_function_graph_autosize.py
git commit -m "test: scatter callback parity for data-sized components"
```

---

## Self-Review

**Spec coverage:**
- Goal 1 (N components → N lines, any label count) → Tasks 1, 3, 6. ✓
- Goal 2 (scalar still one line) → Task 2 guard, re-checked in Task 3. ✓
- Goal 3 (auto-name `base` / `base[i]`) → Tasks 4, 5. ✓
- Goal 4 (explicit labels still win) → Task 5 step 4 (graph-api label tests). ✓
- "Scatter inherits the fix" → confirmed by the shared `Line`/`Scatter` branch; Task 6. ✓
- Open author decision (keep `SingleLine` opt-in) → recorded in Task 3 step 5. ✓

**Placeholder scan:** Change 1 (Task 3) is a complete, exact before/after edit. Change 2 (Task 5) is necessarily described as an approach with the precise entry points to trace, because the component-creation/naming insertion point and the `name_changed` re-apply wiring depend on the live class hierarchy and must be compiled/verified by the implementing agent — the acceptance tests (Task 4) gate correctness. This is intentional, not a placeholder: the *what*, *where*, and *acceptance criteria* are all concrete.

**Type/name consistency:** test helpers `_three_component` / `_scalar` defined once (Tasks 1–2) and reused (Tasks 4, 6). `line_count()` and `labels()` match the bindings used in `tests/integration/test_graph_api.py`. The edited branch location (`src/SciQLopPlot.cpp:966-974`) matches the design doc.
