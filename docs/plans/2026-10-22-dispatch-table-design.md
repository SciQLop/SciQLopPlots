# One dispatch table for plot() and the factories

Status: design for API review item 5.4 (docs/api-review-2026-10-20.md), Phase 4. Revision 3, after three opencode reviews (draft, revision 1, revision 2).

## Why

The `line/scatter/...` dispatch is written three times: in Python `plot()`, in the panel's C++ `plot_impl`/`_plot<T>`, and in each class's factories. They disagree, and several paths fail silently:

- the same factory takes different arguments on a plot and on a panel (finding 1.9): a panel's `line`, `waterfall` and `parametric_curve` have no `marker`;
- `plot_type` is dropped on the parametric-curve path unless it is `Projections`;
- `panel.line(x, y, plot_type=Projections)` and `panel.plot(x, y, plot_type=Projections)` return `(None, None)` and add nothing;
- `plot([t, d0, d1], plot_type=Projections)` dies with a `TypeError` unless `graph_type=ParametricCurve` is also given;
- a plain plot given a list of arrays, and a projection plot's `line`/`colormap`/`histogram2d`, raise `RuntimeError("Not implemented")`;
- `waterfall` has no bindings entry, so its keyword is still `metaData`;
- unknown keywords reach shiboken and fail with its overload listing, except for the three kinds with a hand-written table.

## Scope

In:

1. One table in Python describing every kind `plot()` can make.
2. `plot()` driven by that table.
3. C++ factory signatures aligned so a plot and a panel take the same keywords.
4. The silent failures above turned into results or clear errors, through `plot()` and through the factories.
5. A dispatch walk and a signature-parity test that keep the table and the C++ in step.

Out:

- Return shapes stay as they are: a plot returns what it made, a panel returns `(plot, what it made)`. Unifying them is a 1.0 change for SciQLop.
- `line(marker=...)` vs `scatter(...)` (finding 1.18): both stay separate factories with different default markers; this item doesn't merge them.
- Item factories and `plot.remove` (items 5.2, 5.3, task 12): next item.
- `bars`, `band`, `error_bars`, `add_reference_curve`: called directly, not through `plot()`. `bars` has no `GraphType`.
- Turning the 3-array `graph_type` warning into an error: 1.0, SciQLop's XY plots rely on it.
- Keyword-only panel arguments: shiboken can't mark parameters keyword-only, so `plot_type` and `index` stay positional-or-keyword, documented as panel-only.

## The table

In `SciQLopPlots/_dispatch.py`, imported by `__init__.py`. Keyed on its own kinds, not on `GraphType` (colour maps and histograms return different interfaces from graphs; `GraphType.Histogram` has no C++ entry point).

```python
@dataclass(frozen=True)
class Shape:
    name: str                    # "arrays", "callback" or "list"
    nargs: int | None            # positional count for "arrays"; None otherwise
    options: frozenset[str]      # keywords the C++ factory overload takes for this shape
    post: frozenset[str] = frozenset()   # keywords plot() applies after the factory returns

@dataclass(frozen=True)
class Kind:
    graph_type: GraphType | None
    shapes: tuple[Shape, ...]
    factory: Mapping[type, str]  # host class -> method name; a missing host = unsupported
    native: frozenset[str]       # keywords the factory takes itself, beyond the common ones
    returns: type                # what a plot returns; a panel returns (plot, returns)
```

Common keyword, accepted by every factory: `metadata`. `labels` and `colors` are per-shape options of the curve kinds and of projection: colour map, 2-D histogram and histogram don't take them. `name` is native for colour map, 2-D histogram and histogram, and for line, scatter, parametric curve and waterfall once they gain it (below); `plot()` calls `set_name` afterwards only when a factory still lacks it.

| kind | graph_type | shapes (nargs) | hosts → factory | returns |
|---|---|---|---|---|
| line | Line | arrays (2), callback | plot, time series, panel → `line` | graph |
| scatter | Scatter | arrays (2), callback | plot, time series, panel → `scatter` | graph |
| parametric curve | ParametricCurve | arrays (2), arrays (4: x, y, labels, colors), callback | plot, time series, panel → `parametric_curve` | graph |
| projection | ParametricCurve | list `[t, d0, d1, ...]`, callback, arrays (4: t, d0, d1, d2), arrays (2: t, values with one column per dimension) | panel → `projection`; projection plot → `parametric_curve` (its list and callable paths; `plot()` turns the arrays forms into the list). Panels and projection plots only: a single plot raises `NotImplementedError` | graph |
| colour map | ColorMap | arrays (3), callback | plot, time series, panel → `colormap` | colour map |
| waterfall | Waterfall | arrays (2), callback | plot, time series, panel → `waterfall` | graph |
| 2-D histogram | Histogram2D | arrays (2), callback | plot, time series, panel → `histogram2d` | colour map |
| histogram | Histogram | arrays (1), callback | plot, panel → `histogram` | histogram |

Per-shape factory options follow today's overloads: `marker` on line/scatter/parametric curve/waterfall/projection; `sync_with` on every callback shape; `y_log_scale`/`z_log_scale` on colour map; `x_bins`/`y_bins`/`x_bins_log`/`y_bins_log` on 2-D histogram; `bins` on histogram; on panels, `index` on every factory and `plot_type` on every factory except `projection` (neither its list nor its callable overload has one) and `histogram` (it always makes a plain plot: a time-series plot can't hold a histogram, so `plot_type` has nothing to choose).

`post` options are not C++ keywords. Waterfall's `offsets`/`normalize`/`gain` are applied by `plot()` after the factory returns (`_apply_waterfall_kwargs`). Histogram's `range`/`log_bins`/`normalization`/`style` are applied by the Python `histogram` wrapper the row's factory resolves to, so `plot()` must not apply them a second time.

The table consumes `plot_type` itself (it chooses the container and the kind) and passes it on only to factories that take it; it never forwards `plot_type` to `projection`.

The table is not mirrored in C++: the factories return different types, so a C++ table would need variant returns. The parity test checks the C++ against the Python table.

## Dispatch rules

1. **The kind is chosen by `(shape, graph_type)`.**
   - Two arrays: `graph_type` picks line (the default), scatter, parametric curve, waterfall or 2-D histogram.
   - A callback: `graph_type` picks the kind (default line); `plot_type=Projections` on a panel picks projection whatever `graph_type` says.
   - A list of arrays: always projection. A `graph_type` other than `None` or `ParametricCurve` with a list raises `TypeError`.
   - Two arrays with `plot_type=Projections` on a panel: a projection when the second array has one column per dimension (`(t, values)`); else the curve factory raises the `ValueError` below.
   - Three arrays: always colour map. With a `graph_type` other than `ColorMap`, today's `DeprecationWarning` stays until 1.0 (the one case where the shape overrides `graph_type`).
   - One array: histogram (`graph_type=Histogram` required, as today); four arrays: parametric curve.
   - Any other combination raises `TypeError` listing the accepted forms for the given `graph_type`. It is a `PlotArgumentsError`, a subclass of both `TypeError` and `ValueError`: `plot()` raised `ValueError` for these before, and test_p1_bugs still expects it. The class is not exported. Two arrays with `graph_type=Histogram` keep today's message pointing to `plot.bars(edges, heights)`.
   - A `graph_type` that is not a `GraphType` raises `TypeError` before anything else (also a `PlotArgumentsError`, for the same reason).
   - Only a `list` is a list of arrays: a tuple, as in `plot((x, y))`, is more often a forgotten `*` and goes through the arrays rules.
2. **`plot_type` says which container a panel creates.** On a plot, `plot_type` is a keyword the host doesn't take: `TypeError` from the table (today it is an `AttributeError` from shiboken). On a panel, a `plot_type` value that can't hold the shape (two arrays with `Projections`) is a bad value: `ValueError`, from `plot()` and from the direct factory alike.
3. **An unsupported (kind, host) raises `NotImplementedError` naming the host class**, from the table, before any C++ call. The message uses the same form as the C++ path (`report_unsupported_call`, src/UnsupportedCall.cpp): "<what> is not supported by <class>". Tests assert the exception type and the class name, not the full text.
4. **Unknown keywords raise `TypeError`**, naming the kind that owns the keyword when one does (today's `_reject_foreign_kwargs` message), else listing the keywords the chosen kind and shape accept.

## C++ changes

- Panel `line`, `waterfall`, `parametric_curve` and `projection` gain `marker`, and line/scatter/parametric_curve/waterfall gain a native `name`. New parameters go **after `metaData`**, `name` last, so no existing positional call changes meaning.
- `waterfall` gets bindings entries on the plot and collection interfaces, renaming its keywords like the other factories (`metaData` → `metadata`, `labels`, `colors`, `marker`, `plot_type`, `index`). This is a straight break for `metaData=` with no shim, like Phase 1, listed in the changelog. SciQLop never passes it.
- The panel's `plot_impl(x, y, ...)` switch (src/SciQLopMultiPlotPanel.cpp:403-422) no longer returns `(nullptr, nullptr)` for `Projections`: two arrays with `plot_type=Projections` raise `std::invalid_argument` ("projections take a list of arrays or a callback"), mapped to `ValueError`.
- Unsupported factory calls on a host (the base `plot_impl` stubs throwing `std::runtime_error("Not implemented")`, SciQLopPlotInterface.hpp:59-116) throw a dedicated `sqp::unsupported_call` (derived from `std::runtime_error`, so existing C++ `catch (const std::runtime_error&)` sites keep working) instead. One injection template maps it to `NotImplementedError` (and `std::invalid_argument` to `ValueError`), like `void_raises_valueerror` (bindings.xml:69-79). The template must be attached to every factory entry that can reach a stub, on the plot and the collection interfaces: roughly two dozen `modify-function` entries. The direct-factory tests catch a missed one. C++ callers keep getting an exception, not a null return.

## Silent failures fixed

Each gets a test that fails first, through `plot()` and through the direct factory:

- `plot_type` other than `Projections` is no longer dropped on the parametric-curve path.
- `panel.line(x, y, plot_type=Projections)` and `panel.plot(x, y, plot_type=Projections)` raise `ValueError` instead of returning `(None, None)`.
- `panel.plot([t, d0, d1], plot_type=Projections)` works without `graph_type`.
- A plain plot given a list, and a projection plot's `line`/`colormap`/`histogram2d`, raise `NotImplementedError` (was `RuntimeError`).

## Must keep working (SciQLop, checked read-only)

| call | where |
|---|---|
| `target.plot(callback, labels=, graph_type=, plot_type=)`, incl. `plot_type=Projections` | time_sync_panel.py:995-1010 |
| `target.plot(x, y, z, **kw)`, incl. `graph_type=ParametricCurve` from XY plots | time_sync_panel.py:1016-1029, _plots.py:669-686 |
| `target.plot(x, y, graph_type=ParametricCurve)` (XY plots, two arrays) | _plots.py:669-686 |
| `target.plot([t, d0, d1], plot_type=Projections, graph_type=ParametricCurve)` | test_diagnostic_overlay.py:73 |
| `nd.plot([t, X, Y, Z], graph_type=ParametricCurve)` and `proj.plot(callable, name=, labels=, graph_type=ParametricCurve)` | scene_plot.py:68, 82, 212 |
| `plot.scatter(x, y, labels=, marker=)` | _renderer.py:285-289 |
| `plot_impl.waterfall(x, z.T, labels=[], colors=[])` | _graphs.py:788-801 |
| `histogram2d(x, y, name=, x_bins=, y_bins=, x_bins_log=)` | _graphs.py:816-865 |
| `create_plot(index, PlotType)` (positional) | scene_plot.py:244 |
| `add_timeline(lane_height, index)` / `add_timeline(index=)` | _panel.py:491-516 |
| `add_reference_curve([t, *dims])` | _plots.py:1302 |
| `plot.line(x, y, ["a"])` (positional labels) | SciQLop tests/test_plot_shortcuts.py:38 |
| `set_name` after creation | _panel.py:367-374, _graphs.py:802 |

The SciQLop suite (xvfb-run outside `timeout`, `--no-xvfb`) and the four plugins run against the build before the work is called done.

## Tests

- **Dispatch walk:** for every row × shape × host, `plot()` with that shape and `graph_type` returns the row's `returns` (a panel: `(plot, returns)`); for hosts missing from the row, it raises `NotImplementedError` naming the host. The same for the direct factory where the host has one.
- **Signature parity, per overload:** for every row, shape and host, the C++ factory overload for that shape (read from shiboken's overload listing in `__doc__`, not `inspect.signature`) accepts exactly the common keywords, the row's native keywords and that shape's `options` (not its `post` options), plus `index`/`plot_type` on panels where the factory takes them. A C++ signature change the table doesn't follow fails this test.
- The existing dispatch tests (test_api_footguns, test_p1_bugs, test_waterfall, test_histogram2d, test_histogram2d_log_bins, test_histogram1d, test_plot_name_kwarg, test_scatter_kwargs, the projection tests, test_api_hygiene) keep passing unchanged, except for `metaData=` on waterfall.
