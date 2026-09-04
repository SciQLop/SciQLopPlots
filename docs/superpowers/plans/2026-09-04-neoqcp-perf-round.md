# NeoQCP Performance Round Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate three confirmed NeoQCP performance/correctness defects: per-frame GPU object churn in the span RHI layer, stale RHI-layer/axis registrations (use-after-free), and wasted CPU replots while the widget is hidden.

**Architecture:** Spec: `docs/superpowers/specs/2026-09-04-neoqcp-perf-round-design.md`. The span fix mirrors the established `QCPGridRhiLayer` dirty-detection pattern (cheap per-frame signature comparison, rebuild only on change) and makes span draw-group UBO/SRBs persistent across rebuilds. Stale-entry fixes prune `mPlottableRhiLayers`/`mScatterRhiLayers` in `removeLayer()` and unregister axes from `QCPGridRhiLayer` in `~QCPAxis()` via a new non-lazy accessor. The hidden-replot guard is an **opt-in** `QCustomPlot` property (default off to preserve QCustomPlot offscreen-rendering semantics), enabled by SciQLopPlots.

**Tech Stack:** C++20, Qt 6 (QRhi), meson, QtTest.

## Global Constraints

- All NeoQCP changes happen in the `subprojects/NeoQCP` checkout (its own git repo, remote `origin` = SciQLop/NeoQCP). Commit NeoQCP work with `git -C subprojects/NeoQCP ...` using conventional-commit messages (repo style: `feat: ...`, `fix: ...`).
- The only SciQLopPlots-repo change is the one-line opt-in in Task 4 plus this plan file; commit those separately in the outer repo.
- C++20 is available (`cpp_std=c++20` in `subprojects/NeoQCP/meson.build:3`).
- Tests follow the existing `tests/auto/test-grid-rhi` pattern: hidden `QCustomPlot`, smoke-test style, registered in `tests/auto/autotest.cpp` (include + `QCPTEST(...)`) and in both `test_srcs` and `test_headers` lists in `tests/auto/meson.build`.
- Autotests run with the widget **never shown** — `mRhi` is null there. That is why the hidden-replot guard MUST be opt-in (default `false`): existing autotests and QCustomPlot offscreen-rendering users rely on `replot()` painting into pixmap buffers while hidden.
- NeoQCP build dir: `subprojects/NeoQCP/build` (may first need `meson setup --reconfigure subprojects/NeoQCP/build` — it was generated with an older meson). Build: `meson compile -C subprojects/NeoQCP/build`. Test binary: `subprojects/NeoQCP/build/tests/auto/auto-tests` (use `QT_QPA_PLATFORM=offscreen` if there is no display).
- Do NOT touch the dead `#ifdef QCUSTOMPLOT_USE_OPENGL` block in `src/SciQLopPlot.cpp:92-94` — out of scope.

---

### Task 1: Span layer — dirty detection + persistent draw groups

**Files:**
- Modify: `subprojects/NeoQCP/src/painting/span-rhi-layer.h`
- Modify: `subprojects/NeoQCP/src/painting/span-rhi-layer.cpp`
- Create: `subprojects/NeoQCP/tests/auto/test-span-rhi/test-span-rhi.h`
- Create: `subprojects/NeoQCP/tests/auto/test-span-rhi/test-span-rhi.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/autotest.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/meson.build`

**Interfaces:**
- Consumes: existing `QCPSpanRhiLayer::registerSpan/unregisterSpan/markGeometryDirty` (unchanged signatures); `QCPItemVSpan/QCPItemHSpan/QCPItemRSpan` public edges (`lowerEdge`, `upperEdge`, `leftEdge`, `rightEdge`, `topEdge`, `bottomEdge`, all `QCPItemPosition* const`) and `brush()/borderPen()/selectedBrush()/selectedBorderPen()/selected()` getters.
- Produces: new public method `bool QCPSpanRhiLayer::detectGeometryChanges()` — recomputes the cheap change signature, updates the internal cache, returns `true` when anything affecting geometry changed since the last call. `uploadResources()` calls it every frame; tests call it directly. Safe to call on a `QCPSpanRhiLayer` constructed with `nullptr` RHI (never touches `mRhi`).

- [ ] **Step 1: Write the failing test**

Create `subprojects/NeoQCP/tests/auto/test-span-rhi/test-span-rhi.h`:

```cpp
#pragma once
#include <QtTest/QtTest>

class QCustomPlot;
class QCPItemVSpan;

class TestSpanRhi : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void firstDetectionReportsChange();
    void unchangedFrameReportsNoChange();
    void detectsEdgeMove();
    void detectsBrushChange();
    void detectsAxisRangeChange();
    void detectsAxisRectResize();

private:
    QCustomPlot* mPlot = nullptr;
    QCPItemVSpan* mSpan = nullptr;
};
```

Create `subprojects/NeoQCP/tests/auto/test-span-rhi/test-span-rhi.cpp`:

```cpp
#include "test-span-rhi.h"
#include "../../../src/qcp.h"
#include "../../../src/painting/span-rhi-layer.h"
#include "../../../src/items/item-vspan.h"

void TestSpanRhi::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mSpan = new QCPItemVSpan(mPlot);
    mSpan->lowerEdge->setCoords(2, 0);
    mSpan->upperEdge->setCoords(4, 0);
    mPlot->replot();
}

void TestSpanRhi::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
    mSpan = nullptr;
}

void TestSpanRhi::firstDetectionReportsChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::unchangedFrameReportsNoChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    QVERIFY(!layer.detectGeometryChanges());
}

void TestSpanRhi::detectsEdgeMove()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->upperEdge->setCoords(6, 0);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsBrushChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mSpan->setBrush(QBrush(Qt::red));
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRangeChange()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->xAxis->setRange(5, 15);
    QVERIFY(layer.detectGeometryChanges());
}

void TestSpanRhi::detectsAxisRectResize()
{
    QCPSpanRhiLayer layer(nullptr);
    layer.registerSpan(mSpan);
    QVERIFY(layer.detectGeometryChanges());
    mPlot->resize(500, 400);
    mPlot->replot();
    QVERIFY(layer.detectGeometryChanges());
}
```

Register the test: in `subprojects/NeoQCP/tests/auto/autotest.cpp` add
`#include "test-span-rhi/test-span-rhi.h"` after the `test-scatter-rhi` include and
`QCPTEST(TestSpanRhi);` after `QCPTEST(TestScatterRhi);`. In
`subprojects/NeoQCP/tests/auto/meson.build` add
`'test-span-rhi/test-span-rhi.cpp',` to `test_srcs` (after the test-scatter-rhi line)
and `'test-span-rhi/test-span-rhi.h',` to `test_headers` (same position).

- [ ] **Step 2: Run test to verify it fails**

Run: `meson setup --reconfigure subprojects/NeoQCP/build` (only if the build dir errors about meson version), then `meson compile -C subprojects/NeoQCP/build`
Expected: FAIL to compile — `detectGeometryChanges` is not a member of `QCPSpanRhiLayer`.

- [ ] **Step 3: Header — add signature cache and detection API**

In `subprojects/NeoQCP/src/painting/span-rhi-layer.h`, add forward declarations near the others:

```cpp
class QBrush;
class QPen;
```

Add the public method after `markGeometryDirty();`:

```cpp
    // Recomputes the cheap per-span/per-axis-rect change signature, updates the
    // internal cache and returns true when anything affecting geometry changed since
    // the last call. Called by uploadResources() every frame; also used by tests.
    // Never touches mRhi.
    bool detectGeometryChanges();
```

Add the private struct and method before `void rebuildGeometry(...);`:

```cpp
    struct SpanSignature
    {
        float e0 = 0, e1 = 0, e2 = 0, e3 = 0; // edge pixels; meaning depends on span type
        quint32 fillRgba = 0;
        quint32 borderRgba = 0;
        float borderWidth = 0;
        int borderStyle = 0;
        bool borderCosmetic = false;
        bool selected = false;
        bool operator==(const SpanSignature& other) const = default;
    };

    SpanSignature computeSignature(QCPAbstractItem* span) const;
```

Add the cache member next to `mLastAxisRectBounds`:

```cpp
    QVector<SpanSignature> mSignatureCache;
```

- [ ] **Step 4: Implement `detectGeometryChanges()` and `computeSignature()`**

In `subprojects/NeoQCP/src/painting/span-rhi-layer.cpp`, add include (safe even if redundant):

```cpp
#include "../items/item-position.h"
```

Add after `markGeometryDirty()`:

```cpp
bool QCPSpanRhiLayer::detectGeometryChanges()
{
    QVector<SpanSignature> signatures;
    signatures.reserve(mSpans.size());
    QMap<QCPAxisRect*, QRect> bounds;
    for (auto* span : mSpans)
    {
        signatures.append(computeSignature(span));
        if (auto* ar = span->clipAxisRect())
            bounds.insert(ar, QRect(ar->left(), ar->top(), ar->width(), ar->height()));
    }

    const bool changed = (signatures != mSignatureCache) || (bounds != mLastAxisRectBounds);
    mSignatureCache = signatures;
    mLastAxisRectBounds = bounds;
    return changed;
}

QCPSpanRhiLayer::SpanSignature QCPSpanRhiLayer::computeSignature(QCPAbstractItem* span) const
{
    SpanSignature sig;
    QCPAxisRect* ar = span->clipAxisRect();
    if (!ar)
        return sig;

    const auto fillStyle = [&sig](const QBrush& brush, const QPen& pen)
    {
        if (brush.style() != Qt::NoBrush)
            sig.fillRgba = brush.color().rgba();
        sig.borderStyle = int(pen.style());
        sig.borderRgba = pen.color().rgba();
        sig.borderWidth = float(pen.widthF());
        sig.borderCosmetic = pen.isCosmetic();
    };

    if (auto* vspan = qobject_cast<QCPItemVSpan*>(span))
    {
        auto* keyAxis = ar->axis(QCPAxis::atBottom);
        if (!keyAxis)
            keyAxis = ar->axis(QCPAxis::atTop);
        if (keyAxis)
        {
            sig.e0 = float(vspan->lowerEdge->typeX() == QCPItemPosition::ptPlotCoords
                               ? keyAxis->coordToPixel(vspan->lowerEdge->coords().x())
                               : vspan->lowerEdge->pixelPosition().x());
            sig.e1 = float(vspan->upperEdge->typeX() == QCPItemPosition::ptPlotCoords
                               ? keyAxis->coordToPixel(vspan->upperEdge->coords().x())
                               : vspan->upperEdge->pixelPosition().x());
        }
        fillStyle(vspan->selected() ? vspan->selectedBrush() : vspan->brush(),
                  vspan->selected() ? vspan->selectedBorderPen() : vspan->borderPen());
        sig.selected = vspan->selected();
    }
    else if (auto* hspan = qobject_cast<QCPItemHSpan*>(span))
    {
        auto* valAxis = ar->axis(QCPAxis::atLeft);
        if (!valAxis)
            valAxis = ar->axis(QCPAxis::atRight);
        if (valAxis)
        {
            sig.e0 = float(hspan->lowerEdge->typeY() == QCPItemPosition::ptPlotCoords
                               ? valAxis->coordToPixel(hspan->lowerEdge->coords().y())
                               : hspan->lowerEdge->pixelPosition().y());
            sig.e1 = float(hspan->upperEdge->typeY() == QCPItemPosition::ptPlotCoords
                               ? valAxis->coordToPixel(hspan->upperEdge->coords().y())
                               : hspan->upperEdge->pixelPosition().y());
        }
        fillStyle(hspan->selected() ? hspan->selectedBrush() : hspan->brush(),
                  hspan->selected() ? hspan->selectedBorderPen() : hspan->borderPen());
        sig.selected = hspan->selected();
    }
    else if (auto* rspan = qobject_cast<QCPItemRSpan*>(span))
    {
        auto* keyAxis = ar->axis(QCPAxis::atBottom);
        if (!keyAxis)
            keyAxis = ar->axis(QCPAxis::atTop);
        auto* valAxis = ar->axis(QCPAxis::atLeft);
        if (!valAxis)
            valAxis = ar->axis(QCPAxis::atRight);
        if (keyAxis && valAxis)
        {
            sig.e0 = float(rspan->leftEdge->typeX() == QCPItemPosition::ptPlotCoords
                               ? keyAxis->coordToPixel(rspan->leftEdge->coords().x())
                               : rspan->leftEdge->pixelPosition().x());
            sig.e1 = float(rspan->rightEdge->typeX() == QCPItemPosition::ptPlotCoords
                               ? keyAxis->coordToPixel(rspan->rightEdge->coords().x())
                               : rspan->rightEdge->pixelPosition().x());
            sig.e2 = float(rspan->topEdge->typeY() == QCPItemPosition::ptPlotCoords
                               ? valAxis->coordToPixel(rspan->topEdge->coords().y())
                               : rspan->topEdge->pixelPosition().y());
            sig.e3 = float(rspan->bottomEdge->typeY() == QCPItemPosition::ptPlotCoords
                               ? valAxis->coordToPixel(rspan->bottomEdge->coords().y())
                               : rspan->bottomEdge->pixelPosition().y());
        }
        fillStyle(rspan->selected() ? rspan->selectedBrush() : rspan->brush(),
                  rspan->selected() ? rspan->selectedBorderPen() : rspan->borderPen());
        sig.selected = rspan->selected();
    }
    return sig;
}
```

- [ ] **Step 5: Run test — signature detection passes**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS (all suites, including the new TestSpanRhi cases). Note: `uploadResources`/`rebuildGeometry` are unchanged at this point, so behavior is unchanged; this step validates the detection logic only.

- [ ] **Step 6: Wire detection into `uploadResources()` (remove forced dirty)**

Replace the forced-dirty block at the top of `QCPSpanRhiLayer::uploadResources()` (currently lines ~395-404):

```cpp
    // Spans bake pixel coordinates into vertices on the CPU (double-to-float
    // conversion to avoid float32 precision loss with large values like Unix
    // timestamps), so geometry must track axis range and layout changes.
    // detectGeometryChanges() does a cheap per-frame comparison so the rebuild only
    // runs when a span or its layout actually changed.
    const bool changed = detectGeometryChanges();
    if (mGeometryDirty || changed)
    {
        rebuildGeometry(dpr, outputSize.height());
        mGeometryDirty = false;
```

(The vertex-buffer grow + `updateDynamicBuffer` body inside the `if` stays unchanged, as does the per-group uniform update loop below it.)

- [ ] **Step 7: Persistent draw groups in `rebuildGeometry()`**

Replace the whole body of `QCPSpanRhiLayer::rebuildGeometry()` with:

```cpp
void QCPSpanRhiLayer::rebuildGeometry(float dpr, int outputHeight)
{
    PROFILE_HERE_N("QCPSpanRhiLayer::rebuildGeometry");

    mStagingVertices.clear();

    // Group spans by axis rect
    QMap<QCPAxisRect*, QVector<QCPAbstractItem*>> groupedSpans;
    for (auto* span : mSpans)
    {
        QCPAxisRect* ar = span->clipAxisRect();
        if (!ar)
            continue;
        groupedSpans[ar].append(span);
    }

    QVector<DrawGroup> newGroups;
    for (auto it = groupedSpans.constBegin(); it != groupedSpans.constEnd(); ++it)
    {
        QCPAxisRect* ar = it.key();
        const auto& spans = it.value();

        int groupVertexStart = mStagingVertices.size() / kFloatsPerVertex;

        for (auto* span : spans)
        {
            if (auto* vspan = qobject_cast<QCPItemVSpan*>(span))
                appendVSpanGeometry(vspan, ar);
            else if (auto* hspan = qobject_cast<QCPItemHSpan*>(span))
                appendHSpanGeometry(hspan, ar);
            else if (auto* rspan = qobject_cast<QCPItemRSpan*>(span))
                appendRSpanGeometry(rspan, ar);
        }

        int groupVertexCount = mStagingVertices.size() / kFloatsPerVertex - groupVertexStart;
        if (groupVertexCount == 0)
            continue;

        DrawGroup group;
        group.axisRect = ar;
        group.vertexOffset = groupVertexStart;
        group.vertexCount = groupVertexCount;
        group.scissorRect = qcp::rhi::computeScissor(
            QRect(ar->left(), ar->top(), ar->width(), ar->height()), dpr, outputHeight);
        newGroups.append(group);
    }

    // Reconcile with existing groups: UBO + SRB are keyed by axis rect and survive
    // rebuilds. Only new groups allocate; only vanished groups release.
    QVector<DrawGroup> reconciled;
    reconciled.reserve(newGroups.size());
    for (auto& group : newGroups)
    {
        for (auto& old : mDrawGroups)
        {
            if (old.axisRect == group.axisRect)
            {
                group.uniformBuffer = old.uniformBuffer;
                group.srb = old.srb;
                old.uniformBuffer = nullptr;
                old.srb = nullptr;
                break;
            }
        }
        if (!group.uniformBuffer)
        {
            group.uniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic,
                                                  QRhiBuffer::UniformBuffer,
                                                  kUniformBufferSize);
            if (!group.uniformBuffer->create())
            {
                qDebug() << Q_FUNC_INFO << "Failed to create per-group UBO";
                delete group.uniformBuffer;
                group.uniformBuffer = nullptr;
                continue;
            }
            group.srb = mRhi->newShaderResourceBindings();
            group.srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage, group.uniformBuffer)
            });
            if (!group.srb->create())
            {
                qDebug() << Q_FUNC_INFO << "Failed to create per-group SRB";
                delete group.srb;
                delete group.uniformBuffer;
                group.srb = nullptr;
                group.uniformBuffer = nullptr;
                continue;
            }
        }
        reconciled.append(group);
    }

    // Free resources of vanished groups (nullptrs already stolen above are safe).
    for (auto& old : mDrawGroups)
    {
        delete old.uniformBuffer;
        delete old.srb;
    }
    mDrawGroups = reconciled;
}
```

Delete the old trailing `mLastAxisRectBounds` update block (the comment saying it is
"currently unused since we always rebuild" plus the loop) — the cache is now owned by
`detectGeometryChanges()`.

- [ ] **Step 8: Run full NeoQCP autotests**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS — all suites including TestSpanRhi, TestVSpan, TestHSpan, TestRSpan, TestGridRhi.

- [ ] **Step 9: Commit**

```bash
git -C subprojects/NeoQCP add src/painting/span-rhi-layer.h src/painting/span-rhi-layer.cpp tests/auto/test-span-rhi/test-span-rhi.h tests/auto/test-span-rhi/test-span-rhi.cpp tests/auto/autotest.cpp tests/auto/meson.build
git -C subprojects/NeoQCP commit -m "perf: rebuild span geometry only on change, persist draw-group GPU resources"
```

---

### Task 2: Prune stale RHI-layer entries in `removeLayer()`

**Files:**
- Modify: `subprojects/NeoQCP/src/core.cpp` (`QCustomPlot::removeLayer`, ~lines 1841-1844)
- Create: `subprojects/NeoQCP/tests/auto/test-layer-removal/test-layer-removal.h`
- Create: `subprojects/NeoQCP/tests/auto/test-layer-removal/test-layer-removal.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/autotest.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/meson.build`

**Interfaces:**
- Consumes: `QCustomPlot::plottableRhiLayer(QCPLayer*)` / `scatterRhiLayer(QCPLayer*)` (`core.cpp:1099-1121`) which populate `mPlottableRhiLayers` / `mScatterRhiLayers` (`core.h:393-394`, `QMap<QCPLayer*, ...>`).
- Produces: `removeLayer()` leaves no entries keyed on the deleted layer. No API change.

- [ ] **Step 1: Write the failing test**

Create `subprojects/NeoQCP/tests/auto/test-layer-removal/test-layer-removal.h`:

```cpp
#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestLayerRemoval : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void removeLayerThenReplotDoesNotCrash();
    void removeAxisThenReplotDoesNotCrash();

private:
    QCustomPlot* mPlot = nullptr;
};
```

Create `subprojects/NeoQCP/tests/auto/test-layer-removal/test-layer-removal.cpp`:

```cpp
#include "test-layer-removal.h"
#include "../../../src/qcp.h"

void TestLayerRemoval::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
    mPlot->replot();
}

void TestLayerRemoval::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestLayerRemoval::removeLayerThenReplotDoesNotCrash()
{
    mPlot->addLayer(QLatin1String("extra"));
    auto* graph = mPlot->addGraph();
    graph->setLayer(QLatin1String("extra"));
    graph->setData({0, 1, 2}, {0, 1, 0});
    mPlot->replot();
    QVERIFY(mPlot->removeLayer(mPlot->layer(QLatin1String("extra"))));
    mPlot->replot();
    mPlot->replot();
    QVERIFY(true);
}

void TestLayerRemoval::removeAxisThenReplotDoesNotCrash()
{
    auto* axis = mPlot->axisRect()->addAxis(QCPAxis::atLeft);
    mPlot->replot();
    QVERIFY(mPlot->axisRect()->removeAxis(axis));
    mPlot->replot();
    mPlot->replot();
    QVERIFY(true);
}
```

Register: `autotest.cpp` — add `#include "test-layer-removal/test-layer-removal.h"` and `QCPTEST(TestLayerRemoval);` after the TestItemPosition entries. `meson.build` — add `'test-layer-removal/test-layer-removal.cpp',` to `test_srcs` and `'test-layer-removal/test-layer-removal.h',` to `test_headers`.

Note: headless (`mRhi == nullptr`) the stale-pointer path is not populated, so this is a smoke/regression test — it passes before and after the fix in autotests. The real UAF is verified in Task 5 under ASAN with a live RHI. The test guards the behavior and exercises `removeLayer`/`removeAxis` + replot cycles.

- [ ] **Step 2: Run test to verify it builds and passes (baseline)**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS (baseline — documents current behavior before the fix).

- [ ] **Step 3: Prune the maps in `removeLayer()`**

In `subprojects/NeoQCP/src/core.cpp`, in `QCustomPlot::removeLayer`, immediately before `mLayers.removeOne(layer);` insert:

```cpp
    // Drop GPU-side layer objects keyed on this layer before deleting it, so later
    // replots/renders never dereference the stale QCPLayer* key.
    delete mPlottableRhiLayers.take(layer);
    delete mScatterRhiLayers.take(layer);
```

- [ ] **Step 4: Run full autotests**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git -C subprojects/NeoQCP add src/core.cpp tests/auto/test-layer-removal/test-layer-removal.h tests/auto/test-layer-removal/test-layer-removal.cpp tests/auto/autotest.cpp tests/auto/meson.build
git -C subprojects/NeoQCP commit -m "fix: prune plottable/scatter RHI layer entries in removeLayer (use-after-free)"
```

---

### Task 3: Unregister axes from the grid RHI layer in `~QCPAxis()`

**Files:**
- Modify: `subprojects/NeoQCP/src/core.h` (after `gridRhiLayer()` declaration, line ~230)
- Modify: `subprojects/NeoQCP/src/core.cpp` (after `QCustomPlot::gridRhiLayer()`, ~line 688)
- Modify: `subprojects/NeoQCP/src/axis/axis.cpp` (`QCPAxis::~QCPAxis`, ~lines 487-492)
- Test: `subprojects/NeoQCP/tests/auto/test-layer-removal/test-layer-removal.cpp` (test already added in Task 2 covers the smoke path)

**Interfaces:**
- Consumes: `QCPGridRhiLayer::unregisterAxis(QCPAxis*)` (public, `grid-rhi-layer.cpp:39-46`); `QCPAxis::mParentPlot` (`axis.h:551`).
- Produces: new public method `QCPGridRhiLayer* QCustomPlot::gridRhiLayerIfExists() const` — non-lazy, never creates the layer, safe from destructors. Used by `~QCPAxis()`.

- [ ] **Step 1: Verify the failing-behavior baseline is already covered**

The Task 2 test `removeAxisThenReplotDoesNotCrash` exercises axis destruction. Headless it passes before and after (no RHI); the UAF verification is Task 5 under ASAN. No new test file needed.

- [ ] **Step 2: Add the non-lazy accessor**

In `subprojects/NeoQCP/src/core.h`, immediately after the `QCPGridRhiLayer* gridRhiLayer();` declaration add:

```cpp
    // Non-lazy accessor: returns the grid RHI layer only if it already exists and
    // never creates one. Safe to call from destructors (e.g. ~QCPAxis).
    QCPGridRhiLayer* gridRhiLayerIfExists() const;
```

In `subprojects/NeoQCP/src/core.cpp`, immediately after `QCustomPlot::gridRhiLayer()` add:

```cpp
QCPGridRhiLayer* QCustomPlot::gridRhiLayerIfExists() const
{
    return mGridRhiLayer;
}
```

- [ ] **Step 3: Unregister in `~QCPAxis()`**

In `subprojects/NeoQCP/src/axis/axis.cpp`, replace the destructor body:

```cpp
QCPAxis::~QCPAxis()
{
    if (mParentPlot)
        if (auto* grl = mParentPlot->gridRhiLayerIfExists())
            grl->unregisterAxis(this);
    delete mAxisPainter;
    delete mGrid; // delete grid here instead of via parent ~QObject for better defined deletion
                  // order
}
```

(`axis.cpp` must see the `QCPGridRhiLayer` declaration for the member call — add `#include "../painting/grid-rhi-layer.h"` if it is not already reachable through `../core.h`; add it explicitly to be safe.)

- [ ] **Step 4: Run full autotests**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git -C subprojects/NeoQCP add src/core.h src/core.cpp src/axis/axis.cpp
git -C subprojects/NeoQCP commit -m "fix: unregister axis from grid RHI layer in ~QCPAxis (use-after-free)"
```

---

### Task 4: Opt-in guard against replots while hidden

**Files:**
- Modify: `subprojects/NeoQCP/src/core.h` (property accessors near other public setters; member near `mReplotting`/`mReplotQueued`, `core.h:380-381`)
- Modify: `subprojects/NeoQCP/src/core.cpp` (`QCustomPlot::replot`, after the `mReplotting` guard)
- Modify: `subprojects/NeoQCP/src/layer.cpp` (`QCPLayer::replot`, top of function)
- Modify: `src/SciQLopPlot.cpp` (SciQLopPlots repo — opt-in in the `_impl::SciQLopPlot` constructor, after the layer-setup block ~line 52)
- Create: `subprojects/NeoQCP/tests/auto/test-hidden-replot/test-hidden-replot.h`
- Create: `subprojects/NeoQCP/tests/auto/test-hidden-replot/test-hidden-replot.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/autotest.cpp`
- Modify: `subprojects/NeoQCP/tests/auto/meson.build`

**Interfaces:**
- Consumes: `QWidget::isVisible()`, signals `beforeReplot()`/`afterReplot()` (`core.h:337-339`).
- Produces: `void QCustomPlot::setSkipReplotsWhenHidden(bool skip)` and `bool QCustomPlot::skipReplotsWhenHidden() const` (default `false`). When enabled and the widget is hidden, `replot()` and `QCPLayer::replot()` do nothing, do not emit replot signals, and leave all paint-buffer dirty/invalidated flags untouched; the first visible replot (forced by `initialize()` on show, `core.cpp:2540-2630`) repaints everything.

- [ ] **Step 1: Write the failing test**

Create `subprojects/NeoQCP/tests/auto/test-hidden-replot/test-hidden-replot.h`:

```cpp
#pragma once
#include <QtTest/QtTest>

class QCustomPlot;

class TestHiddenReplot : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void hiddenReplotRunsByDefault();
    void hiddenReplotSkippedWhenEnabled();
    void replotResumesAfterDisable();

private:
    QCustomPlot* mPlot = nullptr;
};
```

Create `subprojects/NeoQCP/tests/auto/test-hidden-replot/test-hidden-replot.cpp`:

```cpp
#include "test-hidden-replot.h"
#include "../../../src/qcp.h"

void TestHiddenReplot::init()
{
    mPlot = new QCustomPlot(); // never shown: isVisible() == false
    mPlot->resize(400, 300);
    mPlot->xAxis->setRange(0, 10);
    mPlot->yAxis->setRange(0, 10);
}

void TestHiddenReplot::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

void TestHiddenReplot::hiddenReplotRunsByDefault()
{
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(spy.count(), 1);
}

void TestHiddenReplot::hiddenReplotSkippedWhenEnabled()
{
    QSignalSpy beforeSpy(mPlot, &QCustomPlot::beforeReplot);
    QSignalSpy afterSpy(mPlot, &QCustomPlot::afterReplot);
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    QCOMPARE(beforeSpy.count(), 0);
    QCOMPARE(afterSpy.count(), 0);
}

void TestHiddenReplot::replotResumesAfterDisable()
{
    mPlot->setSkipReplotsWhenHidden(true);
    mPlot->replot();
    mPlot->setSkipReplotsWhenHidden(false);
    QSignalSpy spy(mPlot, &QCustomPlot::afterReplot);
    mPlot->replot();
    QCOMPARE(spy.count(), 1);
}
```

Register: `autotest.cpp` — add `#include "test-hidden-replot/test-hidden-replot.h"` and `QCPTEST(TestHiddenReplot);` at the end of the respective lists. `meson.build` — add `'test-hidden-replot/test-hidden-replot.cpp',` to `test_srcs` and `'test-hidden-replot/test-hidden-replot.h',` to `test_headers`.

- [ ] **Step 2: Run test to verify it fails**

Run: `meson compile -C subprojects/NeoQCP/build`
Expected: FAIL to compile — `setSkipReplotsWhenHidden` is not a member of `QCustomPlot`.

- [ ] **Step 3: Add property + guards (NeoQCP)**

In `subprojects/NeoQCP/src/core.h`, add public accessors (near `gridRhiLayer()` declarations is fine):

```cpp
    // When enabled, replot() does nothing while the widget is hidden, avoiding
    // wasted CPU painting into invisible paint buffers. Dirty flags are preserved,
    // so the first visible replot repaints everything. Default: false (classic
    // QCustomPlot behavior, required for offscreen rendering).
    void setSkipReplotsWhenHidden(bool skip);
    bool skipReplotsWhenHidden() const;
```

Add the member next to `mReplotQueued` (`core.h:381`):

```cpp
    bool mSkipReplotsWhenHidden = false;
```

In `subprojects/NeoQCP/src/core.cpp`, add the accessors (e.g. after `gridRhiLayerIfExists()`):

```cpp
void QCustomPlot::setSkipReplotsWhenHidden(bool skip)
{
    mSkipReplotsWhenHidden = skip;
}

bool QCustomPlot::skipReplotsWhenHidden() const
{
    return mSkipReplotsWhenHidden;
}
```

In `QCustomPlot::replot`, immediately after the `mReplotting` reentry guard, insert:

```cpp
    if (mSkipReplotsWhenHidden && !isVisible())
    {
        // Defer all painting to the first visible replot (forced by initialize()).
        // Buffer dirty/invalidated flags are intentionally left untouched.
        mReplotQueued = false;
        return;
    }
```

In `subprojects/NeoQCP/src/layer.cpp`, at the very top of `QCPLayer::replot()` insert:

```cpp
    if (mParentPlot->skipReplotsWhenHidden() && !mParentPlot->isVisible())
        return; // deferred to the first visible replot; dirty flags preserved
```

- [ ] **Step 4: Run full autotests**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS — including the three new TestHiddenReplot cases. Crucially, all pre-existing suites still pass because the default is off (they all replot hidden widgets).

- [ ] **Step 5: Commit NeoQCP side**

```bash
git -C subprojects/NeoQCP add src/core.h src/core.cpp src/layer.cpp tests/auto/test-hidden-replot/test-hidden-replot.h tests/auto/test-hidden-replot/test-hidden-replot.cpp tests/auto/autotest.cpp tests/auto/meson.build
git -C subprojects/NeoQCP commit -m "feat: opt-in skipReplotsWhenHidden to avoid wasted CPU replots while hidden"
```

- [ ] **Step 6: Opt in from SciQLopPlots**

In `src/SciQLopPlot.cpp`, in the `_impl::SciQLopPlot` constructor, after the layer-setup block (after the `addLayer(LayersNames::ColorMap, ...)` / `setMode(...)` lines, ~line 52), add:

```cpp
    setSkipReplotsWhenHidden(true);
```

- [ ] **Step 7: Build SciQLopPlots against the modified NeoQCP**

Run: `meson setup --reconfigure build-venv` (only if it errors about meson version) then `meson compile -C build-venv`
Expected: PASS (compile). This confirms the subproject change is source-compatible with SciQLopPlots.

- [ ] **Step 8: Commit SciQLopPlots side**

```bash
git add src/SciQLopPlot.cpp
git commit -m "perf: skip replots while hidden (NeoQCP opt-in)"
```

---

### Task 5: Full verification

**Files:** none (verification only)

- [ ] **Step 1: NeoQCP autotests clean**

Run: `meson compile -C subprojects/NeoQCP/build && QT_QPA_PLATFORM=offscreen subprojects/NeoQCP/build/tests/auto/auto-tests`
Expected: PASS, all suites.

- [ ] **Step 2: SciQLopPlots ASAN suite (UAF regression check)**

Run: `meson setup --reconfigure build-asan` (required if it errors about meson version), then `meson compile -C build-asan && meson test -C build-asan`
Expected: PASS. This is the meaningful verification for the Task 2/3 use-after-free fixes, which the headless autotests cannot exercise (they need a live RHI).

- [ ] **Step 3: SciQLopPlots python test suite**

Run the project's standard python tests against `build-venv` (per `CLAUDE.md`/project convention: `pytest tests/` with the built module importable).
Expected: PASS.

- [ ] **Step 4: Manual span sanity**

Run a SciQLop session (or a NeoQCP manual test from `subprojects/NeoQCP/tests/manual`) with time-range spans; pan/zoom and move a span. Spans must render identically to before (fill + borders, correct scissoring per axis rect, correct behavior across multi-axis-rect layouts).

- [ ] **Step 5: Commit the plan file**

```bash
git add docs/superpowers/plans/2026-09-04-neoqcp-perf-round.md
git commit -m "docs: implementation plan for NeoQCP perf round"
```

---

## Self-Review Notes

- **Spec coverage:** span churn → Task 1; stale layer entries → Task 2; stale axis entries → Task 3; hidden replots → Task 4; testing/verification → per-task tests + Task 5. All spec sections covered.
- **Known limitation (documented in spec and tasks):** headless autotests cannot exercise RHI-backed paths (`mRhi == nullptr`), so the UAF fixes are verified via smoke tests + the ASAN suite (Task 5 Step 2), and span dirty detection is verified through the RHI-free `detectGeometryChanges()` API.
- **Type consistency:** `detectGeometryChanges()`, `SpanSignature`, `gridRhiLayerIfExists()`, `setSkipReplotsWhenHidden()`/`skipReplotsWhenHidden()` are used with the same names/signatures in every task that references them.
