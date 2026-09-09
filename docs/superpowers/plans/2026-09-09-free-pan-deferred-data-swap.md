# Free Pan / Deferred Data Swap / GPU Fade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep pans translation-only while new data is fetched, swap every graph's new data in one debounced repaint, and make the busy fade visible on GPU-drawn lines and scatters.

**Architecture:** Three NeoQCP changes: (C) fade toggles invalidate the layer buffer so the next replot rebuilds RHI entries; (B) per-draw alpha uniform in the plottable/scatter RHI layers fed from `painter->opacity()`; (A) `QCPGraph2`/`QCPMultiGraph` stage a replacement source as *pending* and `QCustomPlot` commits all pending sources in one debounced swap. SciQLopPlots gets one integration test.

**Tech Stack:** C++20, Qt 6.11 (QRhi, QtTest), meson, GLSL 440 via `qsb`; Python/pytest-qt for the SciQLopPlots suite.

**Spec:** `docs/superpowers/specs/2026-09-09-free-pan-deferred-data-swap-design.md`

## Global Constraints

- All C++ changes live in the NeoQCP repo (`subprojects/NeoQCP`, its own git repo on branch `main`). Commit with `git -C subprojects/NeoQCP add <files> && git -C subprojects/NeoQCP commit`. The outer SciQLopPlots repo receives only the two docs files, the Python test, and memory updates. Never `git push`.
- **One build or test invocation at a time, in the foreground, waited to completion.** Never background a build, never run two `meson compile`/test commands against the same build dir concurrently.
- Environment for every build/test command (run from the outer repo root `/home/jeandet/Documents/prog/SciQLopPlots`):
  ```bash
  cd /home/jeandet/Documents/prog/SciQLopPlots
  VENV=$(pwd)/.venv
  export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
  export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
  export LLVM_INSTALL_DIR=/usr
  ```
- NeoQCP test build dir: `subprojects/NeoQCP/build` (source dir given explicitly). First time: `meson setup subprojects/NeoQCP/build subprojects/NeoQCP --buildtype=debugoptimized`. Build: `meson compile -C subprojects/NeoQCP/build`. Test binary: `subprojects/NeoQCP/build/tests/auto/auto-tests`.
- Run the NeoQCP autotests **twice** at every "run the full suite" step: once offscreen (`QT_QPA_PLATFORM=offscreen ...`) and once with the real display (`QT_QPA_PLATFORM=xcb ...`, `DISPLAY=:0` is set on this machine) so the RHI-gated tests execute instead of `QSKIP`. Both must report `0 failed`. If the xcb run cannot create a window at all, report it and rely on the offscreen run.
- SciQLopPlots build: `meson compile -C build-venv` (already configured). Integration suite: `cd /tmp && PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q`. Read the real pass/fail line and exit code.
- Conventional commit messages. No comment-decorated blocks; comments only for non-obvious "why".
- Do not change when replots are scheduled for pans, do not touch the span layer, do not touch colormap/histogram plottables.

---

### Task 1: Fade toggles force a layer repaint

**Files:**
- Modify: `subprojects/NeoQCP/src/layer.h` (public section of `QCPLayer`, near `canSkipRepaintForTranslation()` at line 86)
- Modify: `subprojects/NeoQCP/src/layer.cpp` (add method after `canSkipRepaintForTranslation`, line ~378)
- Modify: `subprojects/NeoQCP/src/plottables/plottable.cpp:1088-1099` (`onDebounceTimeout`)
- Modify: `subprojects/NeoQCP/src/core.cpp:3536-3547` (`setupPaintBuffers`, the `allCanTranslate` loop)
- Modify: `subprojects/NeoQCP/tests/auto/test-busy-indicator/test-busy-indicator.h` and `.cpp`

**Interfaces:**
- Consumes: `QCPLayer::mPaintBuffer` (private weak ref, accessible inside QCPLayer), `QCPAbstractPaintBuffer::setInvalidated()`, `QCPLayerable::mLayer`, `QCPLayer::canSkipRepaintForTranslation()`.
- Produces: `void QCPLayer::invalidatePaintBuffer()` (public). Task 2's test relies on a fade toggle producing a full repaint.

- [ ] **Step 0: Build the NeoQCP test tree and record the baseline**

Run (with the Global Constraints env exported):
```bash
meson setup subprojects/NeoQCP/build subprojects/NeoQCP --buildtype=debugoptimized
meson compile -C subprojects/NeoQCP/build
QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -5
```
Expected: `Totals: N passed, 0 failed, S skipped` (record N and S; they are the baseline for later runs). If `meson setup` complains that the dir exists, use `meson setup --reconfigure subprojects/NeoQCP/build subprojects/NeoQCP`.

- [ ] **Step 1: Write the failing test**

In `test-busy-indicator.h`, add after `fullLifecycleExternalBusy();`:
```cpp
    void visualBusyToggleForcesLayerRepaint();
```

In `test-busy-indicator.cpp`, add `#include <cmath>` at the top if missing and append:
```cpp
void TestBusyIndicator::visualBusyToggleForcesLayerRepaint()
{
    // A fade toggle must not be swallowed by the translate-instead-of-repaint
    // path: the layer buffer is invalidated so the next replot rebuilds the
    // GPU entries with the new alpha.
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

    mPlot->xAxis->setRange(50, 1050);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());

    // Sampled inside the toggle signal, before the queued replot can run:
    // the offset is still valid (it is a pan) yet the layer refuses to translate.
    bool invalidatedAtToggle = false;
    connect(graph, &QCPAbstractPlottable::visuallyBusyChanged, this, [&](bool) {
        invalidatedAtToggle = !mainLayer->pixelOffset().isNull()
            && !mainLayer->canSkipRepaintForTranslation();
    });

    graph->setBusyShowDelayMs(0);
    graph->setBusyHideDelayMs(0);
    graph->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(graph->visuallyBusy(), 2000);
    QVERIFY(invalidatedAtToggle);

    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    mPlot->xAxis->setRange(100, 1100);
    QVERIFY(mainLayer->canSkipRepaintForTranslation());

    invalidatedAtToggle = false;
    graph->setBusy(false);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->visuallyBusy(), 2000);
    QVERIFY(invalidatedAtToggle);
}
```
Check the top of `test-busy-indicator.cpp` for how `mPlot` is created in `init()`; the test above assumes a `QCustomPlot* mPlot` member created per test (it is, see `init()` at line 4). Make sure `plottables/plottable-graph2.h` is reachable through `qcustomplot.h`; add `#include <plottables/plottable-graph2.h>` if the compiler cannot find `QCPGraph2`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests TestBusyIndicator::visualBusyToggleForcesLayerRepaint`
(If the autotest runner does not accept a `Class::method` filter, run the whole binary and grep for `visualBusyToggleForcesLayerRepaint`.)
Expected: FAIL at `QVERIFY(!mainLayer->canSkipRepaintForTranslation())` after `setBusy(true)`.

- [ ] **Step 3: Add `QCPLayer::invalidatePaintBuffer()`**

`layer.h`, next to `canSkipRepaintForTranslation()`:
```cpp
    void invalidatePaintBuffer();
```
`layer.cpp`, after `canSkipRepaintForTranslation`:
```cpp
/*!
  Marks this layer's paint buffer as invalidated so the next replot repaints it
  instead of translating the old content.
*/
void QCPLayer::invalidatePaintBuffer()
{
    if (auto pb = mPaintBuffer.toStrongRef())
        pb->setInvalidated();
}
```

- [ ] **Step 4: Invalidate on fade toggle and honour invalidation in `setupPaintBuffers`**

`plottable.cpp`, `onDebounceTimeout`:
```cpp
void QCPAbstractPlottable::onDebounceTimeout()
{
    if (mVisuallyBusy == mEffectiveBusy)
        return;

    mVisuallyBusy = mEffectiveBusy;
    // The GPU entries carry the fade alpha; a translating layer would keep the
    // stale ones, so force one real repaint of this layer.
    if (mLayer)
        mLayer->invalidatePaintBuffer();
    emit visuallyBusyChanged(mVisuallyBusy);

    if (mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
}
```
(Invalidate **before** emitting: the test observes the layer state from the signal.)
`core.cpp`, `setupPaintBuffers`, inside the `for (auto* layer : std::as_const(mLayers))` loop that computes `allCanTranslate`: replace
```cpp
                    if (!layer->canTranslateInsteadOfRepaint())
```
with
```cpp
                    if (!layer->canSkipRepaintForTranslation())
```
(`canSkipRepaintForTranslation()` = same check plus "buffer not invalidated". Without it an invalidated-but-translatable buffer is neither cleared here nor skipped in `replot()`, and `drawToPaintBuffer()` paints over stale content.)

- [ ] **Step 5: Run the full NeoQCP suite (offscreen and xcb)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Then: `QT_QPA_PLATFORM=xcb subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Expected: `0 failed` in both; passed count = baseline + 1.

- [ ] **Step 6: Commit**

```bash
git -C subprojects/NeoQCP add src/layer.h src/layer.cpp src/plottables/plottable.cpp src/core.cpp tests/auto/test-busy-indicator/test-busy-indicator.h tests/auto/test-busy-indicator/test-busy-indicator.cpp
git -C subprojects/NeoQCP commit -m "fix(busy): repaint the layer when the visual busy state toggles"
```

---

### Task 2: Per-draw alpha in the plottable and scatter RHI layers

**Files:**
- Modify: `subprojects/NeoQCP/src/painting/plottable-rhi-layer.h` (DrawEntry, `addPlottable`, `PerDrawUniforms`, accessor) and `.cpp` (`addPlottable` at ~70-95, uniform upload at ~206-221)
- Modify: `subprojects/NeoQCP/src/painting/scatter-rhi-layer.h` (DrawEntry, `addScatter` at 31-35, `PerDrawUniforms` at 50-57) and `.cpp` (`addScatter` at 86, uniform upload at ~338-351)
- Modify: `subprojects/NeoQCP/src/painting/shaders/plottable.vert`, `scatter.vert`, `scatter.frag`
- Modify: `subprojects/NeoQCP/src/plottables/plottable-draw-utils.cpp` (`drawPolylineWithGpuFallback` 34-74, `drawPolylineCached` 76-119)
- Modify: `subprojects/NeoQCP/src/plottables/plottable-graph.cpp:1004-1020` (fill path)
- Modify: `subprojects/NeoQCP/src/plottables/plottable-graph2.cpp:603-610` and `plottable-multigraph.cpp:728-736` (scatter uploads)
- Modify: `subprojects/NeoQCP/tests/auto/test-busy-indicator/test-busy-indicator.h` and `.cpp`

**Interfaces:**
- Consumes: `QCPPainter::opacity()` (QPainter API), `QCustomPlot::plottableRhiLayer(QCPLayer*)`, `QCPAbstractPlottable::effectiveBusyFadeAlpha()`, Task 1's forced repaint on fade toggle.
- Produces: `QCPPlottableRhiLayer::addPlottable(fill, stroke, clipRect, dpr, outputHeight, offsetX = 0, offsetY = 0, float alpha = 1)`; `QCPScatterRhiLayer::addScatter(points, style, clipRect, dpr, outputHeight, offsetX = 0, offsetY = 0, colormapImage = {}, float alpha = 1)`; `const QVector<DrawEntry>& drawEntries() const` on both; `DrawEntry::alpha`.

- [ ] **Step 1: Write the failing test (RHI-gated)**

In `test-busy-indicator.h`, add:
```cpp
    void gpuEntriesCarryFadeAlpha();
```
In `test-busy-indicator.cpp`, add includes `#include <painting/plottable-rhi-layer.h>` and `#include <QTest>` if missing, an anonymous-namespace helper (copy of the one in `test-pipeline/test-layer-translation.cpp`):
```cpp
namespace {
bool showAndHasRhiBusy(QCustomPlot* plot)
{
    plot->show();
    if (!QTest::qWaitForWindowExposed(plot))
        return false;
    QCoreApplication::processEvents();
    return plot->rhi() != nullptr;
}
} // namespace
```
and the test:
```cpp
void TestBusyIndicator::gpuEntriesCarryFadeAlpha()
{
    if (!showAndHasRhiBusy(mPlot))
        QSKIP("No QRhi available — GPU draw entries need a real backend");

    auto* graph = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    QVector<double> keys(1000), values(1000);
    for (int i = 0; i < 1000; ++i)
    {
        keys[i] = i;
        values[i] = std::sin(i * 0.01);
    }
    graph->setData(std::move(keys), std::move(values));
    graph->setBusyShowDelayMs(0);
    graph->setBusyHideDelayMs(0);
    mPlot->xAxis->setRange(0, 1000);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    auto* prl = mPlot->plottableRhiLayer(mPlot->layer("main"));
    QVERIFY(prl);
    QVERIFY(!prl->drawEntries().isEmpty());
    for (const auto& e : prl->drawEntries())
        QCOMPARE(e.alpha, 1.0f);

    graph->setBusy(true);
    QTRY_VERIFY_WITH_TIMEOUT(graph->visuallyBusy(), 2000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(!prl->drawEntries().isEmpty());
    const float fade = static_cast<float>(graph->effectiveBusyFadeAlpha());
    for (const auto& e : prl->drawEntries())
        QVERIFY(qFuzzyCompare(e.alpha, fade));

    graph->setBusy(false);
    QTRY_VERIFY_WITH_TIMEOUT(!graph->visuallyBusy(), 2000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    for (const auto& e : prl->drawEntries())
        QCOMPARE(e.alpha, 1.0f);
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `meson compile -C subprojects/NeoQCP/build`
Expected: compile error — `drawEntries` / `alpha` are not members. (RED is the compile failure.)

- [ ] **Step 3: Add alpha to the plottable RHI layer**

`plottable-rhi-layer.h`:
```cpp
    struct DrawEntry
    {
        int fillOffset = 0;
        int fillVertexCount = 0;
        int strokeOffset = 0;
        int strokeVertexCount = 0;
        float offsetX = 0;  // per-draw pixel offset (applied in vertex shader)
        float offsetY = 0;
        float alpha = 1;    // busy fade, multiplies the premultiplied vertex colour
        QRect scissorRect; // in physical pixels, Y-flipped for Y-up backends
    };
    ...
    void addPlottable(std::span<const float> fillVerts,
                      std::span<const float> strokeVerts,
                      const QRect& clipRect, double dpr,
                      int outputHeight,
                      float offsetX = 0, float offsetY = 0,
                      float alpha = 1);
    ...
    const QVector<DrawEntry>& drawEntries() const { return mDrawEntries; }
```
and the uniforms:
```cpp
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY, alpha;
        float _pad; // pad to 32 bytes for std140
    };
    static_assert(sizeof(PerDrawUniforms) == 32);
```
`plottable-rhi-layer.cpp`: `addPlottable` gets the extra parameter and `entry.alpha = alpha;` right after `entry.offsetY = offsetY;`. In `uploadResources`, the initializer becomes:
```cpp
        PerDrawUniforms params = {
            float(outputSize.width()),
            float(outputSize.height()),
            yFlip,
            dpr,
            entry.offsetX,
            entry.offsetY,
            entry.alpha,
            0.0f
        };
```
`plottable.vert`:
```glsl
layout(std140, binding = 0) uniform ViewportParams {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;  // per-draw pixel offset
    float offsetY;
    float alpha;    // busy fade
} pc;

void main()
{
    float ndcX = ((position.x + pc.offsetX) * pc.dpr / pc.width) * 2.0 - 1.0;
    float ndcY = pc.yFlip * (((position.y + pc.offsetY) * pc.dpr / pc.height) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_color = color * pc.alpha;
}
```
(Colours are premultiplied — `QCPLineExtruder` uses `qcp::rhi::premultipliedColor` — so all four channels scale.)

- [ ] **Step 4: Add alpha to the scatter RHI layer**

`scatter-rhi-layer.h`: add `float alpha = 1;` to `DrawEntry` (next to `offsetX/offsetY`), extend the signature:
```cpp
    void addScatter(std::span<const float> points,
                    const QCPScatterStyle& style,
                    const QRect& clipRect, double dpr, int outputHeight,
                    float offsetX = 0, float offsetY = 0,
                    const QImage& colormapImage = {},
                    float alpha = 1);
    ...
    const QVector<DrawEntry>& drawEntries() const { return mDrawEntries; }
```
uniforms:
```cpp
    struct alignas(16) PerDrawUniforms
    {
        float width, height, yFlip, dpr;
        float offsetX, offsetY;
        float halfSize;
        float useColorAxis;
        float alpha;
        float _pad[3]; // pad to 48 bytes for std140
    };
    static_assert(sizeof(PerDrawUniforms) == 48);
```
`scatter-rhi-layer.cpp`: `addScatter` takes `float alpha` and stores `entry.alpha = alpha;` where the entry's offsets are set; the upload initializer appends `entry.alpha, {0, 0, 0}` after `mUseColorAxis ? 1.0f : 0.0f`. Check the SRB creation uses `sizeof(PerDrawUniforms)` (it does, via `uniformBufferWithDynamicOffset(..., sizeof(PerDrawUniforms))`) so the size change propagates.

`scatter.vert` and `scatter.frag`: add `float alpha;` after `float useColorAxis;` in **both** `Params` blocks (the fragment shader declares the same UBO). In `scatter.frag`, end `main()` with:
```glsl
    fragColor *= alpha;
```
(after the `if/else` that assigns `fragColor`).

- [ ] **Step 5: Feed `painter->opacity()` at every call site**

`plottable-draw-utils.cpp`:
- `drawPolylineWithGpuFallback`: `prl->addPlottable({}, strokeVerts, clipRect, dpr, outputSize.height(), static_cast<float>(gpuOffset.x()), static_cast<float>(gpuOffset.y()), static_cast<float>(painter->opacity()));`
- `drawPolylineCached`: same trailing argument on its `prl->addPlottable(...)`.

`plottable-graph.cpp` fill path: `prl->addPlottable(fillVerts, {}, clipRect(), dpr, outHeight, 0.0f, 0.0f, static_cast<float>(painter->opacity()));`

`plottable-graph2.cpp` scatter upload:
```cpp
                    srl->addScatter(
                        std::span<const float>(mScatterPts.data(), mScatterPts.size()),
                        mScatterStyle, clipRect(),
                        mParentPlot->devicePixelRatioF(),
                        mParentPlot->rhiOutputSize().height(),
                        static_cast<float>(gpuOffset.x()),
                        static_cast<float>(gpuOffset.y()),
                        hasColor ? mScatterColorMapImage : QImage{},
                        static_cast<float>(painter->opacity()));
```
`plottable-multigraph.cpp` scatter upload: same shape, keeping its existing arguments and appending `QImage{}, static_cast<float>(painter->opacity())`.

Grep for any other `addPlottable(` / `addScatter(` callers (`grep -rn "addPlottable(\|addScatter(" subprojects/NeoQCP/src`) and give each the painter opacity when a painter is in scope; leave the default (1) where none is.

- [ ] **Step 6: Run the full NeoQCP suite (offscreen and xcb)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Then: `QT_QPA_PLATFORM=xcb subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Expected: `0 failed` in both. The new test passes under xcb and `QSKIP`s offscreen. If the xcb run fails only in the new test, print the entries' alpha values in the failure message before changing anything else.

- [ ] **Step 7: Commit**

```bash
git -C subprojects/NeoQCP add src/painting/plottable-rhi-layer.h src/painting/plottable-rhi-layer.cpp src/painting/scatter-rhi-layer.h src/painting/scatter-rhi-layer.cpp src/painting/shaders/plottable.vert src/painting/shaders/scatter.vert src/painting/shaders/scatter.frag src/plottables/plottable-draw-utils.cpp src/plottables/plottable-graph.cpp src/plottables/plottable-graph2.cpp src/plottables/plottable-multigraph.cpp tests/auto/test-busy-indicator/test-busy-indicator.h tests/auto/test-busy-indicator/test-busy-indicator.cpp
git -C subprojects/NeoQCP commit -m "feat(rhi): apply the busy fade to GPU-drawn lines and scatters"
```

---

### Task 3: Plot-level debounced data swap

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable.h` (public virtuals near `busy()` at 192; `updateEffectiveBusy` implementation in `.cpp:1074-1086`)
- Modify: `subprojects/NeoQCP/src/core.h` (public API near `setSkipReplotsWhenHidden` at 240; private members near `mReplotQueued` at 394)
- Modify: `subprojects/NeoQCP/src/core.cpp` (constructor at 396; new methods next to `replot`)
- Modify: `subprojects/NeoQCP/src/datasource/async-pipeline.h` (public accessor)
- Create: `subprojects/NeoQCP/tests/auto/test-data-swap/test-data-swap.h`, `test-data-swap.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/meson.build` (`test_srcs`), `subprojects/NeoQCP/tests/auto/autotest.cpp` (test registration — open it and mirror how `TestBusyIndicator` is registered)

**Interfaces:**
- Consumes: `QCustomPlot::mPlottables`, `replot()`.
- Produces: `virtual bool QCPAbstractPlottable::hasPendingData() const` (default false), `virtual void QCPAbstractPlottable::commitPendingData()` (default no-op); `void QCustomPlot::requestDataSwap()`, `void QCustomPlot::setDataSwapDebounceMs(int)`, `int QCustomPlot::dataSwapDebounceMs() const` (default 100); `uint64_t QCPAsyncPipelineBase::currentGeneration() const`. Tasks 4 and 5 override the two virtuals and call `requestDataSwap()`.

- [ ] **Step 1: Write the failing test**

`tests/auto/test-data-swap/test-data-swap.h`:
```cpp
#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestDataSwap : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void pendingCountsAsBusy();
    void requestsWithinWindowCommitOnce();
    void requestAfterWindowCommitsAgain();

private:
    QCustomPlot* mPlot = nullptr;
};
```
`tests/auto/test-data-swap/test-data-swap.cpp`:
```cpp
#include "test-data-swap.h"
#include "qcustomplot.h"

namespace {
// Minimal plottable that always has pending data and counts commits.
class PendingStub : public QCPAbstractPlottable
{
public:
    PendingStub(QCPAxis* k, QCPAxis* v) : QCPAbstractPlottable(k, v) {}
    int commits = 0;
    bool pending = true;

    bool hasPendingData() const override { return pending; }
    void commitPendingData() override { ++commits; }
    void notifyBusy() { updateEffectiveBusy(); }

    double selectTest(const QPointF&, bool, QVariant* = nullptr) const override { return -1; }
    QCPRange getKeyRange(bool& found, QCP::SignDomain = QCP::sdBoth) const override
    { found = false; return {}; }
    QCPRange getValueRange(bool& found, QCP::SignDomain = QCP::sdBoth,
                           const QCPRange& = QCPRange()) const override
    { found = false; return {}; }
    void draw(QCPPainter*) override {}
    void drawLegendIcon(QCPPainter*, const QRectF&) const override {}
};
} // namespace

void TestDataSwap::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestDataSwap::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestDataSwap::pendingCountsAsBusy()
{
    auto* p = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    p->notifyBusy();
    QVERIFY(p->busy());
    p->pending = false;
    p->notifyBusy();
    QVERIFY(!p->busy());
}

void TestDataSwap::requestsWithinWindowCommitOnce()
{
    auto* a = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    auto* b = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    mPlot->setDataSwapDebounceMs(50);
    QSignalSpy replots(mPlot, &QCustomPlot::afterReplot);

    mPlot->requestDataSwap();
    QTest::qWait(10);
    mPlot->requestDataSwap();
    QCOMPARE(a->commits, 0);
    QCOMPARE(b->commits, 0);

    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 1, 2000);
    QCOMPARE(b->commits, 1);
    QVERIFY(replots.count() >= 1);
    QTest::qWait(120);
    QCOMPARE(a->commits, 1);
    QCOMPARE(b->commits, 1);
}

void TestDataSwap::requestAfterWindowCommitsAgain()
{
    auto* a = new PendingStub(mPlot->xAxis, mPlot->yAxis);
    mPlot->setDataSwapDebounceMs(20);
    mPlot->requestDataSwap();
    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 1, 2000);
    mPlot->requestDataSwap();
    QTRY_COMPARE_WITH_TIMEOUT(a->commits, 2, 2000);
}
```
If the pure-virtual signatures in `plottable.h:155-180` differ from the stub above, match the header exactly (the stub must compile as a concrete class).

Register the suite: in `tests/auto/meson.build` add `'test-data-swap/test-data-swap.cpp'` to `test_srcs` and `'test-data-swap/test-data-swap.h'` to `test_headers` (the list fed to `qtmod.compile_moc`). In `tests/auto/autotest.cpp` add `#include "test-data-swap/test-data-swap.h"` next to the other test includes and `QCPTEST(TestDataSwap);` after `QCPTEST(TestBusyIndicator);` (line 66).

- [ ] **Step 2: Run the test to verify it fails**

Run: `meson compile -C subprojects/NeoQCP/build`
Expected: compile error — `hasPendingData`/`commitPendingData`/`requestDataSwap` do not exist.

- [ ] **Step 3: Add the plottable virtuals and fold pending into busy**

`plottable.h`, public section, right after `void setBusy(bool busy);`:
```cpp
    // Deferred data: a replacement source staged while the displayed one keeps
    // rendering (see QCustomPlot::requestDataSwap). Pending data is busy data.
    virtual bool hasPendingData() const { return false; }
    virtual void commitPendingData() {}
```
`plottable.cpp`, `updateEffectiveBusy`:
```cpp
    const bool newBusy = mExternalBusy || pipelineBusy() || hasPendingData();
```
(the rest of the function unchanged).

- [ ] **Step 4: Add the swap timer to `QCustomPlot`**

`core.h`, public (next to `setSkipReplotsWhenHidden`):
```cpp
    // Deferred data swap: plottables that staged a replacement data source call
    // requestDataSwap() once it is ready. The first request opens a window of
    // dataSwapDebounceMs; every plottable ready by the end of it is committed and
    // drawn in a single replot.
    void requestDataSwap();
    void setDataSwapDebounceMs(int ms);
    [[nodiscard]] int dataSwapDebounceMs() const { return mDataSwapDebounceMs; }
```
`core.h`, private members (next to `mReplotQueued`):
```cpp
    QTimer mDataSwapTimer;
    int mDataSwapDebounceMs = 100;
    void commitPendingData();
```
`core.cpp`, constructor body (after the member-initializer list, anywhere before the constructor returns):
```cpp
    mDataSwapTimer.setSingleShot(true);
    connect(&mDataSwapTimer, &QTimer::timeout, this, &QCustomPlot::commitPendingData);
```
`core.cpp`, next to `replot`:
```cpp
void QCustomPlot::requestDataSwap()
{
    // Leading-edge window: later requests ride along instead of restarting the
    // timer, so the swap latency is bounded by one window under streaming data.
    if (!mDataSwapTimer.isActive())
        mDataSwapTimer.start(mDataSwapDebounceMs);
}

void QCustomPlot::setDataSwapDebounceMs(int ms)
{
    mDataSwapDebounceMs = qMax(0, ms);
}

void QCustomPlot::commitPendingData()
{
    for (auto* plottable : std::as_const(mPlottables))
        plottable->commitPendingData();
    replot();
}
```
`core.h` needs `#include <QTimer>` if not already present.

`async-pipeline.h`, public section of `QCPAsyncPipelineBase`:
```cpp
    // Generation bumped by every data/viewport change; finished(gen) reports
    // the generation a delivered result belongs to.
    uint64_t currentGeneration() const { return mGeneration.load(); }
```

- [ ] **Step 5: Run the full NeoQCP suite (offscreen and xcb)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Then: `QT_QPA_PLATFORM=xcb subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Expected: `0 failed`, passed count up by 3.

- [ ] **Step 6: Commit**

```bash
git -C subprojects/NeoQCP add src/plottables/plottable.h src/plottables/plottable.cpp src/core.h src/core.cpp src/datasource/async-pipeline.h tests/auto/test-data-swap tests/auto/meson.build tests/auto/autotest.cpp
git -C subprojects/NeoQCP commit -m "feat(core): debounced data swap window for plottables with pending data"
```

---

### Task 4: Deferred commit in `QCPMultiGraph`

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.h` (public API at 30-45, private members at 136-161)
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.cpp` (constructor connect at 57-58, `setDataSource` 100-122, `onL1Ready` 144-150, `syncComponentCount` 172-187)
- Modify: `subprojects/NeoQCP/tests/auto/test-data-swap/test-data-swap.h` and `.cpp`

**Interfaces:**
- Consumes: Task 3's `hasPendingData()`, `commitPendingData()`, `QCustomPlot::requestDataSwap()`, `QCPAsyncPipelineBase::currentGeneration()`; `qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(std::any&, std::shared_ptr<...>&, bool&)` (`plottables/plottable-l1-cache.h`); `qcp::algo::kResampleThreshold` (100'000 points × columns).
- Produces: `bool QCPMultiGraph::hasPendingData() const override`, `void QCPMultiGraph::commitPendingData() override`. Task 5 mirrors this shape in `QCPGraph2`.

- [ ] **Step 1: Write the failing tests**

`test-data-swap.h`, add slots:
```cpp
    void multiGraphKeepsTranslationWhileDataPending();
    void multiGraphCommitsAfterWindow();
    void multiGraphFirstDataCommitsImmediately();
    void multiGraphLatestPendingWins();
```
`test-data-swap.cpp`, add includes `#include "plottables/plottable-multigraph.h"`, `#include <cmath>`, `#include <vector>`, a helper in the anonymous namespace:
```cpp
std::vector<double> ramp(int n, double x0 = 0.0)
{
    std::vector<double> v(n);
    for (int i = 0; i < n; ++i)
        v[i] = x0 + i;
    return v;
}
std::vector<std::vector<double>> sines(int n, int cols)
{
    std::vector<std::vector<double>> out(cols, std::vector<double>(n));
    for (int c = 0; c < cols; ++c)
        for (int i = 0; i < n; ++i)
            out[c][i] = std::sin(i * 0.01 * (c + 1));
    return out;
}
```
and the tests:
```cpp
void TestDataSwap::multiGraphKeepsTranslationWhileDataPending()
{
    // Large data so the L1 cache is built asynchronously.
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());

    mPlot->setDataSwapDebounceMs(200);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    QVERIFY(mg->hasPendingData());
    QVERIFY(mg->busy());
    QCOMPARE(mg->componentCount(), 2);

    // Displayed geometry untouched: a small pan still translates.
    mPlot->xAxis->setRange(50, n + 50);
    QVERIFY(!mg->stallPixelOffset().isNull());
    QVERIFY(mPlot->layer("main")->canSkipRepaintForTranslation());
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasPendingData());
    QCOMPARE(mg->dataSource()->mainKey(0), 0.0);
}

void TestDataSwap::multiGraphCommitsAfterWindow()
{
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->setDataSwapDebounceMs(30);
    QSignalSpy replots(mPlot, &QCustomPlot::afterReplot);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    QVERIFY(mg->hasPendingData());

    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QCOMPARE(mg->dataSource()->mainKey(0), 1000.0);
    QVERIFY(replots.count() >= 1);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->busy(), 5000);
    // The committed source draws from its own resampled cache: the next replot
    // must not fall back to the "pipeline active, no L1" early return.
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->hasRenderedRange());
}

void TestDataSwap::multiGraphFirstDataCommitsImmediately()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(10), sines(10, 1));
    QVERIFY(!mg->hasPendingData());
    QVERIFY(mg->dataSource());
    QCOMPARE(mg->componentCount(), 1);
}

void TestDataSwap::multiGraphLatestPendingWins()
{
    constexpr int n = 120'000;
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(ramp(n), sines(n, 2));
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!mg->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->setDataSwapDebounceMs(30);
    mg->setData(ramp(n, 1000.0), sines(n, 2));
    mg->setData(ramp(n, 2000.0), sines(n, 3));
    QCOMPARE(mg->componentCount(), 3);

    QTRY_VERIFY_WITH_TIMEOUT(!mg->hasPendingData(), 5000);
    QCOMPARE(mg->dataSource()->mainKey(0), 2000.0);
    QCOMPARE(mg->dataSource()->columnCount(), 3);
}
```
`mainKey(int)` and `columnCount()` are `QCPAbstractMultiDataSource` methods (see `test-multigraph.cpp`'s `dataMainKeyDelegate` for the exact accessor name; use that one if it differs).

- [ ] **Step 2: Run the tests to verify they fail**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | grep -E "multiGraph(Keeps|Commits|First|Latest)|Totals"`
Expected: `multiGraphKeepsTranslationWhileDataPending` FAILs at `QVERIFY(mg->hasPendingData())` (default returns false); `multiGraphFirstDataCommitsImmediately` passes already. (`hasPendingData` exists from Task 3, so this compiles.)

- [ ] **Step 3: Declare the pending state**

`plottable-multigraph.h`, public (after `bool canProduceContent() const override;`):
```cpp
    bool hasPendingData() const override { return mPendingSource != nullptr; }
    void commitPendingData() override;
```
private (after `QTimer mViewportDebounce;`):
```cpp
    // Replacement data staged while the displayed source keeps rendering; the
    // plot commits it through commitPendingData() (see QCustomPlot::requestDataSwap).
    std::shared_ptr<QCPAbstractMultiDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::MultiGraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;

    void applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
    void syncComponentCount(int newCount);
```
Remove the old `void onL1Ready();` and `void syncComponentCount();` declarations.

- [ ] **Step 4: Implement staging, readiness, and commit**

`plottable-multigraph.cpp`, constructor: change the finished connection to forward the generation:
```cpp
    connect(&mPipeline, &QCPMultiGraphPipeline::finished,
            this, [this](uint64_t gen) { onL1Ready(gen); });
```
Add a file-local helper next to `ensureL1TransformMulti`:
```cpp
static bool needsResamplingMulti(const QCPAbstractMultiDataSource& src)
{
    return src.columnCount() > 0
        && static_cast<int64_t>(src.size()) * src.columnCount() >= qcp::algo::kResampleThreshold;
}
```
Replace `setDataSource(std::shared_ptr<...>)`:
```cpp
void QCPMultiGraph::setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    // Keep the displayed geometry (and its GPU translation) alive while the
    // replacement is prepared; the plot commits it in one debounced swap.
    const bool canDefer = source && mDataSource && mHasRenderedRange && mParentPlot;
    if (canDefer)
        stagePendingSource(std::move(source));
    else
        applySourceNow(std::move(source));
}

void QCPMultiGraph::applySourceNow(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource.reset();
    mPendingL1.reset();
    mPendingReady = false;
    mDataSource = std::move(source);
    syncComponentCount(mDataSource ? mDataSource->columnCount() : 0);
    mL1Cache.reset();
    mL2Result.reset();
    mCachedLines.clear();
    mL2Dirty = false;
    mLineCacheDirty = true;
    mNeedsResampling = mDataSource && needsResamplingMulti(*mDataSource);
    if (mDataSource)
        ensureL1TransformMulti(mPipeline, mDataSource->size(), mDataSource->columnCount());
    mPipeline.setSource(mDataSource);
    if (!mPipeline.hasTransform() && mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    updateEffectiveBusy();
}

void QCPMultiGraph::stagePendingSource(std::shared_ptr<QCPAbstractMultiDataSource> source)
{
    mPendingSource = std::move(source);
    mPendingL1.reset();
    mPendingReady = false;
    syncComponentCount(mPendingSource->columnCount());
    ensureL1TransformMulti(mPipeline, mPendingSource->size(), mPendingSource->columnCount());
    mPipeline.setSource(mPendingSource);
    mPendingGeneration = mPipeline.currentGeneration();
    if (!mPipeline.hasTransform())
        markPendingReady();
    updateEffectiveBusy();
}

void QCPMultiGraph::markPendingReady()
{
    mPendingReady = true;
    if (mParentPlot)
        mParentPlot->requestDataSwap();
}

void QCPMultiGraph::commitPendingData()
{
    if (!mPendingSource || !mPendingReady)
        return;
    mDataSource = std::move(mPendingSource);
    mL1Cache = std::move(mPendingL1);
    mPendingReady = false;
    mL2Result.reset();
    mL2Dirty = mL1Cache != nullptr;
    mNeedsResampling = needsResamplingMulti(*mDataSource);
    mCachedLines.clear();
    mLineCacheDirty = true;
    updateEffectiveBusy();
}
```
Replace `onL1Ready`:
```cpp
void QCPMultiGraph::onL1Ready(uint64_t generation)
{
    if (mPendingSource)
    {
        if (generation < mPendingGeneration)
            return; // result of a superseded pending source
        bool l2Dirty = false;
        qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mPendingL1, l2Dirty);
        markPendingReady();
        return;
    }
    qcp::extractL1Cache<qcp::algo::MultiGraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    mLineCacheDirty = true;
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}
```
Replace `syncComponentCount`:
```cpp
void QCPMultiGraph::syncComponentCount(int newCount)
{
    int oldCount = mComponents.size();
    if (newCount > oldCount) {
        mComponents.resize(newCount);
        for (int i = oldCount; i < newCount; ++i) {
            auto& c = mComponents[i];
            QColor color = sDefaultColors[i % sDefaultColors.size()];
            c.pen = QPen(color, 0);
            c.selectedPen = defaultSelectedPen(c.pen);
            c.name = QString("Component %1").arg(i);
        }
    } else if (newCount < oldCount) {
        mComponents.resize(newCount);
    }
}
```
Also check `dataChanged()` (line 124): it uses `mDataSource` and stays as is, except its `mNeedsResampling = ...` line can use `needsResamplingMulti(*mDataSource)`. The export fallback in `draw()` (`mPipeline.runSynchronously(vp); onL1Ready();`) must call `onL1Ready(mPipeline.currentGeneration())`.

- [ ] **Step 5: Run the full NeoQCP suite (offscreen and xcb)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Then: `QT_QPA_PLATFORM=xcb subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Expected: `0 failed`, passed count up by 4. Pay attention to the existing `multiGraphRapidSetDataSource`, `multiGraphDataChangedInvalidatesL1`, `multiGraphFastPanNeverBlank`, `multiGraphTranslationOffsetWhenBusy` tests: if one of them asserts that `setDataSource` immediately replaces `dataSource()` on an already-rendered graph, that assertion now reflects the old behaviour — update the test to wait for `!hasPendingData()` (via `QTRY_VERIFY_WITH_TIMEOUT`) and say so in the commit body. Do not weaken any other assertion.

- [ ] **Step 6: Commit**

```bash
git -C subprojects/NeoQCP add src/plottables/plottable-multigraph.h src/plottables/plottable-multigraph.cpp tests/auto/test-data-swap
git -C subprojects/NeoQCP commit -m "perf(multigraph): stage replacement data and commit it in the plot's swap window"
```

---

### Task 5: Deferred commit in `QCPGraph2`

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-graph2.h` (public near `setDataSource` at 31-32 and `pipelineBusy` at 125; private members at 130-153)
- Modify: `subprojects/NeoQCP/src/plottables/plottable-graph2.cpp` (constructor `finished` connect near line 40-50, `setDataSource` 94-106, `onL1Ready` 134-140, export fallback in `draw()`)
- Modify: `subprojects/NeoQCP/tests/auto/test-data-swap/test-data-swap.h` and `.cpp`

**Interfaces:**
- Consumes: same as Task 4 plus `qcp::algo::GraphResamplerCache`, `ensureL1Transform(QCPGraphPipeline&, int)` (file-local in `plottable-graph2.cpp`).
- Produces: `bool QCPGraph2::hasPendingData() const override`, `void QCPGraph2::commitPendingData() override`.

- [ ] **Step 1: Write the failing tests**

`test-data-swap.h`, add:
```cpp
    void graph2KeepsTranslationWhileDataPending();
    void graph2CommitsAfterWindow();
```
`test-data-swap.cpp`, add `#include "plottables/plottable-graph2.h"` and:
```cpp
void TestDataSwap::graph2KeepsTranslationWhileDataPending()
{
    constexpr int n = 120'000;
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setData(ramp(n), sines(n, 1)[0]);
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!g->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->hasRenderedRange());

    mPlot->setDataSwapDebounceMs(200);
    g->setData(ramp(n, 1000.0), sines(n, 1)[0]);
    QVERIFY(g->hasPendingData());
    QVERIFY(g->busy());

    mPlot->xAxis->setRange(50, n + 50);
    QVERIFY(!g->stallPixelOffset().isNull());
    QVERIFY(mPlot->layer("main")->canSkipRepaintForTranslation());
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->hasPendingData());
}

void TestDataSwap::graph2CommitsAfterWindow()
{
    constexpr int n = 120'000;
    auto* g = new QCPGraph2(mPlot->xAxis, mPlot->yAxis);
    g->setData(ramp(n), sines(n, 1)[0]);
    mPlot->xAxis->setRange(0, n);
    mPlot->yAxis->setRange(-1.5, 1.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QTRY_VERIFY_WITH_TIMEOUT(!g->pipeline().isBusy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);

    mPlot->setDataSwapDebounceMs(30);
    g->setData(ramp(n, 1000.0), sines(n, 1)[0]);
    QVERIFY(g->hasPendingData());
    QTRY_VERIFY_WITH_TIMEOUT(!g->hasPendingData(), 5000);
    QCOMPARE(g->dataSource()->mainKey(0), 1000.0);
    QTRY_VERIFY_WITH_TIMEOUT(!g->busy(), 5000);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(g->hasRenderedRange());
}
```
(`QCPGraph2::setData(KC&& keys, VC&& values)` accepts `std::vector<double>` for both; `mainKey(int)` is on `QCPAbstractDataSource` — check `test-pipeline.cpp` for the accessor name used there and match it.)

- [ ] **Step 2: Run the tests to verify they fail**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | grep -E "graph2(Keeps|Commits)|Totals"`
Expected: both FAIL at `QVERIFY(g->hasPendingData())`.

- [ ] **Step 3: Implement the same pending model in `QCPGraph2`**

`plottable-graph2.h`, public (next to `pipelineBusy`):
```cpp
    bool hasPendingData() const override { return mPendingSource != nullptr; }
    void commitPendingData() override;
```
private:
```cpp
    std::shared_ptr<QCPAbstractDataSource> mPendingSource;
    std::shared_ptr<qcp::algo::GraphResamplerCache> mPendingL1;
    bool mPendingReady = false;
    uint64_t mPendingGeneration = 0;

    void applySourceNow(std::shared_ptr<QCPAbstractDataSource> source);
    void stagePendingSource(std::shared_ptr<QCPAbstractDataSource> source);
    void markPendingReady();
    void onL1Ready(uint64_t generation);
```
(remove the old `void onL1Ready();`). Constructor: the `finished` connection forwards the generation exactly like Task 4.

`plottable-graph2.cpp`:
```cpp
void QCPGraph2::setDataSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    const bool canDefer = source && mDataSource && mHasRenderedRange && mParentPlot;
    if (canDefer)
        stagePendingSource(std::move(source));
    else
        applySourceNow(std::move(source));
}

void QCPGraph2::applySourceNow(std::shared_ptr<QCPAbstractDataSource> source)
{
    mPendingSource.reset();
    mPendingL1.reset();
    mPendingReady = false;
    mDataSource = std::move(source);
    mL1Cache.reset();
    mL2Result.reset();
    mCachedLines.clear();
    mLineCacheDirty = true;
    mL2Dirty = false;
    mNeedsResampling = mDataSource && mDataSource->size() >= qcp::algo::kResampleThreshold;
    if (mDataSource)
        ensureL1Transform(mPipeline, mDataSource->size());
    mPipeline.setSource(mDataSource);
    if (!mPipeline.hasTransform() && mParentPlot)
        mParentPlot->replot(QCustomPlot::rpQueuedReplot);
    updateEffectiveBusy();
}

void QCPGraph2::stagePendingSource(std::shared_ptr<QCPAbstractDataSource> source)
{
    mPendingSource = std::move(source);
    mPendingL1.reset();
    mPendingReady = false;
    ensureL1Transform(mPipeline, mPendingSource->size());
    mPipeline.setSource(mPendingSource);
    mPendingGeneration = mPipeline.currentGeneration();
    if (!mPipeline.hasTransform())
        markPendingReady();
    updateEffectiveBusy();
}

void QCPGraph2::markPendingReady()
{
    mPendingReady = true;
    if (mParentPlot)
        mParentPlot->requestDataSwap();
}

void QCPGraph2::commitPendingData()
{
    if (!mPendingSource || !mPendingReady)
        return;
    mDataSource = std::move(mPendingSource);
    mL1Cache = std::move(mPendingL1);
    mPendingReady = false;
    mL2Result.reset();
    mL2Dirty = mL1Cache != nullptr;
    mNeedsResampling = mDataSource->size() >= qcp::algo::kResampleThreshold;
    mCachedLines.clear();
    mLineCacheDirty = true;
    updateEffectiveBusy();
}

void QCPGraph2::onL1Ready(uint64_t generation)
{
    PROFILE_HERE_N("QCPGraph2::onL1Ready");
    if (mPendingSource)
    {
        if (generation < mPendingGeneration)
            return;
        bool l2Dirty = false;
        qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mPendingL1, l2Dirty);
        markPendingReady();
        return;
    }
    qcp::extractL1Cache<qcp::algo::GraphResamplerCache>(mPipeline.cache(), mL1Cache, mL2Dirty);
    mLineCacheDirty = true;
    if (parentPlot())
        parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}
```
Export fallback in `QCPGraph2::draw()` (search for `runSynchronously`): call `onL1Ready(mPipeline.currentGeneration())`. Any other `onL1Ready()` caller gets the same argument.

- [ ] **Step 4: Run the full NeoQCP suite (offscreen and xcb)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Then: `QT_QPA_PLATFORM=xcb subprojects/NeoQCP/build/tests/auto/auto-tests 2>&1 | tail -3`
Expected: `0 failed`, passed count up by 2. Same rule as Task 4 for existing graph2 tests (`graph2LineCacheInvalidatedOnDataChange`, `graph2TranslationResetsOnFreshData`, `graph2LargeToSmallDataFallback`, `graph2FastPanNeverBlank`, `pipelineSourceReplacedDuringJob`): an assertion that only encodes the old immediate-replace behaviour on a rendered graph is updated to wait for `!hasPendingData()`; nothing else is weakened; explain each change in the commit body.

- [ ] **Step 5: Commit**

```bash
git -C subprojects/NeoQCP add src/plottables/plottable-graph2.h src/plottables/plottable-graph2.cpp tests/auto/test-data-swap
git -C subprojects/NeoQCP commit -m "perf(graph2): stage replacement data and commit it in the plot's swap window"
```

---

### Task 6: SciQLopPlots integration test and full verification

**Files:**
- Create: `tests/integration/test_pan_deferred_swap.py`
- Verify: `build-venv` (SciQLopPlots + NeoQCP subproject), `tests/integration`

**Interfaces:**
- Consumes: `SciQLopPlot.plot(callable, labels=[...])` (creates a `SciQLopLineGraphFunction` backed by a `QCPMultiGraph`), `plot.x_axis().set_range(SciQLopPlotRange)`, `graph.busy()`, `graph.data()`, the `BlockingCallable` pattern from `tests/integration/test_busy_on_first_fetch.py`.
- Produces: nothing new; regression pin for the end-to-end behaviour.

- [ ] **Step 1: Write the test**

`tests/integration/test_pan_deferred_swap.py`:
```python
"""Panning a plot with several callable graphs: every graph reports busy while
its fetch runs, and once the fetches return, every graph shows the new range.
Pins the deferred data swap end to end (NeoQCP stages the replacement source,
the plot commits all of them in one window)."""
import threading
import time

import numpy as np
from PySide6.QtWidgets import QApplication

from SciQLopPlots import SciQLopPlotRange


class BlockingCallable:
    def __init__(self):
        self.entered = threading.Event()
        self.release = threading.Event()
        self.calls = 0

    def __call__(self, start, stop):
        self.calls += 1
        self.entered.set()
        self.release.wait(5.0)
        x = np.linspace(start, stop, 200_000, dtype=np.float64)
        y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
        return x, y

    def unblock(self):
        self.release.set()


def _pump_until(predicate, timeout=5.0):
    deadline = time.monotonic() + timeout
    while not predicate() and time.monotonic() < deadline:
        QApplication.processEvents()
        time.sleep(0.005)
    return predicate()


def _first_key(graph):
    data = graph.data()
    if len(data) < 2 or data[0] is None:
        return None
    x = np.asarray(data[0])
    return float(x[0]) if x.size else None


def test_pan_keeps_all_graphs_busy_then_swaps_every_graph(plot):
    providers = [BlockingCallable() for _ in range(3)]
    for p in providers:
        p.unblock()
    graphs = [plot.plot(p, labels=["a", "b"]) for p in providers]
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    assert _pump_until(lambda: all(not g.busy() and _first_key(g) == 0.0 for g in graphs),
                       timeout=10.0)

    for p in providers:
        p.release.clear()
        p.entered.clear()
    plot.x_axis().set_range(SciQLopPlotRange(100.0, 110.0))
    assert all(p.entered.wait(5.0) for p in providers)
    assert all(g.busy() for g in graphs)
    assert all(_first_key(g) == 0.0 for g in graphs)

    for p in providers:
        p.unblock()
    assert _pump_until(lambda: all(not g.busy() and _first_key(g) == 100.0 for g in graphs),
                       timeout=10.0)
```
`graph.data()` returns the buffers handed to `set_data` (`SciQLopMultiGraphBase::data()`, transiently `None` while a batch is being swapped, hence the guard), so the first-key check reads the fetched range, not the displayed one; the busy check is what covers the pending window.

- [ ] **Step 2: Rebuild SciQLopPlots against the modified NeoQCP and run the new test**

Run: `meson compile -C build-venv`
Then: `cd /tmp && PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_pan_deferred_swap.py -q`
Expected: `1 passed`. If the build fails on a stale shiboken wrapper, follow CLAUDE.md: remove the offending `build-venv/SciQLopPlots/bindings/**_wrapper.cpp` and `meson setup --reconfigure build-venv`.

- [ ] **Step 3: Run the full SciQLopPlots integration suite**

Run: `cd /tmp && PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q 2>&1 | tail -5`
Expected: all passed (baseline was 1013 passed on 2026-09-09), exit code 0. A test that fails because it asserted immediate replacement of a rendered graph's data source is updated to wait on `not graph.busy()` and reported; any other failure is investigated, not papered over.

- [ ] **Step 4: Commit (outer repo, explicit pathspecs only)**

```bash
git add tests/integration/test_pan_deferred_swap.py docs/superpowers/specs/2026-09-09-free-pan-deferred-data-swap-design.md docs/superpowers/plans/2026-09-09-free-pan-deferred-data-swap.md
git commit -m "test: pin busy + data swap across several graphs on a pan"
```

- [ ] **Step 5: Manual smoothness check — DEFERRED TO THE USER.** Live SciQLop session: pan a panel with several graphs and spans. Expected: the pan stays fluid while fetches are in flight, graphs dim (after the theme's busy show delay) with the `⟳` legend glyph, then all graphs of a plot swap together, no graph blinks out. Tune `QCustomPlot::setDataSwapDebounceMs` / `theme.set_busy_show_delay_ms` if the defaults feel off.

## Self-Review Notes

- **Spec coverage:** C (Task 1), B (Task 2), A for QCustomPlot/QCPMultiGraph/QCPGraph2 (Tasks 3-5), D (Task 6). Out-of-scope items are listed in the spec and untouched.
- **Type consistency:** `hasPendingData()`/`commitPendingData()`/`requestDataSwap()`/`setDataSwapDebounceMs()`/`currentGeneration()`/`drawEntries()`/`DrawEntry::alpha`/`invalidatePaintBuffer()` are named identically in the task that defines them and every task that uses them.
- **Known limitation:** the GPU alpha test `QSKIP`s offscreen (same trade-off as the existing RHI-gated tests); the xcb run on this machine is the real gate for it.
