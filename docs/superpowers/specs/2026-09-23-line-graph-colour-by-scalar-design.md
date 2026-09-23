# Colour a line graph by a scalar (SciQLopPlots #110) — design

Status: revised after two reviews (2026-09-23). Supersedes the "deferred" status of part 3 in
`docs/colour-by-scalar-curves-vs-line-graphs.md`.

## Goal

`SciQLopLineGraph` (NeoQCP `QCPMultiGraph`: N components sharing one x) coloured point by point by
a scalar, with the plot's Z colour scale, like curves already are. Keep the fast path for 10M+
points: async L1 + synchronous L2 min/max decimation, adaptive sampling, GPU lines.

## Decisions (agreed with the maintainer)

1. Colour the line **and** the markers.
2. **Hard colour edge per segment**: segment `k -> k+1` takes the colour of point `k+1` (same rule
   as `SciQLopTimeColoredCurve`). A NaN scalar, or no source point, means the segment is not drawn.
3. Colour survives decimation by carrying **source indices** through every level. Rejected:
   averaging the scalar per bin (a spike's colour vanishes); turning decimation off while coloured
   (what `QCPGraph2` does; defeats the purpose); carrying the scalar value instead of the index
   (the resampler runs on a worker thread, so it would need the colour array there, and every
   colour change would force an L1 rebuild).
4. **No existing API breaks.** Additions only. Source-compatible; not ABI-compatible (new virtuals
   and members): anything compiled against NeoQCP must be rebuilt, which is the case for its only
   consumer (built from source through a meson wrap).
5. No per-plottable state on shared RHI layers (NeoQCP rule).
6. **Zero cost for single-colour graphs.** A graph without colour values runs exactly the code it
   runs today: same algorithms (the indexed ones are a compile-time variant), no origin tables, the
   same 3-float scatter instances and pipeline. Gated by a benchmark against the pinned `a4ad9f0`.

## NeoQCP

### Public API (additions)

`QCPMultiGraph`:

```cpp
void setColorValues(std::shared_ptr<const std::vector<double>> values); // one per key; NaN = gap
void setColorValues(std::vector<double> values);                         // convenience, moves in
void clearColorValues();
bool hasColorValues() const;
void setColorGradient(const QCPColorGradient& gradient);
void setColorRange(const QCPRange& range);
void setColorScaleType(QCPAxis::ScaleType type);   // stLinear / stLogarithmic
```

- Plain setters, no `QCPColorScale` link: NeoQCP stays generic; SciQLopPlots pushes range, log and
  gradient from its own scale.
- Values are shared by all components (one value per key). A length that does not match the data
  size is refused with a warning (graph drawn uncoloured).
- `setDataSource` keeps the colour values when the new source has the same `size()`, clears them
  otherwise. A streaming graph therefore does not re-send them on each same-length refresh. This is
  right for a scalar tied to the samples' position (time-like); a caller whose scalar is derived
  from the refreshed data (e.g. |B|) must re-send it after each refresh. Same semantics as curves.
- The colour setters never touch the data or L1/L2: indices depend only on the data. They bump a
  colour generation counter and invalidate the pixel-line and extrusion caches.

`QCPAbstractMultiDataSource`: two new virtuals, new names (no overloads, so no name hiding in
subclasses), with default implementations built on `keyAt`/`valueAt` so third-party subclasses
still compile and work (slower):

```cpp
virtual QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                         QCPAxis* keyAxis, QCPAxis* valueAxis,
                                         QVector<int>& sourceIndices) const;
virtual QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int pixelWidth,
                                                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                     QVector<int>& sourceIndices) const;
```

`sourceIndices[k]` is the original data index behind output point `k`, `-1` for a gap marker.
Invariant: `sourceIndices.size() == points.size()` on return, always.

### Index emission, per level

Rule for every level: **the index is appended in the same statement block as its point**, under the
same condition. Never computed in a second pass.

The indexed and plain versions of each algorithm are one template with a compile-time
`bool WithIndices` parameter (`if constexpr` around every index statement). The plain
instantiation is the code that runs today; the existing entry points call it.

| Level | Where | Index |
|---|---|---|
| Raw source, full resolution | `linesToPixels` (`algorithms.h`) | `i`; `-1` for each inserted NaN gap marker and each NaN value marker |
| Raw source, adaptive | `optimizedLineData` / `optimizedLineDataMulti` (`algorithms.h`) | `flushInterval`: first sample, argmin, argmax, last sample, each emitted under exactly the same condition as its point; single-sample interval: that sample; the multi variant's closing point under its `!isnan` guard. The loops track argmin/argmax next to min/max |
| SoA / row-major sources | `soa-multi-datasource.h`, `row-major-multi-datasource.h` | override both new virtuals with the templated algorithms (fast path) |
| L1 | `binMinMaxMulti`, `binMinMaxMultiParallel` (`graph-resampler.h`) | `MultiColumnBinResult` gains `std::vector<int> origin`, same layout and stride as `values` (per column, 2 slots per bin: argmin, argmax); bin partitioning is disjoint per chunk, so no race |
| L2 | `resampleL2Multi` (`resampled-multi-datasource.h`) | per bin, the L1 row picked as min/max maps through L1's `origin`; the table has the same 2-per-bin, per-column layout as L2's `values` and is compacted **in lockstep** with the empty-bin compaction and the per-column shift |
| L2 as a source | `QCPResampledMultiDataSource` | holds its composed `origin`; `getLinesIndexed` returns `origin[row]`; the key-gap marker its `getLines` inserts gets `-1`; value-NaN rows are skipped without a marker, exactly as `getLines` does |

**The origin tables are built only for coloured graphs**, so a single-colour graph builds exactly
what it builds today. Mechanism:

- `MultiGraphResamplerCache` (the `std::any` the stateless L1 lambda already receives) gains
  `bool wantOrigin`, and it joins the validity key (`sourceSize`, `columnCount`, `cachedKeyRange`,
  **`wantOrigin`**). The L1 builder reads it from the cache and fills `origin` only when set.
- The graph sets `wantOrigin` in its cache when colour values are first set on a graph whose L1 has
  no origin, and resubmits the L1 job: **one** async L1 rebuild, the first time a graph is coloured
  (usual order: `set_data`, then `set_color_data`). Until it lands, the graph draws uncoloured from
  the old L1, exactly like any other pending L1 job. Later colour changes (values of the same length,
  gradient, range, scale type) never touch L1/L2.
- L2 is built from L1 on the GUI thread; it composes `origin` only when L1 has one.
- Clearing the colour values leaves `wantOrigin` set (no rebuild churn when toggling); a new data
  source that is uncoloured resets it.
- Cost for coloured graphs only: one `int` per L1/L2 value (L1 is capped at 100k bins: about
  6.4 MB at 8 columns, +50% on L1), two index stores per min/max update in the async binning.

### Drawing (`QCPMultiGraph::draw`)

- Coloured graphs take the per-component path (`getLinesIndexed` / `getOptimizedLineDataIndexed`),
  not the batched `getLinesAll` path. Measured in the perf phase; an indexed batched variant is
  added only if the numbers ask for it.
- Per component, two cached index arrays next to `mCachedLines`, same caching and invalidation
  (including the pan `gpuOffset` reuse):
  - `dataIndices`: aligned with the decimated `dataLines` (used by markers);
  - `lineIndices`: aligned with the line actually drawn. Step and impulse transforms
    (`plottable-linestyle.h`) get index-aware versions producing points and indices in one pass.
    With `d[i]` the data index of decimated point `i`:
    - `lsStepLeft`: `out[2i] = (x_i, y_{i-1})` -> `d[i-1]`, `out[2i+1] = (x_i, y_i)` -> `d[i]`;
    - `lsStepRight`: `out[2i] = (x_{i-1}, y_i)` -> `d[i]`, `out[2i+1] = (x_i, y_i)` -> `d[i]`;
    - `lsStepCenter`: `out[2i-1] = (mid_i, y_{i-1})` -> `d[i-1]`, `out[2i] = (mid_i, y_i)` -> `d[i]`;
      the first and last points it emits (the first and last samples) -> `d[0]` and `d[n-1]`;
    - `lsImpulse`: both points of pair `i` -> `d[i]` (drawn as separate lines, one run per pair);
    - `lsLine`: `lineIndices == dataIndices`.
    With the hard-edge rule (segment takes its end point's colour), a held step level is coloured
    by the sample that holds it. The exact formulas are written as tests first, against the
    current transforms.
- A 256-entry premultiplied colour LUT from gradient x range x scale type, rebuilt when one of them
  changes. `bucket(index)` = LUT slot of `values[index]`, or "gap" for `index == -1`, NaN, or a
  non-positive value on a log scale.
- **Lines, GPU**: split the component's polyline into runs of consecutive segments with the same
  bucket (a gap bucket ends a run and is not drawn); extrude each run with the **existing**
  `QCPLineExtruder::extrudePolyline` (the overload returning its vertices, since the out-param one
  clears its output) in that bucket's colour; append all runs to one vertex buffer; one
  `addPlottable`. Butt ends at colour changes, no join between runs.
- **Lines, CPU** (export, non-solid pens, impulses): the same runs, one `drawPolyline` per run
  (as `SciQLopTimeColoredCurve::draw_colored_line`). Same runs, same ends: GPU and export match.
- **Extrusion cache**: coloured graphs use a coloured variant of `drawPolylineCached` whose cache key
  is `(penWidth, colorGeneration)`, not `(penWidth, penColor)` (the pen colour is not what is drawn;
  the variant skips the pen-colour comparison of `plottable-draw-utils.cpp:98-104`).
  Only the colour setters bump `colorGeneration`, never a pan. The existing function and its key
  stay as they are for uncoloured graphs.
- **Markers, GPU per-instance colour** (see "Scatter layer" below): a coloured graph sends its
  markers through `addScatterColored` with one premultiplied colour per marker from the LUT (gap
  bucket: marker skipped), so they draw on top of the line like uncoloured markers and follow the
  pan offset like them. Export / no-RHI fallback: QPainter, batched per bucket, pen and brush in the
  bucket colour (as `SciQLopTimeColoredCurve::draw_colored_scatters`), translated by the same pan
  offset as the line.
- **Legend**: a coloured component's icon becomes a small gradient strip, in
  `QCPMultiGraph::drawLegendIcon` and in the group legend rows (`layoutelement-legend-group.cpp`,
  which draws its own rows, so `QCPMultiGraph` exposes a const accessor for the gradient and
  whether colour values are set).

### Scatter layer: per-draw style and per-instance colour

Today `QCPScatterRhiLayer` has **one** sprite image, one `halfSize` and one `useColorAxis` for the
whole layer, set by the last `addScatter` of the frame (`scatter-rhi-layer.cpp:96-121`,
`:338-347`). Coloured markers need a colour per marker and a colour mode per draw. Only that is in
scope:

- **Per-draw `mode` and `halfSize`**: stored in `DrawEntry`, written into `PerDrawUniforms` per draw
  (`halfSize` is already a per-draw uniform, filled from a layer-wide value). `mode` replaces the
  `useColorAxis` float in place, so the struct stays 32 bytes and std140-compatible. Modes: `0`
  sprite colour (today's default), `1` colormap lookup (today's `useColorAxis`, now per draw), `2`
  instance colour.
- **Instance data stays 3 floats** `(x, y, colorValue)` in the existing instance buffer, with the
  existing pipeline: uncoloured and colormap draws are unchanged, byte for byte.
- **Coloured draws**: a second instance buffer holds one premultiplied `Float4` colour per marker,
  and a second pipeline on the same layer declares both instance bindings (binding 1: the existing
  `Float3`; binding 2: `Float4`). `DrawEntry` records whether it is coloured and its offset in the
  colour buffer; `render` binds the matching pipeline and buffers per draw (setShaderResources
  before setVertexInput, the Metal rule). Both pipelines share the SRB. The colour buffer is created
  and uploaded only when a frame has coloured draws.
- `addScatter` keeps its signature and behaviour. New
  `addScatterColored(std::span<const float> xy, std::span<const float> rgba, style, ...)`.
- Shaders: a second vertex shader variant `scatter_colored.vert` reads the colour attribute and
  forwards it; `scatter.frag` handles mode 2. The existing `scatter.vert` is unchanged.
- **Mode 2 colour**: `fragColor = instanceColor * sprite.a`, with `instanceColor` premultiplied, so
  alpha is counted once. It uses the sprite as a shape mask: a marker whose pen differs from its
  brush is drawn in one colour. That is the intended look for coloured markers (pen = brush = the
  bucket colour, as on curves).
Not in scope, filed as a NeoQCP issue: the shared sprite (two graphs with different marker styles
on one layer draw with the last style; a sprite atlas with per-draw UV rects would fix it) and the
shared colormap texture (`QCPGraph2` with two different gradients on one layer). Per-draw `mode`
already fixes one direction of the latter: a plain draw no longer switches off a colormap draw on
the same layer.

### Also fixed in NeoQCP (separate commit)

`QCPMultiGraph::setLineStyle` does not invalidate the line cache (unlike `setAdaptiveSampling`).

### Not in scope, filed as a NeoQCP issue

`QCPGraph2` scatter colours: indices drift after key gaps (`linesToPixels` inserts markers), refer to
L2 bins when L2 is active, and the colormap / `mUseColorAxis` is layer-wide (last plottable wins).

## SciQLopPlots

- `SciQLopGraphInterface` gains three virtuals with no-op defaults:
  `has_color_values()`, `color_range(bool log)`, `attach_color_scale(QCPColorScale*)`.
  `SciQLopNDProjectionCurves` already has `attach_color_scale`; `SciQLopCurve` keeps its public
  `set_color_scale` and gains an `attach_color_scale` override that forwards to it;
  `SciQLopLineGraph` implements all three. No existing name changes.
- `ColorScaleController` sources become one loop over the plottables using that interface, at
  **both** construction sites: `SciQLopPlot.cpp` (~728) and `SciQLopNDProjectionPlot.cpp` (~378).
- `SciQLopLineGraph::set_color_data(values, gradient)` works (today the base class raises). One
  value per x sample; a length mismatch raises, as on curves. Kept on a same-length data refresh
  (NeoQCP keeps them), dropped otherwise.
- `SciQLopLineGraph::attach_color_scale(scale)` follows the `QCPColorScale`: pushes range, scale
  type and gradient into the multigraph setters and re-pushes on its change signals (as
  `SciQLopTimeColoredCurve::set_color_scale`). With no scale, the graph's own range (data min/max)
  and gradient are used.
- No existing Python or C++ signature changes. The only behaviour change: a call that raised works.

## Delivery plan (NeoQCP commits, each with its tests written first)

Feature branch from `upstream/main` (`a4ad9f0`). One commit per step, the listed tests fail first.

1. `setLineStyle` invalidates the line cache. Test: changing the style re-draws.
2. Index emission on the raw paths: `linesToPixels`, `optimizedLineData`, `optimizedLineDataMulti`,
   the two virtuals with defaults, SoA and row-major overrides. Tests: index invariant; every
   `flushInterval` branch; NaN runs; key gaps; vertical key axis; the default implementation on a
   custom source equals the fast one.
3. **Perf check 1**: `tests/perf/multigraph-perf.cpp`, 10M points. (a) Single-colour gate: every
   existing scenario within noise (3%) of a build of the pinned `a4ad9f0`. (b) Indexed vs plain raw
   paths. Stop and rethink if (a) fails or (b) costs more than the budget allows.
4. L1 origin (`binMinMaxMulti(Parallel)`) behind `wantOrigin` in the cache and its validity key, L2
   composition and lockstep compaction, the `QCPResampledMultiDataSource` indexed method. Tests:
   argmin/argmax per column; parallel equals serial; L2 compaction with empty bins; key-gap marker
   is `-1`; no origin built while `wantOrigin` is unset; toggling it rebuilds L1 once.
5. Index-aware step and impulse transforms. Tests: the formulas above, per style.
6. Colour API on `QCPMultiGraph`, LUT, `colorGeneration`, `setDataSource` keep-on-same-size, first
   colouring requests the one origin rebuild. Tests: first colouring rebuilds L1 once, later colour
   changes do not rebuild L1/L2; LUT for linear, log and non-positive values; NaN; length mismatch
   refused; same-size refresh keeps values.
7. Colour runs and GPU run extrusion with the coloured cache key. Tests: runs at bucket boundaries,
   gaps end runs; re-extrude on colour change, not on pan; uncoloured cache key unchanged.
8. **Perf check 2**: the single-colour gate against `a4ad9f0` again, and coloured vs uncoloured pan
   and zoom at 10M points, target 1.5x.
9. CPU runs (export, dashed pens, impulses). Test: GPU and CPU produce the same runs.
10. Scatter layer: per-draw `mode` and `halfSize`, the colour instance buffer, the coloured
    pipeline and `scatter_colored.vert`, `addScatterColored`, mode 2 in `scatter.frag`. Tests:
    coloured markers get their per-marker colours; a colormap draw and a plain draw on one layer
    keep their own mode (fails today); two graphs with different marker sizes on one layer keep
    their own size (fails today); uncoloured and `QCPGraph2` colormap markers unchanged; no colour
    buffer is created when a frame has no coloured draw.
11. Coloured markers on `QCPMultiGraph` (GPU and QPainter fallback, pan offset). Test: markers
    follow the line on pan; a gap bucket skips its marker.
12. Legend gradient icon, group legend rows.

Then push to the fork (`jeandet/NeoQCP`) and open a PR to `SciQLop/NeoQCP`; the maintainer merges.
Never push to upstream. File the `QCPGraph2` issue.

## Tests in SciQLopPlots (Python integration)

- rendered pixels show the gradient on a line graph; NaN makes a gap; hidden graph does not count;
- shared scale with curves; a colormap takes the scale over; pinning and log;
- same-length callable refresh keeps the colours;
- perf: a 10M-point coloured line graph within 1.5x of the uncoloured one on pan and zoom.

## Delivery order

1. **NeoQCP**: the 12 commits above, then the fork PR, merged by the maintainer.
2. **SciQLopPlots** — developed meanwhile against the branch checked out in `subprojects/NeoQCP`
   (local only). After the merge: pin bump, interface virtuals, uniform scale sources (both sites),
   `SciQLopLineGraph` colour support, tests, docs (decision record status). Full suite; CI on all
   platforms.
3. Release 0.38.0 (already due for the `completer_model` removal).

## Build discipline

One build or test invocation at a time per build directory, in the foreground. NeoQCP and
SciQLopPlots have separate build directories.
