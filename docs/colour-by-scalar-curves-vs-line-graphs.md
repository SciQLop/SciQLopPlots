# Colour by a scalar: curves vs line graphs (decision record, 2026-09-21)

Read this before touching "colour a plot by a scalar" again. It answers the recurring question "why can curves do it and line graphs not".

## The two painters

| | Curves | Line graphs |
|---|---|---|
| Class | `SciQLopCurve` -> `SciQLopTimeColoredCurve` (a `QCPCurve` subclass, **our code**) | `SciQLopLineGraph` -> NeoQCP `QCPMultiGraph`; `SciQLopSingleLineGraph` -> NeoQCP `QCPGraph2` |
| Painting | `draw()` in this repo, segment by segment, colour looked up in a 256-entry LUT | NeoQCP, data-source based, with a GPU (QRhi) path; shared x across components; the resampler decimates before drawing |
| Colour by scalar | **Yes**, line and markers. `set_color_data(values, gradient)`; NaN is a gap | `QCPGraph2`: markers only (`setScatterColorValues`). `QCPMultiGraph`: **yes since 2026-09-23**, line and markers, through decimation (`setColorValues`); NaN is a gap |
| Where a change goes | This repo | **NeoQCP** (separate repo): push to its origin, then bump the pin in `subprojects/NeoQCP.wrap`. Colour values must also survive decimation (indexed by original data index, as `QCPGraph2` does with `dataIdx`). |

The colour *scale* (range, log, gradient, the bar on screen) is independent of the painter. It is `QCPColorScale`, and every `SciQLopPlot` already owns one (colormaps use it). Whatever paints only has to read range, log and gradient from it.

## Status

1. **Projection plots: done (0.37.0).** One shared scale per plot.
2. **Curves on ordinary plots: done (0.37.0).** The same mechanism, in `ColorScaleController` (`include/SciQLopPlots/ColorScaleController.hpp`): a `SciQLopPlot` owns one for its curves, `SciQLopNDProjectionPlot` owns one for all its panes (and switches the panes' own off with `set_curve_color_scale_enabled(False)`, so there is never more than one). A time series can be coloured today by plotting it as a parametric curve (`graph_type=ParametricCurve`, x = time), with a real scale. A colormap always wins the scale, whichever came first: added later it takes the scale over and the curves fall back to their own range and gradient; removed, the scale goes back to the curves, or is hidden if none is coloured. A hidden graph does not count. Python: `plot.z_axis()`, `plot.z_auto_range()`, `plot.set_z_auto_range()`, `plot.set_z_gradient()`, `plot.set_curve_color_scale_enabled()`.
3. **`SciQLopLineGraph`: done (2026-09-23).** NeoQCP's `QCPMultiGraph` now colours line segments and markers by a scalar, on the GPU and CPU paths, and keeps the colours through decimation by carrying source indices (design: `docs/superpowers/specs/2026-09-23-line-graph-colour-by-scalar-design.md`). `SciQLopLineGraph.set_color_data(values, gradient)` uses it: one value per x sample, shared by all components; NaN is a gap; a wrong length raises. A same-length refresh keeps the colours, another length drops them. The line graph is a source of the same `ColorScaleController` as curves, through three `SciQLopGraphInterface` virtuals (`has_color_values`, `color_range`, `attach_color_scale`): one scale shared with coloured curves, colormap wins, pinning, log, hidden graphs do not count.
   - A single-column line plot is still a `SciQLopSingleLineGraph` (`QCPGraph2`): markers only, and not on the shared scale.

## Where part 3 lives
- NeoQCP: `QCPMultiGraph::setColorValues` and friends (`src/plottables/plottable-multigraph.cpp`), pinned in `subprojects/NeoQCP.wrap`.
- Here: `src/SciQLopLineGraph.cpp`, and `ColorScaleController::sources_of()` for the shared scale.
