# NeoQCP performance round: span-path churn, stale RHI-layer entries, hidden-widget replots

Date: 2026-09-04
Status: approved design, pre-planning
Target repo: NeoQCP (edited in the `subprojects/NeoQCP` checkout inside SciQLopPlots, pushed to SciQLop/NeoQCP later)

## Background

Investigation into a reported post-sleep rendering slowdown (macOS M3, also suspected on
Linux) found no GPU leak and no broken context-loss recovery in NeoQCP — the
`QRhiWidget`-based lifecycle (`releaseResources()` / `initialize()`) is structurally
complete. The persistent post-sleep symptom most likely lives in Qt's QRhi/Metal layer or
the macOS compositor and is out of scope here.

The same investigation (independently second-reviewed) found three concrete in-repo
defects that are worth fixing regardless:

1. **Span per-frame GPU churn.** `QCPSpanRhiLayer::uploadResources()` forces
   `mGeometryDirty = true` every rendered frame
   (`subprojects/NeoQCP/src/painting/span-rhi-layer.cpp:399`), so `rebuildGeometry()`
   runs every frame and, via `cleanupDrawGroups()`, destroys and recreates one
   `QRhiBuffer` (UBO) + one `QRhiShaderResourceBindings` per span-bearing axis rect per
   frame (`span-rhi-layer.cpp:150-231`). This is deferred-release churn in the Metal
   driver. The vertex buffer itself is grow-only and not recreated
   (`span-rhi-layer.cpp:408-425`).

2. **Stale RHI-layer entries (use-after-free).** `QCustomPlot::removeLayer()`
   (`subprojects/NeoQCP/src/core.cpp:1806-1845`) deletes the `QCPLayer` without removing
   its entries from `mPlottableRhiLayers` / `mScatterRhiLayers` (`QHash<QCPLayer*, ...>`).
   The stale keys are dereferenced on every subsequent `replot()`
   (`core.cpp:2152-2188`) and the values iterated/uploaded every frame
   (`core.cpp:2701-2713`). Separately, `QCPGridRhiLayer::unregisterAxis()`
   (`subprojects/NeoQCP/src/painting/grid-rhi-layer.cpp:39-46`) has no callers —
   `QCPAxis::~QCPAxis()` (`subprojects/NeoQCP/src/axis/axis.cpp:487-492`) never
   unregisters, leaving raw `QCPAxis*` keys in `mAxes`/`mCachedTicks` that are
   dereferenced every frame (`grid-rhi-layer.cpp:338-371`). Both are latent in current
   SciQLopPlots usage (layers/axes only added at construction) but are real UAF bugs.

3. **Wasted CPU replots while hidden.** When the widget is hidden, the QRhi is gone
   (`mRhi == nullptr`), so `createPaintBuffer()` falls back to `QCPPaintBufferPixmap`
   (`core.cpp:3517-3522`) and `replot()` performs full CPU-raster painting of every
   dirty layer, then still calls `update()` (`core.cpp:2224-2226`). An app streaming
   data while minimized/hidden burns CPU on frames nobody can see. Buffers are retained
   (not thrown away per frame), but the painting work is wasted and redone on the next
   visible replot.

## Design

### 1. Span layer: grid-style change detection + persistent draw groups

Mirror the established pattern in `QCPGridRhiLayer::uploadResources()`
(`grid-rhi-layer.cpp:330-440`): keep a dirty flag, do a cheap per-frame comparison of
cached state, rebuild only on real change.

- Remove the forced `mGeometryDirty = true` in
  `QCPSpanRhiLayer::uploadResources()` (`span-rhi-layer.cpp:395-399`, including its
  now-obsolete comment).
- Add a per-span cached signature, recomputed cheaply each frame:
  edge pixel positions (via the existing `edgePixel` conversion logic — a few
  `coordToPixel` calls per span), fill brush style + premultiplied color, border pen
  style/width/color, selected flag.
- Dirty when: any signature differs, the registered span set changes (already covered by
  `registerSpan`/`unregisterSpan`/`markRhiDirty` setting the flag,
  `span-rhi-layer.cpp:33-51` and `item-spanbase.cpp:12-17`), or an axis-rect's bounds
  change — activating the existing but unused `mLastAxisRectBounds`
  (`span-rhi-layer.cpp:225-230`; update its "currently unused" comment).
- `rebuildGeometry()` no longer calls `cleanupDrawGroups()` unconditionally. Draw groups
  persist keyed by `QCPAxisRect*`: update `vertexOffset`/`vertexCount`/`scissorRect` in
  place for surviving groups, create UBO/SRB only for new groups, delete UBO/SRB only
  for vanished groups. Uniform contents continue to be updated every frame as today
  (`span-rhi-layer.cpp:430-487`).
- Vertex buffer keeps its grow-only behavior (unchanged).

Outcome: zero per-frame QRhi object creation/destruction and near-zero CPU when nothing
moved; pan/zoom/span edits rebuild exactly as now.

### 2. Stale entry pruning

- `QCustomPlot::removeLayer()`: before `delete layer`, erase the layer's entries from
  `mPlottableRhiLayers` and `mScatterRhiLayers` and delete the associated RHI layer
  objects.
- `QCPAxis::~QCPAxis()`: call `QCPGridRhiLayer::unregisterAxis(this)` through a
  **non-lazy** accessor (e.g. a new `QCustomPlot::gridRhiLayerIfExists()` or direct
  member access). It must never *create* the grid RHI layer from a destructor, and must
  tolerate the grid layer already being destroyed during `QCustomPlot` teardown
  (nullptr check only, no creation).
- `moveLayer()` needs no change (keys remain valid; only order changes).

### 3. Hidden-widget replot guard

- In `QCustomPlot::replot()` (`core.cpp:2110-2230`), after the `mReplotting` reentry
  guard: if the widget is not visible, return without painting, **without clearing any
  buffer dirty/invalidated flags**, and without emitting `beforeReplot`/`afterReplot`
  (no replot happened). `mReplotQueued` is still reset.
- Same guard in the partial `QCPLayer::replot()` path (`layer.cpp`).
- Recovery is already in place: `initialize()` forces a full replot when the widget
  becomes visible again (`core.cpp:2540-2630`), and preserved dirty flags guarantee a
  complete repaint of everything that changed while hidden.
- No behavior change while visible. Limitation: this covers hide/minimize; mere
  occlusion (window covered but "visible") is Qt/compositor-throttled already and out of
  scope.

### 4. Testing

NeoQCP has an autotest harness (`subprojects/NeoQCP/tests/auto`, per-test directories
following e.g. `test-grid-rhi`). Add:

- **Stale-entry regression:** create a plot, add a layer with a plottable, remove the
  layer, then replot + render repeatedly. Previously UAF — must pass clean under ASAN.
  Same shape for axis/axis-rect removal (grid layer registration).
- **Span dirty tracking:** after an initial render with spans, an unchanged frame leaves
  the geometry dirty flag clear and draw groups stable; moving a span edge or changing
  an axis range sets it dirty again.
- **Hidden replot:** `replot()` on a hidden widget performs no painting and preserves
  buffer dirty state; a subsequent visible replot repaints.

### 5. Verification

- Build and run NeoQCP's `tests/auto` in this checkout (meson).
- Build and run SciQLopPlots' ASAN suite (`build-asan`) against the modified NeoQCP to
  confirm the UAF fixes and catch regressions.
- Manual sanity: a SciQLop session with time-range spans — pan/zoom must render spans
  identically to before.

## Out of scope

- The persistent post-sleep slowdown itself (believed Qt QRhi/Metal/compositor; to be
  investigated separately).
- Colormap RHI layer bookkeeping (already correctly registered/unregistered).
- Per-frame uniform buffer *content* updates (dynamic buffer updates are the intended
  QRhi mechanism and stay).
