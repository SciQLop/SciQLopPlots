# Function Line-Graph Auto-Sizing — Design

**Date:** 2026-06-26
**Status:** Approved (pending implementation plan)
**Scope:** Make callback-driven line graphs size their components from the
returned data instead of from the label count, eliminating a silent failure
mode where a multi-component callback with no `labels` renders as a single line.

> **Context for a fresh agent.** This spec is self-contained. It originates from
> a SciQLop `user_api` ergonomics review; the companion SciQLop-side work
> (documenting plot kwargs) is tracked separately in the SciQLop repo and does
> **not** block this change. This change is the root fix.

## Problem

`SciQLopPlot::plot_impl()` selects the graph class for a callable by **label
count**, not by the data the callback produces. In `src/SciQLopPlot.cpp`
(~L958–989):

```cpp
case GraphType::Line:
    if (labels.size() <= 1)
        plottable = m_impl->add_plottable<SciQLopSingleLineGraphFunction>(
            std::move(callable), labels, metaData);
    else
        plottable = m_impl->add_plottable<SciQLopLineGraphFunction>(
            std::move(callable), labels, metaData);
```

Consequence: a callback that returns `(x, y)` with a multi-column `y` but is
called **without `labels`** (`labels.size() <= 1`) gets a
`SciQLopSingleLineGraphFunction`, which can only ever draw one component. The
extra components are silently dropped. No error, no warning — the plot just
looks wrong or empty.

### Verified behavior (Python probe)

| Call | Graph class | `line_count()` |
|---|---|---|
| scalar callback, no labels | `SciQLopSingleLineGraphFunction` | 1 (ok) |
| **multi-component callback, no labels** | `SciQLopSingleLineGraphFunction` | **1 (wrong)** |
| multi-component callback, 3 labels | `SciQLopLineGraphFunction` | **0 at creation**, populated on first data |

The third row is the key: `SciQLopLineGraphFunction` reports `line_count() == 0`
*before any data arrives* and creates one component per data column on the first
callback result — it **sizes from the data, not the labels**. So routing all
line callbacks to that class fixes the defect.

The data-driven path already exists for *static* data:
`test_line_graph_2d_creates_component_per_column` asserts a 2-D `y` yields one
component per column. This change brings the *callback* path to parity.

## Goals

1. A callback returning N components renders N lines, regardless of how many
   `labels` were supplied (including zero).
2. A scalar callback still renders exactly one line (no regression).
3. When `labels` are absent, auto-name components from the graph's name:
   one component → the base name; N>1 → `base[0] … base[N-1]`.
4. Explicit `labels`, when supplied and matching the component count, still win.

## Design

### Change 1 — always use the data-sized class for line callbacks

In `plot_impl()` for `GraphType::Line` with a callable, stop branching on
`labels.size()`; always construct `SciQLopLineGraphFunction`
(`SciQLopSingleLineGraphFunction` is no longer used for the `Line` callable
path). The multi-line class already sizes components from data, so a scalar
callback yields one component and a multi-component callback yields one per
column.

#### Open author decision

`SciQLopSingleLineGraphFunction` is a single-component fast-path. Dropping it for
callables is the simplest fix and is assumed here. If its per-render cost
advantage matters for the common single-line case, keep it as an **explicit
opt-in** (e.g. a dedicated `GraphType` or a flag) and default the plain `Line`
callable path to the data-sized class. Either way, the default behavior for
`plot(callable, graph_type=Line)` must be data-sized.

### Change 2 — auto-name components from the base name when labels are absent

Component naming for function graphs cannot happen in `_configure_plotable()`:
that runs at creation time, when a function graph has **zero** components
(`std::size(plottable->components()) == 0`), so its `set_labels` guard is a
no-op. Naming must occur where components are created from data — in the
multi-graph/function-graph component-creation path
(`SciQLopMultiGraphBase` / `SciQLopFunctionGraph`, see `create_graphs` and the
data-arrival slot in `src/SciQLopLineGraph.cpp` / `src/SciQLopMultiGraphBase.*`).

When a component is created and no explicit label exists for its index, derive
its name from the graph's base name (the `name` set on the plottable):

- 1 component  → `base`
- N>1 components → `base[i]` for `i` in `0 … N-1`

The base name is provided by the caller. On the SciQLop side, `user_api`
`plot_function` passes `name=f.__name__` when the caller gives neither `name`
nor `labels`, so the auto-generated names are meaningful (e.g. `b_gse[0]`).
If the graph has no name, fall back to a neutral base (e.g. the existing default
component naming) — never crash, never leave the components mis-sized.

## Out of scope

- Changing the static-data (`SciQLopLineGraph`) path — it already sizes from data.
- Curve / scatter / colormap callable paths — only `GraphType::Line` is in scope
  (scatter shares the line classes; confirm it inherits the fix and add a guard
  test, but do not redesign it).
- The SciQLop `user_api` documentation work (separate repo).

## Backward compatibility

- Turns a silently-wrong single-line render into a correct multi-line one — a
  strict improvement. No public API signature changes.
- Scalar callbacks are unaffected in behavior (one line); only the backing class
  changes (decision above).
- Existing tests that pass `labels` and assert component counts continue to hold
  (`SciQLopLineGraphFunction` already honors them).

## Release / handoff

SciQLopPlots is built and released here. After this lands and a release is cut,
SciQLop bumps its `pyproject.toml` SciQLopPlots pin to that release; only then
does SciQLop's user-facing docstring claim that `labels` is optional for
multi-component callbacks become true.
