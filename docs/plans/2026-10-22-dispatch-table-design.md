# One dispatch table for plot() and the factories

Status: design for API review item 5.4 (docs/api-review-2026-10-20.md), Phase 4. Revised once after an opencode review.

## Why

The `line/scatter/...` dispatch is written three times: in Python `plot()`, in the panel's C++ `_plot<T>`, and in each class's factories. The three disagree:

- the same factory takes different arguments on a plot and on a panel (finding 1.9): a panel's `line`, `waterfall` and `parametric_curve` have no `marker`, and a plain plot has no `projection`;
- `plot_type` is dropped silently on the parametric-curve path, except for `Projections`;
- a panel given projection data as arrays returns `(None, None)`;
- `plot([t, d0, d1], plot_type=Projections)` dies with a `TypeError` unless `graph_type=ParametricCurve` is also given;
- a projection plot's `line`, `colormap` and `histogram2d` raise `RuntimeError("Not implemented")` instead of `NotImplementedError`;
- `waterfall` has no bindings entry, so its keyword is still `metaData`;
- unknown keywords reach shiboken and fail with its overload listing, except for three kinds that have a hand-written table.

## Scope

In:

1. One table in Python describing every kind `plot()` can make.
2. `plot()` driven by that table, with four rules (below).
3. C++ factory signatures aligned so a plot and a panel take the same keywords.
4. The silent failures above turned into results or clear errors.
5. A parity test that keeps the table and the C++ classes in step.

Out:

- Return shapes stay as they are (a plot returns the graph, a panel returns `(plot, graph)`). Unifying them is a 1.0 change for SciQLop.
- Item factories and `plot.remove` (items 5.2, 5.3, task 12): next item.
- `add_reference_curve` (projection plots only) and `band`/`error_bars`: they are not `plot()` kinds.
- Turning the 3-array `graph_type` warning into an error: 1.0, SciQLop's XY plots rely on it.

## The table

The table lives in `SciQLopPlots/__init__.py` (or a small `SciQLopPlots/_dispatch.py`), keyed on its own kinds, not on `GraphType`:

- `bars` has no `GraphType`;
- `GraphType.Histogram` has no C++ entry point;
- colour maps return a different interface from graphs.

```python
@dataclass(frozen=True)
class Kind:
    shapes: frozenset[str]              # "arrays", "callback", "list"
    factory: Mapping[type, str]         # host class -> method name
    options: frozenset[str]             # keywords of this kind only
    native: frozenset[str]              # keywords the factory takes itself (e.g. "name")
    graph_type: GraphType | None        # what graph_type= selects it, if any
```

Rows, one per kind: line, scatter (a line with a marker preset; `line(marker=...)` keeps working), parametric curve, projection, colour map, waterfall, 2-D histogram, histogram, bars.

Hosts: `SciQLopPlot`, `SciQLopTimeSeriesPlot`, `SciQLopNDProjectionPlot`, `SciQLopMultiPlotPanel`. A host missing from a row's `factory` does not support that kind.

The table is not mirrored in C++. The C++ factories return different types, so a C++ table would need variant returns. The parity test (below) checks the C++ classes against the Python table instead.

## The four rules

1. **The data's shape selects the kind; `graph_type` only validates it.** Shapes: arrays (2 for a line, 3 for a colour map), a callback, a list of arrays. Three arrays with a `graph_type` other than `ColorMap` keeps today's `DeprecationWarning` until 1.0.
2. **A list of arrays means a projection**, with or without `graph_type`. `plot_type` only says which container gets it. On a host that can't make a projection, rule 4 applies.
3. **Panel-only keywords (`plot_type`, `index`) are keyword-only on the graph factories only.** `create_plot(index, plot_type)` and `add_timeline(lane_height, index)` keep their positional forms: SciQLop calls them positionally.
4. **An unsupported kind on a host raises `NotImplementedError` naming the host class**, for example `histogram is not supported by SciQLopTimeSeriesPlot`. The table raises before the call. The C++ base `plot_impl` stubs that throw `std::runtime_error("Not implemented")` move to the project's `NotImplementedError` path too, so C++ callers and direct factory calls get the same answer.

Unknown keywords raise `TypeError` naming the kind that owns the keyword when there is one (today's `_reject_foreign_kwargs` message), else listing the kind's accepted keywords.

`name=` is applied by the factory when it is in the row's `native`, else with `set_name` afterwards, as today.

## C++ alignment

- A panel's `line`, `waterfall` and `parametric_curve` gain `marker`, **appended after `index`**, so existing positional calls keep their meaning.
- `line`, `scatter`, `parametric_curve` and `waterfall` gain a native `name=`, **appended last**. `set_name` keeps working (SciQLop uses it after creation).
- `waterfall` gets its bindings entries (plot and collection): `metaData` → `metadata`, `labels`/`colors`/`marker`/`plot_type`/`index` named like the other factories. This is a straight break for `metaData=`, with no shim, like Phase 1; it goes in the changelog. SciQLop never passes it.
- `SciQLopNDProjectionPlot` factories it can't serve raise `NotImplementedError` (rule 4).

## Silent failures fixed

Each gets a test that fails first:

- `plot_type` other than `Projections` is no longer dropped on the parametric-curve path.
- A panel given a list of arrays with `plot_type=Projections` builds a projection plot; with `plot_type=TimeSeries` or `BasicXY` it raises rule 4's error. It never returns `(None, None)`.
- `plot([t, d0, d1], plot_type=Projections)` works without `graph_type`.
- A projection plot's `line`/`colormap`/`histogram2d` raise `NotImplementedError`.

## Must keep working (SciQLop, checked read-only)

| call | where |
|---|---|
| `target.plot(callback, labels=, graph_type=, plot_type=)` | time_sync_panel.py:995-1010 |
| `target.plot(x, y, z, **kw)`, incl. `graph_type=ParametricCurve` from XY plots | time_sync_panel.py:1016-1029, _plots.py:669-686 |
| `target.plot([t, d0, d1], plot_type=Projections, graph_type=ParametricCurve)` | test_diagnostic_overlay.py:73 |
| `proj.plot(callable, name=, labels=, graph_type=ParametricCurve)` | scene_plot.py:68, 82, 212 |
| `plot.scatter(x, y, labels=, marker=)` | _renderer.py:285-289 |
| `plot_impl.waterfall(x, z.T, labels=[], colors=[])` | _graphs.py:788-801 |
| `histogram2d(x, y, name=, x_bins=, y_bins=, x_bins_log=)` | _graphs.py:816-865 |
| `create_plot(index, PlotType)` (positional) | scene_plot.py:244 |
| `add_timeline(lane_height, index)` / `add_timeline(index=)` | _panel.py:491-516 |
| `add_reference_curve([t, *dims])` | _plots.py:1302 |
| `plot.line(x, y, ["a"])` (positional labels) | SciQLop tests/test_plot_shortcuts.py:38 |
| `set_name` after creation | _panel.py:367-374, _graphs.py:802 |

The SciQLop suite and the four plugins run against the build before the work is called done.

## Tests

- **A dispatch walk:** for every kind × shape × host, the call works where the table says it does and returns the right shape (graph on plots, `(plot, graph)` on panels). It raises `NotImplementedError` naming the host where the table says it doesn't.
- **Signature parity:** for every row and host, the C++ factory's overloads (read from shiboken's overload listing in the method's `__doc__`, not `inspect.signature`, which only shows one) accept exactly the row's `options` plus the shared keywords. A C++ signature change that the table doesn't follow fails this test.
- The existing dispatch tests (test_api_footguns, test_p1_bugs, test_waterfall, test_histogram2d, test_histogram1d, test_plot_name_kwarg, test_scatter_kwargs, the projection tests, test_api_hygiene) keep passing unchanged, except for `metaData=` on waterfall.
