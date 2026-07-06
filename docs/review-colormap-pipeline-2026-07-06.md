# Qt Code Review Report — Colormap Plotting Pipeline

**Date**: 2026-07-06
**Scope**: colormap plotting pipeline — `SciQLopColorMap`/`SciQLopColorMapBase`, `SciQLopPlotColorScaleAxis`, `SciQLopPlot` colormap paths, PyBuffer bridge, inspector model/delegates, Python binding surface, and NeoQCP `QCPColorMap2`/`QCPColorMapData`/renderer/RHI layer/async resample pipeline.
**Files reviewed**: 28 core files + traced symbols across `subprojects/NeoQCP/src/datasource`, `src/Model.cpp`, `src/Registrations.cpp`, `src/SciQLopHistogram2D.cpp`, bindings.
**Method**: deterministic lint pass + six parallel deep-analysis agents (Model Contracts, Ownership & Lifecycle, Thread Safety, API & C++ Correctness, Error Handling & Bounds, Performance & Quality), findings deduplicated and confidence-scored.
**Issues found**: 24 deep findings (38 lint hits, mostly style/portability), 10 investigation targets.

Verified-clean areas (checked, do not re-litigate):
- Wrapper↔plot destruction in **both** orders (QPointer + `destroyed` hooks correct); no use-after-free or double-delete confirmed anywhere in the pipeline.
- Async pipeline buffer lifetime: aliased `shared_ptr(_dataHolder, ...)` + by-value job copies keep PyBuffers alive across data replacement and in-flight jobs; `DestroyGuard` mutex-across-check+invoke is sound; generation/ABA logic delivers results in order; job coalescing correct.
- GIL handling in PyBuffer ctor/copy/move/dtor is refcount-balanced; worker-thread destruction defers to the release queue (no GIL deadlock in destructors).
- RHI resource teardown (layer dtor, `releaseResources`, device loss) leak-free.
- `PlotsModel` index/parent/rowCount protocol, begin/end bracketing, move-row convention, drop-event double-removal neutralization: all correct.
- bindings.xml colormap `modify-function` signatures all exactly match the C++ headers; per-add-function try/catch present.
- float32 dtype hazard is fixed on the colormap set_data/percentile path (`dispatch_dtype`; `PyBuffer::data()` now throws on non-float64).
- Pan-freeze fix (markDirty + stallPixelOffset) present and correct; no re-entrancy cycles in set_data→autoscale→resample.

---

## Deep analysis findings (deduplicated, most severe first)

### Memory safety / segfault

#### [D-01] Py_buffer leak when a non-numeric dtype is rejected
- **File**: `src/PythonInterface.cpp:319-347` (throw at 333-335)
- **Category**: Ownership & Lifecycle — **Confidence**: 88
- **Finding**: `_PyBuffer_impl::init_buffer` calls `PyObject_GetBuffer` (which stores a strong ref in `buffer.obj`), then throws `std::runtime_error` from the *constructor* on a non-numeric format (`'?'` bool, complex, structured dtypes). The dtor — the only caller of `PyBuffer_Release` — never runs for a partially-constructed object, so the exporter ref and its memory are leaked per attempt. Reachable from the shiboken conversion rule (Python sees a clean `TypeError` while C++ leaks) and from `_collect_buffers` — a provider callback returning a bool array leaks on **every** pan/zoom of a function/remote colormap.
- **Mitigation**: validate the format before/inside the GIL scope and `PyBuffer_Release(&buffer)` (and clear `is_valid`) before throwing; reproducer: repeated `plot.colormap(x, y, z.astype(bool))`, assert `sys.getrefcount` stable.

#### [D-02] Allocation failure in `QCPColorMapData` → `memset(nullptr)` SIGSEGV on the worker pool
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap.cpp:87-100` (ctor), `:202-239` (`setSize`), `:470-476` (`fill`); `subprojects/NeoQCP/src/datasource/resample.cpp:347-372`
- **Category**: Error Handling & Bounds — **Confidence**: 82
- **Finding**: `setSize` catches `bad_alloc` and leaves `mData = nullptr` but keeps `mKeySize/mValueSize` set and `mIsEmpty == false`. The ctor then calls `fill(0)` unconditionally and `fill` has no null guard → `memset(nullptr, 0, n*8)` SIGSEGV. Even surviving that, `resample()` writes through unchecked `data->rawData()`. The resample grid is up to 4×viewport per axis (~1.3e8 cells ≈ 1 GB doubles on a 4K widget), so the allocation can genuinely fail — turning "big spectrogram + low RAM" into a worker-thread crash instead of the intended qDebug-and-degrade.
- **Mitigation**: on alloc failure reset sizes to 0 / `mIsEmpty=true`; null-guard `fill`/ctor; make `resample()` bail on `!data->rawData()`.

#### [D-03] `QCPColorMapData::fill(double z)` rewritten to `memset` — garbage for any z ≠ 0, UB for NaN
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap.cpp:470-476` (NeoQCP commit `982dc87`)
- **Category**: Correctness — **Confidence**: 90 (defect certain; currently latent)
- **Finding**: `memset(mData, z, dataCount * sizeof(*mData))` truncates the double to a byte and splats per byte — only `fill(0)` yields valid doubles. `fill(1.0)` produces ~7.75e-304 cells while `mDataBounds` is set to `(1,1)` (bounds/data desync); `fill(NAN)` is a double→int conversion of NaN — UB. All in-tree callers pass 0, but `fill` is public API and upstream QCP semantics are element-wise. (The `fillAlpha` memset is fine — alpha is bytes.)
- **Mitigation**: `std::fill_n(mData, dataCount, z)` (compiles to memset for z==0 anyway); add the null guard from D-02.

### Correctness / API contract

#### [D-04] Pipeline busy-state latches `true` forever on empty data (`setSource(nullptr)`)
- **File**: `subprojects/NeoQCP/src/datasource/async-pipeline.cpp:51-76,152-170`, `async-pipeline.h:113-121`; entry `src/SciQLopColorMap.cpp:90-98`
- **Category**: Thread Safety / State — **Confidence**: 85 (found independently by two agents)
- **Finding**: `onDataChanged()` bumps `mGeneration` before knowing whether a job can be built. With a null source (`SciQLopColorMap::set_data` with empty buffers → `setDataSource(nullptr)`), `makeJob` returns empty and there is no rollback: `isBusy()` (`mGeneration > mDisplayedGeneration`) stays true forever; in the job-running branch a `busyChanged(true)` is emitted and `busyChanged(false)` never follows. `updateEffectiveBusy` ORs `pipelineBusy()` into every later busy evaluation — plausibly the "stuck in fetch" symptom in project memory. The `setGapThreshold` comment shows the hazard was known; the clear-data path wasn't guarded.
- **Mitigation**: when `makeJob` is empty and no job runs, resync `mDisplayedGeneration = mGeneration` under the mutex and emit `busyChanged(false)` if needed; treat a null pre-baked `mPending` as "no pending" in `deliverResult`. Reproducer: `cm.set_data(big); cm.set_data(empty)`; spin loop; assert not busy.

#### [D-05] Fortran-order (column-major) y/z numpy arrays silently accepted and misrendered
- **File**: `src/SciQLopColorMap.cpp:121-141`; `src/PythonInterface.cpp:325-336`
- **Category**: Error Handling & Bounds — **Confidence**: 85
- **Finding**: `init_buffer` requests `PyBUF_ANY_CONTIGUOUS`, so `arr.T` / `np.asfortranarray(...)` are accepted and `is_row_major` recorded — but `set_data` never checks `row_major()`; flat-size validation passes and `QCPSoADataSource2D` indexes row-major → every cell wrong, no warning. The curve path handles this via `ArrayView2D<false>`; the colormap path is the odd one out. The dead `_dataOrder` member (see D-23) was presumably meant for this.
- **Mitigation**: reject or copy-normalize `ndim()>=2 && !row_major()` buffers in `set_data` (or route through `sqp::validation::validate_xyz` extended with an order check).

#### [D-06] `plot(x, y, z)` with invalid shapes throws *after* colormap registration — plot left half-broken, colormap plotting permanently disabled
- **File**: `src/SciQLopPlot.cpp:960-971` (`plot_impl`), `:143-155` (`add_color_map`); throw at `src/SciQLopColorMap.cpp:80-114`
- **Category**: Error Handling — **Confidence**: 82
- **Finding**: `add_color_map` registers the wrapper, sets `m_color_map`, shows the colorbar — then `set_data` throws on shape violations. `_configure_color_map` is skipped (no rescale/data_changed wiring), an empty colorbar stays visible, and since `add_color_map` refuses when `m_color_map != nullptr`, every later colormap `plot()` on that plot returns `None`. One bad call permanently disables colormaps on the plot.
- **Mitigation**: validate the buffers (cheap, side-effect-free) before `add_color_map`, or unwind registration (remove plottable, reset `m_color_map`, hide colorscale) on failure.

#### [D-07] `auto_scale_y` in `set_data` calls raw `QCPAxis::setRange` — the exact PR #76 anti-pattern
- **File**: `src/SciQLopColorMap.cpp:145-151`
- **Category**: API & C++ Correctness — **Confidence**: 80 (found independently by three agents)
- **Finding**: `_valueAxis->qcp_axis()->setRange(yRange)` bypasses `SciQLopPlotAxis::set_range`: (a) with a min/max range-size limit configured, the `rangeChanged` clamp handler sees `clamped != requested` and reverts to `m_last_valid_range` — auto-scale-y silently no-ops; (b) zero-width yRange (single-row spectrogram) is silently rejected by `validRange`; (c) all-NaN y makes `qcp::algo::keyRange` return `(DBL_MAX, -DBL_MAX)` with `found=true` (broken `found` contract). Also unguarded `_valueAxis` deref. Both `rescale()` paths were converted to `set_range` for exactly this reason; this call site was missed.
- **Mitigation**: `_valueAxis->set_range(SciQLopPlotRange(yRange.lower, yRange.upper))` with a null check; skip when `!found` or zero-width.

#### [D-08] Zero-width percentile z-range → colorbar rescale silently no-ops (missing the min/max fallback the value-axis path has)
- **File**: `src/SciQLopColorMapBase.cpp:115-126`; `src/SciQLopPlotAxis.cpp:660-672`
- **Category**: Error Handling — **Confidence**: 85
- **Finding**: `z_rescale_range()` only rejects NaN. A zero-width percentile result (sparse spectrogram, >98% identical visible cells) reaches `rescale()`, which `set_range(*r)`s and returns early — `QCPRange::validRange` rejects it downstream and nothing happens, and the min/max fallback is never reached. The value-axis path guards this exact case (`SciQLopPlotAxis.cpp:392-396`); the z path lacks it. Side effect: `QCPColorScale::mDataRange` can disagree with what's drawn.
- **Mitigation**: treat `r.start()==r.stop()` like NaN in `z_rescale_range()` (return `nullopt`) so the plain `rescaleDataRange` fallback runs.

#### [D-09] Shared color-scale rescale provider: last-install-wins, destructor nulls unconditionally
- **File**: `src/SciQLopColorMapBase.cpp:40-47` (dtor), `:128-132` (install); `include/SciQLopPlots/SciQLopPlotAxis.hpp:322-336`
- **Category**: Ownership & Lifecycle — **Confidence**: 85 (two agents; functional bug, not UAF)
- **Finding**: every z-plottable installs its own `[this]` provider on the *one* shared `SciQLopPlotColorScaleAxis` (`m_axes[4]`) and `~SciQLopColorMapBase` sets it to `nullptr` without ownership check. `add_histogram2d` has no single-instance guard, so colormap+histogram (or 2 histograms) coexist: (a) install order silently decides whose percentile config drives 'm'-rescale; (b) destroying either plotable degrades the survivor to plain min/max. The unconditional null-out is what *prevents* a dangling `this` — no memory hole, but a stateful contract break.
- **Mitigation**: owner-aware provider slot (clear only if `owner == this`), or per-plottable providers aggregated by the axis.

#### [D-10] `QCPColorMap2::selectTest` never sets `details` — plot-area click can never select the colormap
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap2.cpp:360-377` vs `plottable-colormap.cpp:952-975`
- **Category**: API & C++ Correctness — **Confidence**: 80
- **Finding**: legacy `QCPColorMap::selectTest` gates on the axis rect, sets `details = QCPDataSelection(QCPDataRange(0,1))` ("to facilitate whole-plottable selection") and returns `selectionTolerance()*0.99`. `QCPColorMap2` ignores `details` and returns 0 → `selectEvent` gets an empty selection → `setSelection(empty)`: clicking a spectrogram never selects it. Masked today because SciQLopPlots wires colormap selection through the legend item only. Also: no axis-rect gating (margin clicks whose coords fall in data range "hit").
- **Mitigation**: mirror the original (axis-rect gate, set `details`, return `selectionTolerance()*0.99`).

#### [D-11] Contour-labels API is dead: `set_contour_labels_enabled` is a silent no-op
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap2.cpp:391-394`; surface `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp:145-157`; inspector `src/SciQLopColorMapDelegate.cpp:72-75`
- **Category**: API — **Confidence**: 90
- **Finding**: `mContourLabelEnabled` is written by the setter and never read anywhere; the GPU contour path emits line segments only. Python API + signal + a live inspector checkbox round-trip a flag that does nothing.
- **Mitigation**: implement labels or remove/hide the setter, signal, and inspector row.

#### [D-12] Contour pen/levels setters never trigger a redraw; pen *width* is never consumed at all
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap2.cpp:379-401`, `:322-326`
- **Category**: API — **Confidence**: 85
- **Finding**: contour lines are baked with `mContourPen.color()` inside `updateContourGpu`, which runs only in `draw()` when `imageWasInvalidated || contour gens differ`. `setContourPen` neither invalidates the contour cache nor dirties the layer/queues a replot → `set_contour_color/width` change nothing until data/viewport changes. `setContourLevels`/`setAutoContourLevels` invalidate the cache but also never dirty/replot. Only `.color()` reaches `setContourLines` — width is fully dead. Contrast `setGradient`/`setDataRange` in the same file, which do markDirty + queued replot.
- **Mitigation**: in `setContourPen`: invalidate cache + markDirty + queued replot (mirror `setGradient`); add markDirty+replot to the level setters; wire or hide width.

#### [D-13] `set_contour_levels` doesn't emit `contour_levels_changed` (sibling `set_auto_contour_levels` does)
- **File**: `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp:74-92`; delegate `src/SciQLopColorMapDelegate.cpp:80-85`
- **Category**: API — **Confidence**: 82
- **Finding**: setting explicit levels clears `mAutoContourCount` without notifying — the inspector spin box keeps showing the stale auto count.
- **Mitigation**: emit `contour_levels_changed()` after the `_cmap` call.

#### [D-14] Colormap `set_name` breaks `has_user_name()` and `name_changed` never fires for any colormap
- **File**: `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp:77-82`; `src/SciQLopGraphInterface.cpp:40` vs `:70-73`
- **Category**: API — **Confidence**: 85
- **Finding**: (a) the override never sets `m_user_named`, so `cmap.set_name("Flux"); cmap.has_user_name()` is False forever, against the documented contract; (b) the `objectNameChanged → name_changed` wiring exists only in `SciQLopGraphInterface`'s ctor — the shared-base `name_changed` signal never fires for colormaps. The inspector survives only because it uses `objectNameChanged` directly.
- **Mitigation**: set `m_user_named = true` in the override; move the connect up into `SciQLopPlottableInterface`'s ctor.

#### [D-15] `PlotsModel` always emits `node_removed(nullptr)` — QPointer already cleared when `destroyed` fires
- **File**: `src/Model.cpp:177-195` (defect at 190); `include/.../Node.hpp:34`
- **Category**: Model Contracts — **Confidence**: 90 (**empirically verified** with a compiled Qt 6.11.1 test: QPointer null inside the `destroyed` handler while the sender arg is valid)
- **Finding**: the `destroyed` lambda reads the object back through `guardedNode->object()` (a `QPointer`), which Qt clears at the *start* of `~QObject` — so `node_removed` carries `nullptr` for every destruction-driven removal, which is the *only* removal path for plottables. `PlotsModel` is Python-exposed; any subscriber keying cleanup on the pointer gets null. `PropertiesView::on_node_removed` works only by the `nullptr == nulled-QPointer` coincidence — meaning any object's destruction can tear down the panel.
- **Mitigation**: take the pointer from the `destroyed(QObject* sender)` argument (identity only, never dereference), or capture `obj` by value.

#### [D-16] Delete key removes non-deletable inspector nodes (axes, components) — permanent tree/plot desync
- **File**: `src/TreeView.cpp:40-52`; `src/Model.cpp:120-157`; `src/Registrations.cpp:252-260`
- **Category**: Model Contracts — **Confidence**: 85
- **Finding**: `keyPressEvent` calls `removeRow` for any selected index; `removeRows` consults `deletable()` only for object deletion, but removes the node unconditionally. Axis/component nodes are selectable and `deletable=false`; pressing Delete on "X Axis" or the colormap's "Color Scale" removes the row + its update connections while the axis lives on. X/Y axis nodes are never re-published (`children()` runs once at addNode; `connect_children` re-adds only z/y2 + plottables).
- **Mitigation**: skip non-deletable nodes in `keyPressEvent`, or make `removeRows` refuse to detach `deletable == false` rows from the view path.

#### [D-17] `cellToCoord` divides 0/0 for size-1 dimensions — NaN defeats percentile viewport filtering
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap.cpp:540-548`; consumer `src/SciQLopHistogram2D.cpp:249-260`
- **Category**: Bounds/Correctness — **Confidence**: 80
- **Finding**: `keyIndex / double(mKeySize - 1)` is NaN for 1-cell dimensions. `SciQLopHistogram2D::z_percentile_range` filters cells by coordinate comparisons that are all false for NaN → every cell passes; percentile autoscale silently ignores the visible range for 1-bin histograms (`x_bins=1` is reachable; ctor applies user bin counts with no guard). Wrong-but-plausible z-range, no crash.
- **Mitigation**: guard `cellToCoord` for size-1 dims (return range midpoint) or treat non-finite coords as out-of-view; clamp ctor bin counts like `set_bins`.

#### [D-18] Deferred GIL-release queue has no drain trigger outside buffer-API calls — released arrays retained unboundedly
- **File**: `src/PythonInterface.cpp:88-163`; drain sites only at `_inc_ref`/`_dec_ref`/`init_buffer`/`get_data`
- **Category**: Thread Safety / Ownership — **Confidence**: 85 (behavior; low-medium severity)
- **Finding**: PyBuffer deaths on non-GIL threads (pool worker dropping the last ref, GUI thread with GIL released in `app.exec()`) enqueue the DECREF/`PyBuffer_Release` and drain only when some thread next enters a PythonInterface entry point. If plotting stops, multi-hundred-MB arrays stay pinned until the next interaction or process exit (`Py_AtExit` drains buffer releases only).
- **Mitigation**: opportunistic drain via `Py_AddPendingCall` when the queue becomes non-empty, or a queued GUI-thread GIL-acquiring drain past a size threshold.

#### [D-22] THREAD-01 (known): signals emitted while resampler mutexes held
- **File**: `include/SciQLopPlots/Plotables/Resamplers/AbstractResampler.hpp:150-168`
- **Category**: Thread Safety — **Confidence**: 95 existence, benign today
- **Finding**: `_resample()` emits `_resample_sig` under `_plot_info_mutex` (and `_next_data_mutex` via `setData`). Connection is explicitly queued and the lock order is acyclic — no deadlock today; any future DirectConnection subscriber would run under both mutexes.
- **Mitigation**: move the emit outside the lock scopes (as done for the earlier P1 fixes).

### Performance (hot paths)

#### [D-19] float32 y/z (the Speasy default) demotes the entire resample hot loop to per-cell virtual dispatch
- **File**: `subprojects/NeoQCP/src/datasource/resample.cpp:359-372`, `:127-141`; `soa-datasource-2d.h:93-115`; `src/SciQLopColorMap.cpp:121-141`
- **Category**: Performance — **Confidence**: 85
- **Finding**: the fast `RawAccessor` path is gated on `rawX() && rawY() && rawZ()`, which return non-null only for float64. Production float32 y/z therefore takes `VirtualAccessor`: 3–5 virtual calls per visible source cell in the accumulation loop, virtual gap-detection and y-bin passes, even virtual binary searches on x (which is *guaranteed* double). Multiplies every viewport-dependent resample job during pan/zoom on multi-million-cell spectrograms.
- **Mitigation**: dispatch once per job on element formats and instantiate typed accessors (mirror `dispatch_dtype`); at minimum use `lowerBoundRaw` whenever `rawX()` is non-null.

#### [D-20] Colormap GPU texture re-uploaded on every draw even when the image is unchanged
- **File**: `subprojects/NeoQCP/src/painting/colormap-rhi-layer.cpp:59-63`; `colormap-renderer.cpp:135`; `core.cpp:2700-2708`
- **Category**: Performance — **Confidence**: 85
- **Finding**: `draw()` calls `setImage()` every invocation and `setImage` sets `mTextureDirty` unconditionally. During interactive zoom (translate fast path rejects zooms), the identical staging image — up to 4× the axis rect per dimension, e.g. ~75 MB for a 1500×800 rect — is re-uploaded per input event while the resample is still pending.
- **Mitigation**: skip when `image.cacheKey() == mStagingImage.cacheKey() && size unchanged` (shallow copies through `flippedMapImage` share the data block, so cacheKey matches).

#### [D-21] Resample output plane zero-filled twice then fully overwritten; per-job accumulation buffers reallocated despite the cache object
- **File**: `subprojects/NeoQCP/src/plottables/plottable-colormap.cpp:87-100`, `:225-226`; `resample.cpp:211-214`, `:289-298`
- **Category**: Performance — **Confidence**: 82
- **Finding**: `new QCPColorMapData` memsets the plane in `setSize` *and* in the ctor's `fill(0)`; `resampleImpl` then assigns every cell anyway — two full-plane memsets of waste per pan/zoom job (plane up to 4×viewport, tens of MB). `accum` (8 B/cell), `counts` (4 B/cell), `gapBetween` are allocated fresh per job even though `ResampleCache` is already threaded through.
- **Mitigation**: drop the ctor's redundant `fill(0)` (halves it) or add a no-fill construction path; move the scratch vectors into `ResampleCache` with `assign()` reuse.

### Quality / hygiene

#### [D-23] Dead code cluster (verified no callers)
- **Confidence**: 90
- `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp:169` — `_dataOrder` never read (related: D-05).
- `src/SciQLopPlot.cpp:111-117` + `SciQLopPlot.hpp:80` — `_impl::SciQLopPlot::addColorMap()` (legacy `QCPColorMap`) has zero callers; only in-repo constructor of legacy QCPColorMap outside NeoQCP.
- `subprojects/NeoQCP/src/painting/colormap-renderer.cpp:63-70` — `rescaleDataRange` uncalled, `recalc` param ignored; `colormap-renderer.h:52` — `invalidateFlippedCache()` uncalled.
- `src/PythonInterface.cpp:305-308` — `_PyBuffer_impl` inherits `PyObjectWrapper` *and* holds a `PyObjectWrapper py_obj` member; only the member is used.
- `src/PythonInterface.cpp:491` — public `SciQLopPyBuffer::release()` is a no-op (asymmetric with `GetDataPyCallable::release()`, which really releases); misleading API.
- `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp:177` — `SciQLopColorMapFunction::_set_data` never connected.

#### [D-24] Threading docs describe superseded mechanisms
- **File**: `subprojects/NeoQCP/docs/architecture/async-pipeline.md:145`; `docs/specs/2026-03-13-colormap2-async-resampler-design.md`
- **Category**: Docs — **Confidence**: 95, informational
- **Finding**: docs describe an atomic-bool destroyed flag (code uses the stronger DestroyGuard mutex) and a `QCPResamplerScheduler`/receiver-side-discard design that doesn't exist. `setTransform`'s "must be called before setSource" comment is contradicted by `setGapThreshold` re-baking at runtime (safe only under the implicit GUI-thread-only contract).
- **Mitigation**: update the architecture doc, mark the 2026-03-13 spec superseded, fix the comment to state the real contract.

---

## Investigation targets (human verification needed, max 10)

#### [I-01] Full-source `zRange` O(nz) scan on the GUI thread per rescale (twice on first data) — conf 72
`src/SciQLopPlotAxis.cpp:680` → `plottable-colormap2.cpp:213-226` → SoA `zRange` scans all cells. With auto_scale on (streaming), runs per set_data; first batch triggers it twice (`check_first_data` + `data_changed` hook). Verify with `SCIQLOP_TRACE` on a streaming 10M-cell colormap; fix = cache min/max at `QCPSoADataSource2D` construction (source is immutable).

#### [I-02] Off-GUI-thread `SciQLopColorMap::set_data` mutates QCP state unsynchronized — conf 70
Built-in providers marshal to the GUI thread, but `set_data` is a plain bound slot: a Python worker thread calling it writes `mDataSource` concurrently with `draw()/getKeyRange()/selectTest()` reads and mutates QCPAxis off-thread (unlike `SciQLopCurve::set_data`, which is accidentally worker-safe). Verify: audit SciQLop app/plugins for background-thread set_data; consider a thread-affinity assert or `QMetaObject::invokeMethod` marshal.

#### [I-03] `SciQLopPlotColorScaleAxis` double-emits `range_changed` per data-range change — conf 70
Base ctor wires `QCPAxis::rangeChanged → range_changed`; derived ctor also wires `QCPColorScale::dataRangeChanged → range_changed` (`SciQLopPlotAxis.cpp:490-499` vs `:64-80`); the base clamp/revert logic also runs against the scale's internal axis with axis-range semantics. Verify with a counting spy around `set_range`.

#### [I-04] `z_percentile_range` is `noexcept` but `reserve()` can throw `bad_alloc` → `std::terminate` — conf 68
`src/SciQLopColorMap.cpp:170-246` (reserve at :205) catches only `std::invalid_argument`. A routine 'm' rescale on a huge zoomed-out spectrogram under memory pressure terminates the process instead of skipping the percentile. Fix is one `catch (const std::exception&)`.

#### [I-05] GIL ↔ `QThread::wait()` deadlock window on synchronous graph destruction — conf 65
`~SciQLopPlot` → `~DataProviderWorker` does `quit()+wait()` (`DataProducer.cpp:220-224`); if the destroying thread holds the GIL while the worker sits in `PyGILState_Ensure` at the top of `get_data`, `wait()` never returns. Likely covered by `deleteLater` paths and shiboken's dealloc GIL release — verify `SbkDeallocWrapper` releases the GIL around `cpp_dtor` in shiboken 6.11; stress-test delete-plot-mid-fetch.

#### [I-06] int truncation of `flat_size()` for ≥2³¹-element buffers — conf 65
`src/SciQLopColorMap.cpp:117,127-128`; `soa-datasource-2d.h:30,51`. Validation in `size_t`, spans built with `static_cast<int>`; some combinations compute negative `mYSize` (silent whole-batch drop with a misleading warning). No OOB constructed. Cheapest closure: reject `flat_size() > INT_MAX` with a clear error.

#### [I-07] `nx==1` colormap accepted but silently never renders — conf 65
`set_data` accepts one time column; the resample transform requires ≥2 (`plottable-colormap2.cpp:60`) → pipeline result stays null, blank plot, busy idle — indistinguishable from "no data". Decide: render single-column, or warn/signal.

#### [I-08] `SciQLopFunctionGraph::observe` captures the non-QObject mixin `this` and calls a virtual — conf 65
`src/SciQLopGraphInterface.cpp:83-96`. In `SciQLopColorMapFunction`, the mixin subobject dies before `~QObject` disconnects. The sibling `SciQLopRemoteGraph` ctor comment documents this exact hazard and avoids it. No synchronous emission path found in the destruction window; cheap hardening: match the remote-graph capture pattern.

#### [I-09] `QCPDataLocator` colormap `mDataIndex` uses transposed cell encoding — conf 65
`data-locator.cpp:139,198`: `keyIndex*valueSize+valueIndex` vs storage `valueIndex*keySize+keyIndex`. No in-repo consumer dereferences it (crosshair uses value()/data() only); verify downstream consumers before anyone indexes raw data with it.

#### [I-10] `SciQLopColorMapRemote::set_busy` double-emits `busy_changed` — conf 63
`SciQLopColorMap.hpp:204-209`: calls base `set_busy` (whose `busyChanged` is already connected to `busy_changed`) then also emits directly. Verify with a pytest-qt spy.

---

## Lint findings (Phase 1, 38 hits — mostly style/portability)

- **HDR-3** (12×): unparenthesized `std::min/max`/`numeric_limits::min/max` — Windows `<windows.h>` macro hazard (`src/SciQLopColorMap.cpp:189-192`, `src/SciQLopPlotAxis.cpp:172,515`, `PythonInterface.hpp:204`, `plottable-colormap.cpp:419-420`, `plottable-colormap2.cpp:75-78,406`). Only matters if a TU ever includes windows.h without NOMINMAX; low priority.
- **LCY-5** (10×): "unbounded container growth" on `m_connections`/`m_axes`/`m_observer_connections` — false positives; growth is bounded by object lifetime/axis count.
- **PAT-12** (5×): non-const range-for possibly detaching COW containers (`SciQLopPlot.hpp:482`, `SciQLopPlot.cpp:629,635`, `PythonInterface.cpp:109,129`) — worth a `const auto&` sweep, no measured cost.
- **DEP-2** (4×): `QSharedPointer` in `SciQLopPlotAxis.cpp` — style preference, not a defect.
- **DEP-7** (1×): `qMin` at `plottable-colormap2.cpp:398`; **VAR-3** (3×): brace-init style in NeoQCP RHI layer — noise.

---

## Summary

| Category | Deep | Investigate |
|----------|------|-------------|
| Memory safety / segfault | 3 (D-01,02,03) | 2 (I-04,05) |
| Thread safety / state | 3 (D-04,18,22) | 2 (I-02, part of I-05) |
| Correctness / API | 12 (D-05..17) | 4 (I-03,07,09,10) |
| Performance | 3 (D-19,20,21) | 2 (I-01,06) |
| Quality / docs | 2 (D-23,24) | — |
| **Total** | **24** (deduplicated from 6 agents) | **10** |

Findings below confidence 60 suppressed. Sub-threshold notes retained by the agents: `bindable_data` BINDABLE never updated by colormap set_data (62); 4×-supersampling colorize/upload cost knob (65, deliberate); colorize-on-GUI-thread architecture option (62); `ArrayView1DIterator` const-`offset` copy-assign quirk (65); `has_colormap()` not const; duplicate `selected` Q_PROPERTY redeclaration; unconditional `auto_scale_y_changed` emit; `std::move` on const params in `plot_impl` (harmless); NaN/monotonicity of `m_data_range` unchecked (in-bounds either way).
