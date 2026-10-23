# API review, 2026-10-20

Two independent reviews of the whole public Python API (0.52.0 + plot groups on main), merged here:
one by Claude, one by opencode (DeepSeek V4.1 Flash), each without seeing the other's.
Claude checked opencode's surprising claims at runtime before merging.

Marks: **[both]** found by both reviews, **[C]** Claude only, **[O]** opencode only.
Severity: **H** high, **M** medium, **L** low. Claims are verified unless marked *(uncertain)*.

## The philosophy (both reviews agree)

1. A Qt widget library: plots are widgets, state changes are signals, styling takes Qt types.
2. Numpy in, no copies: data is a buffer read in place; time is float seconds, UTC.
3. One call, data decides: `plot()` dispatches on the arguments, `graph_type=` overrides.
4. snake_case, `set_x` / `x` / `x_changed`.
5. Fail loudly on misuse.
6. Live data is first class: callables and remote pipelines, invisible resampling.
7. Plots own what they draw; everything is inspectable.

Most findings are places where a feature was added per class instead of at a base, so one of these
rules holds in some classes and not in others.

## 1. Inconsistencies

| # | Sev | Finding | Evidence |
|---|---|---|---|
| 1.1 | H | **[C] Two ownership models.** Plottables and groups belong to the plot; items (text, ellipse, spans, lines, pixmaps) belong to their Python object. `SciQLopTextItem(plot, ...)` without keeping the result disappears at the next GC. | `tests/integration/test_item_ownership.py:1-15` |
| 1.2 | H | **[both] Items come from constructors, plottables from plot factories**, and removal has five forms: `remove_plottable`, `remove_group`, `delete_span`, `del item`, `item.deleteLater()`. | guide `user-guide.md:526-534` |
| 1.3 | H | **[O] `name` is a property on concrete classes and a method elsewhere.** `graph.name` is a `str` (so `graph.name()` raises `TypeError`), `component.name` is a method, `plot.set_name` doesn't exist, axes use `label`. The property is copy-pasted 21 times in `bindings.xml`. | runtime; `bindings.xml:237,451,595,...` |
| 1.4 | H | **[O] `metaData=` is the only camelCase keyword**; `metadata=` raises. | runtime; `bindings.xml:281,295,...` |
| 1.5 | H | **[both] camelCase leaks:** panel `setSelected` (and *no* `set_selected`: `panel.set_selected` raises), `addWidget/removeWidget/indexOf`, `addObject/removeObject/replotAll/updatePlotList`, model `addNode/setName/nameChanged`, behaviour slots `plotAdded/panelAdded`. | runtime; `SciQLopMultiPlotPanel.hpp:201` |
| 1.6 | M | **[both] Signal spellings:** `selection_changed` (10 classes) vs `selectionChanged` (spans, panel). | API surface |
| 1.7 | M | **[both] Factory names follow several schemes:** `line/scatter/colormap/histogram2d/waterfall/parametric_curve`, `add_histogram2d/add_waterfall/add_timeline/add_remote_*`, `add_reference_curve/add_model_curve`, `create_plot/add_plot`. `histogram2d()` and `add_histogram2d()` coexist. | API surface |
| 1.8 | M | **[O] Return shapes differ:** `plot.plot()` returns a graph, `panel.plot()` returns `(plot, graph)`. The library itself has to test `isinstance(result, tuple)`. | `__init__.py:88`; guide `:368-370` |
| 1.9 | M | **[O] The same factory takes different arguments on a plot and on a panel:** `panel.line(..., marker=...)` raises, `plot.line(..., marker=...)` works; `projection()` exists only on the panel. | `SciQLopPlotCollection.hpp:280-304` vs `SciQLopPlotInterface.hpp:296-325` |
| 1.10 | M | **[both] One concept, several words:** `set_read_only` / `set_movable` / `set_editable`; `set_tool_tip` / `set_tooltip` / both `tool_tip` and `tooltip` on one class; `visible()` vs legend `is_visible()` (also `is_collapsed`, `is_time_axis`). | API surface |
| 1.11 | M | **[both] Range setters:** axis `set_range(start, stop)`, span `set_range(range)`; the code comments on it. | `__init__.py:283-290` |
| 1.12 | M | **[O] Four ways to set the time range** (panel `set_time_axis_range`, time-series `set_time_range`, `x_axis().set_range`, auto-fit), none on a plain plot, no `time_range()` getter. The guide lists them all as equivalent. | `user-guide.md:340-343` |
| 1.13 | M | **[O] Data vs style containers:** data must be a buffer, style takes lists, but `set_offsets`, `SciQLopCurve.set_time_values`/`set_color_values` take lists while `set_color_data` (same class) takes a buffer. | `SciQLopCurve.hpp:109-118` |
| 1.14 | M | **[O] Three failure styles:** exceptions, `WARN_ABSTRACT_METHOD` warnings (≈170 sites in public headers), and silent `False` (the save family). The same null `plot` argument raises `RuntimeError` in one item, `ValueError` in another. | `SciQLopTextItem.hpp:96` vs `SciQLopPixmapItem.hpp:105` |
| 1.15 | M | **[O] Spans: three classes, three APIs** (`range` / `key_range`+`value_range` / properties on the multi-plot span); `span.selected = True` works on `MultiPlotsVerticalSpan`, not on `SciQLopVerticalSpan`. Vertical/horizontal lines are subclasses that only fix an `orientation` argument. | `SciQLopRectangularSpan.hpp:71-140`; `bindings.xml:1162-1168` |
| 1.16 | L | **[both] Two crosshair switches:** `enable_cursor()` and `set_crosshair_enabled()`; `show_crosshair_at_key` is a no-op while disabled. | `SciQLopPlot.hpp:371-398` |
| 1.17 | L | **[C] Colour-map scale in two places:** `cmap.set_gradient/set_y_log_scale/set_z_log_scale` and `plot.set_z_gradient` / `axis.set_log`. Precedence isn't stated. *(uncertain on precedence)* | API surface |
| 1.18 | L | **[O] `line` vs `scatter`:** `line(marker=...)` and `scatter(...)` make the same thing. | `SciQLopPlotInterface.hpp:295-310` |
| 1.19 | M | **[O] Thread contract:** item setters queue to the GUI thread, getters don't; GIL release (`allow-thread`) is set for graphs, not plots or items. *(uncertain)* | `bindings.xml:233-235` |

## 2. Half-baked

| # | Sev | Finding | Evidence |
|---|---|---|---|
| 2.1 | H | **[C] `legend.set_position()` / `position()` only warn.** Legend placement (corner, outside, columns) isn't possible, though NeoQCP does it. | `src/SciQLopPlotLegend.cpp:48-57` |
| 2.2 | H | **[C] A projection plot answers `None` for `x_axis/x2_axis/y_axis/y2_axis`**, so generic code over `panel.plots()` crashes on it. `WidgetPlot` and the projection fake `time_axis()` with a dummy axis that ignores `set_range`. | `SciQLopNDProjectionPlot.hpp:241-244`; `SciQLopPlotAxis.hpp:273-291` |
| 2.3 | H | **[both] The `.on` reactive layer covers a hand-picked slice:** graph `data`, five waterfall settings, axis `range`, vertical span `range`/`tooltip`. Nothing for visibility, colour, selection, timelines, other spans, legends, colour maps, plots. And `graph.data` is a method while `graph.on.data` is a value: a parallel API, not sugar. | `__init__.py:238-313` |
| 2.4 | H | **[O] Contour labels are public but draw nothing** (the header says so). | `SciQLopColorMap.hpp:150-173` |
| 2.5 | H | **[O] Curve colouring has two half-finished paths** (`set_color_data` + enum gradient vs `set_time_values`/`set_color_values` + two-colour gradient), with no getters for the second. | `SciQLopCurve.hpp:109-118` |
| 2.6 | M | **[both] State changes without signals:** colour-map gradient, log scales and percentiles; plot theme; item colour; group visibility and name. The reactive layer can't observe them. | `SciQLopColorMapBase.hpp:159-162` |
| 2.7 | M | **[O] Stubs on concrete classes:** `panel.move_panel(panel, to)` only warns; `uuid()` warns on every call though implemented; `observe(obj, "signal")` is an empty overload; `SciQLopTimeline.set_data` warns and does nothing. | `SciQLopMultiPlotPanel.hpp:219-222`; `SciQLopPlotInterface.hpp:132-136`; `SciQLopGraphInterface.hpp:452-459` |
| 2.8 | M | **[C] `set_movable` without `movable()`** on items (only straight lines have both). **[O] `set_auto_contour_levels` / `auto_contour_level_count`** asymmetric names. | API surface; `SciQLopColorMap.hpp:90-97` |
| 2.9 | M | **[O] Stringly typed timeline state** (`style`, `stack`, `snap_mode`, `edit_modes`): unknown values are ignored in C++. Histogram normalization is a bare int. Everything else uses enums. | `SciQLopTimeline.hpp:84-108`; `SciQLopHistogram2D.hpp:73-74` |
| 2.10 | M | **[O] Two timeline signals for one event:** `interval_created` and `interval_created_in` (with category). | `SciQLopTimeline.hpp:119-127` |
| 2.11 | M | **[O] `AxisType` covers 4 of 6 axes** (no x2/y2); `axis(anchor, index>0)` returns `None`. | `SciQLopPlotInterface.hpp:174-211` |
| 2.12 | L | **[O] Histogram2D has the colour-map base but not contours or `auto_scale_y`.** | `SciQLopColorMap.hpp:68-164` |
| 2.13 | L | Plot groups (this week): no tree node, no items as members, no JS API. Deferred on purpose. | `docs/plans/2026-10-19-plot-groups-design.md` |

## 3. Footguns

| # | Sev | Finding | Evidence |
|---|---|---|---|
| 3.1 | H | **[C] 40 protected `m_*` fields are writable from Python** (`plot.m_legend`, `proj.m_plots`, `m_pane_names`, `graph.m_metaData`, `m_in_legend`, ...). | API surface dump |
| 3.2 | H | **[O] `plot(x, y, z, graph_type=GraphType.Line)` returns a `SciQLopColorMap`**: the arity wins and `graph_type` is ignored silently. | runtime |
| 3.3 | H | **[O] A Python exception in a data callback becomes a console warning and a gap.** No traceback in the notebook, no signal. | `SciQLopGraphInterface.hpp:370-384` |
| 3.4 | M | **[both] Namespace leaks:** `from SciQLopPlots import *` brings `np`, `sys`, `datetime`, `traceback`, Qt modules; no `__all__` (while `dsp.py` has one). Internal plumbing is public too (`SQPQCPAbstractPlottableWrapper`, `QIntList`, `*Ptr`, `plot_impl`, `sync_components`, `check_batch_size`, `is_pane_curve`, ...). | `SciQLopPlots/__init__.py:5` |
| 3.5 | M | **[O] `SciQLopPlotRange` assignment drops the time flag:** the copy constructor keeps `_is_time_range`, `operator=` doesn't. **[C]** `start_date()` truncates to whole seconds and returns local time. | `SciQLopPlotRange.hpp:112-121` |
| 3.6 | M | **[O] `remove_plottable(g)` deletes `g`**; the handle then raises "C++ object already deleted". Not documented as consuming. | runtime |
| 3.7 | M | **[O] A theme is single-use:** `set_theme` reparents it, so sharing one theme between plots moves it. | `bindings.xml:1121-1125` |
| 3.8 | M | **[O] Colouring a line by data is turned off with an empty array**, and "no colour data" can't be asked from Python. | `SciQLopLineGraph.hpp:72-110` |
| 3.9 | M | **[O] `save()` returns `False` for every failure** (bad path, unknown format, unsupported class). | runtime |
| 3.10 | L | **[both] Leftovers:** `SciQLopTimeSeriesPlot.unity()` (identity), `LineTermination.SPikeArrow` typo, `SciQLopSingleLineGraph*` aliases shown as current. | `SciQLopTimeSeriesPlot.hpp:34-36` |
| 3.11 | L | **[O] `SciQLopPlotRange(30, 10)` silently becomes (10, 30)**: a reversed axis can't be expressed. *(likely by design)* | runtime |

## 4. Missing

| # | Sev | Missing | Note |
|---|---|---|---|
| 4.1 | H | **[both] Fill / bands / error bars.** No fill under or between curves, no error bars. The scene boundaries (median + min + max) are a band drawn as three lines. NeoQCP has `QCPErrorBars` and channel fill. | |
| 4.2 | H | **[O] Plot title** that reflows with the layout (today: a positioned text item). | |
| 4.3 | H | **[both] 1-D histogram / bars** (`QCPBars` exists in NeoQCP). | |
| 4.4 | H | **[C] Legend placement** (see 2.1). | |
| 4.5 | M | **[both] Panel-level legend**: one call for every plot's legend. | |
| 4.6 | M | **[O] `plot.clear()`**, **[C] one `plot.remove(obj)`** for plottables, items and groups. | |
| 4.7 | M | **[O] SVG / bytes export** (QtSvg is already linked). | |
| 4.8 | M | **[O] Tick format control** (date format, scientific notation) beyond hand-set labels. | `SciQLopPlotAxis.hpp:130-144` |
| 4.9 | M | **[O] Line width / marker on a whole line graph** (exists on curves, per component on line graphs). | |
| 4.10 | L | **[O] `plot.axes()` iteration, `plot.data_range()`, an axis scale enum** (instead of `set_log(bool)`), **state save/restore**. **[C] polar plots.** | |

## 5. What should be generalized

| # | One-offs today | One shared mechanism |
|---|---|---|
| 5.1 | `.on` registered by hand per property [both] | Derive observable properties from the `x` / `set_x` / `x_changed` convention for every class; the hand list becomes the exceptions. |
| 5.2 | Factories, removal, ownership [both] | `plot.<kind>(...)` for everything drawn (items included), one `plot.remove(x)`, one ownership model. |
| 5.3 | `visible`/`selected`/`name`/`color` per class [C] | One small "drawn object" interface with signals, shared by plottables, items, groups and legend rows (the plot groups had to dispatch per capability for lack of it). |
| 5.4 | The `line/scatter/...` dispatch written three times [O] | One table `GraphType → factory, arity, options`, used by the plot, the panel and `plot()`. Ends 1.9 and 3.2. |
| 5.5 | Five save methods × three classes [O] | One `save(path, *, format=None, ...)` that raises on failure. Adds SVG for free. |
| 5.6 | Three span classes, two line classes [O] | One span with `key_range`/`value_range` and a scope (one plot / a panel); one line with `orientation`. |
| 5.7 | Date coercion in four places [O] | One `to_epoch_seconds()` used by every entry point. |
| 5.8 | `WARN_ABSTRACT_METHOD` [both] | Raise `NotImplementedError` at the binding boundary (one macro change), so Python never gets a silent no-op. |
| 5.9 | `name` property ×21, `read_only`/`movable`/`editable` [both] | Declare once on the bases; one `editable` concept with a getter and a signal. |
| 5.10 | Plot vs panel vs `WidgetPlot` contracts [both] | One "plot container" interface that every member really implements, or a smaller base for "something in a panel" so projections and widgets don't fake axes. |

## Not adopted from the reviews

- **[O] "Hovered/selected-axes shortcuts don't belong in the library"** (`rescale_hovered_or_selected_axes`, ...): they were moved there on purpose on 09-11, as plain methods for SciQLop to bind its own keys. By design.
- **[O] "Gap detection on by default surprises"**: a deliberate default for instrument data; the guide documents `set_gap_threshold(0)`. Debatable, not a bug.
- **[O] "Timeline `set_in_legend` has no legend model"**: outdated, fixed on 10-19 (timelines can be grouped and kept out of the legend).
- **[O] `SciQLopPlotTimeRange` in `__repr__`**: not seen at runtime (marked uncertain by opencode).

## Suggested order

1. **Hygiene, no behaviour change:** `__all__` (3.4), hide `m_*` fields and plumbing in `bindings.xml` (3.1), snake_case aliases for the camelCase names and signals with the old ones kept (1.4, 1.5, 1.6), `metadata=` accepted (1.4), `SPikeArrow`, `unity()`, the `operator=` time flag (3.5).
2. **Footguns:** `graph_type` vs arity raises (3.2), data-callback errors surfaced (3.3), stubs raise `NotImplementedError` (5.8), projection axes (2.2), item ownership (1.1).
3. **Features with the most demand:** fill/bands/error bars (4.1), legend placement and a panel legend (2.1, 4.5), title (4.2), 1-D histogram (4.3).
4. **Unification, one at a time:** dispatch table (5.4), `.on` from conventions (5.1), `save` (5.5), drawn-object base and `remove` (5.2, 5.3), spans (5.6).

## Progress

**Phase 1, hygiene: done** (47801cd). `__all__`; `m_*` fields and plumbing hidden; snake_case twins for the camelCase names; `SciQLopPlotRange` assignment, UTC and millisecond fixes; `unity()` gone, `SpikeArrow`. One deliberate break: `metaData=` is `metadata=`, with no shim, because a Python shim hid shiboken's signature listing on bad calls.

**Phase 2, footguns: done**, checked against SciQLop's full suite (4690 tests) and the four plugins using SciQLopPlots (718 tests).

| Task | Outcome |
| --- | --- |
| 7 Unsupported calls | Raise `NotImplementedError` from Python (shiboken's `storeErrorOrPrint`, safe in `noexcept` stubs); from C++ they print. Stubs that were hooks or honest defaults became plain defaults (behaviour hooks, a plottable's axes, the dummy axis's selection). The pixmap item got the geometry it lacked. |
| 8 Arity vs `graph_type` | `DeprecationWarning` instead of an error: SciQLop's XY panels pass `ParametricCurve` with three arrays. An error at 1.0. |
| 9 Data-callback errors | `graph.fetch_failed(message)`; a failed range is not recorded as loaded. |
| 10 Projection axes | Rejected: `x_axis()` returning None means "not on the panel's x axis", and the panel's x sync relies on it. |
| 11 `name` | Deferred to 1.0, done with SciQLop: `name()` methods everywhere was chosen, but SciQLop reads `impl.name` as a property on panels. |
| 12 Item factories | Moved to phase 4, with `plot.remove(x)` and one ownership model. |
| 13 `save()` | Raises `OSError("failed to save ...")`, `ValueError` for an unknown format. |
| (found) | A second colour map on a plot raised nothing and returned None; it raises now. |

**Phase 3, features: 5 of 5 done.**

| Task | Outcome |
| --- | --- |
| 15 Legend | `set_alignment`, `set_position` (fractions of the plot area), `set_columns`; `panel.set_legend_visible` (4102b7f). |
| 16 Title | `plot.set_title()`, themed; the colour scale now follows the plot area's row (73d93ca). |
| 18 SVG | `save_svg()`, `save()` writes `.svg`; no `format=` keyword (d62aa8c). |
| 14 Bands, error bars | Done. Fills and bands as column decorations, error bars that become a band when dense, `plot.band` and `plot.error_bars`. Side fix: a callback batch with the wrong number of arrays now emits `fetch_failed` instead of vanishing, which also fixed 2-pane projection callbacks. |
| 17 1-D histogram | Done. `SciQLopHistogram` over NeoQCP's `QCPHistogram`: `plot.histogram` (values or time-range callback), `plot.bars(edges, heights)`, bins, range, log bins, normalization, bars or step. A per-plot opt-out from the panel's x sync keeps the value axis still. Guide section "Histograms". |

**Phase 3 gate: met.** Every feature has a guide section and a picture (band, fills, error bars sparse and dense, histograms, bars, legend placement, title). Checking an SVG export found graphs drawn outside the plot area; fixed by writing SVG 1.1, which keeps clip paths.

**Phase 4, unification: item 5.4 done.** One dispatch table (`SciQLopPlots/_dispatch.py`) drives `plot()` on plots and panels. `plot_type` is panel-only; unknown keywords raise `TypeError`; an unsupported (kind, host) raises `NotImplementedError`. Panel factories take `marker` and `name` like plots, and projection plots apply the marker and colours they are given. One break: waterfall's `metaData=` is `metadata=`. Item 5.3 part A done: SciQLopDrawnObject under plottables, components, items, spans, panel spans and lines, groups and the legend. Fixed on the way: graph.set_selected() did nothing, the timeline never signalled visible_changed, the name property bypassed set_name(), text/pixmap tooltips and text/ellipse set_movable were stubs, a graph hidden or selected before its first data came up shown and unselected (an empty graph now reads its last set value, visible by default). Breaks: C++ overrides of these accessors move to the _x hooks and drop noexcept; color/selected raise NotImplementedError where the kind has none; setObjectName() no longer emits name_changed (set_name does); the Q_PROPERTY(selected) declarations on graphs and components are gone. Deprecated aliases: span selectionChanged, legend is_visible/visibility_changed. Part B (factories, plot.remove, ownership) is next.
