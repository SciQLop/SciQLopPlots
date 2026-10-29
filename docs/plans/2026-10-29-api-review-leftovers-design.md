# API review leftovers: design

Status: reviewed by opencode 2026-10-29 (its notes applied), waiting for approval. Source: the items of docs/api-review-2026-10-20.md that no phase
scheduled (completeness audit of 2026-10-29).

Rules for every change:

- Nothing SciQLop or a plugin calls today stops working. A renamed name stays, with a
  `DeprecationWarning` naming the new one (the suite's filter matches `is deprecated: use <Name>.`).
- A renamed signal gets its new name as a second attribute, never a Python wrapper (a wrapped
  signal or connected slot dies on GC). A renamed method warns through a Python wrapper on the old
  name only; the new name stays the plain C++ attribute.
- Reproducer or spec test first; full suites before each commit; opencode review per batch.

## Decisions

### Batch 1: names (consistency, deprecate the old)

| # | Today | Decision |
|---|---|---|
| 1.5 | `PlotsModel.addNode`, `PlotsModelNode.setName` / `nameChanged`, behaviour slots `plotAdded` / `panelAdded` | snake_case twins `add_node`, `set_name` / `name_changed`, `plot_added` / `panel_added`, same mechanism as the panel twins (`_SNAKE_CASE_ALIASES`): plain second names, no warning. |
| 1.7 | `add_histogram2d`, `add_waterfall`, `add_timeline` next to `line`, `histogram2d`; projection `add_reference_curve`, `add_model_curve` | Canonical: `waterfall()`, `timeline()`, `reference_curve()`, `model_curve()` (`histogram2d()` exists). The `add_*` names warn. `add_remote_*` keep their names: a remote source is a different constructor shape, not a factory spelling (decide in review). `create_plot` / `add_plot` differ in meaning (make vs adopt): kept. |
| 1.10 | `set_tool_tip` / `tool_tip` and the `tooltip` property on one class; text and pixmap items use `set_movable` | One word: `tooltip()` / `set_tooltip()`, the old pair warns. Text, pixmap, ellipse and curved-line items get `read_only` like spans and lines; their `set_movable` / `movable` warn and map the inverse (`set_movable(True)` is `set_read_only(False)`). `is_collapsed` / `is_time_axis` stay: `is_` predicates are idiomatic. |
| 1.16 | `enable_cursor()` and `set_crosshair_enabled()` | `set_crosshair_enabled` is the one; `enable_cursor` warns. `show_crosshair_at_key` while disabled: documented no-op (it shows what is enabled). |
| 2.8 | `set_auto_contour_levels(n)` / `auto_contour_level_count()` | getter `auto_contour_levels()`; the old getter warns. |
| 2.11 | `AxisType` has no x2 / y2 | add `XAxis2`, `YAxis2`; `plot.axis(type)` returns them. |
| 5.9 | `name` is a property on 22 binding classes (plottables, plots, panels), a method elsewhere | `name()` / `set_name()` / `name_changed` everywhere: the 24 `<property name="name">` lines go; plots and panels get the C++ trio (`name_changed` follows `objectNameChanged`). A hard break (a value and a method cannot share the name, so no warning): `x.name` reads become `x.name()`, `x.name = s` becomes `x.set_name(s)`. Listed as a deliberate break. |

### Batch 2: small features

| # | Decision |
|---|---|
| 1.12 | `time_range()` getter on time-series plots and panels (the panel's time axis range). The guide names `panel.set_time_axis_range` / `plot.set_time_range` as the way, `x_axis().set_range` as the low-level one. |
| 1.13 | `set_time_values`, `set_color_values`, `set_offsets` accept any 1-D sequence or array (converted once in Python). |
| 3.8 | `line.clear_color_values()` and `line.has_color_values()`, the names the curve already uses; the empty array keeps working. |
| 3.11 | A reversed range stays normalised (`SciQLopPlotRange` is an interval). A reversed axis is `axis.set_reversed(bool)` / `reversed()` / `reversed_changed`, over NeoQCP `setRangeReversed`. |
| 4.6 | `plot.clear()`: removes every plottable, group and item the plot owns (what `plot.remove` takes), not axes or legend settings. |
| 4.7 | `plot.to_bytes(format="png", width=0, height=0, scale=1.0, quality=-1)` and the same on panels and projections, sharing `save()`'s format rules. |
| 4.8 | Tick format: `axis.set_number_format(format, precision)` (`format` in `"g"`, `"f"`, `"e"`, over NeoQCP's `setNumberFormat` / `setNumberPrecision`) and, on a time axis, `axis.set_date_format(str)` (over `QCPAxisTickerDateTime::setDateTimeFormat`). Getters and `_changed` signals. Ignored when labels are set by hand or on a log axis, and the docs say so. |
| 4.9 | `line_graph.set_line_width(w)` / `set_marker_shape(s)` / `set_marker_size(px)` apply to every component, now and later (a component setter still wins after). |
| 4.10 | `plot.axes()` (the plot's axes, x, y, x2, y2, z when present), `plot.data_range()` (union of the plottables' key ranges), axis `set_scale(AxisScale.Linear | Log)` / `scale()` next to `set_log`. State save/restore and polar plots: out of scope (each its own design). |

### Batch 3: signals and ownership

| # | Decision |
|---|---|
| 2.6 | Colour map plottables emit `gradient_changed`, `z_log_changed`, `y_log_changed`, `percentiles_changed`; plots emit `theme_changed`. `.on` picks them up by convention. |
| 3.7 | `set_theme` no longer reparents a theme that already has a parent: a theme can be shared. A parentless theme is still adopted (so a temporary is not lost). The plot follows the theme's `changed` signal and drops it on `destroyed`. |
| 1.17 | Document the colour-map scale precedence (the plottable's setters and the plot's z-axis are the same state; the last call wins) and add a test that pins it. |
| 1.18 | Document: `scatter()` is `line()` without lines and with a marker. No change. |

### Batch 4: half-baked features

| # | Decision |
|---|---|
| 2.4 | Contour labels: implement in NeoQCP. `contour-extractor.h` gives each level's segments; a label sits at the midpoint of each long enough segment, rotated along it. A feature with a picture test: last batch. |
| 2.5 | Curve colouring: the two paths are two features (colour by a data value through an enum gradient; colour by time through a two-colour gradient). Keep both, document both, add the missing getters `time_values()`, `color_values()`, `time_color_gradient()`. |
| 2.10 | `interval_created` and `interval_created_in`: keep both (a signal cannot be overloaded for Python without breaking `connect`); document `interval_created_in` as the full one. |
| 2.12 | Histogram2D: move `auto_scale_y` from `SciQLopColorMap` to `SciQLopColorMapBase` (both hold a `DataSourceWithBuffers`), so Histogram2D gets it. Contours: its result is a `QCPColorMapData`, which the contour extractor takes; with 2.4, in the last batch. |

### Not done, and why

- 1.8 `panel.plot()` returns `(plot, graph)`: by design. A call that creates a container returns
  the container and its content, like matplotlib's `fig, ax = plt.subplots()`; `plot.plot()` creates
  no plot, so it returns the graph only. The guide says so.
- 5.10 one plot-container interface: a hierarchy refactor; its user-visible part (2.2 projection axes) was rejected on purpose. Its own design if wanted.
- 4.10 state save/restore, polar plots: features, each their own design.
