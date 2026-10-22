# Fills, bands, error bars and 1-D histograms

Status: design approved section by section on 2026-10-21, after three opencode reviews. Not implemented.
API review tasks 14 (bands, error bars) and 17 (1-D histogram), see `docs/api-review-2026-10-20.md`.

## Why

SciQLop needs four ways to show uncertainty or area, all on dense time series that pan and fetch through callbacks:

1. an uncertainty band around a value (millions of points);
2. a fill under one curve, down to a baseline;
3. shading between two curves;
4. error bars on sparse points.

It also needs a histogram of values that follows the panel's visible time range.

Today NeoQCP's time-series graph (`QCPMultiGraph`) has no fill at all.
Only the old `QCPGraph` fills, and only its simplest case runs on the GPU.
`QCPErrorBars` and `QCPBars` exist but are CPU only and do not take a multi-column source.
SciQLop's scene "envelope" is three separate dashed curves.

## Scope

v1:

1. **NeoQCP + SciQLopPlots:** fills between columns of one `QCPMultiGraph`, or from a column to a constant. `plot.band()`.
2. **NeoQCP + SciQLopPlots:** error bars from columns of one `QCPMultiGraph`, turning into a band when too dense. `plot.error_bars()`.
3. **NeoQCP:** a per-component "in legend" flag (needed by 1 and 2).
4. **NeoQCP + SciQLopPlots:** `QCPHistogram`, a 1-D histogram that bins in the background, plus pre-binned bars. `plot.histogram()`, `plot.bars()`, `panel.histogram()`.

Not in v1:

- Fills between two *separate* graphs. Two spacecraft usually have different timestamps; pairing two downsampling caches that update at different times is a project of its own. Put the data on one time grid first (a SciQLop virtual product).
- Clicking a fill to select it.
- A gap between a marker and its error bar (`QCPErrorBars::symbolGap`), horizontal error bars.
- A strict min/max envelope of raw data on a downsampled band (see "Fill geometry").

Order of implementation: 3, then 1, then 2, then 4. Error bars add three columns to a graph; without 3 every error-bar graph would show four legend rows.

## The idea shared by fills and error bars

The extra data lives as **extra columns of the same graph**.
An uncertainty band is the graph `[y, y - e, y + e]`; error bars use the same columns.

Why: one data source means one time grid, one downsampling cache, one fetch and one data swap.
A separate band plottable would fetch and swap separately and could drift from its line while panning.

Bounds are stored as absolute values, not as magnitudes.
Absolute bounds are y-like: auto-scale is right without special cases, and they hide like any line.

## Decorations (NeoQCP `QCPMultiGraph`)

Fills and error bars are a graph-level list of decorations, by column index, not fields of `Component`:

```cpp
struct Fill      { int owner; std::variant<double, int> target; QBrush brush; };
struct ErrorBars { int value; int lower; int upper; std::optional<QPen> pen; double capWidthPx = 6; };
```

Why graph-level: components only exist after the first data, and a shrinking source truncates them.
A graph-level list can be set at creation, before any data, and survives components being created or regrown.

Rules:

- Columns are checked at set time: in range of the current column count when known, not equal to the owner.
- Columns are checked again at draw time against the displayed column count; an invalid decoration is skipped.
- When a data swap shrinks the column count, decorations that reference a missing column are dropped. Shrinking is lossy: a later regrow does not bring them back.
- Per-column vectors (`mComponents` side data, line extrusion caches) are resized in one helper, at the point where `draw()` decides the displayed column count.
- Fill and error-bar caches are per decoration, not per column: they live next to their decoration and go away with it.
- Setters invalidate the caches they affect, like `setComponentLineStyle` does.

A band edge is hidden with `lineStyle = lsNone`, not with `visible = false`.
It stays visible as a component, so auto-scale and selection still count it.

## Fill geometry

The fill walks the **displayed data source**, not the per-column pixel lines.
The displayed source is the downsampled result when there is one, the raw source otherwise.
Both share one key array across columns, so row *i* has the same x for both edges.

Why not the pixel lines: each column's line array is built on its own (NaN values dropped per column, adaptive sampling per column, hidden components get no array), so their indices do not line up.

Construction:

1. Each pair of consecutive rows gives one quad between the two edges. With downsampled data this follows the drawn lines, including across bins.
2. A row where either edge is NaN breaks the strip. A key gap breaks it too, with the rule the lines use: the shared key array against `mGapThreshold` (`detectKeyGaps` on raw rows, `gapRatio` on downsampled rows). Computed once for both edges.
3. Where the edges cross inside a segment, the quad is split at the crossing, so the overlap is not blended twice.
4. When the raw rows in view exceed 4× the plot width (no downsampling: small sources under the resampling threshold, log key axis), rows are binned into vertical spans, 4 per pixel column (the lines' own `kLevel2PixelMultiplier`), so the fill is not blockier than a downsampled line. This caps the vertex count; it is local to the fill.
5. Step line styles: the step transform runs before the fill pass, and the fill uses the stepped points. An impulse component refuses a fill.
6. A fill to a constant uses the constant as the second edge. On a log value axis a constant ≤ 0 is clamped to about two plot heights past the axis edge; the scissor clips it. A clamp at the edge would drift during a GPU pan; a far clamp would lose float precision.

The fill pass is its own loop **before** the line loop. The line loop skips components with no line and no marker (`plottable-multigraph.cpp:1084`), which would kill a band's fill.

A fill is drawn only when both of its edge components are visible.

On downsampled data the band spans between the drawn lines. It is not a strict min/max envelope of the raw rows; that would need joint per-row bounds in the resampler. Documented.

Drawing:

- GPU: a separate fill entry through `QCPPlottableRhiLayer::addPlottable(fillVerts, {}, ...)`, with the same pan offset as the lines.
- Fill vertices are cached next to the line caches and cleared in the same place.
- CPU and export: one `QPainter` polygon per gap-free segment (lower edge forward, upper edge back).
- Colour-mapped graphs: the fill uses the brush, not per-run colours.

## Error bars

A bar is drawn per row at the row's x, from `lower` to `upper`, with two caps.

When bars are drawn:

- Only when the graph shows raw rows, tested as `ds == mDataSource.get()` after the downsampling result is re-resolved in `draw()`. A missing downsampled result is not proof of raw rows.
- And only when bars do not touch: no two consecutive rows in view are closer than `3 × capWidthPx`. A mean spacing would hide bursty data.
- Otherwise the error bars are drawn **as a band** between `lower` and `upper` (bar colour, about 25 % opacity), with the fill machinery. The downsampled result carries the bound columns, so this works zoomed out. Uncertainty never silently disappears; it changes form.

Geometry:

- Stem and caps are separate NaN-separated runs through the existing polyline helpers (`drawPolylineCached`, `drawPolylineWithGpuFallback`). GPU, pan offset and export come with them. Only NaN sentinels, never infinities: the CPU splitter ignores infinities.
- A NaN bound is replaced by `y` (a one-sided bar). A bar is drawn while at least two of its three points are finite.
- A bound on the wrong side of `y` is clamped to `y`.
- On a log value axis a bound ≤ 0 is clamped to the axis edge.
- A vertical key axis gives horizontal bars.
- The selected pen is used when the component is selected.
- Error bars have their **own** extrusion cache. Sharing the line cache would swap the line and bar geometry on pan frames.

`QCPErrorBars` stays as it is: it attaches to a 1-D plottable interface that `QCPMultiGraph` does not implement, and it is CPU only.

## Legend

A per-component `inLegend` flag in NeoQCP:

- Band edges and error-bar bounds default to out of the legend.
- The group legend's expanded/collapsed decision, row height, size hint and click-to-row mapping all use the in-legend components. Using the full count would change the group's shape when error bars are added, and clicking a row would toggle the wrong column.
- The collapsed icon only splits its width between in-legend components.
- A band's icon is a filled swatch, drawn directly from the rectangle `drawLegendIcon` already receives.
- A component with error bars gets its marker plus a small bar glyph.

## SciQLopPlots API, fills and error bars

On `SciQLopGraphComponentInterface` (the bound class):

```python
comp.set_fill_to_value(0.0)
comp.set_fill_between(other_comp)
comp.clear_fill()
comp.set_fill_color(color)        # default: line colour at ~25 % opacity
comp.set_error_bars(lower_comp, upper_comp, cap_width=6)   # cap width in logical pixels
comp.clear_error_bars()
comp.component_index()            # exposes the existing m_componentIndex
```

They write into the graph's decoration list and emit `fill_changed` / `error_bars_changed`.
The inspector gets rows for them.
`SciQLopGraphComponent` also wraps `QCPCurve` and `QCPGraph` components (`SciQLopGraphComponent.hpp:40`). On those the fill and error-bar methods raise `NotImplementedError`, the project rule since phase 2.

Registration: the new methods and signals go in the `SciQLopGraphComponentInterface` block of `bindings.xml`; `HistogramNormalization` and `HistogramStyle` are registered like the other enums in `enums.hpp`.

Plot factories:

```python
plot.band(x, lower, upper)                    # and plot.band(callback)
plot.error_bars(x, y, err)                    # symmetric
plot.error_bars(x, y, err_minus, err_plus)    # asymmetric
plot.error_bars(callback)                     # callback returns (x, y, err) or (x, y, err_minus, err_plus)
```

- `band` makes a 2-column line graph, both lines `lsNone`, fill from column 0 to 1. Column 0 stays in the legend and column 1 does not, so the band is one legend row; its icon is a swatch in the fill colour.
- `error_bars` makes a 3-column graph: `y` with markers and no line, error bars `0 → (1, 2)`. The bounds get `lineStyle = lsNone` and `inLegend = false`; they stay **visible**, otherwise the dense-case band and auto-scale would lose them. `y` is the legend row.
- The callback form is wrapped in Python into a callback returning `(x, np.column_stack([y, y - e, y + e]))`: two buffers, the second 2-D, like any multi-column line callback. Four 1-D buffers would not work: a provider emits a 4-buffer batch on `new_data_nd` (`src/DataProducer.cpp:104-119`), which line graphs do not listen to, so the batch would be dropped silently. A Python wrapper on a data callback is fine; the "no Python wrappers" rule is about Qt slots.
- `err` lengths must match `y`, else `ValueError`.
- Both set their decorations at creation, so a callback graph is configured before its first data.

Uncertainty around an existing value is a guide recipe, not a new function: plot `[y, y - e, y + e]` and use `set_fill_between`.

## 1-D histogram

### NeoQCP `QCPHistogram`

A new plottable, not `QCPBars`: `QCPBars` draws one CPU rectangle per bar, ties widths to key coordinates and cannot take a multi-column source.

- Source: `QCPAbstractMultiDataSource`. Keys are ignored; every value column is binned with **shared edges**.
- Binning runs in the background, like `QCPHistogram2D`: a viewport-independent transform, a new `bin1d` over the multi source reusing `BinAxis` and `expandIfFlat`. The multi source has no `finiteKeyValueBounds`; `bin1d` computes the finite (positive for log) value bounds across all columns itself.
- Pre-binned data (`bars()`) is written into the same stored result the transform writes. The pipeline returns nothing when no transform is installed, so the drawing must read this stored result, never `pipeline().result()` directly.
- Export reruns the binning synchronously when there is no result, like `QCPHistogram2D`. Without it PNG/PDF exports are empty.

Bin edges:

- `range=None`: edges are fixed on the first data, and only widen when new data falls outside them. Recomputing them on every fetch would make the histogram "breathe" while panning: widths, x range and counts all moving at once.
- `range=(lo, hi)`: fixed edges.
- `bins_changed` is emitted when edges move.
- Log bins: `log_bins=True` also sets the x axis to log, as `SciQLopHistogram2D` does; log edges on a linear axis are unreadable. Non-positive data are skipped by `bin1d`; a fixed `range` with `lo <= 0` and log bins raises `ValueError` in SciQLopPlots, before NeoQCP's `Q_ASSERT` in `BinAxis::make`.

Normalization, an enum (the 2-D histogram takes a bare `int` for it today, the odd one out):

- `Count`;
- `Density`: count / (N × width of that bin), so log bins are right;
- `Probability`: count / N.

Style, an enum:

- `Bars`: GPU quads plus outline, in one `addPlottable(fill, stroke)` entry. Each bar's outline is its own NaN-separated run, so no diagonal joins one bar to the next.
- `Step`: the outline only.
- Default: `Bars` for one column; `Step` with a light fill for several columns, so overlaps stay readable. An explicit `style` always wins.
- On a log value axis empty bins are left out of the outline (count 0 has no log position) but kept in the fill, clamped past the axis edge.

Ranges and auto-scale:

- Key range = edges, value range = [0, max height]. On a log value axis the lower bound is the smallest positive height, like other plottables' sign-domain ranges.
- Empty or all-NaN data: no bars, an empty range, no error. An empty fetch is a normal state for a time-windowed callback, so range accessors must not dereference a missing result.
- Auto-scale runs on the first result and whenever edges move, like `SciQLopHistogram2D::set_data` does through `pipeline().finished`. The pre-binned path has no pipeline; both paths call the same rescale function.
- Layer: `Main`, which is above `Grid`.

### Following the panel's time range

Every `SciQLopPlot` already has a hidden time axis (`m_time_axis`, a `SciQLopPlotDummyAxis`) that the panel's time synchronizer drives. No new plot class is needed.

The histogram's callback graph watches `time_axis()` instead of `x_axis()`, like projection curves do (`time_axis_range_changed` → `set_range`). Today `_connect_callable_sync` observes `x_axis()`; it gets an explicit sync source.

A panel holding only histograms has no plot that defines a time range. It needs `panel.set_time_axis_range(...)`. Documented.

### SciQLopPlots API

```python
plot.histogram(values, bins=64, range=None, log_bins=False,
               normalization=HistogramNormalization.Count, style=HistogramStyle.Bars)
plot.histogram(callback, ...)     # callback(start, stop) returns (x, y) like a line; only y is binned
plot.bars(edges, heights)         # len(edges) == rows of heights + 1, else ValueError
panel.histogram(callback, ...)
```

- C++ classes `SciQLopHistogram`, `SciQLopHistogramFunction`, `SciQLopHistogramRemote`, mirroring `SciQLopHistogram2D`.
- `bins <= 0` raises `ValueError`.
- `GraphType.Histogram` dispatches only in the 1-argument form of `plot()`. The 2-argument form would be ambiguous between `(values, bins)` and `(edges, heights)`: `TypeError`.
- Histogram keyword arguments are rejected on every other dispatch path, like the 2-D histogram's are. Otherwise `plot(x, y, bins=32)` would make a line and drop `bins`.
- `takes_name` includes `Histogram`.
- Legend: one entry for the whole histogram (a plottable has one legend item); its icon is split into one swatch per column, in each column's fill colour.

## Testing

NeoQCP unit tests (`subprojects/NeoQCP/tests/unit`), on pure builders that take pixel or data points, not axes:

- fill strip: raw rows, downsampled rows, NaN and gap breaks, edge crossing split, pixel-column binning, log clamp;
- error-bar builder: NaN rows, one-sided bars, crossed bounds, caps, vertical key axis;
- `bin1d`: shared edges, log bins, NaN and infinities, flat data, normalization sums, per-bin-width density, widen-only edges.

NeoQCP regression tests, one per bug class the reviews found:

- a hidden band edge keeps its fill;
- auto-scale includes band edges and error bounds;
- a value-axis pan with a clamped log baseline does not drift;
- fill and error-bar caches are cleared on zoom, and a cap-width change shows on a cached frame;
- a decoration whose column disappears is dropped, not re-attached;
- error bars switch to a band when too dense and when the downsampled result appears;
- the selected pen is used for bars;
- the legend's click mapping skips out-of-legend components.

SciQLopPlots integration tests (`tests/integration`), with sampled pixels (no golden images):

- fill to 0 under a sine: fill colour below, background above, at raw and downsampled zoom;
- band and error-bar factories, including the callback forms (3 columns reach the graph);
- histogram: pre-binned path, export not empty, auto-scale on the first result, edges stay put while panning, a panel refetches on a time range change;
- dispatch: `plot(..., graph_type=Histogram)` with 2 arguments raises, `plot(x, y, bins=32)` raises.

## Found while designing

An earlier draft claimed that `SciQLopNDProjectionPlot` creates a second hidden time axis. That was wrong. It derives `SciQLopPlotInterface`, not `SciQLopPlot`, so its `m_time_axis` is its only one. A guard test keeps it that way (`tests/integration/test_projection_time_axis.py`).

## Where v1 differs

- Step with one column draws the outline only, with no fill.
- The panel's x sync gained a per-plot opt-out, `set_axis_synchronized`, so a histogram's value axis is not dragged with the time.
- A callback batch with the wrong number of arrays now emits `fetch_failed` instead of vanishing. This is a side fix, shared with bands and error bars.
- NeoQCP's async pipeline was fixed for jobs started from a `finished()` handler.
- `plot.bars(edges, heights)` reports its bar count through `bins()`.
- The inspector has a "Fill color" row only. There is no error-bar row.
- A dense error-bar band keeps its plain colour when the graph is selected.
- An empty data window keeps fills and error bars. A non-empty narrower one still drops them.
- `SciQLopTimeSeriesPlot` does not host histograms: its x axis is time. Use a plain plot or `panel.histogram`.
- Histograms are not selectable in v1: a click on a bar selects nothing.
- A callable `plot.histogram2d` follows the time axis, not x: on a plain plot it fetches only once `plot.time_axis()` has a range, like the 1-D histograms.
- On a 1-pane projection, `add_reference_curve` raises `ValueError`, and a model or callback curve reports its batches through `fetch_failed`. Both used to drop the data silently.
