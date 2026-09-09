# Free pan, deferred data swap, and GPU stale-data fade

Date: 2026-09-09
Status: approved design (user-described in conversation), pre-planning
Target repos: NeoQCP (`subprojects/NeoQCP`, own git repo) + SciQLopPlots (integration test only)

## Problem

Panning a panel with several graphs and span overlays still lags. Tracing the pan
path from SciQLopPlots down into NeoQCP shows three independent causes.

### 1. Every data arrival un-translates the layer and repaints it twice

Pan → `TimeAxisSynchronizer` → each plot's x axis `set_range` → each graph's
`range_changed` → Python callable fetch (worker thread) → `set_data` →
`SciQLopMultiGraphBase::build_data_source` → `QCPMultiGraph::setDataSource`.

`QCPMultiGraph::setDataSource` (`plottable-multigraph.cpp:100-122`) and
`QCPGraph2::setDataSource` (`plottable-graph2.cpp:94-106`) immediately replace
`mDataSource`, reset `mL1Cache`, clear `mCachedLines` and set `mLineCacheDirty`.
Consequences:

- `stallPixelOffset()` returns null while `mCachedLines` is empty, so
  `QCPLayer::canTranslateInsteadOfRepaint()` is false: **every drag frame becomes a
  full main-layer repaint** until the resampled L1 cache lands (worker thread,
  tens to hundreds of ms for large data). The pan is not "free" any more.
- `draw()` bails out with "Pipeline active, no L1 yet" for the graph whose data just
  arrived. The stale-frame preservation in `setupPaintBuffers()`
  (`core.cpp:3550-3594`) only kicks in when **no** plottable on the buffer can
  produce content. With several graphs, the ready ones force a clear and the
  pending ones **blink out**.
- Each arrival costs two full repaints (one at `set_data` via the emitted `replot()`,
  one at `onL1Ready`), and each full repaint clears the plottable RHI layer and
  re-uploads every graph's geometry. N graphs arriving at different moments → 2N
  full repaints interleaved with non-translatable drag frames.

### 2. The busy fade never reaches GPU-drawn geometry

`QCPLayer::draw()` (`layer.cpp:211-218`) sets `painter->setOpacity(fadeAlpha)` when a
plottable is `visuallyBusy()`. That only affects QPainter output. Lines go through
`QCPPlottableRhiLayer` and scatters through `QCPScatterRhiLayer`, whose per-draw
uniforms carry offsets but no alpha, so **GPU graphs never dim**. The legend glyph
(`⟳`) still works because the legend is CPU-painted. This is the "we used to dim the
graphs after a pan" regression: it disappeared with the GPU line path.

### 3. A fade toggle during a pan is not repainted

`QCPAbstractPlottable::onDebounceTimeout()` (`plottable.cpp:1088-1099`) flips
`mVisuallyBusy` and queues a replot. During a pan the layer translates instead of
repainting, so the RHI entries (and any alpha they carry) are not rebuilt. Even with
cause 2 fixed, the dim would only appear at the next full repaint, which is usually
the swap itself, i.e. when the data is no longer stale.

## Design

### A. Deferred data commit with a per-plot debounced swap (NeoQCP)

`QCPGraph2` and `QCPMultiGraph` keep the **displayed** source, L1/L2 caches, cached
lines and rendered range untouched when a replacement source arrives while
geometry is on screen. The replacement is staged as **pending**:

- `setDataSource(src)`: if `src && mDataSource && mHasRenderedRange && mParentPlot`
  → stage pending; else apply immediately (first data, clear, never-drawn graph:
  today's behaviour, plus a queued replot when the pipeline has no transform so the
  caller no longer has to replot).
- Staging: `mPendingSource = src`, components synced to the pending column count
  (legend wrappers in SciQLopPlots read `componentCount()` right after
  `setDataSource`), transform re-evaluated for the pending size, `mPipeline.setSource`
  (L1 built in the worker for the pending source), `mPendingGeneration =
  mPipeline.currentGeneration()`. No transform → pending is ready immediately.
  Syncing eagerly to a *fewer*-column replacement hides the surplus displayed
  components for the rest of the window (they stop being drawn even though their
  old data is still technically on screen): this is required because SciQLopPlots
  deletes the trailing component wrappers on shrink, so keeping them displayed
  until the commit would leave the plottable referencing wrappers Python has
  already discarded.
- `onL1Ready(generation)`: with a pending source, results older than
  `mPendingGeneration` are ignored (a superseded source's job); otherwise the L1 is
  extracted into `mPendingL1` and the plottable calls
  `mParentPlot->requestDataSwap()`.
- `QCustomPlot::requestDataSwap()`: **leading-edge window**. The first request arms
  a single-shot timer (`dataSwapDebounceMs`, default 100 ms); later requests within
  the window ride along. On timeout every plottable's `commitPendingData()` runs,
  then one `replot()`. Max added latency = one window; graphs that become ready
  within it swap in a single repaint. `setDataSwapDebounceMs(int)` for tuning.
- `commitPendingData()`: moves pending source/L1 into the displayed slots, resets
  L2, marks lines dirty. The next draw rebuilds from the new data; the compositor
  shows the old translated texture until then. Not ready (superseded pending) →
  no-op; the next `requestDataSwap` will come from its own `onL1Ready`.
- Pending data counts as busy: `updateEffectiveBusy()` becomes
  `mExternalBusy || pipelineBusy() || hasPendingData()`, so the stale window keeps
  the fade and legend glyph on until the swap.
- `dataChanged()` (in-place mutation) keeps today's immediate semantics.
  `getKeyRange/getValueRange/dataCount/selectTest` read the displayed source.
  `QCPColorMap2`, `QCPHistogram2D`, and CPU `QCPCurve` are out of scope.

Outcome: a pan is translation-only from the first drag frame until the swap, no
graph ever blinks out, and N arrivals within a window cost one full repaint.

### B. Per-draw alpha in the plottable and scatter RHI layers (NeoQCP)

- `QCPPlottableRhiLayer::DrawEntry` and `QCPScatterRhiLayer::DrawEntry` gain
  `float alpha = 1`. `addPlottable(...)` / `addScatter(...)` take a trailing
  `float alpha = 1`. The per-draw UBO carries it (`PerDrawUniforms`: the plottable
  layer's spare pad becomes `alpha`, 32 bytes unchanged; the scatter layer grows to
  48 bytes). Vertex colours are premultiplied, so the shaders multiply the whole
  RGBA by alpha (`plottable.vert`: `v_color = color * pc.alpha`; `scatter.frag`:
  `fragColor *= alpha`).
- Call sites pass `painter->opacity()` (the value `QCPLayer::draw()` already sets):
  `drawPolylineCached`, `drawPolylineWithGpuFallback`, `QCPGraph`'s fill path, and
  the scatter uploads in `QCPGraph2::draw` and `QCPMultiGraph::draw`.
- `drawEntries()` const accessors on both RHI layers for test read-back.

### C. Fade toggles force one repaint of the layer (NeoQCP)

- New `QCPLayer::invalidatePaintBuffer()` (sets the paint buffer invalidated).
- `onDebounceTimeout()` calls `mLayer->invalidatePaintBuffer()` before queuing the
  replot, so the next replot rebuilds the RHI entries with the new alpha. Cached
  extrusions are reused (no CPU line extraction), so the cost is one geometry
  upload, at most twice per fetch cycle.
- `setupPaintBuffers()` decides "all layers can translate" with
  `canSkipRepaintForTranslation()` (which honours invalidation) instead of
  `canTranslateInsteadOfRepaint()`. Today an invalidated-but-translatable buffer is
  neither cleared nor skipped, and `drawToPaintBuffer()` then paints over stale
  content — a latent ghosting bug that this path would otherwise hit.

### D. SciQLopPlots

No API change. `SciQLopMultiGraphBase::busy()` already reads the QCPMultiGraph's
effective busy, so pending data shows up as busy for free. One integration test pins
the multi-graph pan behaviour end to end (busy on all graphs during the fetch, all
data swapped after release). Theme defaults (`busy_show_delay_ms` 500,
`busy_fade_alpha` 0.3) stay; SciQLop can tune them.

## Testing

- NeoQCP autotests (`tests/auto`), offscreen: layer invalidation on fade toggle,
  swap debounce coalescing, deferred commit for QCPMultiGraph and QCPGraph2
  (translation survives `setDataSource`, commit after the window, busy includes
  pending, rapid `setDataSource` latest wins).
- NeoQCP autotests with a real QRhi (this machine has `DISPLAY=:0`): PRL/SRL entry
  alpha equals the fade alpha while visually busy and 1 otherwise. `QSKIP` offscreen.
- SciQLopPlots integration suite (`tests/integration`) plus the new panel test.
- Manual: live SciQLop pan with several graphs + spans (deferred to the user).

## Out of scope / follow-ups

- Colormap and histogram deferred commit and GPU fade.
- Dropping the redundant `Q_EMIT replot()` in `SciQLopMultiGraphBase::set_data`
  (one idle full repaint per arrival; harmless during a pan).
- Persistent per-plottable RHI entries (avoid re-uploading every graph on any
  layer repaint) — the next lever once repaint count is down.
- Legend layer repainting every drag frame (CPU text) — measure before touching.
