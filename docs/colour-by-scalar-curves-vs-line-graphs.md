# Colour by a scalar: curves vs line graphs (decision record, 2026-09-21)

Read this before touching "colour a plot by a scalar" again. It answers the recurring question "why can curves do it and line graphs not".

## The two painters

| | Curves | Line graphs |
|---|---|---|
| Class | `SciQLopCurve` -> `SciQLopTimeColoredCurve` (a `QCPCurve` subclass, **our code**) | `SciQLopLineGraph` -> NeoQCP `QCPMultiGraph`; `SciQLopSingleLineGraph` -> NeoQCP `QCPGraph2` |
| Painting | `draw()` in this repo, segment by segment, colour looked up in a 256-entry LUT | NeoQCP, data-source based, with a GPU (QRhi) path; shared x across components; the resampler decimates before drawing |
| Colour by scalar | **Yes**, line and markers. `set_color_data(values, gradient)`; NaN is a gap | Markers only, and only on `QCPGraph2` (`setScatterColorValues`, about 120 lines over the CPU and GPU paths). **`QCPMultiGraph` has nothing. No NeoQCP plottable colours line segments.** |
| Where a change goes | This repo | **NeoQCP** (separate repo): push to its origin, then bump the pin in `subprojects/NeoQCP.wrap`. Colour values must also survive decimation (indexed by original data index, as `QCPGraph2` does with `dataIdx`). |

The colour *scale* (range, log, gradient, the bar on screen) is independent of the painter. It is `QCPColorScale`, and every `SciQLopPlot` already owns one (colormaps use it). Whatever paints only has to read range, log and gradient from it.

## Status

1. **Projection plots: done (0.37.0).** One shared scale per plot.
2. **Curves on ordinary plots: done (0.37.0).** The same mechanism, in `ColorScaleController` (`include/SciQLopPlots/ColorScaleController.hpp`): a `SciQLopPlot` owns one for its curves, `SciQLopNDProjectionPlot` owns one for all its panes (and switches the panes' own off with `set_curve_color_scale_enabled(False)`, so there is never more than one). A time series can be coloured today by plotting it as a parametric curve (`graph_type=ParametricCurve`, x = time), with a real scale. A colormap always wins the scale, whichever came first: added later it takes the scale over and the curves fall back to their own range and gradient; removed, the scale goes back to the curves, or is hidden if none is coloured. A hidden graph does not count. Python: `plot.z_axis()`, `plot.z_auto_range()`, `plot.set_z_auto_range()`, `plot.set_z_gradient()`, `plot.set_curve_color_scale_enabled()`.
3. **`SciQLopLineGraph`: deferred by decision.** Needs a NeoQCP change. Two sizes:
   - markers only on `QCPMultiGraph`, mirroring `QCPGraph2`: smaller, but a coloured line is still drawn in one colour;
   - markers and per-segment line colour, CPU and GPU paths: what the SciQLop issue (#141 item 7) asks for, and the largest.
   Neither is needed for the Magnetopause notebook or #142. It only buys the fast path (GPU, decimation, shared x) for very large coloured series. Do it when a real case shows a parametric curve is too slow or too heavy.

## If you pick part 3 up
- Start in NeoQCP `src/plottables/plottable-graph2.cpp` around `mScatterColorValues` (lines ~545, 659-740) and copy the idea into `plottable-multigraph.cpp` (`QCPMultiGraph::draw`, line ~645).
- Keep the colour values indexed by *original* data index so decimation cannot shift them.
- Then wire `SciQLopLineGraph::set_color_data` (today the base class throws "does not support per-point colour data") and give it the same shared-scale behaviour as curves.
- Pushing NeoQCP and bumping the pin needs the maintainer's consent. Until then this repo cannot build for anyone else against a local NeoQCP commit.
