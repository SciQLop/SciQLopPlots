# Render-time translation offsets: keep GPU graph geometry in lockstep with spans during pans

Date: 2026-09-04
Status: approved design (user approved in conversation), pre-planning
Target repo: NeoQCP (subprojects/NeoQCP checkout inside SciQLopPlots)

## Problem

During a pan, two render-time-drawn paths use the *freshest* axis range on every QRhi
frame — span geometry (`QCPSpanRhiLayer::detectGeometryChanges()` in
`uploadResources()`) and the paint-buffer compositing offset
(`executeRenderPass()`, `core.cpp:2831-2832` computes `layer->pixelOffset()` live).

But the plottable/scatter GPU geometry (graph lines) gets its translation offsets only
at **replot time** (`replot()` calls `QCPPlottableRhiLayer::setAllOffsets()`,
`core.cpp:2187-2194`), and replots are coalesced through `rpQueuedReplot`. When a
`setRange` lands between the last coalesced replot and the frame, the spans move with
the new range while the graph line trails by up to a frame. Visually: "with spans on
screen, the graph layer feels slower than the graph alone" — the spans provide a
fast-moving reference that exposes the trail.

## Design

Refresh the GPU translation offsets at **render time**, in the render path that already
runs every frame.

- New private method `QCustomPlot::refreshLayerTranslationOffsets()`, called at the top
  of `uploadLayerTextures()` (before the PRL/SRL/colormap `uploadResources()` calls,
  which already upload per-draw uniforms every frame — "offsets change on pan frames",
  plottable-rhi-layer.cpp:204).
- For each `QCPLayer*` key in `mPlottableRhiLayers` and `mScatterRhiLayers`:
  if `layer->canSkipRepaintForTranslation()`, call
  `setAllOffsets(layer->pixelOffset())`. For each `QCPColormapRhiLayer*` whose
  `layer()` can skip, call `setPixelOffset(layer->pixelOffset())` — the colormap has
  the identical replot-time latch (set in `replot()`, core.cpp:2223-2238).
- **Guard semantics:** `canSkipRepaintForTranslation()` is false whenever the pixel
  offset is null (zoom, pan beyond one viewport, pre-first-render) or the buffer is
  invalidated — exactly the cases where the replot's full-repaint path owns the
  geometry. Those frames behave exactly as today. The refresh only ever adjusts the
  uniform offsets of geometry that is deliberately not being repainted.
- `QCPPlottableRhiLayer::setAllOffsets()` / `QCPScatterRhiLayer::setAllOffsets()` gain
  `mLastOffsetX/mLastOffsetY` members plus a `QPointF lastUniformOffset() const`
  getter, so tests can read back what the compositor will apply.

## Testing

RHI-gated test in the existing `test-pipeline` suite (follows the
`showAndHasRhiLT` + `QSKIP` pattern in `test-layer-translation.cpp`):

1. Graph2 with data; replot; wait for pipeline idle; replot.
2. Pan A: `setRange` + `replot(rpImmediateRefresh)` → layer translates; assert PRL
   `lastUniformOffset()` == `layer->pixelOffset()` (replot-time path still works).
3. Pan B: `setRange` **without replot**, then drive a frame (`update()` + event
   processing) and assert PRL `lastUniformOffset()` tracks the *new*
   `layer->pixelOffset()` — this is the case that fails before the fix.

`QSKIP` when no QRhi is available (headless/offscreen), like the existing
colormap-pan test.

## Out of scope

- Changing when replots are scheduled or how spans update.
- Perceived-smoothness validation is a human check (live pan with spans + graph).
