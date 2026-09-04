# Render-Time Translation Offsets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refresh GPU translation offsets for plottable/scatter/colormap RHI layers at render time, so graph lines stay in lockstep with spans during pans (no up-to-one-frame trail after coalesced replots).

**Architecture:** Spec: `docs/superpowers/specs/2026-09-04-render-time-translation-offsets-design.md`. One new private `QCustomPlot::refreshLayerTranslationOffsets()` called at the top of `uploadLayerTextures()`; guarded per layer by `canSkipRepaintForTranslation()`. Offset read-back members added to the PRL/SRL classes for testability.

**Tech Stack:** C++20, Qt 6 (QRhi), meson, QtTest.

## Global Constraints

- All code changes in the NeoQCP repo (`subprojects/NeoQCP`, own git repo). Commit with `git -C subprojects/NeoQCP ...`, conventional messages. Outer SciQLopPlots repo gets ONLY the two docs files.
- C++20. Tests live in the existing `tests/auto/test-pipeline/` suite (no new test dir): add the slot to `test-pipeline.h` and the implementation to `test-layer-translation.cpp`, which already has the `showAndHasRhiLT` helper (anonymous namespace at its top).
- Build from the outer repo root /var/home/jeandet/Documents/prog/SciQLopPlots: `meson compile -C subprojects/NeoQCP/build`. If it errors about meson version: `meson setup --reconfigure subprojects/NeoQCP/build subprojects/NeoQCP` (source dir EXPLICIT). Test binary: `QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`.
- Do not change when replots are scheduled; do not touch the span layer.

---

### Task 1: Render-time offset refresh

**Files:**
- Modify: `subprojects/NeoQCP/src/painting/plottable-rhi-layer.h` and `.cpp` (`setAllOffsets` at .cpp:39-46; class declaration in .h around line 36)
- Modify: `subprojects/NeoQCP/src/painting/scatter-rhi-layer.h` and `.cpp` (`setAllOffsets` at .cpp:58)
- Modify: `subprojects/NeoQCP/src/core.h` (private method declaration)
- Modify: `subprojects/NeoQCP/src/core.cpp` (`uploadLayerTextures`, ~line 2716; replot's PRL/SRL loops at ~2181-2217 show the pattern to mirror)
- Modify: `subprojects/NeoQCP/tests/auto/test-pipeline/test-pipeline.h` (add slot)
- Modify: `subprojects/NeoQCP/tests/auto/test-pipeline/test-layer-translation.cpp` (add test)

**Interfaces:**
- Consumes: `QCPLayer::canSkipRepaintForTranslation()` and `QCPLayer::pixelOffset()` (layer.cpp:344-378); `QCustomPlot::plottableRhiLayer(QCPLayer*)` public accessor (core.h:138); existing `setAllOffsets(float, float)` on both RHI layer classes; `QCPColormapRhiLayer::setPixelOffset(float, float)` and `layer()` accessor.
- Produces: `QPointF QCPPlottableRhiLayer::lastUniformOffset() const` and same on `QCPScatterRhiLayer` (test read-back). `void QCustomPlot::refreshLayerTranslationOffsets()` (private). New test slot `void TestPipeline::plottableOffsetsRefreshedAtRenderTime()`.

- [ ] **Step 1: Write the failing test**

In `test-pipeline.h`, add to the translation-test section (near the other layer-translation slots):

```cpp
    void plottableOffsetsRefreshedAtRenderTime();
```

In `test-layer-translation.cpp`, append (the file already has the `showAndHasRhiLT` helper and includes; add `#include <painting/plottable-rhi-layer.h>` at the top with the existing colormap-rhi-layer include):

```cpp
void TestPipeline::plottableOffsetsRefreshedAtRenderTime()
{
    // The graph's GPU translation offsets must track the freshest axis range at
    // render time, not only the range at the last (coalesced) replot — otherwise
    // a range change landing between replot and frame leaves the graph trailing
    // render-time-updated layers (spans, grid) by up to a frame.
    if (!showAndHasRhiLT(mPlot))
        QSKIP("No QRhi available — translation offsets need render frames");

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));

    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    QCPLayer* mainLayer = mPlot->layer("main");
    QVERIFY(mainLayer);

    // Pan A with a replot: translation path engages (replot-time offset set).
    mPlot->xAxis->setRange(100, 1100);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    if (!mainLayer->canSkipRepaintForTranslation())
        QSKIP("layer repainted instead of translating — nothing to verify");

    auto* prl = mPlot->plottableRhiLayer(mainLayer);
    QVERIFY(prl);
    const QPointF offsetA = mainLayer->pixelOffset();
    QVERIFY(!offsetA.isNull());
    QCOMPARE(prl->lastUniformOffset(), offsetA);

    // Pan B WITHOUT a replot: the next frame must refresh the offset from the
    // new range anyway.
    mPlot->xAxis->setRange(200, 1200);
    const QPointF expectedB = mainLayer->pixelOffset();
    QVERIFY(!expectedB.isNull());
    QVERIFY(expectedB != offsetA);
    mPlot->update();
    QTRY_VERIFY_WITH_TIMEOUT(prl->lastUniformOffset() == expectedB, 2000);
}
```

Note for the implementer: if `update()` + `QTRY_VERIFY_WITH_TIMEOUT` never delivers a frame in the test environment, look at how other RHI-gated tests in this suite force frames (`QCoreApplication::processEvents()` after `update()`); if frames genuinely cannot be driven without a replot, report DONE_WITH_CONCERNS with the evidence rather than weakening the assertion.

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: FAIL to compile — `lastUniformOffset` is not a member of `QCPPlottableRhiLayer`. (If the environment has no display/RHI the test will QSKIP later at runtime; the RED here is the compile failure.)

- [ ] **Step 3: Add offset read-back to both RHI layer classes**

In `plottable-rhi-layer.h` (and identically in `scatter-rhi-layer.h`), add after the `setAllOffsets` declaration:

```cpp
    void setAllOffsets(float offsetX, float offsetY);
    QPointF lastUniformOffset() const { return QPointF(mLastOffsetX, mLastOffsetY); }
```

and add private members:

```cpp
    float mLastOffsetX = 0;
    float mLastOffsetY = 0;
```

In `plottable-rhi-layer.cpp` and `scatter-rhi-layer.cpp`, update `setAllOffsets`:

```cpp
void QCPPlottableRhiLayer::setAllOffsets(float offsetX, float offsetY)
{
    mLastOffsetX = offsetX;
    mLastOffsetY = offsetY;
    for (auto& entry : mDrawEntries)
    {
        entry.offsetX = offsetX;
        entry.offsetY = offsetY;
    }
    // UBO needs re-upload but vertex data is unchanged
}
```

(same body in the scatter class, member names identical).

- [ ] **Step 4: Add `refreshLayerTranslationOffsets()` and call it every frame**

In `core.h`, private section (near other render helpers), declare:

```cpp
    void refreshLayerTranslationOffsets();
```

In `core.cpp`, add the definition (place it immediately before `uploadLayerTextures`):

```cpp
/*! \internal

  Refreshes the GPU translation offsets of layers that are skipping their repaint
  via translation, using the freshest axis ranges. replot() sets these offsets too,
  but replots are coalesced (rpQueuedReplot): a range change landing between the
  last replot and this frame would otherwise leave plottable/scatter/colormap
  geometry trailing render-time-updated layers (spans, grid) by up to a frame.

  The canSkipRepaintForTranslation() guard makes this a no-op whenever the offset
  is invalid (zoom, pan beyond one viewport, invalidated buffer) — those frames are
  owned by the replot's full-repaint path and behave exactly as before.
*/
void QCustomPlot::refreshLayerTranslationOffsets()
{
    for (auto it = mPlottableRhiLayers.begin(); it != mPlottableRhiLayers.end(); ++it)
    {
        if (it.key()->canSkipRepaintForTranslation())
        {
            const QPointF offset = it.key()->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
    }
    for (auto it = mScatterRhiLayers.begin(); it != mScatterRhiLayers.end(); ++it)
    {
        if (it.key()->canSkipRepaintForTranslation())
        {
            const QPointF offset = it.key()->pixelOffset();
            it.value()->setAllOffsets(static_cast<float>(offset.x()),
                                      static_cast<float>(offset.y()));
        }
    }
    for (auto* crl : mColormapRhiLayers)
    {
        QCPLayer* layer = crl->layer();
        if (layer && layer->canSkipRepaintForTranslation())
        {
            const QPointF offset = layer->pixelOffset();
            crl->setPixelOffset(static_cast<float>(offset.x()),
                                static_cast<float>(offset.y()));
        }
    }
}
```

Call it at the very top of `uploadLayerTextures()` (before the paint-buffer upload loop), with a short comment:

```cpp
void QCustomPlot::uploadLayerTextures(QRhiResourceUpdateBatch* updates, const QSize& outputSize)
{
    // Freshen translation offsets first — the PRL/SRL/colormap uploadResources()
    // calls below upload per-draw uniforms every frame.
    refreshLayerTranslationOffsets();

    for (const auto& buffer : mPaintBuffers)
    ...
```

- [ ] **Step 5: Run the full autotest suite**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS, all suites. The new test either passes (RHI available) or QSKIPs (offscreen) — a QSKIP is an accepted outcome, consistent with `colormapQuadFollowsPanWhileTranslating`.

- [ ] **Step 6: Commit**

```bash
git -C subprojects/NeoQCP add src/painting/plottable-rhi-layer.h src/painting/plottable-rhi-layer.cpp src/painting/scatter-rhi-layer.h src/painting/scatter-rhi-layer.cpp src/core.h src/core.cpp tests/auto/test-pipeline/test-pipeline.h tests/auto/test-pipeline/test-layer-translation.cpp
git -C subprojects/NeoQCP commit -m "perf: refresh translation offsets at render time so graphs track spans during pans"
```

---

### Task 2: Verification

**Files:** none (verification only)

- [ ] **Step 1: Full NeoQCP autotests** — `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests` — all green.
- [ ] **Step 2: SciQLopPlots build + integration suite** — `meson compile -C build-venv` (add `CLANG_INSTALL_DIR=/usr` if shiboken complains), then per CLAUDE.md: `cd /tmp && PYTHONPATH=<PROJ>/build-venv <PROJ>/.venv/bin/python -m pytest <PROJ>/tests/integration -q` — expect all pass (the previously failing percentile test was fixed by commit 1eb0b97).
- [ ] **Step 3: Manual smoothness check — DEFERRED TO HUMAN.** Live SciQLop session: pan a plot with graph + spans; the graph should now track the spans exactly instead of trailing.

## Self-Review Notes

- **Spec coverage:** refresh mechanism (Task 1 Steps 3-4), test (Step 1), verification (Task 2). All spec sections covered.
- **Type consistency:** `lastUniformOffset()`/`refreshLayerTranslationOffsets()`/`setPixelOffset` names match between the plan's test code and implementation code.
- **Known limitation:** the new test QSKIPs in offscreen CI; the render-time path then has no automated coverage there — same accepted trade-off as the existing RHI-gated tests in this suite.
