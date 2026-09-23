# NeoQCP: colour a multigraph by a scalar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `QCPMultiGraph` draws its lines and markers coloured point by point by a scalar, through every decimation level, with zero cost for graphs that are not coloured.

**Architecture:** Every line-producing algorithm gets a compile-time indexed variant that emits, next to each pixel point, the original data index behind it (`-1` for a gap marker). L1 and L2 decimation carry per-column argmin/argmax origin tables, built only for coloured graphs. At draw time a 256-entry LUT turns `values[index]` into a colour bucket; consecutive segments with the same bucket form runs that are extruded (GPU) or painted (QPainter) in one colour. Markers get a per-instance colour through a second scatter pipeline.

**Tech Stack:** C++20, Qt 6.11 (QRhi, QtTest), Meson, NeoQCP.

**Spec:** `/home/jeandet/Documents/prog/SciQLopPlots/docs/superpowers/specs/2026-09-23-line-graph-colour-by-scalar-design.md` — read it first.

**Scope:** NeoQCP only. The SciQLopPlots wiring gets its own plan after the NeoQCP PR is merged.

## Global Constraints

- Repository: `/home/jeandet/Documents/prog/NeoQCP`. Branch `feature/multigraph-colour-by-scalar` from `upstream/main` (`a4ad9f0`).
- **No existing API breaks.** Additions only. Existing functions keep their signatures and behaviour.
- **Zero cost for single-colour graphs:** a graph without colour values runs exactly today's code. Indexed algorithms are separate `if constexpr` instantiations; origin tables are not built; the plain scatter pipeline and 3-float instances are unchanged.
- No per-plottable state on shared RHI layers (layers are shared by every plottable on a `QCPLayer`).
- Index rule: an index is appended in the same statement block as its point, under the same condition. Invariant everywhere: `indices.size() == points.size()`.
- Segment rule: segment `k -> k+1` takes the colour of point `k+1`; a gap bucket (index `-1`, NaN scalar, non-positive on log) means the segment is not drawn.
- Never push to `upstream` (SciQLop/NeoQCP). Push only to `origin` (`jeandet/NeoQCP`).
- **One build or test invocation at a time per build directory, in the foreground.** Never background a build. The feature build dir is `build-colour`; the baseline worktree has its own `build-perf`.
- Build environment (every shell):
  ```bash
  export PATH="/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
  export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig"
  ```
- Run the tests: `cd /home/jeandet/Documents/prog/NeoQCP && QT_QPA_PLATFORM=offscreen ./build-colour/tests/auto/auto-tests 2>&1 | tail -40`. It runs every test class; read the `Totals:` line of each class and the exit code.
- Shell quirk on this machine: `cat` is aliased to `bat` and hangs in heredocs. Write commit messages to a file and use `git commit -F <file>`.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.

## File Structure

| File | Responsibility |
|---|---|
| `src/datasource/algorithms.h` (modify) | `linesToPixels` and `optimizedLineData` become thin wrappers over `detail::…Impl<bool WithIndices>`; new `linesToPixelsIndexed`, `optimizedLineDataIndexed` |
| `src/datasource/abstract-multi-datasource.h` (modify) | two new virtuals |
| `src/datasource/abstract-multi-datasource.cpp` (create) | their generic default implementations (out of line, see Task 2) |
| `src/datasource/soa-multi-datasource.h`, `row-major-multi-datasource.h` (modify) | fast overrides of the two virtuals |
| `src/datasource/graph-resampler.h` (modify) | `MultiColumnBinResult::origin`; `withOrigin` parameter on `binMinMaxMulti(Parallel)` and `buildL1CacheMulti` |
| `src/datasource/resampled-multi-datasource.h` (modify) | L2 origin composition and compaction; `getLinesIndexed` |
| `src/plottables/plottable-linestyle.h` (modify) | pure index mappers `stepLeftIndices`, `stepRightIndices`, `stepCenterIndices`, `impulseIndices` |
| `src/plottables/plottable-color-mapper.h` (create) | `qcp::ColorScalarMapper`: values, gradient, range, scale type, LUT, bucket lookup, generation counter |
| `src/plottables/plottable-color-runs.h` (create) | `qcp::ColorRun`, `qcp::colorRuns(...)` |
| `src/plottables/plottable-draw-utils.{h,cpp}` (modify) | `ExtrusionCache::colorGeneration`; `extrudeColorRuns`, `drawColoredPolylineCached`, `drawColoredPolylineRuns` |
| `src/plottables/plottable-multigraph.{h,cpp}` (modify) | colour API, origin flag, indexed fetch, coloured drawing, coloured markers, legend line helper |
| `src/layoutelements/layoutelement-legend-group.cpp` (modify) | component rows drawn through `QCPMultiGraph::drawComponentLegendLine` |
| `src/painting/scatter-rhi-layer.{h,cpp}` (modify) | per-draw `halfSize`/`useColorAxis`; colour instance buffer; coloured pipeline; `addScatterColored` |
| `src/painting/shaders/scatter_colored.{vert,frag}` (create) | coloured marker shaders |
| `meson.build` (modify) | compile and embed the two new shaders |
| `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}` (create) | every test of this feature |
| `tests/auto/meson.build`, `tests/auto/autotest.cpp` (modify) | register `TestColorByScalar` |
| `tests/perf/multigraph-perf.cpp` (modify) | coloured scenarios |

---

### Task 1: Branch, builds, test class, and `setLineStyle` invalidation

**Files:**
- Create: `tests/auto/test-color-by-scalar/test-color-by-scalar.h`, `tests/auto/test-color-by-scalar/test-color-by-scalar.cpp`
- Modify: `tests/auto/meson.build`, `tests/auto/autotest.cpp`, `src/plottables/plottable-multigraph.h:86`

**Interfaces:**
- Produces: test class `TestColorByScalar` (friend of `QCPMultiGraph`, later of `QCPScatterRhiLayer`), with `mPlot` (a 400x300 `QCustomPlot`) and helper `makeSource(...)`.

- [ ] **Step 1: Branch and baseline worktree**

```bash
cd /home/jeandet/Documents/prog/NeoQCP
git fetch upstream
git switch -c feature/multigraph-colour-by-scalar upstream/main
git log --oneline -1   # expect a4ad9f0
git worktree add ../NeoQCP-baseline a4ad9f0
```

- [ ] **Step 2: Configure and build the feature tree (foreground, wait for it)**

```bash
cd /home/jeandet/Documents/prog/NeoQCP
meson setup build-colour --buildtype=debugoptimized
meson compile -C build-colour
QT_QPA_PLATFORM=offscreen ./build-colour/tests/auto/auto-tests 2>&1 | grep -E "^Totals|FAIL!" | head -60; echo "exit=${pipestatus[1]}"
```
Expected: every class `0 failed`, exit 0. Record the totals: this is the "before" state of the existing suite.

- [ ] **Step 3: Create the test class**

`tests/auto/test-color-by-scalar/test-color-by-scalar.h`:
```cpp
#pragma once
#include <QtTest/QtTest>
#include <memory>
#include <vector>

class QCustomPlot;
class QCPAbstractMultiDataSource;

class TestColorByScalar : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void setLineStyleInvalidatesLineCache();

private:
    QCustomPlot* mPlot = nullptr;
    static std::shared_ptr<QCPAbstractMultiDataSource> makeSource(
        std::vector<double> keys, std::vector<std::vector<double>> columns);
};
```

`tests/auto/test-color-by-scalar/test-color-by-scalar.cpp`:
```cpp
#include "test-color-by-scalar.h"
#include "qcustomplot.h"
#include "datasource/soa-multi-datasource.h"

using SoA = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;

void TestColorByScalar::init()
{
    mPlot = new QCustomPlot();
    mPlot->resize(400, 300);
}

void TestColorByScalar::cleanup()
{
    delete mPlot;
    mPlot = nullptr;
}

std::shared_ptr<QCPAbstractMultiDataSource> TestColorByScalar::makeSource(
    std::vector<double> keys, std::vector<std::vector<double>> columns)
{
    return std::make_shared<SoA>(std::move(keys), std::move(columns));
}

void TestColorByScalar::setLineStyleInvalidatesLineCache()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mPlot->xAxis->setRange(0, 3);
    mPlot->yAxis->setRange(-1, 2);
    mPlot->replot();
    QVERIFY(!mg->mLineCacheDirty);

    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    QVERIFY(mg->mLineCacheDirty);
    QVERIFY(mg->mCachedLines.isEmpty());
}
```

Register it. In `tests/auto/meson.build`, append `'test-color-by-scalar/test-color-by-scalar.cpp',` to `test_srcs` and `'test-color-by-scalar/test-color-by-scalar.h',` to `test_headers`. In `tests/auto/autotest.cpp`, add `#include "test-color-by-scalar/test-color-by-scalar.h"` with the other includes and `QCPTEST(TestColorByScalar);` as the last `QCPTEST` line in `main`. In `src/plottables/plottable-multigraph.h`, next to `friend class TestPipeline;`, add `friend class TestColorByScalar;`.

- [ ] **Step 4: Build and run; the new test fails**

```bash
meson compile -C build-colour && QT_QPA_PLATFORM=offscreen ./build-colour/tests/auto/auto-tests 2>&1 | grep -A3 "setLineStyleInvalidatesLineCache"
```
Expected: `FAIL!  : TestColorByScalar::setLineStyleInvalidatesLineCache() 'mg->mLineCacheDirty' returned FALSE.`

- [ ] **Step 5: Fix `setLineStyle`** (`src/plottables/plottable-multigraph.h:86`)

```cpp
    void setLineStyle(LineStyle style)
    {
        if (mLineStyle == style)
            return;
        mLineStyle = style;
        mLineCacheDirty = true;
        mCachedLines.clear();
    }
```

- [ ] **Step 6: Build and run the whole suite**

Same command as Step 2. Expected: all classes `0 failed`, `TestColorByScalar` included, exit 0.

- [ ] **Step 7: Commit**

```bash
git add tests/auto/test-color-by-scalar tests/auto/meson.build tests/auto/autotest.cpp src/plottables/plottable-multigraph.h
printf 'fix(multigraph): setLineStyle invalidates the line cache\n\nChanging the line style kept the cached lines and their extrusion, so the\nold style stayed on screen until something else dirtied the cache.\nAlso adds the TestColorByScalar class used by the colour-by-scalar work.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 2: Index emission on the raw paths

**Files:**
- Modify: `src/datasource/algorithms.h:232-504`, `src/datasource/abstract-multi-datasource.h`, `src/datasource/soa-multi-datasource.h:73-89`, `src/datasource/row-major-multi-datasource.h:142-160`
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces (namespace `qcp::algo`):
  ```cpp
  template <IndexableNumericRange KC, IndexableNumericRange VC>
  QVector<QPointF> linesToPixelsIndexed(const KC& keys, const VC& values, int begin, int end,
                                        QCPAxis* keyAxis, QCPAxis* valueAxis,
                                        QVector<int>& indices,
                                        double gapThreshold = kDefaultGapThreshold,
                                        const GapVector* precomputedGaps = nullptr);
  template <IndexableNumericRange KC, IndexableNumericRange VC>
  QVector<QPointF> optimizedLineDataIndexed(const KC& keys, const VC& values, int begin, int end,
                                            int pixelWidth, QCPAxis* keyAxis, QCPAxis* valueAxis,
                                            QVector<int>& indices,
                                            const GapVector* precomputedGaps = nullptr);
  ```
- Produces (`QCPAbstractMultiDataSource`):
  ```cpp
  virtual QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                           QCPAxis* keyAxis, QCPAxis* valueAxis,
                                           QVector<int>& sourceIndices) const;
  virtual QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int pixelWidth,
                                                       QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                       QVector<int>& sourceIndices) const;
  ```
  `sourceIndices` is overwritten; on return `sourceIndices.size() == result.size()`; entries are original data indices or `-1` for a NaN/gap marker.

- [ ] **Step 1: Write the failing tests**

Add to the header's `private slots:`
```cpp
    void linesToPixelsIndexedMatchesPlainAndMarksGaps();
    void optimizedLineDataIndexedValuesComeFromTheirIndex();
    void indexedVerticalKeyAxis();
    void defaultIndexedImplementationMatchesSoA();
    void rowMajorIndexedMatchesSoA();
```

Add to the `.cpp` (after the existing includes add `#include "datasource/algorithms.h"`, `#include "datasource/row-major-multi-datasource.h"`, `#include <cmath>`):
```cpp
namespace {

// Wraps a source and forwards only the pure virtuals, so the indexed calls
// hit QCPAbstractMultiDataSource's default implementations.
class ForwardingSource final : public QCPAbstractMultiDataSource
{
public:
    explicit ForwardingSource(std::shared_ptr<QCPAbstractMultiDataSource> inner)
        : mInner(std::move(inner)) {}
    int columnCount() const override { return mInner->columnCount(); }
    int size() const override { return mInner->size(); }
    double keyAt(int i) const override { return mInner->keyAt(i); }
    QCPRange keyRange(bool& f, QCP::SignDomain sd) const override { return mInner->keyRange(f, sd); }
    int findBegin(double k, bool e) const override { return mInner->findBegin(k, e); }
    int findEnd(double k, bool e) const override { return mInner->findEnd(k, e); }
    double valueAt(int c, int i) const override { return mInner->valueAt(c, i); }
    QCPRange valueRange(int c, bool& f, QCP::SignDomain sd, const QCPRange& r) const override
    { return mInner->valueRange(c, f, sd, r); }
    QVector<QPointF> getOptimizedLineData(int c, int b, int e, int w, QCPAxis* k, QCPAxis* v) const override
    { return mInner->getOptimizedLineData(c, b, e, w, k, v); }
    QVector<QPointF> getLines(int c, int b, int e, QCPAxis* k, QCPAxis* v) const override
    { return mInner->getLines(c, b, e, k, v); }
private:
    std::shared_ptr<QCPAbstractMultiDataSource> mInner;
};

// 9000 dense samples on [0, 45], a key gap, then 100 sparse samples on [55, 100];
// NaN values sprinkled in both parts. Exercises every branch of the adaptive path.
void mixedData(std::vector<double>& keys, std::vector<double>& values)
{
    for (int i = 0; i < 9000; ++i)
    {
        keys.push_back(45.0 * i / 8999.0);
        values.push_back(i % 997 == 0 ? std::nan("") : std::sin(i * 0.01) + 0.1 * std::sin(i * 1.3));
    }
    for (int i = 0; i < 100; ++i)
    {
        keys.push_back(55.0 + 45.0 * i / 99.0);
        values.push_back(i % 17 == 0 ? std::nan("") : std::cos(i * 0.2));
    }
}

void checkValuesComeFromIndices(const QVector<QPointF>& pts, const QVector<int>& idx,
                                const std::vector<double>& values, QCPAxis* valueAxis, bool valueIsX)
{
    QCOMPARE(idx.size(), pts.size());
    for (int k = 0; k < pts.size(); ++k)
    {
        if (idx[k] < 0)
        {
            QVERIFY(std::isnan(pts[k].x()) || std::isnan(pts[k].y()));
            continue;
        }
        const double expected = valueAxis->coordToPixel(values[idx[k]]);
        const double got = valueIsX ? pts[k].x() : pts[k].y();
        QVERIFY2(qAbs(got - expected) < 1e-6,
                 qPrintable(QString("point %1: index %2").arg(k).arg(idx[k])));
    }
}

} // namespace

void TestColorByScalar::linesToPixelsIndexedMatchesPlainAndMarksGaps()
{
    std::vector<double> keys {0, 1, 2, 3, 4, 10, 11, 12};
    std::vector<double> values {0, 1, 2, std::nan(""), 4, 5, 6, 7};
    mPlot->xAxis->setRange(0, 12);
    mPlot->yAxis->setRange(0, 8);
    mPlot->replot();

    QVector<int> idx;
    const auto indexed = qcp::algo::linesToPixelsIndexed(keys, values, 0, 8,
                                                         mPlot->xAxis, mPlot->yAxis, idx);
    const auto plain = qcp::algo::linesToPixels(keys, values, 0, 8, mPlot->xAxis, mPlot->yAxis);

    QCOMPARE(indexed.size(), plain.size());
    for (int k = 0; k < plain.size(); ++k)
        QVERIFY(indexed[k] == plain[k] || (std::isnan(indexed[k].x()) && std::isnan(plain[k].x())));
    QCOMPARE(idx, (QVector<int>{0, 1, 2, -1, 4, -1, 5, 6, 7}));
}

void TestColorByScalar::optimizedLineDataIndexedValuesComeFromTheirIndex()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> idx;
    const auto indexed = qcp::algo::optimizedLineDataIndexed(keys, values, 0, n, 400,
                                                             mPlot->xAxis, mPlot->yAxis, idx);
    const auto plain = qcp::algo::optimizedLineData(keys, values, 0, n, 400,
                                                    mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(indexed.size(), plain.size());
    QVERIFY(indexed.size() < n);  // the adaptive path really ran
    checkValuesComeFromIndices(indexed, idx, values, mPlot->yAxis, false);
}

void TestColorByScalar::indexedVerticalKeyAxis()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    mPlot->yAxis->setRange(0, 100);   // key axis is vertical here
    mPlot->xAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> idx;
    const auto pts = qcp::algo::optimizedLineDataIndexed(keys, values, 0, n, 300,
                                                         mPlot->yAxis, mPlot->xAxis, idx);
    checkValuesComeFromIndices(pts, idx, values, mPlot->xAxis, true);

    QVector<int> idx2;
    const auto full = qcp::algo::linesToPixelsIndexed(keys, values, 0, n,
                                                      mPlot->yAxis, mPlot->xAxis, idx2);
    checkValuesComeFromIndices(full, idx2, values, mPlot->xAxis, true);
}

void TestColorByScalar::defaultIndexedImplementationMatchesSoA()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    auto soa = makeSource(keys, {values});
    ForwardingSource generic(soa);
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    const int n = static_cast<int>(keys.size());

    QVector<int> a, b;
    const auto gl = generic.getLinesIndexed(0, 0, n, mPlot->xAxis, mPlot->yAxis, a);
    const auto sl = soa->getLinesIndexed(0, 0, n, mPlot->xAxis, mPlot->yAxis, b);
    QCOMPARE(gl.size(), sl.size());
    QCOMPARE(a, b);

    QVector<int> c, d;
    const auto g = generic.getOptimizedLineDataIndexed(0, 100, 5000, 400, mPlot->xAxis, mPlot->yAxis, c);
    const auto s = soa->getOptimizedLineDataIndexed(0, 100, 5000, 400, mPlot->xAxis, mPlot->yAxis, d);
    QCOMPARE(g.size(), s.size());
    QCOMPARE(c, d);
}

void TestColorByScalar::rowMajorIndexedMatchesSoA()
{
    std::vector<double> keys, values;
    mixedData(keys, values);
    const int n = static_cast<int>(keys.size());
    std::vector<double> interleaved(2 * n);
    for (int i = 0; i < n; ++i)
    {
        interleaved[2 * i] = values[i];
        interleaved[2 * i + 1] = -values[i];
    }
    QCPRowMajorMultiDataSource<double, double> rowMajor(
        std::span<const double>(keys), interleaved.data(), n, 2, 2);
    std::vector<double> negated(values.size());
    std::transform(values.begin(), values.end(), negated.begin(), [](double v) { return -v; });
    auto soa = makeSource(keys, {values, negated});
    mPlot->xAxis->setRange(0, 100);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();

    for (int col = 0; col < 2; ++col)
    {
        QVector<int> a, b;
        rowMajor.getOptimizedLineDataIndexed(col, 0, n, 400, mPlot->xAxis, mPlot->yAxis, a);
        soa->getOptimizedLineDataIndexed(col, 0, n, 400, mPlot->xAxis, mPlot->yAxis, b);
        QCOMPARE(a, b);
        rowMajor.getLinesIndexed(col, 0, n, mPlot->xAxis, mPlot->yAxis, a);
        soa->getLinesIndexed(col, 0, n, mPlot->xAxis, mPlot->yAxis, b);
        QCOMPARE(a, b);
    }
}
```

- [ ] **Step 2: Build; expect compile errors** (`linesToPixelsIndexed` etc. do not exist)

```bash
meson compile -C build-colour 2>&1 | grep -m5 "error"
```

- [ ] **Step 3: Implement the indexed algorithms** in `src/datasource/algorithms.h`

Replace the body of `linesToPixels` (lines 232-290) by a `detail` template and two thin wrappers:
```cpp
namespace detail {

template <bool WithIndices, IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixelsImpl(const KC& keys, const VC& values, int begin, int end,
                                   QCPAxis* keyAxis, QCPAxis* valueAxis,
                                   double gapThreshold, const GapVector* precomputedGaps,
                                   QVector<int>* indices)
{
    using V = std::ranges::range_value_t<VC>;
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(keys)));
    Q_ASSERT(begin >= 0 && end <= static_cast<int>(std::ranges::size(values)));
    if constexpr (WithIndices) indices->clear();
    const int count = end - begin;
    if (count <= 0) return {};

    GapVector computedGaps;
    if (!precomputedGaps)
        computedGaps = detectKeyGaps(keys, begin, end, gapThreshold);
    const auto& gaps = precomputedGaps ? *precomputedGaps : computedGaps;

    QVector<QPointF> result;
    result.reserve(count + count / 10);
    if constexpr (WithIndices) indices->reserve(count + count / 10);

    const bool isVertical = keyAxis->orientation() == Qt::Vertical;
    const auto nanPt = QPointF(qQNaN(), qQNaN());
    const auto keyTf = AffineTransform::fromAxis(keyAxis);
    const auto valTf = AffineTransform::fromAxis(valueAxis);
    const bool bothLinear = keyTf.isLinear && valTf.isLinear;

    for (int i = begin; i < end; ++i)
    {
        int ri = i - begin;
        if (gaps.hasAnyGap && gaps[ri])
        {
            result.append(nanPt);
            if constexpr (WithIndices) indices->append(-1);
        }

        double v = static_cast<double>(values[i]);
        if constexpr (!std::is_integral_v<V>)
        {
            if (std::isnan(v))
            {
                result.append(nanPt);
                if constexpr (WithIndices) indices->append(-1);
                continue;
            }
        }

        double k = static_cast<double>(keys[i]);
        if (bothLinear)
        {
            double kp = keyTf.toPixel(k);
            double vp = valTf.toPixel(v);
            result.append(isVertical ? QPointF(vp, kp) : QPointF(kp, vp));
        }
        else if (isVertical)
            result.append(QPointF(valueAxis->coordToPixel(v), keyAxis->coordToPixel(k)));
        else
            result.append(QPointF(keyAxis->coordToPixel(k), valueAxis->coordToPixel(v)));
        if constexpr (WithIndices) indices->append(i);
    }
    return result;
}

} // namespace detail

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixels(const KC& keys, const VC& values, int begin, int end,
                               QCPAxis* keyAxis, QCPAxis* valueAxis,
                               double gapThreshold = kDefaultGapThreshold,
                               const GapVector* precomputedGaps = nullptr)
{
    return detail::linesToPixelsImpl<false>(keys, values, begin, end, keyAxis, valueAxis,
                                            gapThreshold, precomputedGaps, nullptr);
}

template <IndexableNumericRange KC, IndexableNumericRange VC>
QVector<QPointF> linesToPixelsIndexed(const KC& keys, const VC& values, int begin, int end,
                                      QCPAxis* keyAxis, QCPAxis* valueAxis, QVector<int>& indices,
                                      double gapThreshold = kDefaultGapThreshold,
                                      const GapVector* precomputedGaps = nullptr)
{
    return detail::linesToPixelsImpl<true>(keys, values, begin, end, keyAxis, valueAxis,
                                           gapThreshold, precomputedGaps, &indices);
}
```

`optimizedLineDataMulti` gets no indexed variant: it is only reached through `getOptimizedLineDataAll`, which nothing in NeoQCP calls, and coloured graphs fetch per component anyway.

Do the same for `optimizedLineData` (lines 292-504): move its body into
`template <bool WithIndices, …> QVector<QPointF> detail::optimizedLineDataImpl(keys, values, begin, end, pixelWidth, keyAxis, valueAxis, precomputedGaps, QVector<int>* indices)`, keep `optimizedLineData` as a wrapper calling `<false>` with `nullptr`, and add `optimizedLineDataIndexed` calling `<true>` with `&indices`. Inside the impl, change exactly these places and nothing else:

1. First statement: `if constexpr (WithIndices) indices->clear();`
2. The early delegation (`if (dataCount < maxCount) return linesToPixels(...)`) becomes
   `return linesToPixelsImpl<WithIndices>(keys, values, begin, end, keyAxis, valueAxis, kDefaultGapThreshold, precomputedGaps, indices);`
3. Next to `double minValue …; double maxValue = minValue;` (line ~377) declare `int minIndex = i; int maxIndex = i;`
4. `flushInterval` gains two parameters `int minIdx, int maxIdx` and emits an index with each point, same conditions:
   ```cpp
    auto emit = [&](const QPointF& p, int index) {
        result.append(p);
        if constexpr (WithIndices) indices->append(index);
    };
    auto flushInterval = [&](int intervalFirst, int intervalLast, int intervalCount,
                              double intervalStartKey, double lastEndKey,
                              double minVal, double maxVal, int minIdx, int maxIdx,
                              double epsilon, double nextKey) {
        if (intervalCount >= 2)
        {
            double firstVal = static_cast<double>(values[intervalFirst]);
            if (lastEndKey < intervalStartKey - epsilon)
                emit(toPixel(intervalStartKey + epsilon * 0.2, firstVal), intervalFirst);
            emit(toPixel(intervalStartKey + epsilon * 0.25, minVal), minIdx);
            emit(toPixel(intervalStartKey + epsilon * 0.75, maxVal), maxIdx);
            if (nextKey > intervalStartKey + epsilon * 2)
                emit(toPixel(intervalStartKey + epsilon * 0.8,
                             static_cast<double>(values[intervalLast])), intervalLast);
        }
        else
        {
            emit(toPixel(static_cast<double>(keys[intervalFirst]),
                         static_cast<double>(values[intervalFirst])), intervalFirst);
        }
    };
   ```
   (In the plain instantiation `emit` compiles to `result.append(p)`: same code as today.)
5. Every `flushInterval(...)` call passes `minValue, maxValue, minIndex, maxIndex` in place of `minValue, maxValue`.
6. The gap branch: `result.append(nanPt);` becomes `emit(nanPt, -1);`.
7. Wherever the loop does `minValue = std::min(minValue, v); maxValue = std::max(maxValue, v);` replace by
   ```cpp
            if (v < minValue) { minValue = v; minIndex = i; }
            if (v > maxValue) { maxValue = v; maxIndex = i; }
   ```
   (same result as `std::min`/`std::max`; ties keep the earlier index).
8. Wherever it resets `minValue = v; maxValue = v;` also reset `minIndex = i; maxIndex = i;`.

- [ ] **Step 4: Add the virtuals** in `src/datasource/abstract-multi-datasource.h`, after `getLinesAll`:
```cpp
    // Same points as getLines / getOptimizedLineData, plus the data index behind each
    // point in sourceIndices (-1 for a NaN or gap marker). Generic and slow here;
    // the built-in sources override them.
    virtual QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                             QCPAxis* keyAxis, QCPAxis* valueAxis,
                                             QVector<int>& sourceIndices) const;
    virtual QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end,
                                                         int pixelWidth,
                                                         QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                         QVector<int>& sourceIndices) const;
```
The defaults copy the window and reuse the templated algorithms. They are defined out of line
in a new `src/datasource/abstract-multi-datasource.cpp`: the class has no other out-of-line
member, so inline definitions in a header that some translation units do not include would leave
the vtable referring to undefined symbols. Add `'src/datasource/abstract-multi-datasource.cpp',`
to the `NeoQCP = static_library('NeoQCP', …)` source list in `meson.build`.

`src/datasource/abstract-multi-datasource.cpp`:
```cpp
#include "abstract-multi-datasource.h"
#include "algorithms.h"
#include <vector>

namespace {

// Copies [begin, end) of one column so the templated algorithms can run on any source.
struct ColumnWindow {
    std::vector<double> keys;
    std::vector<double> values;
    ColumnWindow(const QCPAbstractMultiDataSource& src, int column, int begin, int end)
    {
        keys.reserve(end - begin);
        values.reserve(end - begin);
        for (int i = begin; i < end; ++i)
        {
            keys.push_back(src.keyAt(i));
            values.push_back(src.valueAt(column, i));
        }
    }
};

inline void shiftIndices(QVector<int>& indices, int begin)
{
    for (int& i : indices)
        if (i >= 0) i += begin;
}

} // namespace

QVector<QPointF> QCPAbstractMultiDataSource::getLinesIndexed(
    int column, int begin, int end, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    ColumnWindow w(*this, column, begin, end);
    auto pts = qcp::algo::linesToPixelsIndexed(w.keys, w.values, 0, end - begin,
                                               keyAxis, valueAxis, sourceIndices);
    shiftIndices(sourceIndices, begin);
    return pts;
}

QVector<QPointF> QCPAbstractMultiDataSource::getOptimizedLineDataIndexed(
    int column, int begin, int end, int pixelWidth, QCPAxis* keyAxis, QCPAxis* valueAxis,
    QVector<int>& sourceIndices) const
{
    ColumnWindow w(*this, column, begin, end);
    auto pts = qcp::algo::optimizedLineDataIndexed(w.keys, w.values, 0, end - begin, pixelWidth,
                                                   keyAxis, valueAxis, sourceIndices);
    shiftIndices(sourceIndices, begin);
    return pts;
}
```
Gap detection over the copied window equals detection over the full source for that window (it only compares neighbours inside `[begin, end)`), so the default matches the SoA override.

- [ ] **Step 5: Fast overrides.** In `src/datasource/soa-multi-datasource.h`, after `getLines`:
```cpp
    QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                                     QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixelsIndexed(mKeys, mValues[column], begin, end, keyAxis, valueAxis,
                                               sourceIndices, qcp::algo::kDefaultGapThreshold,
                                               &mGapCache.gaps);
    }

    QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int pixelWidth,
                                                 QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                 QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < columnCount());
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineDataIndexed(mKeys, mValues[column], begin, end, pixelWidth,
                                                   keyAxis, valueAxis, sourceIndices, &mGapCache.gaps);
    }
```
In `src/datasource/row-major-multi-datasource.h`, the same two overrides using the column view exactly as `getLines`/`getOptimizedLineData` do:
```cpp
    QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                                     QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < mColumns);
        qcp::detail::StridedColumnView<V> colView(mValues + column, mRows, mStride);
        ensureGapCache(begin, end);
        return qcp::algo::linesToPixelsIndexed(mKeys, colView, begin, end, keyAxis, valueAxis,
                                               sourceIndices, qcp::algo::kDefaultGapThreshold,
                                               &mGapCache.gaps);
    }

    QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int pixelWidth,
                                                 QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                 QVector<int>& sourceIndices) const override
    {
        Q_ASSERT(column >= 0 && column < mColumns);
        qcp::detail::StridedColumnView<V> colView(mValues + column, mRows, mStride);
        ensureGapCache(begin, end);
        return qcp::algo::optimizedLineDataIndexed(mKeys, colView, begin, end, pixelWidth,
                                                   keyAxis, valueAxis, sourceIndices, &mGapCache.gaps);
    }
```

- [ ] **Step 6: Build and run the whole suite**

```bash
meson compile -C build-colour && QT_QPA_PLATFORM=offscreen ./build-colour/tests/auto/auto-tests 2>&1 | grep -E "^Totals|FAIL!" | head -60; echo "exit=${pipestatus[1]}"
```
Expected: the five new tests pass; every existing class still `0 failed`; exit 0. If `optimizedLineDataIndexedValuesComeFromTheirIndex` fails at a specific point, the failing index tells you which emission lost its index — fix the emission, never the test.

- [ ] **Step 7: Commit**

```bash
git add src/datasource tests/auto/test-color-by-scalar meson.build
printf 'feat(datasource): indexed line extraction for colour by scalar\n\nlinesToPixels and optimizedLineData get compile-time indexed variants that\nreturn, next to each pixel point, the data index behind it (-1 for a NaN or\ngap marker). The plain entry points instantiate the unindexed variant, so\nexisting callers run the same code as before.\n\nQCPAbstractMultiDataSource gains getLinesIndexed and\ngetOptimizedLineDataIndexed with generic default implementations; the SoA and\nrow-major sources override them with the fast algorithms.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 3: Perf check 1 — single-colour gate against `a4ad9f0`

**Files:** none changed (measurement only). Results go into the PR description later; keep them in `/tmp/perf-check-1.txt`.

**Interfaces:** Consumes the `multigraph_perf` binary (`tests/perf/multigraph-perf.cpp`, scenarios `l1_resampling`, `l2_resampling`, `adaptive`, `adaptive_multi`, `full_replot`, `pan_replot`, flag `--no-barrier`).

- [ ] **Step 1: Build the baseline perf binary (foreground; the feature build must not be running)**

```bash
cd /home/jeandet/Documents/prog/NeoQCP-baseline
meson setup build-perf --buildtype=debugoptimized
meson compile -C build-perf tests/perf/multigraph_perf
```

- [ ] **Step 2: Build the feature perf binary**

```bash
cd /home/jeandet/Documents/prog/NeoQCP
meson compile -C build-colour tests/perf/multigraph_perf
```

- [ ] **Step 3: Measure, alternating baseline and feature, 3 rounds**

The replot scenarios show a window to get a real `QRhi` (GPU path); run them on the desktop
session, not with `QT_QPA_PLATFORM=offscreen`, and check that each run prints `rhi=yes`.
```bash
for round in 1 2 3; do
  for s in l1_resampling l2_resampling adaptive adaptive_multi full_replot pan_replot; do
    for tree in NeoQCP-baseline/build-perf NeoQCP/build-colour; do
      printf '%s %s ' "$tree" "$s"
      /home/jeandet/Documents/prog/$tree/tests/perf/multigraph_perf --no-barrier $s 2>&1 \
        | grep -o 'rhi=[a-z]*\|per-iter: [0-9.]* ms' | tr '\n' ' '; echo
    done
  done
done | tee /tmp/perf-check-1.txt
```

- [ ] **Step 4: Decide**

For each scenario take the median of the 3 per-iter values per tree. Gate: feature ≤ baseline × 1.03 for every scenario. The feature tree has no coloured graph in these scenarios, so any excess means the plain path changed: inspect the generated code path (did a wrapper stop inlining? did a `reserve` or branch leak into the plain instantiation?) and fix before going on. If a scenario is noisy (spread across rounds > 3%), run 5 rounds for it.

---

### Task 4: L1 and L2 origin tables

**Files:**
- Modify: `src/datasource/graph-resampler.h:241-533`, `src/datasource/resampled-multi-datasource.h`
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Consumes: `getLinesIndexed` / `getOptimizedLineDataIndexed` (Task 2).
- Produces (namespace `qcp::algo`):
  ```cpp
  struct MultiColumnBinResult {
      std::vector<double> keys;
      std::vector<double> values;
      std::vector<int> origin;   // empty, or same layout as values: source index, -1 for an empty slot
      int numColumns = 0;
      int stride() const;
  };
  MultiColumnBinResult binMinMaxMulti(const QCPAbstractMultiDataSource&, int begin, int end,
                                      const QCPRange& keyRange, int numBins, bool withOrigin = false);
  MultiColumnBinResult binMinMaxMultiParallel(/* same */, bool withOrigin = false);
  std::shared_ptr<QCPAbstractMultiDataSource> buildL1CacheMulti(const QCPAbstractMultiDataSource&,
                                      const ViewportParams&, std::any& cache, bool withOrigin = false);
  std::shared_ptr<QCPResampledMultiDataSource> resampleL2Multi(const MultiGraphResamplerCache&,
                                      const ViewportParams&);   // composes origin iff L1 has one
  ```
  `QCPResampledMultiDataSource` overrides `getLinesIndexed` (returns `origin[column * stride + row]`, `-1` for key-gap markers, value-NaN rows skipped like `getLines`) and `getOptimizedLineDataIndexed` (delegates to `getLinesIndexed`, as `getOptimizedLineData` delegates to `getLines`).

- [ ] **Step 1: Write the failing tests**

Header slots:
```cpp
    void l1OriginPointsAtEachBinsExtremes();
    void l1WithoutOriginBuildsNone();
    void l1ParallelOriginEqualsSerial();
    void l2OriginComposesThroughL1AndCompacts();
```

`.cpp` (add `#include "datasource/graph-resampler.h"`, `#include "datasource/resampled-multi-datasource.h"`, `#include <any>`):
```cpp
namespace {

// n samples, two columns, keys 0..n with a hole in [0.3 n, 0.4 n) so some bins stay empty.
std::shared_ptr<QCPAbstractMultiDataSource> holeySource(int n)
{
    std::vector<double> keys, a, b;
    for (int i = 0; i < n; ++i)
    {
        if (i >= 3 * n / 10 && i < 4 * n / 10) continue;
        keys.push_back(i);
        a.push_back(std::sin(i * 0.001) + 0.3 * std::sin(i * 0.37));
        b.push_back(i % 101 == 0 ? std::nan("") : std::cos(i * 0.002));
    }
    return std::make_shared<SoA>(std::move(keys),
                                 std::vector<std::vector<double>>{std::move(a), std::move(b)});
}

void checkOrigin(const qcp::algo::MultiColumnBinResult& bins, const QCPAbstractMultiDataSource& src)
{
    const int s = bins.stride();
    QCOMPARE(static_cast<int>(bins.origin.size()), static_cast<int>(bins.values.size()));
    for (int c = 0; c < bins.numColumns; ++c)
        for (int r = 0; r < s; ++r)
        {
            const double v = bins.values[c * s + r];
            const int o = bins.origin[c * s + r];
            if (std::isnan(v)) { QCOMPARE(o, -1); continue; }
            QVERIFY(o >= 0);
            QCOMPARE(src.valueAt(c, o), v);
        }
}

} // namespace

void TestColorByScalar::l1OriginPointsAtEachBinsExtremes()
{
    auto src = holeySource(50'000);
    bool found = false;
    const auto range = src->keyRange(found);
    const auto bins = qcp::algo::binMinMaxMulti(*src, 0, src->size(), range, 1000, true);
    checkOrigin(bins, *src);
}

void TestColorByScalar::l1WithoutOriginBuildsNone()
{
    auto src = holeySource(50'000);
    bool found = false;
    const auto bins = qcp::algo::binMinMaxMulti(*src, 0, src->size(), src->keyRange(found), 1000);
    QVERIFY(bins.origin.empty());
    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache);
    QVERIFY(std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache)->level1.origin.empty());
}

void TestColorByScalar::l1ParallelOriginEqualsSerial()
{
    auto src = holeySource(1'300'000);   // above the 1M parallel threshold
    bool found = false;
    const auto range = src->keyRange(found);
    const auto serial = qcp::algo::binMinMaxMulti(*src, 0, src->size(), range, 20'000, true);
    const auto parallel = qcp::algo::binMinMaxMultiParallel(*src, 0, src->size(), range, 20'000, true);
    QCOMPARE(parallel.origin, serial.origin);
    checkOrigin(parallel, *src);
}

void TestColorByScalar::l2OriginComposesThroughL1AndCompacts()
{
    auto src = holeySource(200'000);
    std::any cache;
    qcp::algo::buildL1CacheMulti(*src, ViewportParams{}, cache, true);
    const auto* l1 = std::any_cast<qcp::algo::MultiGraphResamplerCache>(&cache);
    QVERIFY(l1 && !l1->level1.origin.empty());

    mPlot->xAxis->setRange(0, 200'000);
    mPlot->yAxis->setRange(-2, 2);
    mPlot->replot();
    ViewportParams vp;
    vp.keyRange = mPlot->xAxis->range();
    vp.valueRange = mPlot->yAxis->range();
    vp.plotWidthPx = 100;   // 400 L2 bins, far fewer than the visible L1 rows
    const auto l2 = qcp::algo::resampleL2Multi(*l1, vp);
    QVERIFY(l2);
    // The hole leaves L1 bins whose values are NaN; L2 skips NaN rows, so the ~40 L2 bins
    // covering only the hole receive no data and are compacted away (720 rows, not 800).
    QVERIFY(l2->size() < 800);

    for (int c = 0; c < 2; ++c)
    {
        QVector<int> idx;
        const auto pts = l2->getLinesIndexed(c, 0, l2->size(), mPlot->xAxis, mPlot->yAxis, idx);
        QCOMPARE(idx.size(), pts.size());
        int gapMarkers = 0;
        for (int k = 0; k < pts.size(); ++k)
        {
            if (idx[k] < 0) { ++gapMarkers; QVERIFY(std::isnan(pts[k].y())); continue; }
            QVERIFY(qAbs(pts[k].y() - mPlot->yAxis->coordToPixel(src->valueAt(c, idx[k]))) < 1e-6);
        }
        QVERIFY(gapMarkers >= 1);   // the hole is a key gap in L2
    }
}
```

- [ ] **Step 2: Build; expect compile errors** (no `withOrigin` parameter, no `origin` member).

- [ ] **Step 3: Implement L1 origin** in `src/datasource/graph-resampler.h`

Add `std::vector<int> origin;` to `MultiColumnBinResult` (between `values` and `numColumns`, with the comment from the Interfaces block).

Turn `binMinMaxMulti` into `template <bool WithOrigin> MultiColumnBinResult detail::binMinMaxMultiImpl(...)` with the current body, plus:
- after `out.values.resize(...)`: `if constexpr (WithOrigin) out.origin.assign(N * numBins * 2, -1);`
- in the per-column loop, next to `double* colOut = …`: `[[maybe_unused]] int* orgOut = WithOrigin ? out.origin.data() + c * s : nullptr;`
- the two updates become
  ```cpp
            if (std::isnan(mn) || v < mn) { mn = v; if constexpr (WithOrigin) orgOut[bin * 2 + 0] = i; }
            if (std::isnan(mx) || v > mx) { mx = v; if constexpr (WithOrigin) orgOut[bin * 2 + 1] = i; }
  ```
and the public function dispatches:
```cpp
inline MultiColumnBinResult binMinMaxMulti(const QCPAbstractMultiDataSource& src, int begin, int end,
                                           const QCPRange& keyRange, int numBins, bool withOrigin = false)
{
    return withOrigin ? detail::binMinMaxMultiImpl<true>(src, begin, end, keyRange, numBins)
                      : detail::binMinMaxMultiImpl<false>(src, begin, end, keyRange, numBins);
}
```
Same treatment for `binMinMaxMultiParallel` (`detail::binMinMaxMultiParallelImpl<bool WithOrigin>`): its fallback calls `binMinMaxMulti(src, begin, end, keyRange, numBins, WithOrigin)`, it assigns `out.origin` like the serial one, and inside `worker` the source index is `srcBegin + i`:
```cpp
                if (std::isnan(mn) || v < mn) { mn = v; if constexpr (WithOrigin) orgOut[bin * 2 + 0] = srcBegin + i; }
                if (std::isnan(mx) || v > mx) { mx = v; if constexpr (WithOrigin) orgOut[bin * 2 + 1] = srcBegin + i; }
```
(chunks own disjoint bin ranges, so the origin writes do not race).

`buildL1CacheMulti` gains `bool withOrigin = false`, passes it to `binMinMaxMultiParallel`, and its early "cache still valid" return also requires the origin state to match. The request itself is not stored in `MultiGraphResamplerCache`: `extractL1Cache` (`plottable-l1-cache.h`) moves the cache out of the pipeline slot after every build, so a flag stored there would be lost. The graph owns it instead (a shared atomic its L1 lambda captures, Task 6); this check only keeps a stale origin-less cache from being reused when origin is wanted:
```cpp
    if (c && c->sourceSize == srcSize && c->columnCount == N
        && c->cachedKeyRange == fullKeyRange
        && (!withOrigin || !c->level1.origin.empty()))
        return nullptr;
```

- [ ] **Step 4: Implement L2 composition** in `src/datasource/resampled-multi-datasource.h`

Turn `resampleL2Multi`'s body into `template <bool WithOrigin> detail::resampleL2MultiImpl(l1Cache, vp)`; the public function dispatches on `!l1Cache.level1.origin.empty()`. Inside:
- next to `l2.values.resize(N * l2Stride);`:
  ```cpp
    [[maybe_unused]] std::vector<int> minRow, maxRow;
    if constexpr (WithOrigin) { minRow.assign(N * l2Bins, -1); maxRow.assign(N * l2Bins, -1); }
  ```
- in the scatter loop:
  ```cpp
            const int row = l1Begin + i;
            if (v < colOut[slot])     { colOut[slot] = v;     if constexpr (WithOrigin) minRow[c * l2Bins + bin] = row; }
            if (v > colOut[slot + 1]) { colOut[slot + 1] = v; if constexpr (WithOrigin) maxRow[c * l2Bins + bin] = row; }
  ```
- `if constexpr (WithOrigin) l2.origin.resize(N * l2Stride);` before the compaction loop; inside it, next to the two value writes:
  ```cpp
            if constexpr (WithOrigin)
            {
                const int mnRow = minRow[c * l2Bins + b], mxRow = maxRow[c * l2Bins + b];
                int* orgOut = l2.origin.data() + c * l2Stride;
                orgOut[outSize]     = mnRow < 0 ? -1 : l1.origin[c * l1Stride + mnRow];
                orgOut[outSize + 1] = mxRow < 0 ? -1 : l1.origin[c * l1Stride + mxRow];
            }
  ```
- the final column shift moves `origin` in lockstep:
  ```cpp
    for (int c = 1; c < N; ++c)
        for (int i = 0; i < outSize; ++i)
        {
            l2.values[c * outSize + i] = l2.values[c * l2Stride + i];
            if constexpr (WithOrigin) l2.origin[c * outSize + i] = l2.origin[c * l2Stride + i];
        }
    l2.values.resize(N * outSize);
    if constexpr (WithOrigin) l2.origin.resize(N * outSize);
  ```

Add to `QCPResampledMultiDataSource`, after `getLines`:
```cpp
    QVector<QPointF> getLinesIndexed(int column, int begin, int end,
                                     QCPAxis* keyAxis, QCPAxis* valueAxis,
                                     QVector<int>& sourceIndices) const override
    {
        sourceIndices.clear();
        if (column < 0 || column >= mBins.numColumns) return {};
        const int s = mBins.stride();
        const bool keyIsVertical = keyAxis->orientation() == Qt::Vertical;
        ensureGapCache(begin, end);
        const auto nanPt = QPointF(qQNaN(), qQNaN());
        const bool hasOrigin = !mBins.origin.empty();

        QVector<QPointF> lines;
        lines.reserve(end - begin + (end - begin) / 10);
        sourceIndices.reserve(lines.capacity());
        for (int i = begin; i < end; ++i)
        {
            if (mGapCache.gaps.hasAnyGap && mGapCache.gaps[i - begin])
            {
                lines.append(nanPt);
                sourceIndices.append(-1);
            }
            const double v = mBins.values[column * s + i];
            if (std::isnan(v)) continue;
            const double kp = keyAxis->coordToPixel(mBins.keys[i]);
            const double vp = valueAxis->coordToPixel(v);
            lines.append(keyIsVertical ? QPointF(vp, kp) : QPointF(kp, vp));
            sourceIndices.append(hasOrigin ? mBins.origin[column * s + i] : -1);
        }
        return lines;
    }

    QVector<QPointF> getOptimizedLineDataIndexed(int column, int begin, int end, int /*pixelWidth*/,
                                                 QCPAxis* keyAxis, QCPAxis* valueAxis,
                                                 QVector<int>& sourceIndices) const override
    {
        return getLinesIndexed(column, begin, end, keyAxis, valueAxis, sourceIndices);
    }
```
(`coordToPixel` gives the same pixels as the affine path of `getLines`; the indexed path is only used for coloured graphs, so it does not need the affine fast path.)

- [ ] **Step 5: Build and run the whole suite** (command of Task 2 Step 6). Expected: four new tests pass, nothing else changes, exit 0.

- [ ] **Step 6: Commit**

```bash
git add src/datasource tests/auto/test-color-by-scalar
printf 'feat(resampler): per-column origin tables for L1 and L2\n\nThe min/max binning can record, per column and bin, the source index of the\nminimum and the maximum. L2 composes them through L1 and compacts them in\nlockstep with its values. Built only on request (compile-time variants), so\nan uncoloured graph bins exactly as before.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 5: Index maps for step and impulse styles

**Files:**
- Modify: `src/plottables/plottable-linestyle.h`
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces (namespace `qcp`), each returning the index array aligned with the output of the matching `to…Lines` for input indices `d` aligned with its input lines:
  ```cpp
  QVector<int> stepLeftIndices(const QVector<int>& d);
  QVector<int> stepRightIndices(const QVector<int>& d);
  QVector<int> stepCenterIndices(const QVector<int>& d);
  QVector<int> impulseIndices(const QVector<int>& d);
  ```
  The transforms are positional (no data-dependent branches), so a pure mapping over the same positions is exact; the point transforms stay untouched.

- [ ] **Step 1: Write the failing tests**

Header slot: `void stepIndexMapsFollowTheTransforms();`

`.cpp` (add `#include "plottables/plottable-linestyle.h"`):
```cpp
void TestColorByScalar::stepIndexMapsFollowTheTransforms()
{
    const QVector<int> d {10, 20, 30};
    QCOMPARE(qcp::stepLeftIndices(d),   (QVector<int>{10, 10, 10, 20, 20, 30}));
    QCOMPARE(qcp::stepRightIndices(d),  (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::stepCenterIndices(d), (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::impulseIndices(d),    (QVector<int>{10, 10, 20, 20, 30, 30}));
    QCOMPARE(qcp::stepLeftIndices({7}), (QVector<int>{7}));   // < 2 points: returned as is

    // Sizes match the point transforms, gaps (-1) travel with their positions.
    QVector<QPointF> pts {{0, 0}, {1, 5}, {2, 2}, {3, 7}};
    const QVector<int> di {0, -1, 2, 3};
    QCOMPARE(qcp::stepLeftIndices(di).size(),   qcp::toStepLeftLines(pts, false).size());
    QCOMPARE(qcp::stepRightIndices(di).size(),  qcp::toStepRightLines(pts, false).size());
    QCOMPARE(qcp::stepCenterIndices(di).size(), qcp::toStepCenterLines(pts, false).size());
    QCOMPARE(qcp::impulseIndices(di).size(),    qcp::toImpulseLines(pts, false, 0).size());
}
```
Check the expected arrays against the transforms before implementing: for `toStepLeftLines`, `result[2i] = (key_i, value_{i-1})` (for `i = 0`, `value_0`), `result[2i+1] = (key_i, value_i)` → `{d0, d0, d0, d1, d1, d2}`; for `toStepRightLines`, `result[2i] = (key_{i-1}, value_i)`, `result[2i+1] = (key_i, value_i)` → `{d0, d0, d1, d1, d2, d2}`; for `toStepCenterLines`, `result[0] = p0`, `result[2i-1] = (mid, value_{i-1})`, `result[2i] = (mid, value_i)`, `result[2n-1] = p_{n-1}` → `{d0, d0, d1, d1, d2, d2}`; for `toImpulseLines`, both points of pair `i` → `d_i`.

- [ ] **Step 2: Build; expect compile errors.**

- [ ] **Step 3: Implement** at the end of `src/plottables/plottable-linestyle.h` (inside `namespace qcp`):
```cpp
// Index maps: the data index behind each point the matching to…Lines transform emits.
inline QVector<int> stepLeftIndices(const QVector<int>& d)
{
    if (d.size() < 2)
        return d;
    QVector<int> r(d.size() * 2);
    int last = d.first();
    for (int i = 0; i < d.size(); ++i)
    {
        r[i * 2 + 0] = last;
        last = d[i];
        r[i * 2 + 1] = last;
    }
    return r;
}

inline QVector<int> stepRightIndices(const QVector<int>& d)
{
    if (d.size() < 2)
        return d;
    QVector<int> r(d.size() * 2);
    for (int i = 0; i < d.size(); ++i)
        r[i * 2 + 0] = r[i * 2 + 1] = d[i];
    return r;
}

inline QVector<int> stepCenterIndices(const QVector<int>& d)
{
    if (d.size() < 2)
        return d;
    QVector<int> r(d.size() * 2);
    r[0] = d[0];
    for (int i = 1; i < d.size(); ++i)
    {
        r[i * 2 - 1] = d[i - 1];
        r[i * 2 + 0] = d[i];
    }
    r[d.size() * 2 - 1] = d.last();
    return r;
}

inline QVector<int> impulseIndices(const QVector<int>& d)
{
    QVector<int> r(d.size() * 2);
    for (int i = 0; i < d.size(); ++i)
        r[i * 2 + 0] = r[i * 2 + 1] = d[i];
    return r;
}
```

- [ ] **Step 4: Build and run the whole suite.** Expected: pass, exit 0.

- [ ] **Step 5: Commit**

```bash
git add src/plottables/plottable-linestyle.h tests/auto/test-color-by-scalar
printf 'feat(linestyle): index maps for the step and impulse transforms\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 6: Colour mapper and the colour API on `QCPMultiGraph`

**Files:**
- Create: `src/plottables/plottable-color-mapper.h`
- Modify: `src/plottables/plottable-multigraph.{h,cpp}` (NeoQCP has no installed-headers list: plain headers are found through the include directory, and only `Q_OBJECT` headers go in `neoqcp_moc_headers`, so the new header needs no `meson.build` change)
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces `qcp::ColorScalarMapper` (header-only):
  ```cpp
  namespace qcp {
  class ColorScalarMapper {
  public:
      static constexpr int kGap = -1;
      ColorScalarMapper();                                   // gpJet, range [0, 1], linear
      bool hasValues() const;
      int size() const;
      void setValues(std::shared_ptr<const std::vector<double>> values);
      void clearValues();
      void setGradient(const QCPColorGradient& gradient);
      const QCPColorGradient& gradient() const;
      void setRange(const QCPRange& range);
      QCPRange range() const;
      void setScaleType(QCPAxis::ScaleType type);
      QCPAxis::ScaleType scaleType() const;
      quint64 generation() const;       // bumped by every setter
      int bucket(int dataIndex) const;  // 0..255, or kGap
      QRgb color(int bucket) const;     // premultiplied ARGB32
  };
  }
  ```
- Produces on `QCPMultiGraph` (public):
  ```cpp
  void setColorValues(std::shared_ptr<const std::vector<double>> values);
  void setColorValues(std::vector<double> values);
  void clearColorValues();
  [[nodiscard]] bool hasColorValues() const;
  void setColorGradient(const QCPColorGradient& gradient);
  [[nodiscard]] QCPColorGradient colorGradient() const;
  void setColorRange(const QCPRange& range);
  [[nodiscard]] QCPRange colorRange() const;
  void setColorScaleType(QCPAxis::ScaleType type);
  [[nodiscard]] QCPAxis::ScaleType colorScaleType() const;
  ```
  Protected members used by later tasks: `qcp::ColorScalarMapper mColor;`, `std::shared_ptr<std::atomic<bool>> mWantOrigin;`, `bool mL2HasOrigin = false;`, `QVector<QVector<int>> mCachedIndices;`, `void invalidateLines();`.

- [ ] **Step 1: Write the failing tests**

Header slots:
```cpp
    void mapperBucketsLinearLogNaNAndGaps();
    void mapperGenerationBumpsOnEverySetter();
    void colorValuesOfTheWrongLengthAreRefused();
    void sameSizeDataRefreshKeepsColorValues();
    void firstColoringRebuildsL1OnceWithOrigin();
```

`.cpp` (add `#include "plottables/plottable-color-mapper.h"`):
```cpp
void TestColorByScalar::mapperBucketsLinearLogNaNAndGaps()
{
    qcp::ColorScalarMapper m;
    m.setValues(std::make_shared<const std::vector<double>>(
        std::vector<double>{0.0, 0.5, 1.0, 2.0, -1.0, std::nan(""), 10.0, 100.0}));
    m.setRange(QCPRange(0, 1));
    QCOMPARE(m.bucket(0), 0);
    QCOMPARE(m.bucket(1), 128);
    QCOMPARE(m.bucket(2), 255);
    QCOMPARE(m.bucket(3), 255);                        // clamped
    QCOMPARE(m.bucket(4), 0);                          // clamped
    QCOMPARE(m.bucket(5), qcp::ColorScalarMapper::kGap);   // NaN
    QCOMPARE(m.bucket(-1), qcp::ColorScalarMapper::kGap);  // gap marker
    QCOMPARE(m.bucket(99), qcp::ColorScalarMapper::kGap);  // out of range index

    m.setScaleType(QCPAxis::stLogarithmic);
    m.setRange(QCPRange(1, 100));
    QCOMPARE(m.bucket(6), 128);                        // 10 is half way on log [1, 100]
    QCOMPARE(m.bucket(4), qcp::ColorScalarMapper::kGap);   // non-positive on log
    QCOMPARE(m.bucket(0), qcp::ColorScalarMapper::kGap);

    m.setScaleType(QCPAxis::stLinear);
    m.setRange(QCPRange(3, 3));                        // degenerate: every finite value -> 0
    QCOMPARE(m.bucket(1), 0);

    QCPColorGradient redToBlue;
    redToBlue.clearColorStops();
    redToBlue.setColorStopAt(0, Qt::red);
    redToBlue.setColorStopAt(1, Qt::blue);
    m.setGradient(redToBlue);
    QCOMPARE(QColor::fromRgba(m.color(0)), QColor(Qt::red));
    QCOMPARE(QColor::fromRgba(m.color(255)), QColor(Qt::blue));
}

void TestColorByScalar::mapperGenerationBumpsOnEverySetter()
{
    qcp::ColorScalarMapper m;
    auto g = m.generation();
    m.setValues(std::make_shared<const std::vector<double>>(std::vector<double>{1}));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setGradient(QCPColorGradient(QCPColorGradient::gpHot));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setRange(QCPRange(0, 2));
    QVERIFY(m.generation() > g); g = m.generation();
    m.setScaleType(QCPAxis::stLogarithmic);
    QVERIFY(m.generation() > g); g = m.generation();
    m.clearValues();
    QVERIFY(m.generation() > g);
}

void TestColorByScalar::colorValuesOfTheWrongLengthAreRefused()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setColorValues(std::vector<double>{1, 2, 3});      // no data yet
    QVERIFY(!mg->hasColorValues());
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mg->setColorValues(std::vector<double>{1, 2, 3});      // 3 != 4
    QVERIFY(!mg->hasColorValues());
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    QVERIFY(mg->hasColorValues());
}

void TestColorByScalar::sameSizeDataRefreshKeepsColorValues()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{0, 1, 0, 1}}));
    mg->setColorValues(std::vector<double>{1, 2, 3, 4});
    mg->setDataSource(makeSource({0, 1, 2, 3}, {{5, 6, 7, 8}}));
    QVERIFY(mg->hasColorValues());
    mg->setDataSource(makeSource({0, 1, 2}, {{5, 6, 7}}));
    QVERIFY(!mg->hasColorValues());
}

void TestColorByScalar::firstColoringRebuildsL1OnceWithOrigin()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    const int n = 200'000;   // above kResampleThreshold: the async L1 pipeline runs
    std::vector<double> keys(n), values(n), scalar(n);
    for (int i = 0; i < n; ++i) { keys[i] = i; values[i] = std::sin(i * 0.001); scalar[i] = i; }
    mg->setDataSource(makeSource(keys, {values}));
    mPlot->xAxis->setRange(0, n);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache != nullptr, 5000);
    QVERIFY(mg->mL1Cache->level1.origin.empty());     // uncoloured: no origin built

    const auto uncolouredL1 = mg->mL1Cache;
    mg->setColorValues(scalar);
    mPlot->replot();
    QTRY_VERIFY_WITH_TIMEOUT(mg->mL1Cache != uncolouredL1, 5000);
    QVERIFY(!mg->mL1Cache->level1.origin.empty());

    const auto colouredL1 = mg->mL1Cache;
    mg->setColorGradient(QCPColorGradient(QCPColorGradient::gpHot));
    mg->setColorRange(QCPRange(0, 10));
    mg->setColorScaleType(QCPAxis::stLogarithmic);
    std::vector<double> other(n, 1.0);
    mg->setColorValues(other);                        // same length: no rebuild either
    QTest::qWait(200);
    QCOMPARE(mg->mL1Cache, colouredL1);
}
```

- [ ] **Step 2: Build; expect compile errors.**

- [ ] **Step 3: Create `src/plottables/plottable-color-mapper.h`**
```cpp
#pragma once
#include "../axis/axis.h"
#include "../colorgradient.h"
#include <QColor>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace qcp {

//! Maps a per-point scalar to one of 256 gradient colours. The segment/point
//! drawing code only asks bucket(dataIndex) and color(bucket).
class ColorScalarMapper
{
public:
    static constexpr int kGap = -1;

    ColorScalarMapper() : mGradient(QCPColorGradient::gpJet) { rebuildLut(); }

    [[nodiscard]] bool hasValues() const { return mValues && !mValues->empty(); }
    [[nodiscard]] int size() const { return mValues ? static_cast<int>(mValues->size()) : 0; }

    void setValues(std::shared_ptr<const std::vector<double>> values)
    {
        mValues = std::move(values);
        ++mGeneration;
    }
    void clearValues() { mValues.reset(); ++mGeneration; }

    void setGradient(const QCPColorGradient& gradient) { mGradient = gradient; rebuildLut(); ++mGeneration; }
    [[nodiscard]] const QCPColorGradient& gradient() const { return mGradient; }
    void setRange(const QCPRange& range) { mRange = range; ++mGeneration; }
    [[nodiscard]] QCPRange range() const { return mRange; }
    void setScaleType(QCPAxis::ScaleType type) { mScaleType = type; ++mGeneration; }
    [[nodiscard]] QCPAxis::ScaleType scaleType() const { return mScaleType; }
    [[nodiscard]] quint64 generation() const { return mGeneration; }

    [[nodiscard]] int bucket(int dataIndex) const
    {
        if (dataIndex < 0 || dataIndex >= size())
            return kGap;
        const double v = (*mValues)[dataIndex];
        if (!std::isfinite(v))
            return kGap;
        const bool log = mScaleType == QCPAxis::stLogarithmic;
        if (log && v <= 0)
            return kGap;
        return toBucket(log ? logPosition(v) : linearPosition(v));
    }

    [[nodiscard]] QRgb color(int bucket) const
    {
        Q_ASSERT(bucket >= 0 && bucket < 256);   // callers skip kGap first
        return mLut[bucket];
    }

private:
    double linearPosition(double v) const
    {
        const double span = mRange.upper - mRange.lower;
        return span > 0 ? (v - mRange.lower) / span : 0.0;
    }
    double logPosition(double v) const
    {
        if (mRange.lower <= 0 || mRange.upper <= mRange.lower)
            return 0.0;
        return std::log(v / mRange.lower) / std::log(mRange.upper / mRange.lower);
    }
    static int toBucket(double t)
    {
        return std::clamp(static_cast<int>(std::lround(t * 255.0)), 0, 255);
    }
    void rebuildLut()
    {
        std::array<double, 256> positions;
        for (int i = 0; i < 256; ++i)
            positions[i] = i / 255.0;
        QCPColorGradient g = mGradient;   // colorize() is non-const (lazy buffer)
        g.colorize(positions.data(), QCPRange(0, 1), mLut.data(), 256);
    }

    std::shared_ptr<const std::vector<double>> mValues;
    QCPColorGradient mGradient;
    QCPRange mRange {0, 1};
    QCPAxis::ScaleType mScaleType = QCPAxis::stLinear;
    std::array<QRgb, 256> mLut {};
    quint64 mGeneration = 0;
};

} // namespace qcp
```
(Bucket for 0.5 on [0, 1]: `lround(127.5) = 128`, which the test expects.)

- [ ] **Step 4: Add the API to `QCPMultiGraph`**

In `plottable-multigraph.h`: `#include "plottable-color-mapper.h"` and `#include <atomic>`; the public declarations of the Interfaces block after `setScatterSkip`; in the protected members:
```cpp
    qcp::ColorScalarMapper mColor;
    // Read by the L1 transform on the pipeline thread: build origin tables only for coloured graphs.
    std::shared_ptr<std::atomic<bool>> mWantOrigin = std::make_shared<std::atomic<bool>>(false);
    bool mL2HasOrigin = false;
    QVector<QVector<int>> mCachedIndices;   // per component, aligned with mCachedLines when coloured
    void invalidateLines();
    void requestOrigin();
```

In `plottable-multigraph.cpp`:

`ensureL1TransformMulti` takes the flag and passes it:
```cpp
static void ensureL1TransformMulti(QCPMultiGraphPipeline& pipeline, int sourceSize, int colCount,
                                   std::shared_ptr<std::atomic<bool>> wantOrigin)
{
    …
            pipeline.setTransform(TransformKind::ViewportIndependent,
                [wantOrigin](const QCPAbstractMultiDataSource& src,
                             const ViewportParams& vp,
                             std::any& cache) -> std::shared_ptr<QCPAbstractMultiDataSource> {
                    return qcp::algo::buildL1CacheMulti(src, vp, cache, wantOrigin->load());
                });
    …
}
```
and both call sites pass `mWantOrigin`.

`setDataSource`, after `mDataSource = std::move(source);`:
```cpp
    if (mColor.hasValues() && (!mDataSource || mColor.size() != mDataSource->size()))
    {
        mColor.clearValues();
        mWantOrigin->store(false);
    }
    mCachedIndices.clear();
    mL2HasOrigin = false;
```

`rebuildL2`:
```cpp
void QCPMultiGraph::rebuildL2(const ViewportParams& vp)
{
    if (!mL1Cache) return;
    mL2Result = qcp::algo::resampleL2Multi(*mL1Cache, vp);
    mL2HasOrigin = mL2Result && !mL1Cache->level1.origin.empty();
}
```

New functions:
```cpp
void QCPMultiGraph::invalidateLines()
{
    mLineCacheDirty = true;
    mCachedLines.clear();
    mCachedIndices.clear();
}

// One async L1 rebuild, the first time the graph is coloured; later colour changes never touch L1/L2.
void QCPMultiGraph::requestOrigin()
{
    if (!mWantOrigin->exchange(true) && mPipeline.hasTransform())
        mPipeline.onDataChanged();
}

void QCPMultiGraph::setColorValues(std::shared_ptr<const std::vector<double>> values)
{
    if (!values || values->empty())
        return clearColorValues();
    const int expected = mDataSource ? mDataSource->size() : 0;
    if (static_cast<int>(values->size()) != expected)
    {
        qWarning() << "QCPMultiGraph::setColorValues: expected" << expected
                   << "values (one per key), got" << values->size();
        return;
    }
    const bool wasColoured = mColor.hasValues();
    mColor.setValues(std::move(values));
    if (!wasColoured)
    {
        invalidateLines();
        requestOrigin();
    }
}

void QCPMultiGraph::setColorValues(std::vector<double> values)
{
    setColorValues(std::make_shared<const std::vector<double>>(std::move(values)));
}

void QCPMultiGraph::clearColorValues()
{
    if (!mColor.hasValues())
        return;
    mColor.clearValues();
    invalidateLines();
}

bool QCPMultiGraph::hasColorValues() const { return mColor.hasValues(); }
void QCPMultiGraph::setColorGradient(const QCPColorGradient& gradient) { mColor.setGradient(gradient); }
QCPColorGradient QCPMultiGraph::colorGradient() const { return mColor.gradient(); }
void QCPMultiGraph::setColorRange(const QCPRange& range) { mColor.setRange(range); }
QCPRange QCPMultiGraph::colorRange() const { return mColor.range(); }
void QCPMultiGraph::setColorScaleType(QCPAxis::ScaleType type) { mColor.setScaleType(type); }
QCPAxis::ScaleType QCPMultiGraph::colorScaleType() const { return mColor.scaleType(); }
```
Also make the existing cache-clearing places clear `mCachedIndices` with `mCachedLines`: the two `scaleTypeChanged` lambdas in the constructor, `setAdaptiveSampling`, `setLineStyle` (Task 1) — replace their `mLineCacheDirty = true; mCachedLines.clear();` by `invalidateLines();` (for the inline header setters, move `setAdaptiveSampling` and `setLineStyle` bodies to the `.cpp`).

- [ ] **Step 5: Build and run the whole suite.** Expected: five new tests pass, exit 0.

- [ ] **Step 6: Commit**

```bash
git add src/plottables tests/auto/test-color-by-scalar
printf 'feat(multigraph): colour values, gradient, range and scale type\n\nColour values are one scalar per key, shared by all components. The first\ncolouring asks the L1 pipeline for origin tables once; later colour changes\nonly bump a generation counter. A same-length data refresh keeps the values,\nany other length drops them. ColorScalarMapper turns a data index into one\nof 256 gradient colours.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 7: Colour runs and coloured lines (GPU path and its cache)

**Files:**
- Create: `src/plottables/plottable-color-runs.h`
- Modify: `src/plottables/plottable-draw-utils.{h,cpp}`, `src/plottables/plottable-multigraph.cpp` (`draw`)
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces (namespace `qcp`):
  ```cpp
  struct ColorRun { int first; int last; int bucket; };   // points [first, last], last > first
  std::vector<ColorRun> colorRuns(const QVector<QPointF>& points, const QVector<int>& indices,
                                  const ColorScalarMapper& mapper);
  struct ExtrusionCache { std::vector<float> vertices; float penWidth; QRgb penColor;
                          quint64 colorGeneration = 0; … };           // new field
  void extrudeColorRuns(const QVector<QPointF>& points, const std::vector<ColorRun>& runs,
                        float penWidth, const ColorScalarMapper& mapper, std::vector<float>& out);
  bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines,
                               float penWidth, quint64 colorGeneration);
  // Builds the runs itself, only when it re-extrudes (or has no GPU layer): any reason to
  // re-extrude (fresh lines, empty cache, pen width, colour generation) gets correct runs.
  void drawColoredPolylineCached(QCPPainter*, QCustomPlot*, QCPLayer*,
                                 const QVector<QPointF>& points, const QVector<int>& indices,
                                 const ColorScalarMapper& mapper, const QPen& pen,
                                 const QPointF& gpuOffset, const QRect& clipRect,
                                 bool freshLines, ExtrusionCache& cache);
  void drawColoredPolylineRuns(QCPPainter*, const QVector<QPointF>& points,
                               const std::vector<ColorRun>& runs, const ColorScalarMapper& mapper,
                               const QPen& pen, const QPointF& gpuOffset);   // QPainter; Task 9 tests it
  ```
  Colours reach QPainter/extruder as `QColor::fromRgba(qUnpremultiply(mapper.color(b)))` (the extruder premultiplies itself).

- [ ] **Step 1: Write the failing tests**

Header slots:
```cpp
    void colorRunsMergeEqualBucketsAndSkipGaps();
    void extrudedRunsCarryTheirColour();
    void coloredReextrusionOnlyOnColorChangeNotPan();
    void coloredLineRendersTheGradient();
```

`.cpp` (add `#include "plottables/plottable-color-runs.h"`, `#include "plottables/plottable-draw-utils.h"`, `#include "painting/line-extruder.h"`):
```cpp
namespace {

QCPColorGradient redToBlue()
{
    QCPColorGradient g;
    g.clearColorStops();
    g.setColorStopAt(0, Qt::red);
    g.setColorStopAt(1, Qt::blue);
    return g;
}

qcp::ColorScalarMapper mapperFor(std::vector<double> values, QCPRange range)
{
    qcp::ColorScalarMapper m;
    m.setGradient(redToBlue());
    m.setRange(range);
    m.setValues(std::make_shared<const std::vector<double>>(std::move(values)));
    return m;
}

} // namespace

void TestColorByScalar::colorRunsMergeEqualBucketsAndSkipGaps()
{
    // values per data index: 0, 0, 1, 1, NaN, 1
    const auto m = mapperFor({0, 0, 1, 1, std::nan(""), 1}, QCPRange(0, 1));
    const QVector<QPointF> pts {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {qQNaN(), qQNaN()}, {5, 0}, {6, 0}};
    const QVector<int> idx     {0,      1,      2,      3,      -1,                  4,      5};
    const auto runs = qcp::colorRuns(pts, idx, m);
    // Segment k -> k+1 uses idx[k+1]: 0->1 value 0 (bucket 0); 1->2 and 2->3 value 1
    // (bucket 255, merged); 3->4 and 4->5 touch the NaN point (skipped); 5->6 value 1.
    QCOMPARE(static_cast<int>(runs.size()), 3);
    QCOMPARE(runs[0].first, 0); QCOMPARE(runs[0].last, 1); QCOMPARE(runs[0].bucket, 0);
    QCOMPARE(runs[1].first, 1); QCOMPARE(runs[1].last, 3); QCOMPARE(runs[1].bucket, 255);
    QCOMPARE(runs[2].first, 5); QCOMPARE(runs[2].last, 6); QCOMPARE(runs[2].bucket, 255);
}

void TestColorByScalar::extrudedRunsCarryTheirColour()
{
    const auto m = mapperFor({0, 1, 0, 1}, QCPRange(0, 1));
    const QVector<QPointF> pts {{0, 0}, {10, 0}, {20, 5}, {30, 0}};
    const QVector<int> idx {0, 1, 2, 3};
    const auto runs = qcp::colorRuns(pts, idx, m);   // three one-segment runs: blue, red, blue
    QCOMPARE(static_cast<int>(runs.size()), 3);

    std::vector<float> out;
    qcp::extrudeColorRuns(pts, runs, 2.0f, m, out);
    int expected = 0;
    for (const auto& r : runs)
        expected += QCPLineExtruder::extrudePolyline(pts.mid(r.first, r.last - r.first + 1), 2.0f,
                                                     Qt::black).size();
    QCOMPARE(static_cast<int>(out.size()), expected);
    // first run is blue (bucket 255): every vertex of it has r == 0, b == 1
    QCOMPARE(out[2], 0.0f);
    QCOMPARE(out[4], 1.0f);
}

void TestColorByScalar::coloredReextrusionOnlyOnColorChangeNotPan()
{
    qcp::ExtrusionCache cache;
    cache.vertices = {1, 2, 3, 4, 5, 6};
    cache.penWidth = 2.0f;
    cache.colorGeneration = 7;
    QVERIFY(!qcp::needsColoredReextrusion(cache, false, 2.0f, 7));   // pan frame
    QVERIFY(qcp::needsColoredReextrusion(cache, true, 2.0f, 7));     // fresh lines
    QVERIFY(qcp::needsColoredReextrusion(cache, false, 3.0f, 7));    // pen width
    QVERIFY(qcp::needsColoredReextrusion(cache, false, 2.0f, 8));    // colour changed
    cache.clear();
    QVERIFY(qcp::needsColoredReextrusion(cache, false, 2.0f, 7));    // empty
}

void TestColorByScalar::coloredLineRendersTheGradient()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> keys(200), values(200, 0.0), scalar(200);
    for (int i = 0; i < 200; ++i) { keys[i] = i; scalar[i] = i / 199.0; }
    mg->setDataSource(makeSource(keys, {values}));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mg->setColorGradient(redToBlue());
    mg->setColorRange(QCPRange(0, 1));
    mg->setColorValues(scalar);
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-1, 1);
    const QImage img = mPlot->toPixmap(400, 300).toImage();

    const int y = qRound(mPlot->yAxis->coordToPixel(0));
    const QColor left = img.pixelColor(qRound(mPlot->xAxis->coordToPixel(10)), y);
    const QColor right = img.pixelColor(qRound(mPlot->xAxis->coordToPixel(189)), y);
    QVERIFY2(left.red() > 150 && left.blue() < 100, qPrintable(left.name()));
    QVERIFY2(right.blue() > 150 && right.red() < 100, qPrintable(right.name()));
}
```

- [ ] **Step 2: Build; expect compile errors.**

- [ ] **Step 3: Create `src/plottables/plottable-color-runs.h`**
```cpp
#pragma once
#include "plottable-color-mapper.h"
#include <QPointF>
#include <QVector>
#include <cmath>
#include <vector>

namespace qcp {

//! Points [first, last] drawn in one colour: segments first->first+1 ... last-1->last.
struct ColorRun
{
    int first;
    int last;
    int bucket;
};

//! Segment k -> k+1 takes the colour of point k+1; it is not drawn when either point
//! is a gap (non-finite) or point k+1 has no colour. Equal consecutive colours merge.
inline std::vector<ColorRun> colorRuns(const QVector<QPointF>& points, const QVector<int>& indices,
                                       const ColorScalarMapper& mapper)
{
    std::vector<ColorRun> runs;
    auto finite = [](const QPointF& p) { return std::isfinite(p.x()) && std::isfinite(p.y()); };
    for (int k = 0; k + 1 < points.size(); ++k)
    {
        const int b = finite(points[k]) && finite(points[k + 1]) ? mapper.bucket(indices[k + 1])
                                                                  : ColorScalarMapper::kGap;
        if (b == ColorScalarMapper::kGap)
            continue;
        if (!runs.empty() && runs.back().bucket == b && runs.back().last == k)
            runs.back().last = k + 1;
        else
            runs.push_back({k, k + 1, b});
    }
    return runs;
}

inline QColor runColor(const ColorScalarMapper& mapper, int bucket)
{
    return QColor::fromRgba(qUnpremultiply(mapper.color(bucket)));
}

} // namespace qcp
```

- [ ] **Step 4: Extrusion helpers** in `plottable-draw-utils.h`: add `quint64 colorGeneration = 0;` to `ExtrusionCache`; `#include "plottable-color-runs.h"`; declare `extrudeColorRuns`, `needsColoredReextrusion`, `drawColoredPolylineCached`, `drawColoredPolylineRuns` (signatures from the Interfaces block). In `plottable-draw-utils.cpp`:
```cpp
void extrudeColorRuns(const QVector<QPointF>& points, const std::vector<ColorRun>& runs,
                      float penWidth, const ColorScalarMapper& mapper, std::vector<float>& out)
{
    out.clear();
    for (const auto& run : runs)
    {
        const auto verts = QCPLineExtruder::extrudePolyline(
            points.mid(run.first, run.last - run.first + 1), penWidth, runColor(mapper, run.bucket));
        out.insert(out.end(), verts.cbegin(), verts.cend());
    }
}

bool needsColoredReextrusion(const ExtrusionCache& cache, bool freshLines,
                             float penWidth, quint64 colorGeneration)
{
    return freshLines || cache.isEmpty() || cache.penWidth != penWidth
        || cache.colorGeneration != colorGeneration;
}

void drawColoredPolylineRuns(QCPPainter* painter, const QVector<QPointF>& points,
                             const std::vector<ColorRun>& runs, const ColorScalarMapper& mapper,
                             const QPen& pen, const QPointF& gpuOffset)
{
    painter->setBrush(Qt::NoBrush);
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    QPen runPen = pen;
    for (const auto& run : runs)
    {
        runPen.setColor(runColor(mapper, run.bucket));
        painter->setPen(runPen);
        painter->drawPolyline(points.constData() + run.first, run.last - run.first + 1);
    }
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}

void drawColoredPolylineCached(QCPPainter* painter, QCustomPlot* parentPlot, QCPLayer* layer,
                               const QVector<QPointF>& points, const QVector<int>& indices,
                               const ColorScalarMapper& mapper, const QPen& pen,
                               const QPointF& gpuOffset, const QRect& clipRect,
                               bool freshLines, ExtrusionCache& cache)
{
    auto* prl = (parentPlot && parentPlot->rhi()
                 && !painter->modes().testFlag(QCPPainter::pmVectorized)
                 && !painter->modes().testFlag(QCPPainter::pmNoCaching)
                 && pen.style() == Qt::SolidLine)
        ? parentPlot->plottableRhiLayer(layer) : nullptr;
    if (!prl)
        return drawColoredPolylineRuns(painter, points, colorRuns(points, indices, mapper),
                                       mapper, pen, gpuOffset);

    const double dpr = parentPlot->bufferDevicePixelRatio();
    const float penWidth = (pen.isCosmetic() || qFuzzyIsNull(pen.widthF()))
        ? static_cast<float>(1.0 / dpr)
        : qMax(1.0f, static_cast<float>(pen.widthF()));
    if (needsColoredReextrusion(cache, freshLines, penWidth, mapper.generation()))
    {
        extrudeColorRuns(points, colorRuns(points, indices, mapper), penWidth, mapper, cache.vertices);
        cache.penWidth = penWidth;
        cache.colorGeneration = mapper.generation();
    }
    if (cache.isEmpty())
        return;
    prl->addPlottable({}, cache.vertices, clipRect, dpr, parentPlot->rhiOutputSize().height(),
                      static_cast<float>(gpuOffset.x()), static_cast<float>(gpuOffset.y()));
}
```
(The pen-colour comparison of `drawPolylineCached` is deliberately absent: the pen colour is not what a coloured graph draws.)

- [ ] **Step 5: Coloured fetch and drawing in `QCPMultiGraph::draw`**

1. After the L2 re-resolve block (after line ~613), decide once per frame:
   ```cpp
    const bool coloured = mColor.hasValues() && (ds == mDataSource.get() || mL2HasOrigin);
   ```
2. Next to `QVector<QVector<QPointF>> exportLines;` add `QVector<QVector<int>> exportIndices;` and `auto& indicesTarget = isExportMode ? exportIndices : mCachedIndices;`.
3. In the `if (needFreshLines)` block, before the batched/per-component branch:
   ```cpp
        indicesTarget.resize(coloured ? nc : 0);
        if (coloured)
        {
            for (int c = 0; c < nc; ++c)
            {
                if (!mComponents[c].visible) { linesTarget[c].clear(); indicesTarget[c].clear(); continue; }
                linesTarget[c] = (mAdaptiveSampling && !mL2Result)
                    ? ds->getOptimizedLineDataIndexed(c, cacheBegin, cacheEnd, pixelWidth,
                                                      mKeyAxis.data(), mValueAxis.data(), indicesTarget[c])
                    : ds->getLinesIndexed(c, cacheBegin, cacheEnd,
                                          mKeyAxis.data(), mValueAxis.data(), indicesTarget[c]);
            }
        }
        else if (allVisible && nc > 1 && !(mAdaptiveSampling && !mL2Result))
   ```
   (the existing `if (allVisible …)` becomes `else if`, the existing `else` stays).
4. In the per-component loop, after `dataLines` is taken:
   ```cpp
        const QVector<int>* dataIdx = (coloured && c < indicesTarget.size()
                                       && indicesTarget[c].size() == dataLines.size())
            ? &indicesTarget[c] : nullptr;
   ```
   (a size mismatch — lines cached before colouring on a pan frame — draws uncoloured until the next fresh fetch).
5. In the line branch, for the non-impulse case:
   ```cpp
            } else if (dataIdx) {
                applyDefaultAntialiasingHint(painter);
                if (!isExportMode)
                    qcp::drawColoredPolylineCached(painter, mParentPlot, mLayer, lines,
                                                   lineIndices(*dataIdx), mColor, activePen,
                                                   gpuOffset, clipRect(), needFreshLines,
                                                   mExtrusionCaches[c]);
                else
                    qcp::drawColoredPolylineRuns(painter, lines,
                                                 qcp::colorRuns(lines, lineIndices(*dataIdx), mColor),
                                                 mColor, activePen, gpuOffset);
            } else {
   ```
   with a protected helper (declared in `plottable-multigraph.h` next to `invalidateLines`) `QVector<int> lineIndices(const QVector<int>& dataIdx) const` that returns `dataIdx` for `lsLine` and `qcp::stepLeftIndices`/`stepRightIndices`/`stepCenterIndices`/`impulseIndices` for the step styles and impulse (mirror of the `switch` that builds `styledLines`).
   A coloured component always gets its styled points, because `drawColoredPolylineCached` may re-extrude for any reason (fresh lines, empty cache, pen width, colour generation) and must then see the styled geometry. Change the definition to
   `const bool needStyledLines = needFreshLines || mExtrusionCaches[c].isEmpty() || dataIdx != nullptr;`
   (a step transform over the decimated points — a few thousand — per coloured frame; uncoloured graphs keep today's condition).

- [ ] **Step 6: Build and run the whole suite.** Expected: four new tests pass; all existing classes unchanged; exit 0. `coloredLineRendersTheGradient` goes through the export path (`toPixmap`), which is the QPainter runs path.

- [ ] **Step 7: Commit**

```bash
git add src/plottables tests/auto/test-color-by-scalar
printf 'feat(multigraph): draw lines coloured by a scalar\n\nA coloured graph fetches its lines with source indices, splits them into\nruns of equal colour (segment k -> k+1 takes the colour of point k+1) and\nextrudes each run with the existing extruder into one vertex buffer. The\ncoloured extrusion cache is keyed on pen width and a colour generation, so a\npan reuses it and a colour change rebuilds it. Uncoloured graphs take the\nunchanged path.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 8: Perf check 2 — coloured scenarios and the single-colour gate again

**Files:**
- Modify: `tests/perf/multigraph-perf.cpp`

**Interfaces:** Consumes `QCPMultiGraph::setColorValues` (Task 6) and the perf fixture `setupPlotWithRhi` (returns `{plot, mg}`), `kDefaultPoints` (10M), `kDefaultCols` (8).

- [ ] **Step 1: Add two scenarios** to `tests/perf/multigraph-perf.cpp`, before `// ── Main`:
```cpp
static void colourAndSettle(QCustomPlot* plot, QCPMultiGraph* mg)
{
    std::vector<double> scalar(mg->dataCount());
    for (size_t i = 0; i < scalar.size(); ++i)
        scalar[i] = std::sin(i * 1e-5);
    mg->setColorRange(QCPRange(-1, 1));
    mg->setColorValues(std::move(scalar));
    for (int w = 0; w < 100 && mg->pipeline().isBusy(); ++w) {   // the one origin rebuild
        QThread::msleep(50);
        QApplication::processEvents();
    }
    plot->replot(QCustomPlot::rpImmediateRefresh);
    QApplication::processEvents();
}

static void scenarioColouredFullReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);
    colourAndSettle(plot, mg);
    fprintf(stderr, "coloured_full_replot: %d pts × %d cols, %d iters (rhi=%s)\n",
            kDefaultPoints, kDefaultCols, iters, plot->rhi() ? "yes" : "no");
    waitForProfiler();
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        mg->setAdaptiveSampling(mg->adaptiveSampling());
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
    delete plot;
}

static void scenarioColouredPanReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);
    colourAndSettle(plot, mg);
    double panStep = plot->xAxis->range().size() * 0.005;
    fprintf(stderr, "coloured_pan_replot: %d pts × %d cols, %d iters (rhi=%s)\n",
            kDefaultPoints, kDefaultCols, iters, plot->rhi() ? "yes" : "no");
    waitForProfiler();
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        QCPRange r = plot->xAxis->range();
        plot->xAxis->setRange(r.lower + panStep, r.upper + panStep);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
    delete plot;
}
```
and register `{"coloured_full_replot", scenarioColouredFullReplot, 20},` and `{"coloured_pan_replot", scenarioColouredPanReplot, 200},` in `scenarios[]`.

`setAdaptiveSampling(sameValue)` is a no-op before and after this work (it only invalidates on a
change), so `full_replot` mostly measures cached redraws, and `coloured_full_replot` mirrors it.
To measure the coloured fetch and run building, add a zoom pair that changes the range size on
every iteration (a size change always forces fresh lines):
```cpp
static void zoomLoop(QCustomPlot* plot, int iters)
{
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iters; ++i) {
        const QCPRange r = plot->xAxis->range();
        const double f = (i % 2) ? 1.0 / 0.98 : 0.98;
        plot->xAxis->setRange(r.center() - r.size() * f / 2, r.center() + r.size() * f / 2);
        plot->replot(QCustomPlot::rpImmediateRefresh);
    }
    double ms = timer.nsecsElapsed() / 1e6;
    fprintf(stderr, "  total: %.1f ms, per-iter: %.1f ms\n", ms, ms / iters);
}

static void scenarioZoomReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);
    fprintf(stderr, "zoom_replot: %d iters (rhi=%s)\n", iters, plot->rhi() ? "yes" : "no");
    waitForProfiler();
    zoomLoop(plot, iters);
    delete plot;
}

static void scenarioColouredZoomReplot(int iters)
{
    auto data = generateMultiData(kDefaultPoints, kDefaultCols);
    auto [plot, mg] = setupPlotWithRhi(data);
    colourAndSettle(plot, mg);
    fprintf(stderr, "coloured_zoom_replot: %d iters (rhi=%s)\n", iters, plot->rhi() ? "yes" : "no");
    waitForProfiler();
    zoomLoop(plot, iters);
    delete plot;
}
```
registered as `{"zoom_replot", scenarioZoomReplot, 50},` and `{"coloured_zoom_replot", scenarioColouredZoomReplot, 50},`.

- [ ] **Step 2: Build the feature perf binary** (`meson compile -C build-colour tests/perf/multigraph_perf`).

- [ ] **Step 3: Single-colour gate again** — Task 3 Steps 3-4, saved to `/tmp/perf-check-2-plain.txt`. Same 3% gate.

- [ ] **Step 4: Coloured vs uncoloured** on the feature binary, 3 rounds:
```bash
for round in 1 2 3; do
  for s in full_replot coloured_full_replot pan_replot coloured_pan_replot zoom_replot coloured_zoom_replot; do
    printf '%s ' "$s"
    /home/jeandet/Documents/prog/NeoQCP/build-colour/tests/perf/multigraph_perf --no-barrier $s 2>&1 \
      | grep -o 'rhi=[a-z]*\|per-iter: [0-9.]* ms' | tr '\n' ' '; echo
  done
done | tee /tmp/perf-check-2-coloured.txt
```
Gate: median coloured ≤ 1.5 × median uncoloured for all three pairs (the zoom pair is the one that exercises the coloured fetch, runs and extrusion on every frame). If it fails, profile before changing anything (`tests/perf/run-perf.py coloured_full_replot --stat`); the likely costs are the per-run `QVector` allocations in `extrudeColorRuns` (reuse one scratch vector) and the per-component indexed fetch replacing the batched `getLinesAll` (add a batched indexed variant only if the profile points there).

- [ ] **Step 5: Commit**
```bash
git add tests/perf/multigraph-perf.cpp
printf 'test(perf): coloured full and pan replot scenarios\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 9: Coloured lines on the CPU paths (export, dashed pens, impulses, step styles)

**Files:**
- Modify: `src/plottables/plottable-multigraph.cpp` (`draw`, impulse branch)
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:** Consumes `colorRuns`, `drawColoredPolylineRuns`, `lineIndices` (Task 7), `impulseIndices` (Task 5).

- [ ] **Step 1: Write the failing tests**

Header slots:
```cpp
    void coloredDashedLineRendersTheGradient();
    void coloredStepLineRendersTheGradient();
    void coloredImpulsesRenderTheGradient();
    void nanScalarLeavesAGap();
```
`.cpp` — one fixture helper and four tests (the helper builds 200 points on y = 0 coloured left red, right blue; each test sets a style, renders through `toPixmap` and reads the pixels at x = 10 and x = 189):
```cpp
namespace {

QCPMultiGraph* rampGraph(QCustomPlot* plot, std::shared_ptr<QCPAbstractMultiDataSource> src,
                         std::vector<double> scalar)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setDataSource(std::move(src));
    mg->setComponentPens({QPen(Qt::black, 6)});
    mg->setColorGradient(redToBlue());
    mg->setColorRange(QCPRange(0, 1));
    mg->setColorValues(std::move(scalar));
    plot->xAxis->setRange(0, 199);
    plot->yAxis->setRange(-1, 1);
    return mg;
}

std::vector<double> ramp(int n)
{
    std::vector<double> s(n);
    for (int i = 0; i < n; ++i) s[i] = i / double(n - 1);
    return s;
}

QColor pixelAt(QCustomPlot* plot, const QImage& img, double key, double value)
{
    return img.pixelColor(qRound(plot->xAxis->coordToPixel(key)),
                          qRound(plot->yAxis->coordToPixel(value)));
}

bool isRed(const QColor& c) { return c.red() > 150 && c.blue() < 100; }
bool isBlue(const QColor& c) { return c.blue() > 150 && c.red() < 100; }

} // namespace

void TestColorByScalar::coloredDashedLineRendersTheGradient()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(200));
    mg->setComponentPens({QPen(QBrush(Qt::black), 6, Qt::DashLine)});
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    // A dash may fall on the probe: look at a few pixels around it.
    auto anyRed = [&](double key) { for (int d = 0; d < 6; ++d) if (isRed(pixelAt(mPlot, img, key + d, 0))) return true; return false; };
    auto anyBlue = [&](double key) { for (int d = 0; d < 6; ++d) if (isBlue(pixelAt(mPlot, img, key - d, 0))) return true; return false; };
    QVERIFY(anyRed(10));
    QVERIFY(anyBlue(189));
}

void TestColorByScalar::coloredStepLineRendersTheGradient()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(200));
    mg->setLineStyle(QCPMultiGraph::lsStepLeft);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 10, 0)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 189, 0)));
}

void TestColorByScalar::coloredImpulsesRenderTheGradient()
{
    std::vector<double> keys(20), values(20, 0.8);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setLineStyle(QCPMultiGraph::lsImpulse);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 0, 0.4)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 190, 0.4)));
}

void TestColorByScalar::nanScalarLeavesAGap()
{
    std::vector<double> keys(200), values(200, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto scalar = ramp(200);
    for (int i = 90; i < 110; ++i) scalar[i] = std::nan("");
    rampGraph(mPlot, makeSource(keys, {values}), scalar);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    const QColor background = pixelAt(mPlot, img, 100, 0.8);   // nothing is drawn up there
    QCOMPARE(pixelAt(mPlot, img, 100, 0), background);
    QVERIFY(isRed(pixelAt(mPlot, img, 10, 0)));
}
```
(add `#include <numeric>`).

- [ ] **Step 2: Build and run; expect `coloredImpulsesRenderTheGradient` to fail** (impulses still use the component pen). The dashed, step and NaN tests may already pass through Task 7's runs path; that is fine — they pin the behaviour.

- [ ] **Step 3: Coloured impulses.** In `draw`, the `if (mLineStyle == lsImpulse)` branch becomes:
```cpp
            if (mLineStyle == lsImpulse) {
                applyDefaultAntialiasingHint(painter);
                QPen impulsePen = activePen;
                impulsePen.setCapStyle(Qt::FlatCap);
                if (dataIdx)
                    drawColoredImpulses(painter, lines, qcp::impulseIndices(*dataIdx), impulsePen);
                else {
                    painter->setPen(impulsePen);
                    painter->drawLines(lines);
                }
            }
```
with a private helper that batches the pairs per bucket (≤ 256 `drawLines` calls):
```cpp
void QCPMultiGraph::drawColoredImpulses(QCPPainter* painter, const QVector<QPointF>& pairs,
                                        const QVector<int>& indices, QPen pen) const
{
    std::array<QVector<QLineF>, 256> byBucket;
    for (int i = 0; i + 1 < pairs.size(); i += 2)
    {
        const int b = mColor.bucket(indices[i + 1]);
        if (b != qcp::ColorScalarMapper::kGap)
            byBucket[b].append(QLineF(pairs[i], pairs[i + 1]));
    }
    for (int b = 0; b < 256; ++b)
    {
        if (byBucket[b].isEmpty()) continue;
        pen.setColor(qcp::runColor(mColor, b));
        painter->setPen(pen);
        painter->drawLines(byBucket[b]);
    }
}
```
(declare it in the header, protected; `#include <array>`).

- [ ] **Step 4: Build and run the whole suite.** Expected: all four pass, exit 0.

- [ ] **Step 5: Commit**
```bash
git add src/plottables tests/auto/test-color-by-scalar
printf 'feat(multigraph): coloured impulses; pin the CPU coloured paths\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 10: Scatter layer — per-draw state and a coloured pipeline

**Files:**
- Create: `src/painting/shaders/scatter_colored.vert`, `src/painting/shaders/scatter_colored.frag`
- Modify: `src/painting/scatter-rhi-layer.{h,cpp}`, `meson.build`
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces on `QCPScatterRhiLayer`:
  ```cpp
  struct DrawEntry {
      int instanceOffset = 0;
      int instanceCount = 0;
      float offsetX = 0, offsetY = 0;
      QRect scissorRect;
      float halfSize = 0;           // new: per draw (was layer-wide)
      bool useColorAxis = false;    // new: per draw (was layer-wide)
      int colorOffset = -1;         // new: first colour of this draw in the colour buffer, -1 = plain
  };
  void addScatterColored(std::span<const float> xy, std::span<const float> rgba,
                         const QCPScatterStyle& style, const QRect& clipRect, double dpr,
                         int outputHeight, float offsetX = 0, float offsetY = 0);
  ```
  `xy` holds 2 floats per marker, `rgba` 4 premultiplied floats per marker (same count). `addScatter` keeps its signature and behaviour; `mHalfSize` and `mUseColorAxis` members are removed in favour of the entry fields. The sprite image and the colormap image stay layer-wide (out of scope, see the spec).

- [ ] **Step 1: Write the failing tests**

Header slots:
```cpp
    void scatterEntriesKeepTheirOwnSizeAndMode();
    void coloredScatterStagesColoursPerMarker();
```
`.cpp` (add `#include "painting/scatter-rhi-layer.h"`, `#include "scatterstyle.h"`):
```cpp
void TestColorByScalar::scatterEntriesKeepTheirOwnSizeAndMode()
{
    QCPScatterRhiLayer layer(nullptr);
    const std::vector<float> pts {10, 10, 0, 20, 20, 0};
    QImage cmap(256, 1, QImage::Format_ARGB32_Premultiplied);
    cmap.fill(Qt::red);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 10), QRect(0, 0, 400, 300), 1.0, 300);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 20), QRect(0, 0, 400, 300), 1.0, 300,
                     0, 0, cmap);
    layer.addScatter(pts, QCPScatterStyle(QCPScatterStyle::ssDisc, 6), QRect(0, 0, 400, 300), 1.0, 300);
    QCOMPARE(layer.mDrawEntries.size(), 3);
    QCOMPARE(layer.mDrawEntries[0].halfSize, 5.0f);
    QCOMPARE(layer.mDrawEntries[1].halfSize, 10.0f);
    QCOMPARE(layer.mDrawEntries[2].halfSize, 3.0f);
    QVERIFY(!layer.mDrawEntries[0].useColorAxis);
    QVERIFY(layer.mDrawEntries[1].useColorAxis);   // a later plain draw no longer switches it off
    QVERIFY(!layer.mDrawEntries[2].useColorAxis);
    QCOMPARE(layer.mDrawEntries[0].colorOffset, -1);
    QVERIFY(layer.mColorStaging.empty());           // no coloured draw: no colour data at all
}

void TestColorByScalar::coloredScatterStagesColoursPerMarker()
{
    QCPScatterRhiLayer layer(nullptr);
    const std::vector<float> plain {1, 1, 0};
    const std::vector<float> xy {10, 10, 20, 20};
    const std::vector<float> rgba {1, 0, 0, 1,  0, 0, 1, 1};
    layer.addScatter(plain, QCPScatterStyle(QCPScatterStyle::ssDisc, 8), QRect(0, 0, 400, 300), 1.0, 300);
    layer.addScatterColored(xy, rgba, QCPScatterStyle(QCPScatterStyle::ssDisc, 8),
                            QRect(0, 0, 400, 300), 1.0, 300);
    QCOMPARE(layer.mDrawEntries.size(), 2);
    const auto& e = layer.mDrawEntries[1];
    QCOMPARE(e.instanceOffset, 1);
    QCOMPARE(e.instanceCount, 2);
    QCOMPARE(e.colorOffset, 0);
    QCOMPARE(layer.mStagingSize, 9);                          // 3 floats per instance, both draws
    QCOMPARE(layer.mColorStaging, (std::vector<float>{1, 0, 0, 1, 0, 0, 1, 1}));
    QCOMPARE(layer.mStagingData[3], 10.0f);                   // x of the first coloured marker
    QCOMPARE(layer.mStagingData[5], 0.0f);                    // its padding colorValue
}
```
Add `friend class TestColorByScalar;` in `QCPScatterRhiLayer` (private section).

- [ ] **Step 2: Build; expect compile errors** (no `halfSize`, `addScatterColored`, `mColorStaging`).

- [ ] **Step 3: Per-draw state.** In `scatter-rhi-layer.h`: extend `DrawEntry` as in the Interfaces block; remove `float mHalfSize` and `bool mUseColorAxis`; add
```cpp
    std::vector<float> mColorStaging;          // 4 floats per coloured instance
    QRhiBuffer* mColorBuffer = nullptr;
    int mColorBufferSize = 0;
    QRhiGraphicsPipeline* mColoredPipeline = nullptr;
    void addDraw(std::span<const float> xyz, const QCPScatterStyle& style, const QRect& clipRect,
                 double dpr, int outputHeight, float offsetX, float offsetY,
                 bool useColorAxis, int colorOffset);
```
In `scatter-rhi-layer.cpp`, split `addScatter` into the sprite/colormap handling plus a shared `addDraw`:
```cpp
void QCPScatterRhiLayer::addDraw(std::span<const float> xyz, const QCPScatterStyle& style,
                                 const QRect& clipRect, double dpr, int outputHeight,
                                 float offsetX, float offsetY, bool useColorAxis, int colorOffset)
{
    const int newShape = static_cast<int>(style.shape());
    const double newSize = style.size();
    if (newShape != mCachedShape || !qFuzzyCompare(newSize, mCachedSize)
        || style.pen() != mCachedPen || style.brush() != mCachedBrush)
    {
        mSpriteImage = style.renderToImage(64);
        mSpriteTextureDirty = true;
        mCachedShape = newShape;
        mCachedSize = newSize;
        mCachedPen = style.pen();
        mCachedBrush = style.brush();
    }

    DrawEntry entry;
    entry.scissorRect = qcp::rhi::computeScissor(clipRect, dpr, outputHeight);
    entry.offsetX = offsetX;
    entry.offsetY = offsetY;
    entry.instanceOffset = mStagingSize / 3;
    entry.instanceCount = static_cast<int>(xyz.size()) / 3;
    entry.halfSize = static_cast<float>(newSize * 0.5);
    entry.useColorAxis = useColorAxis;
    entry.colorOffset = colorOffset;

    stagingAppend(xyz.data(), static_cast<int>(xyz.size()));
    mDrawEntries.append(entry);
    mDirty = true;
}

void QCPScatterRhiLayer::addScatter(std::span<const float> points, const QCPScatterStyle& style,
                                    const QRect& clipRect, double dpr, int outputHeight,
                                    float offsetX, float offsetY, const QImage& colormapImage)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatter");
    if (points.empty() || points.size() % 3 != 0)
        return;
    if (!colormapImage.isNull())
    {
        mColormapImage = colormapImage;
        mColormapTextureDirty = true;
    }
    addDraw(points, style, clipRect, dpr, outputHeight, offsetX, offsetY,
            !colormapImage.isNull(), -1);
}

void QCPScatterRhiLayer::addScatterColored(std::span<const float> xy, std::span<const float> rgba,
                                           const QCPScatterStyle& style, const QRect& clipRect,
                                           double dpr, int outputHeight, float offsetX, float offsetY)
{
    PROFILE_HERE_N("QCPScatterRhiLayer::addScatterColored");
    const size_t n = xy.size() / 2;
    if (n == 0 || xy.size() % 2 != 0 || rgba.size() != n * 4)
        return;
    std::vector<float> xyz(n * 3);
    for (size_t i = 0; i < n; ++i)
    {
        xyz[i * 3 + 0] = xy[i * 2 + 0];
        xyz[i * 3 + 1] = xy[i * 2 + 1];
        xyz[i * 3 + 2] = 0.0f;
    }
    const int colorOffset = static_cast<int>(mColorStaging.size() / 4);
    mColorStaging.insert(mColorStaging.end(), rgba.begin(), rgba.end());
    addDraw(xyz, style, clipRect, dpr, outputHeight, offsetX, offsetY, false, colorOffset);
}
```
`clear()` also does `mColorStaging.clear();`. In `uploadResources`, the per-draw uniforms become `entry.halfSize` and `entry.useColorAxis ? 1.0f : 0.0f`.

- [ ] **Step 4: Build and run the tests.** Expected: the two scatter tests pass; the rest unchanged.

- [ ] **Step 5: Shaders.**

`src/painting/shaders/scatter_colored.vert` — a copy of `scatter.vert` with the instance colour forwarded instead of `colorValue`:
```glsl
#version 440

layout(location = 0) in vec2 cornerOffset;
layout(location = 1) in vec3 instanceData;   // x, y, unused
layout(location = 2) in vec4 instanceColor;  // premultiplied

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;

layout(std140, binding = 0) uniform Params {
    float width;
    float height;
    float yFlip;
    float dpr;
    float offsetX;
    float offsetY;
    float halfSize;
    float useColorAxis;
};

void main()
{
    float px = (instanceData.x + offsetX) * dpr;
    float py = (instanceData.y + offsetY) * dpr;
    float cx = cornerOffset.x * halfSize * dpr;
    float cy = cornerOffset.y * halfSize * dpr;
    float ndcX = ((px + cx) / width) * 2.0 - 1.0;
    float ndcY = yFlip * (((py + cy) / height) * 2.0 - 1.0);
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    v_uv = cornerOffset * 0.5 + 0.5;
    v_color = instanceColor;
}
```
`src/painting/shaders/scatter_colored.frag`:
```glsl
#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_color;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D spriteTexture;

void main()
{
    // The sprite is a shape mask here; the colour is premultiplied, so alpha counts once.
    fragColor = v_color * texture(spriteTexture, v_uv).a;
}
```
`meson.build`: two targets next to `scatter_frag_qsb`:
```meson
scatter_colored_vert_qsb = custom_target('scatter_colored_vert_qsb',
    input: 'src/painting/shaders/scatter_colored.vert',
    output: 'scatter_colored.vert.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])

scatter_colored_frag_qsb = custom_target('scatter_colored_frag_qsb',
    input: 'src/painting/shaders/scatter_colored.frag',
    output: 'scatter_colored.frag.qsb',
    command: [qsb, '--qt6', '-o', '@OUTPUT@', '@INPUT@'])
```
append both to the `embedded_shaders` `input:` list (after `scatter_frag_qsb`) and to its command:
`'scatter_colored_vert_qsb_data:@INPUT9@', 'scatter_colored_frag_qsb_data:@INPUT10@'`.

- [ ] **Step 6: Coloured pipeline.** In `ensurePipeline`, after the plain pipeline is created (the SRB is shared; the coloured fragment shader only uses bindings 0 and 1 of it):
```cpp
    auto colVert = qcp::rhi::loadEmbeddedShader(scatter_colored_vert_qsb_data, scatter_colored_vert_qsb_data_len);
    auto colFrag = qcp::rhi::loadEmbeddedShader(scatter_colored_frag_qsb_data, scatter_colored_frag_qsb_data_len);
    if (!colVert.isValid() || !colFrag.isValid())
        return false;
    mColoredPipeline = mRhi->newGraphicsPipeline();
    mColoredPipeline->setShaderStages({{QRhiShaderStage::Vertex, colVert},
                                       {QRhiShaderStage::Fragment, colFrag}});
    QRhiVertexInputLayout colLayout;
    colLayout.setBindings({
        {2 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerVertex},
        {3 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerInstance},
        {4 * static_cast<quint32>(sizeof(float)), QRhiVertexInputBinding::PerInstance}
    });
    colLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {1, 1, QRhiVertexInputAttribute::Float3, 0},
        {2, 2, QRhiVertexInputAttribute::Float4, 0}
    });
    mColoredPipeline->setVertexInputLayout(colLayout);
    mColoredPipeline->setTargetBlends({qcp::rhi::premultipliedAlphaBlend()});
    mColoredPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
    mColoredPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    mColoredPipeline->setSampleCount(sampleCount);
    mColoredPipeline->setRenderPassDescriptor(rpDesc);
    mColoredPipeline->setShaderResourceBindings(mSrb);
    if (!mColoredPipeline->create())
    {
        delete mColoredPipeline;
        mColoredPipeline = nullptr;
        return false;
    }
```
`invalidatePipeline` deletes `mColoredPipeline` (next to `mPipeline`). `mColorBuffer` lives exactly like `mInstanceBuffer`: `invalidatePipeline` leaves it alone and only the destructor deletes it — `invalidatePipeline` does not set `mDirty`, so a buffer deleted there would not be recreated until the next geometry change.

In `uploadResources`, inside the "instance data changed" part (after the instance upload, still under `mDirty`), upload colours only when there are some:
```cpp
    if (!mColorStaging.empty())
    {
        const int colorBytes = static_cast<int>(mColorStaging.size() * sizeof(float));
        if (!mColorBuffer || mColorBufferSize < colorBytes)
        {
            delete mColorBuffer;
            mColorBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, colorBytes);
            if (!mColorBuffer->create())
            {
                delete mColorBuffer;
                mColorBuffer = nullptr;
                mColorBufferSize = 0;
                return;
            }
            mColorBufferSize = colorBytes;
        }
        updates->updateDynamicBuffer(mColorBuffer, 0, colorBytes, mColorStaging.data());
    }
```
(place it before the existing `mDirty = false;`).

In `render`, bind the pipeline per draw (the Metal rule: shader resources before vertex input; set the viewport after each pipeline switch):
```cpp
    QRhiGraphicsPipeline* current = nullptr;
    const int stride = ubufStride();
    for (int i = 0; i < mDrawEntries.size(); ++i)
    {
        const auto& entry = mDrawEntries[i];
        if (entry.instanceCount <= 0)
            continue;
        const bool coloured = entry.colorOffset >= 0;
        if (coloured && (!mColoredPipeline || !mColorBuffer))
            continue;
        QRhiGraphicsPipeline* wanted = coloured ? mColoredPipeline : mPipeline;
        if (wanted != current)
        {
            cb->setGraphicsPipeline(wanted);
            cb->setViewport({0, 0, float(outputSize.width()), float(outputSize.height())});
            current = wanted;
        }

        const QPair<int, quint32> dynamicOffset(0, quint32(i * stride));
        cb->setShaderResources(mSrb, 1, &dynamicOffset);

        const QRhiCommandBuffer::VertexInput plainInputs[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, quint32(entry.instanceOffset * 3 * sizeof(float))}
        };
        const QRhiCommandBuffer::VertexInput colouredInputs[] = {
            {mQuadVertexBuffer, 0},
            {mInstanceBuffer, quint32(entry.instanceOffset * 3 * sizeof(float))},
            {mColorBuffer, quint32(entry.colorOffset * 4 * sizeof(float))}
        };
        if (coloured)
            cb->setVertexInput(0, 3, colouredInputs, mQuadIndexBuffer, 0, QRhiCommandBuffer::IndexUInt16);
        else
            cb->setVertexInput(0, 2, plainInputs, mQuadIndexBuffer, 0, QRhiCommandBuffer::IndexUInt16);

        cb->setScissor({entry.scissorRect.x(), entry.scissorRect.y(),
                        entry.scissorRect.width(), entry.scissorRect.height()});
        cb->drawIndexed(6, entry.instanceCount, 0, 0, 0);
    }
```
replacing the current loop (and the `cb->setGraphicsPipeline(mPipeline); cb->setViewport(...)` lines before it). For a frame without coloured draws this issues the same commands as today.

- [ ] **Step 7: Build and run the whole suite.** Expected: pass, exit 0 (the shaders compile through `qsb` during the build: a GLSL error shows there).

- [ ] **Step 8: Commit**
```bash
git add src/painting meson.build tests/auto/test-color-by-scalar
printf 'feat(scatter-rhi): per-draw marker size and colour mode, per-marker colours\n\nMarker half-size and the colormap flag were layer-wide, so every draw on a\nlayer used the last addScatter call'"'"'s values. They are per draw now. New\naddScatterColored gives each marker its own premultiplied colour through a\nsecond pipeline with a colour instance buffer; plain draws use the unchanged\npipeline and 3-float instances, and no colour buffer exists without a\ncoloured draw.\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 11: Coloured markers on `QCPMultiGraph`

**Files:**
- Modify: `src/plottables/plottable-multigraph.{h,cpp}` (`draw`, marker branch)
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:** Consumes `addScatterColored` (Task 10), `mColor`, `dataIdx` (Task 7).

- [ ] **Step 1: Write the failing test**

Header slot: `void coloredMarkersTakeTheirPointsColour();`
```cpp
void TestColorByScalar::coloredMarkersTakeTheirPointsColour()
{
    std::vector<double> keys(20), values(20, 0.0);
    for (int i = 0; i < 20; ++i) keys[i] = i * 10.0;
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setLineStyle(QCPMultiGraph::lsNone);
    mg->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 12);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(isRed(pixelAt(mPlot, img, 0, 0)));
    QVERIFY(isBlue(pixelAt(mPlot, img, 190, 0)));
}
```

- [ ] **Step 2: Build and run; expect failure** (markers use the component pen: black).

- [ ] **Step 3: Implement.** In the marker branch of `draw`, when `dataIdx` is set, build the xy/rgba arrays (skipping gap buckets, honouring `mScatterSkip`) and send them to `addScatterColored`; the QPainter fallback batches per bucket and translates by `gpuOffset`:
```cpp
        if (!comp.scatterStyle.isNone()) {
            if (dataIdx)
                drawColoredScatters(painter, dataLines, *dataIdx, comp, gpuOffset, isExportMode);
            else {
                … existing code unchanged …
            }
        }
```
```cpp
void QCPMultiGraph::drawColoredScatters(QCPPainter* painter, const QVector<QPointF>& points,
                                        const QVector<int>& indices, const QCPGraphComponent& comp,
                                        const QPointF& gpuOffset, bool isExportMode) const
{
    const int skip = mScatterSkip + 1;
    if (auto* srl = (!isExportMode && mParentPlot) ? mParentPlot->scatterRhiLayer(mLayer) : nullptr)
    {
        std::vector<float> xy, rgba;
        xy.reserve((points.size() / skip) * 2);
        rgba.reserve((points.size() / skip) * 4);
        for (int i = 0; i < points.size(); i += skip)
        {
            const int b = mColor.bucket(indices[i]);
            if (b == qcp::ColorScalarMapper::kGap || !qIsFinite(points[i].x()) || !qIsFinite(points[i].y()))
                continue;
            const QRgb c = mColor.color(b);   // premultiplied
            xy.insert(xy.end(), {float(points[i].x()), float(points[i].y())});
            rgba.insert(rgba.end(), {qRed(c) / 255.f, qGreen(c) / 255.f, qBlue(c) / 255.f, qAlpha(c) / 255.f});
        }
        if (!xy.empty())
            srl->addScatterColored(xy, rgba, comp.scatterStyle, clipRect(),
                                   mParentPlot->devicePixelRatioF(),
                                   mParentPlot->rhiOutputSize().height(),
                                   float(gpuOffset.x()), float(gpuOffset.y()));
        return;
    }
    applyScattersAntialiasingHint(painter);
    if (!gpuOffset.isNull())
        painter->translate(gpuOffset);
    std::array<QVector<QPointF>, 256> byBucket;
    for (int i = 0; i < points.size(); i += skip)
    {
        const int b = mColor.bucket(indices[i]);
        if (b != qcp::ColorScalarMapper::kGap && qIsFinite(points[i].x()) && qIsFinite(points[i].y()))
            byBucket[b].append(points[i]);
    }
    for (int b = 0; b < 256; ++b)
    {
        if (byBucket[b].isEmpty()) continue;
        const QColor color = qcp::runColor(mColor, b);
        QCPScatterStyle style = comp.scatterStyle;
        style.setPen(QPen(color));
        style.setBrush(color);
        style.applyTo(painter, QPen(color));
        for (const auto& p : byBucket[b])
            style.drawShape(painter, p.x(), p.y());
    }
    if (!gpuOffset.isNull())
        painter->translate(-gpuOffset);
}
```
(declare it protected in the header). Marker colour follows the same bucket as its point (`indices[i]`), pen and brush both in that colour, as coloured curves do.

- [ ] **Step 4: Build and run the whole suite.** Expected: pass, exit 0.

- [ ] **Step 5: Commit**
```bash
git add src/plottables tests/auto/test-color-by-scalar
printf 'feat(multigraph): markers coloured by the scalar\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 12: Legend gradient icon

**Files:**
- Modify: `src/plottables/plottable-multigraph.{h,cpp}` (`drawLegendIcon`), `src/layoutelements/layoutelement-legend-group.cpp:135-141, 170-173`
- Test: `tests/auto/test-color-by-scalar/test-color-by-scalar.{h,cpp}`

**Interfaces:**
- Produces on `QCPMultiGraph` (public): `void drawComponentLegendLine(QCPPainter* painter, int component, const QLineF& line) const;` — draws the component's legend line: its pen, or, when the graph is coloured, the pen with a `QLinearGradient` brush along `line` sampled from the 256-entry LUT at 5 stops.

- [ ] **Step 1: Write the failing test**

Header slot: `void coloredLegendLineShowsTheGradient();`
```cpp
void TestColorByScalar::coloredLegendLineShowsTheGradient()
{
    std::vector<double> keys(20), values(20, 0.0);
    std::iota(keys.begin(), keys.end(), 0.0);
    auto* mg = rampGraph(mPlot, makeSource(keys, {values}), ramp(20));
    mg->setComponentPens({QPen(Qt::black, 8)});
    QImage img(100, 20, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QCPPainter painter(&img);
        mg->drawComponentLegendLine(&painter, 0, QLineF(0, 10, 100, 10));
    }
    QVERIFY(isRed(img.pixelColor(3, 10)));
    QVERIFY(isBlue(img.pixelColor(96, 10)));

    mg->clearColorValues();
    img.fill(Qt::white);
    {
        QCPPainter painter(&img);
        mg->drawComponentLegendLine(&painter, 0, QLineF(0, 10, 100, 10));
    }
    QCOMPARE(img.pixelColor(50, 10), QColor(Qt::black));   // uncoloured: the component pen
}
```

- [ ] **Step 2: Build; expect a compile error** (no `drawComponentLegendLine`).

- [ ] **Step 3: Implement**
```cpp
void QCPMultiGraph::drawComponentLegendLine(QCPPainter* painter, int component, const QLineF& line) const
{
    QPen pen = mComponents[component].pen;
    if (mColor.hasValues())
    {
        QLinearGradient gradient(line.p1(), line.p2());
        for (int s = 0; s <= 4; ++s)
            gradient.setColorAt(s / 4.0, qcp::runColor(mColor, s * 255 / 4));
        pen.setBrush(gradient);
    }
    painter->setPen(pen);
    painter->drawLine(line);
}
```
Use it in `QCPMultiGraph::drawLegendIcon` (replace `painter->setPen(mComponents[i].pen); … painter->drawLine(QLineF(x0, y, x1, y));` by `drawComponentLegendLine(painter, i, QLineF(x0, y, x1, y));`) and in `QCPGroupLegendItem::draw`, both the collapsed loop (`painter->setPen(mMultiGraph->component(i).pen); … painter->drawLine(QLineF(x0, y, x1, y));` → `mMultiGraph->drawComponentLegendLine(painter, i, QLineF(x0, y, x1, y));`) and the expanded rows (`painter->setPen(comp.pen); … painter->drawLine(QLineF(…));` → `mMultiGraph->drawComponentLegendLine(painter, i, QLineF(…));`). For an uncoloured graph this draws exactly what it drew before.

- [ ] **Step 4: Build and run the whole suite.** Expected: pass, exit 0 (the existing legend tests in `TestMultiGraph`/`TestQCPLegend` still pass).

- [ ] **Step 5: Commit**
```bash
git add src/plottables src/layoutelements tests/auto/test-color-by-scalar
printf 'feat(legend): a coloured multigraph shows its gradient in the legend\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

---

### Task 13: Visual GPU check, final gate, push and PR

**Files:**
- Modify: `tests/manual/gallery/main.cpp` (a "MultiGraph coloured" tab)

The auto tests run offscreen without a `QRhi`, so the GPU line and marker paths are only exercised here and in the perf runs.

- [ ] **Step 1: Gallery tab.** Add, next to `createMultiGraphTab`:
```cpp
static QWidget* createColoredMultiGraphTab()
{
    auto* plot = new QCustomPlot();
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    const int n = 2'000'000;   // large enough for L1 + L2
    std::vector<double> keys(n), a(n), b(n), scalar(n);
    for (int i = 0; i < n; ++i)
    {
        keys[i] = i * 1e-3;
        a[i] = std::sin(i * 1e-4) + 0.2 * std::sin(i * 0.37);
        b[i] = std::cos(i * 1e-4);
        scalar[i] = (i / 50'000) % 7 == 3 ? std::nan("") : std::sin(i * 3e-5);
    }
    mg->setData(std::move(keys), std::vector<std::vector<double>>{std::move(a), std::move(b)});
    mg->setComponentPens({QPen(Qt::black, 2), QPen(Qt::black, 2)});
    mg->component(0).scatterStyle = QCPScatterStyle(QCPScatterStyle::ssDisc, 5);
    mg->setColorGradient(QCPColorGradient(QCPColorGradient::gpJet));
    mg->setColorRange(QCPRange(-1, 1));
    mg->setColorValues(std::move(scalar));
    plot->rescaleAxes();
    plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    return wrapPlot(plot);
}
```
and `tabs->addTab(createColoredMultiGraphTab(), "MultiGraph coloured");` after the "MultiGraph" tab.

- [ ] **Step 2: Build and look** (on the desktop session):
```bash
meson compile -C build-colour tests/manual/gallery/gallery && ./build-colour/tests/manual/gallery/gallery
```
Check on the "MultiGraph coloured" tab, and report what you saw:
1. Both lines show the jet gradient; NaN stretches are gaps (no line, no markers).
2. Zoom in until L2 turns off (points spread out): colours stay attached to the same data (a peak keeps its colour).
3. Pan: colours and markers move with the lines, no lag or offset between markers and line.
4. The legend icon shows a gradient.
5. The "MultiGraph" and "Scatter" tabs look exactly as before.
Try `NEOQCP_RHI_BACKEND=vulkan` and `=opengl` if available.

- [ ] **Step 3: Final gate.** Full auto suite (exit 0, every class `0 failed`, compare totals with Task 1 Step 2: only `TestColorByScalar` grew). Perf check 2 (Task 8 Steps 3-4) once more on the final tree. Record both in `/tmp/final-gate.txt`.

- [ ] **Step 4: Commit the gallery tab**
```bash
git add tests/manual/gallery/main.cpp
printf 'test(gallery): coloured multigraph tab\n\nCo-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>\n' > /tmp/msg && git commit -F /tmp/msg
```

- [ ] **Step 5: Push to the fork and open the PR** (never to `upstream`):
```bash
git remote -v   # origin = jeandet/NeoQCP, upstream = SciQLop/NeoQCP
git push -u origin feature/multigraph-colour-by-scalar
gh pr create --repo SciQLop/NeoQCP --base main --head jeandet:feature/multigraph-colour-by-scalar \
  --title "QCPMultiGraph: colour lines and markers by a scalar" --body-file /tmp/pr-body.md
```
`/tmp/pr-body.md`: what it does (spec link), the zero-cost guarantee with the Task 3 and Task 8 tables, the scatter per-draw fixes, what is out of scope (shared sprite, shared colormap, `QCPGraph2`), how it was checked (auto tests, gallery, perf), and end with `🤖 Generated with [Claude Code](https://claude.com/claude-code)`. Ask the maintainer before pushing: pushing is always an explicit request.

- [ ] **Step 6: File the out-of-scope NeoQCP issue** (after the maintainer agrees): shared scatter sprite (two graphs with different marker styles on one layer draw with the last style; a sprite atlas with per-draw UV rects fixes it), shared colormap texture, and `QCPGraph2` scatter-colour index drift after key gaps and under L2.
