# Fills, Bands and Error Bars Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fills between columns of one `QCPMultiGraph` (or down to a constant), error bars from columns of the same graph that turn into a band when too dense, and the Python factories `plot.band()` / `plot.error_bars()`.

**Architecture:** Bounds and edges are extra columns of the same graph. `QCPMultiGraph` keeps a graph-level list of decorations (fills, error bars) by column index. Pure geometry builders (pixel space in, vertices or polylines out) are unit-tested on their own; `draw()` adapts data rows to them. SciQLopPlots exposes column-indexed methods on the graph, component-level methods on the component, and Python factories built on `plot.line()`.

**Tech Stack:** C++20, Qt 6.11.2 (QRhi), NeoQCP (subprojects/NeoQCP, its own git repo), Shiboken6 bindings, pytest + pytest-qt, QtTest.

**Spec:** `docs/plans/2026-10-21-bands-errorbars-histogram-design.md` (sections "The idea shared by fills and error bars" to "SciQLopPlots API, fills and error bars"). The histogram is a separate plan: `docs/plans/2026-10-21-histogram-plan.md`.

## Global Constraints

- One build or test run at a time, in the foreground, waited out. Never background a build. `-j 4` maximum. This applies to subagents too.
- SciQLopPlots build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`. Never a bare `meson setup build`.
- SciQLopPlots tests run from `/tmp`: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`. Full suite = `tests/unit tests/fuzzing tests/resampler tests/tracing` plus `tests/integration` split in two halves (each under 590 s timeout). Never `tests/` (manual tests hang).
- NeoQCP tests: `cd /home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP && /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/meson compile -C build -j 4 && QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" QCP_TEST_CLASS=<TestClass> ./build/tests/auto/auto-tests`. Full NeoQCP suite: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/meson test -C build`.
- After changing a header-only binding or rejecting/adding bound members: `rm build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.cpp build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.h && .venv/bin/meson setup --reconfigure build-venv`, then build. Stale wrappers segfault.
- NeoQCP changes are committed in `subprojects/NeoQCP` (its own repo, branch `main`). SciQLopPlots changes in the top repo. Commit with explicit pathspecs: the untracked `qt.ini` and `subprojects/NeoQCP/subprojects/jkqtmathtext/` are not ours.
- Before any NeoQCP push (not part of this plan; pushing needs the user's explicit consent): `clang++ -fsyntax-only` on touched files with flags from `subprojects/NeoQCP/build/compile_commands.json`.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- Unsupported calls raise `NotImplementedError` from Python through `report_unsupported_call` (`src/UnsupportedCall.cpp`, macros `WARN_ABSTRACT_METHOD` / `WARN_UNSUPPORTED_FUNCTIONALITY` in `Debug.hpp`). Methods that throw must not be `noexcept`.
- `std::invalid_argument` becomes `ValueError` only through an explicit `bindings.xml` injection (see the `remove_group` pattern, `SciQLopPlots/bindings/bindings.xml:300-312`); the global handling makes every other exception a `RuntimeError`.
- Default fill colour: the owner's pen colour at alpha 64 (about 25 %). Default cap width: 6 logical pixels. Error bars are drawn only when consecutive rows in view are at least `3 × capWidthPx` apart, else as a band.
- Code style (user's global rules): small functions at one abstraction level, no comment-decorated blocks (extract a function), comments only for "why", `simplify:` comment on deliberate corner cuts.

## Review Focus

1. **A band callback that returns `None` or an empty batch** (normal for a time window with no data): the wrapped callback must pass it through, not crash in `np.column_stack`. Test in Task 11.
2. **A user calls `set_fill_between` before any data on a callback graph**, then data arrives with fewer columns than the target: the fill is dropped silently on the swap, and a later wider batch does not resurrect it. Test in Task 4.
3. **Error bars on a log value axis where `y - e <= 0`** (very common for small positive quantities): the bar reaches the axis edge, the marker stays, nothing breaks the polyline. Test in Task 7.
4. **Toggling a band edge's visibility from the legend or inspector**: hiding either edge removes the fill; showing it again brings it back without a zoom. Test in Task 5.
5. **Selecting a component with error bars**: the bars take the selected pen, and deselecting restores them on the next frame (cache invalidated by pen change). Test in Task 8.

---

### Task 1: Per-component "in legend" flag (NeoQCP)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.h` (struct `Component`, public API)
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.cpp` (`drawLegendIcon`, ~line 1164)
- Modify: `subprojects/NeoQCP/src/layoutelements/layoutelement-legend-group.cpp` (`shownExpanded` :46, `rowHeight` :55-62, `headerName`, `headerRowText`, `selectTest` :104-112, `draw` :148-229, `minimumOuterSizeHint` :232-260)
- Test: `subprojects/NeoQCP/tests/auto/test-multigraph/test-multigraph.{h,cpp}`

**Interfaces:**
- Produces: `bool QCPMultiGraph::Component::inLegend = true;`, `void QCPMultiGraph::setComponentInLegend(int index, bool inLegend)`, `QVector<int> QCPMultiGraph::legendComponents() const` (indices of components with `inLegend`, in order).

- [ ] **Step 1: Write the failing tests** — add to `test-multigraph.h` under `// Legend`:

```cpp
    void legendSkipsComponentsOutOfTheLegend();
    void legendClickMapsRowsToInLegendComponents();
    void legendOneInLegendComponentHasNoExpander();
```

and to `test-multigraph.cpp`:

```cpp
static QCPMultiGraph* threeColumnGraph(QCustomPlot* plot)
{
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    mg->setComponentNames({"y", "low", "high"});
    return mg;
}

static QCPGroupLegendItem* groupItemOf(QCustomPlot* plot, QCPMultiGraph* mg)
{
    for (int i = 0; i < plot->legend->itemCount(); ++i)
        if (auto* gi = qobject_cast<QCPGroupLegendItem*>(plot->legend->item(i)))
            if (gi->multiGraph() == mg)
                return gi;
    return nullptr;
}

void TestMultiGraph::legendSkipsComponentsOutOfTheLegend()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(2, false);
    QCOMPARE(mg->legendComponents(), (QVector<int>{0, 1}));
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    QVERIFY(gi);
    gi->setExpanded(true);
    const int rh = gi->rowHeight();
    QCOMPARE(gi->minimumOuterSizeHint().height(),
             rh * 3 + gi->margins().top() + gi->margins().bottom());
}

void TestMultiGraph::legendClickMapsRowsToInLegendComponents()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(1, false);
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    gi->setExpanded(true);
    mPlot->replot();
    QVariant details;
    const QRect r = gi->rect();
    const int rh = gi->rowHeight();
    QVERIFY(gi->selectTest(QPointF(r.left() + 5, r.top() + 2 * rh + rh / 2), false, &details) >= 0);
    QCOMPARE(details.toMap().value("componentIndex").toInt(), 2);
}

void TestMultiGraph::legendOneInLegendComponentHasNoExpander()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentInLegend(1, false);
    mg->setComponentInLegend(2, false);
    mg->addToLegend(mPlot->legend);
    auto* gi = groupItemOf(mPlot, mg);
    gi->setExpanded(true);
    QVERIFY(!gi->shownExpanded());
    QVERIFY(!gi->headerRowText(false).startsWith(QString::fromUtf8("▾")));
}
```

If `rowHeight()`, `shownExpanded()` or `headerRowText()` are private in `layoutelement-legend-group.h`, add `friend class TestMultiGraph;` to `QCPGroupLegendItem` (same pattern as `QCPMultiGraph`).

- [ ] **Step 2: Run to verify they fail**

Run: `cd subprojects/NeoQCP && ../../.venv/bin/meson compile -C build -j 4` → compile error "no member named 'setComponentInLegend'". Expected: FAIL.

- [ ] **Step 3: Implement**

In `plottable-multigraph.h`, `struct Component` gets `bool inLegend = true;` after `visible`. Public API next to `setComponentVisible`:

```cpp
    //! Keeps a component out of the legend (a band edge, an error bound) while it still draws
    //! and counts for auto-scale.
    void setComponentInLegend(int index, bool inLegend);
    [[nodiscard]] QVector<int> legendComponents() const;
```

In `plottable-multigraph.cpp`:

```cpp
void QCPMultiGraph::setComponentInLegend(int index, bool inLegend)
{
    if (index < 0 || index >= mComponents.size() || mComponents[index].inLegend == inLegend)
        return;
    mComponents[index].inLegend = inLegend;
    Q_EMIT componentVisibilityChanged();
}

QVector<int> QCPMultiGraph::legendComponents() const
{
    QVector<int> shown;
    for (int i = 0; i < mComponents.size(); ++i)
        if (mComponents[i].inLegend)
            shown.append(i);
    return shown;
}
```

`drawLegendIcon` splits `rect.width()` by `legendComponents().size()` and iterates that list (skipping `!visible` as today). In `layoutelement-legend-group.cpp` replace every `mMultiGraph->componentCount()` / `component(i)` loop with the `legendComponents()` list:
- `shownExpanded()`: `mExpanded && mMultiGraph && mMultiGraph->legendComponents().size() > 1`.
- `rowHeight()`, `minimumOuterSizeHint()`: iterate `legendComponents()`; height `rh * (1 + shown.size())`.
- `headerName()` and `headerRowText()`: use the first/last of `legendComponents()` and its size for the "▸/▾" and "first … last".
- `selectTest`: `componentIndex = shown.value(hitRow - 1, shown.isEmpty() ? -1 : shown.last())`.
- `draw()` collapsed: `segWidth = iconWidth / shown.size()`, position by position in `shown`; expanded: row `k` draws `shown[k]`.

- [ ] **Step 4: Run to verify they pass**

Run: `QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" QCP_TEST_CLASS=TestMultiGraph ./build/tests/auto/auto-tests` → all PASS (existing legend tests included).

- [ ] **Step 5: Commit (NeoQCP repo)**

```bash
cd subprojects/NeoQCP && git add src/plottables/plottable-multigraph.h src/plottables/plottable-multigraph.cpp src/layoutelements/layoutelement-legend-group.cpp src/layoutelements/layoutelement-legend-group.h tests/auto/test-multigraph
git commit -m "feat(multigraph): keep a component out of the legend"
```

---

### Task 2: Gaps of a multi data source, from outside (NeoQCP)

The lines break at key gaps through each source's private `GapCache`. The fill walks rows itself and needs the same breaks.

**Files:**
- Modify: `subprojects/NeoQCP/src/datasource/abstract-multi-datasource.h` (+ its `.cpp` if the class has one)
- Modify: `subprojects/NeoQCP/src/datasource/soa-multi-datasource.h`, `row-major-multi-datasource.h`, `resampled-multi-datasource.h` (overrides using their `ensureGapCache`)
- Test: `subprojects/NeoQCP/tests/auto/test-multi-datasource/test-multi-datasource.{h,cpp}`

**Interfaces:**
- Produces: `virtual qcp::algo::GapVector QCPAbstractMultiDataSource::gapsBefore(int begin, int end) const;` — element `i` is 1 when the step into row `begin + i` is a gap under `gapThreshold()`, exactly what `getLines` uses.

- [ ] **Step 1: Write the failing tests**

```cpp
void TestMultiDataSource::gapsBeforeMatchesTheLineBreaks()
{
    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> src(
        std::vector<double>{0, 1, 2, 3, 10, 11, 12},
        std::vector<std::vector<double>>{{0, 1, 2, 3, 4, 5, 6}});
    src.setGapThreshold(1.5);
    const auto gaps = src.gapsBefore(0, 7);
    QCOMPARE(gaps.size(), 7);
    QVERIFY(gaps.hasAnyGap);
    for (int i = 0; i < 7; ++i)
        QCOMPARE(int(gaps[i]), i == 4 ? 1 : 0);
}

void TestMultiDataSource::gapsBeforeOffByZeroThreshold()
{
    QCPSoAMultiDataSource<std::vector<double>, std::vector<double>> src(
        std::vector<double>{0, 1, 2, 3, 10, 11, 12},
        std::vector<std::vector<double>>{{0, 1, 2, 3, 4, 5, 6}});
    src.setGapThreshold(0);
    QVERIFY(!src.gapsBefore(0, 7).hasAnyGap);
}
```

Also one case on `QCPRowMajorMultiDataSource` with the same keys (same expectations).

- [ ] **Step 2: Run to verify they fail** — compile error "no member named 'gapsBefore'".

- [ ] **Step 3: Implement**

Base class default (generic and slow, like `getLinesIndexed`):

```cpp
    virtual qcp::algo::GapVector gapsBefore(int begin, int end) const
    {
        std::vector<double> keys(std::max(0, end - begin));
        for (int i = begin; i < end; ++i)
            keys[i - begin] = keyAt(i);
        return qcp::algo::detectKeyGaps(keys, 0, static_cast<int>(keys.size()), gapThreshold());
    }
```

Overrides in the three built-in sources:

```cpp
    qcp::algo::GapVector gapsBefore(int begin, int end) const override
    {
        ensureGapCache(begin, end);
        return mGapCache.gaps;
    }
```

- [ ] **Step 4: Run to verify they pass** — `QCP_TEST_CLASS=TestMultiDataSource ./build/tests/auto/auto-tests` → PASS.

- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(datasource): gapsBefore() on multi sources"` with the touched files.

---

### Task 3: Fill strip builder, pure geometry (NeoQCP)

**Files:**
- Create: `subprojects/NeoQCP/src/plottables/multigraph-fill.h`, `subprojects/NeoQCP/src/plottables/multigraph-fill.cpp`
- Modify: `subprojects/NeoQCP/meson.build` (add the `.cpp` next to `plottable-multigraph.cpp`; check whether headers are listed for install too)
- Create test: `subprojects/NeoQCP/tests/auto/test-fill-geometry/test-fill-geometry.{h,cpp}`; register in `tests/auto/meson.build` (`test_srcs`, `test_headers`) and `tests/auto/autotest.cpp` (`QCPTEST(TestFillGeometry);`).

**Interfaces:**
- Produces (namespace `qcp::fill`):

```cpp
//! One row of a fill, in pixels: the key position and the two edges' value positions.
//! breakBefore: the strip does not join this row to the previous one (a key gap).
struct StripPoint { double key; double a; double b; bool breakBefore = false; };

//! Triangles (6 floats per vertex: x, y, r, g, b, a premultiplied) between the edges of
//! consecutive rows, appended to out. A non-finite edge or breakBefore ends the strip; a segment
//! where the edges cross is split at the crossing.
void appendStrip(std::span<const StripPoint> rows, bool keyIsVertical,
                 const std::array<float, 4>& premultipliedRgba, std::vector<float>& out);

//! Rows binned into spans: per bin of binWidthPx along the key, one row at the bin centre with
//! a = min and b = max of both edges. Bins never join across a break.
std::vector<StripPoint> binToSpans(std::span<const StripPoint> rows, double binWidthPx);

//! The same strip as closed polygons, one per gap-free run (edge a forward, edge b back), for
//! QPainter and export.
QVector<QPolygonF> stripPolygons(std::span<const StripPoint> rows, bool keyIsVertical);

//! Step transforms applied to both edges, matching qcp::toStepLeftLines & co.
enum class Step { None, Left, Right, Center };
std::vector<StripPoint> stepped(std::span<const StripPoint> rows, Step step);
```

- [ ] **Step 1: Write the failing tests** (`test-fill-geometry.h` declares `TestFillGeometry` with these slots)

```cpp
#include "test-fill-geometry.h"
#include "plottables/multigraph-fill.h"
#include <cmath>

using qcp::fill::StripPoint;
namespace {
constexpr std::array<float, 4> kRed { 1.f, 0.f, 0.f, 1.f };
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
int triangles(const std::vector<float>& v) { return static_cast<int>(v.size() / 18); }
double area(const std::vector<float>& v)
{
    double total = 0;
    for (std::size_t t = 0; t + 17 < v.size(); t += 18)
    {
        const double x0 = v[t], y0 = v[t + 1], x1 = v[t + 6], y1 = v[t + 7], x2 = v[t + 12], y2 = v[t + 13];
        total += std::abs((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)) / 2;
    }
    return total;
}
}

void TestFillGeometry::twoRowsMakeOneQuad()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(triangles(out), 2);
    QCOMPARE(area(out), 100.0);
}

void TestFillGeometry::nanEdgeBreaksTheStrip()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, nan, 10}, {20, 0, 10}, {30, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 100.0); // only rows 2-3 join
}

void TestFillGeometry::breakBeforeBreaksTheStrip()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10}, {20, 0, 10, true}, {30, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 200.0);
}

void TestFillGeometry::crossingEdgesAreSplitNotDoubleBlended()
{
    // a goes 0 -> 10, b goes 10 -> 0: they cross at key 5; two triangles of area 25 each.
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 10, 0} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, false, kRed, out);
    QCOMPARE(area(out), 50.0);
}

void TestFillGeometry::verticalKeySwapsCoordinates()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10} };
    std::vector<float> out;
    qcp::fill::appendStrip(rows, true, kRed, out);
    for (std::size_t v = 0; v < out.size(); v += 6)
        QVERIFY(out[v] == 0.f || out[v] == 10.f); // x is the value position
}

void TestFillGeometry::binToSpansTakesMinAndMaxOfBothEdges()
{
    const std::vector<StripPoint> rows { {0.1, 5, 6}, {0.4, 1, 9}, {0.9, 3, 4}, {1.2, 2, 2} };
    const auto spans = qcp::fill::binToSpans(rows, 1.0);
    QCOMPARE(spans.size(), std::size_t(2));
    QCOMPARE(spans[0].a, 1.0);
    QCOMPARE(spans[0].b, 9.0);
    QCOMPARE(spans[1].a, 2.0);
    QCOMPARE(spans[1].b, 2.0);
}

void TestFillGeometry::binToSpansKeepsBreaks()
{
    const std::vector<StripPoint> rows { {0.1, 0, 1}, {0.9, 0, 1}, {5.1, 0, 1, true} };
    const auto spans = qcp::fill::binToSpans(rows, 1.0);
    QCOMPARE(spans.size(), std::size_t(2));
    QVERIFY(spans[1].breakBefore);
}

void TestFillGeometry::stripPolygonsOnePerRun()
{
    const std::vector<StripPoint> rows { {0, 0, 10}, {10, 0, 10}, {20, 0, 10, true}, {30, 0, 10} };
    const auto polys = qcp::fill::stripPolygons(rows, false);
    QCOMPARE(polys.size(), 2);
    QCOMPARE(polys[0].size(), 4); // a forward (2), b back (2)
}

void TestFillGeometry::stepLeftHoldsEachValueUntilTheNextKey()
{
    const std::vector<StripPoint> rows { {0, 0, 1}, {10, 2, 3} };
    const auto s = qcp::fill::stepped(rows, qcp::fill::Step::Left);
    QCOMPARE(s.size(), std::size_t(3));
    QCOMPARE(s[1].key, 10.0);
    QCOMPARE(s[1].a, 0.0); // still the first row's value at the second key
    QCOMPARE(s[2].a, 2.0);
}
```

Before writing `stepped`, read `qcp::toStepLeftLines/RightLines/CenterLines` (grep in `src/plottables/plottable-draw-utils.*` or `plottable1d.h`) and make `stepped` produce the same corner sequence for one edge, so the fill meets the drawn steps. Add `stepRightAndCenterMatchTheLineHelpers` comparing `stepped(...).a` against those helpers on a 4-row input.

- [ ] **Step 2: Run to verify they fail** — compile error (missing header).

- [ ] **Step 3: Implement** `multigraph-fill.cpp`. Core of `appendStrip`:

```cpp
namespace qcp::fill {
namespace {
bool finite(const StripPoint& p) { return std::isfinite(p.key) && std::isfinite(p.a) && std::isfinite(p.b); }

void vertex(std::vector<float>& out, bool keyIsVertical, double key, double value,
            const std::array<float, 4>& c)
{
    const float x = static_cast<float>(keyIsVertical ? value : key);
    const float y = static_cast<float>(keyIsVertical ? key : value);
    out.insert(out.end(), { x, y, c[0], c[1], c[2], c[3] });
}

void triangle(std::vector<float>& out, bool v, std::array<std::pair<double, double>, 3> pts,
              const std::array<float, 4>& c)
{
    for (auto [k, val] : pts)
        vertex(out, v, k, val, c);
}

void quad(std::vector<float>& out, bool v, const StripPoint& p, const StripPoint& q,
          const std::array<float, 4>& c)
{
    const double d0 = p.b - p.a, d1 = q.b - q.a;
    if (d0 * d1 >= 0)
    {
        triangle(out, v, {{{p.key, p.a}, {q.key, q.a}, {q.key, q.b}}}, c);
        triangle(out, v, {{{p.key, p.a}, {q.key, q.b}, {p.key, p.b}}}, c);
        return;
    }
    const double t = d0 / (d0 - d1);
    const double xk = p.key + t * (q.key - p.key);
    const double xv = p.a + t * (q.a - p.a);
    triangle(out, v, {{{p.key, p.a}, {xk, xv}, {p.key, p.b}}}, c);
    triangle(out, v, {{{xk, xv}, {q.key, q.a}, {q.key, q.b}}}, c);
}
}

void appendStrip(std::span<const StripPoint> rows, bool keyIsVertical,
                 const std::array<float, 4>& rgba, std::vector<float>& out)
{
    for (std::size_t i = 1; i < rows.size(); ++i)
        if (finite(rows[i - 1]) && finite(rows[i]) && !rows[i].breakBefore)
            quad(out, keyIsVertical, rows[i - 1], rows[i], rgba);
}
}
```

`binToSpans`: walk rows, `bin = floor(key / binWidthPx)`; start a new output row when the bin changes or `breakBefore` is set (carrying `breakBefore`); accumulate `min(a, b)` into `a` and `max(a, b)` into `b`; output key = bin centre `(bin + 0.5) * binWidthPx`; skip non-finite rows but let them set `breakBefore` on the next row. `stripPolygons`: split into gap-free runs like `appendStrip`, polygon = `(key, a)` forward then `(key, b)` backward (swap for vertical). `stepped`: per the line helpers.

- [ ] **Step 4: Run to verify they pass** — `QCP_TEST_CLASS=TestFillGeometry ./build/tests/auto/auto-tests` → PASS.

- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(fill): pure strip builder for multigraph fills"`.

---

### Task 4: Fill decorations on `QCPMultiGraph` (storage, validation, lifecycle)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.h`, `.cpp` (`syncComponentCount` :291-307 for the drop on shrink)
- Test: `subprojects/NeoQCP/tests/auto/test-multigraph/test-multigraph.{h,cpp}`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:

```cpp
    struct Fill {
        int owner = -1;
        std::variant<double, int> target;   // a constant, or another column
        QBrush brush;                       // Qt::NoBrush = owner pen colour at alpha 64
    };
    bool setComponentFill(int owner, double value, const QBrush& brush = Qt::NoBrush);
    bool setComponentFillBetween(int owner, int target, const QBrush& brush = Qt::NoBrush);
    void setComponentFillBrush(int owner, const QBrush& brush);
    void clearComponentFill(int owner);
    [[nodiscard]] std::optional<Fill> componentFill(int owner) const;
    [[nodiscard]] QBrush effectiveFillBrush(const Fill& fill) const;
```

`setComponentFill*` return `false` (and change nothing) for `owner < 0`, `target < 0`, `target == owner`, or a column `>= dataSource()->columnCount()` when a source exists. One fill per owner: setting again replaces. Storage `QVector<FillEntry> mFills` where `struct FillEntry { Fill spec; std::vector<float> verts; };` (the vertex cache is used in Task 5). Every setter calls `invalidateLines()` and `mLayer->invalidatePaintBuffer()` when a layer exists (see `setComponentVisible`, `plottable-multigraph.cpp:352-362`).

- [ ] **Step 1: Write the failing tests**

```cpp
void TestMultiGraph::fillRejectsBadColumns()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(!mg->setComponentFillBetween(1, 1));
    QVERIFY(!mg->setComponentFillBetween(0, 3));
    QVERIFY(!mg->setComponentFillBetween(-1, 0));
    QVERIFY(mg->setComponentFillBetween(1, 2));
    QCOMPARE(std::get<int>(mg->componentFill(1)->target), 2);
}

void TestMultiGraph::fillBeforeDataIsKept()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    QVERIFY(mg->setComponentFillBetween(1, 2));
    mg->setData(std::vector<double>{1, 2, 3},
                std::vector<std::vector<double>>{{1, 2, 3}, {0, 1, 2}, {2, 3, 4}});
    QVERIFY(mg->componentFill(1).has_value());
}

void TestMultiGraph::fillIsDroppedWhenItsColumnDisappears()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(mg->setComponentFillBetween(1, 2));
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{1, 2}, {0, 1}});
    QVERIFY(!mg->componentFill(1).has_value());
    mg->setData(std::vector<double>{1, 2},
                std::vector<std::vector<double>>{{1, 2}, {0, 1}, {3, 4}});
    QVERIFY(!mg->componentFill(1).has_value()); // shrinking is lossy
}

void TestMultiGraph::fillDefaultBrushIsTheOwnerColourAtQuarterAlpha()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentColors({Qt::red, Qt::green, Qt::blue});
    mg->setComponentFill(0, 0.0);
    const QBrush b = mg->effectiveFillBrush(*mg->componentFill(0));
    QCOMPARE(b.color().rgb(), QColor(Qt::red).rgb());
    QCOMPARE(b.color().alpha(), 64);
}

void TestMultiGraph::fillReplacesAndClears()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFill(0, 1.0);
    mg->setComponentFillBetween(0, 2);
    QCOMPARE(std::get<int>(mg->componentFill(0)->target), 2);
    mg->clearComponentFill(0);
    QVERIFY(!mg->componentFill(0).has_value());
}
```

Declare the five slots in `test-multigraph.h` under a new `// Fills` heading.

- [ ] **Step 2: Run to verify they fail** — compile error.

- [ ] **Step 3: Implement.** In `syncComponentCount`, on shrink, after `mComponents.resize(newCount)`:

```cpp
        dropDecorationsBeyond(newCount);
```

with

```cpp
// A decoration pointing at a column that no longer exists is dropped for good: a later, wider
// source puts unrelated data in that column.
void QCPMultiGraph::dropDecorationsBeyond(int columnCount)
{
    const auto outside = [columnCount](const FillEntry& e) {
        const auto* col = std::get_if<int>(&e.spec.target);
        return e.spec.owner >= columnCount || (col && *col >= columnCount);
    };
    mFills.erase(std::remove_if(mFills.begin(), mFills.end(), outside), mFills.end());
}
```

(Task 8 extends it to error bars.) `effectiveFillBrush`: `fill.brush.style() != Qt::NoBrush ? fill.brush : QBrush(QColor(ownerPenColour.red(), ...green, ...blue, 64))`.

- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestMultiGraph` → PASS.

- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(multigraph): fill decorations by column"`.

---

### Task 5: Drawing fills (NeoQCP)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.cpp` (`draw()` :944-1160), `.h` (private helpers)
- Test: `subprojects/NeoQCP/tests/auto/test-multigraph/test-multigraph.{h,cpp}`

**Interfaces:**
- Consumes: `qcp::fill::*` (Task 3), `QCPAbstractMultiDataSource::gapsBefore` (Task 2), `Fill`/`mFills`/`effectiveFillBrush` (Task 4).
- Produces: private `std::vector<qcp::fill::StripPoint> fillRows(const Fill&, const QCPAbstractMultiDataSource& ds, int begin, int end) const;` and `void drawFills(QCPPainter*, const QCPAbstractMultiDataSource& ds, int begin, int end, bool fresh, const QPointF& gpuOffset, bool isExportMode);`

Rules (spec, "Fill geometry"):
1. `fillRows`: for each row `i` in `[begin, end)`: `key = keyAxis->coordToPixel(ds.keyAt(i))`, `a = valueAxis->coordToPixel(ds.valueAt(owner, i))`, `b` = the target column's pixel, or the constant's pixel. `breakBefore = gaps[i - begin]` from `ds.gapsBefore(begin, end)`.
2. Constant on a log value axis with `value <= 0`: `b` = the axis rect's bottom edge pixel (top when the axis is reversed or vertical-key layouts flip it) pushed out by `2 × axisRect height`. Read `QCPGraph::getFillBasePoint` (`plottable-graph.cpp:1702-1760`) for the edge selection rules and mirror them.
3. When `end - begin > 4 × pixelWidth`, pass the rows through `qcp::fill::binToSpans(rows, 0.25)` (4 bins per pixel).
4. Owner line style: `lsImpulse` → no fill (also refused at the SciQLopPlots layer); steps → `qcp::fill::stepped`.
5. Drawn only when the owner and (for a column target) the target are `visible`.
6. Fresh frames (`needFreshLines`, the same flag the lines use) rebuild `entry.verts`; cache-hit frames reuse them with `gpuOffset`. The rebuild happens over the same `cacheBegin/cacheEnd` window as the lines (`plottable-multigraph.cpp:1022-1025`), so pans within the cache stay correct.
7. GPU: `prl->addPlottable(entry.verts, {}, clipRect(), dpr, outHeight, gpuOffset.x(), gpuOffset.y(), painter->opacity())` as `QCPGraph::drawFill` does (`plottable-graph.cpp:1003-1024`). Export or no RHI: `painter->setBrush(brush); painter->setPen(Qt::NoPen); painter->drawPolygon(poly)` for each `qcp::fill::stripPolygons(rows, keyIsVertical)` (translate by `gpuOffset` when non-null, like impulses do at :1121-1125).
8. Per-column vectors (`mExtrusionCaches`, `mCachedLines` side data) are resized in one helper `resizePerColumn(int nc)`, called where `draw()` decides `nc` (:1030); today `mExtrusionCaches` is resized separately at :1079 on `mComponents.size()`. Decoration caches live in their entries and need no resize.
9. Call `drawFills` once, **before** the per-component loop (before line 1081, after the `mExtrusionCaches` clear at :1074), so `lsNone` edges keep their fill. Clear every `entry.verts` in the same block as `mExtrusionCaches`.

- [ ] **Step 1: Write the failing tests** (CPU path through `toPixmap`, which exports with `QPainter`; GPU path checked via the cache)

```cpp
static QImage renderOf(QCustomPlot* plot)
{
    return plot->toPixmap(400, 300).toImage().convertToFormat(QImage::Format_ARGB32);
}

static bool isRedish(QColor c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }

void TestMultiGraph::fillToZeroColoursBelowTheCurveOnly()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    std::vector<double> x(200), y(200);
    for (int i = 0; i < 200; ++i) { x[i] = i; y[i] = 5.0; }
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y)});
    mg->setComponentColors({Qt::red});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->xAxis->setRange(0, 199);
    mPlot->yAxis->setRange(-5, 10);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    const int xm = r.center().x();
    QVERIFY(isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(2.5)))));
    QVERIFY(!isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(7.5)))));
    QVERIFY(!isRedish(img.pixelColor(xm, int(mPlot->yAxis->coordToPixel(-2.5)))));
}

void TestMultiGraph::hiddenEdgeLinesKeepTheirFill()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    const QImage img = renderOf(mPlot);
    QVERIFY(isRedish(img.pixelColor(int(mPlot->xAxis->coordToPixel(2)),
                                    int(mPlot->yAxis->coordToPixel(2)))));
}

void TestMultiGraph::hidingAnEdgeRemovesTheFillAndShowingBringsItBack()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(-1, 5);
    const QPoint inside(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(2)));
    mg->setComponentVisible(2, false);
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(inside)));
    mg->setComponentVisible(2, true);
    QVERIFY(isRedish(renderOf(mPlot).pixelColor(inside)));
}

void TestMultiGraph::fillBreaksAtAKeyGap()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{0, 1, 2, 3, 20, 21, 22, 23},
                std::vector<std::vector<double>>{{5, 5, 5, 5, 5, 5, 5, 5}});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->xAxis->setRange(0, 23);
    mPlot->yAxis->setRange(-1, 6);
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(int(mPlot->xAxis->coordToPixel(11)),
                                                 int(mPlot->yAxis->coordToPixel(2)))));
}

void TestMultiGraph::fillOnALogAxisClampsAZeroBaseline()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2, 3}, std::vector<std::vector<double>>{{10, 10, 10}});
    mg->setComponentFill(0, 0.0, QBrush(Qt::red));
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(1, 3);
    mPlot->yAxis->setRange(0.1, 100);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    QVERIFY(isRedish(img.pixelColor(r.center().x(), r.bottom() - 2)));
    QVERIFY(!isRedish(img.pixelColor(r.center().x(), int(mPlot->yAxis->coordToPixel(50)))));
}

void TestMultiGraph::fillCacheIsRebuiltOnZoom()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentFillBetween(1, 2, QBrush(Qt::red));
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    if (!mPlot->rhi())
        QSKIP("no RHI in this environment");
    const auto before = mg->mFills.first().verts;
    QVERIFY(!before.empty());
    mPlot->xAxis->setRange(1.5, 2.5);
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    QVERIFY(mg->mFills.first().verts != before);
}

void TestMultiGraph::autoScaleIncludesHiddenLineBandEdges()
{
    auto* mg = threeColumnGraph(mPlot);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    bool found = false;
    QCOMPARE(mg->getValueRange(found).upper, 4.0);
}
```

Also one downsampled case: `fillFollowsADownsampledLine` — 500 000 rows of `y = sin`, fill to 0, render after the pipeline finished (wait like `zoomedOutLinesAreThinnedToThePixelWidth` does), assert red below the curve at a crest, not above.

- [ ] **Step 2: Run to verify they fail** — the fill tests FAIL (no red pixels); `autoScaleIncludesHiddenLineBandEdges` already passes (it documents the lsNone rule; keep it).

- [ ] **Step 3: Implement** `fillRows` and `drawFills` per the rules above, with small helpers: `fillBaselinePixel(double value)` (rule 2), `ownerStep(const Component&)` mapping `LineStyle` to `qcp::fill::Step`, `fillVisible(const Fill&)`. Use `qcp::rhi::premultipliedColor(brush.color())` for the vertex colour (as `tessellateFillPolygon` does).

- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestMultiGraph`, then the full NeoQCP suite (`meson test -C build`) → PASS.

- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(multigraph): draw fills, GPU and export"`.

---

### Task 6: Fill swatch in the legend (NeoQCP)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.cpp` (`drawComponentLegendIcon` :1180-1201, `drawLegendIcon`)
- Test: `test-multigraph.{h,cpp}`

**Interfaces:**
- Consumes: `componentFill`, `effectiveFillBrush`.

A component that owns a fill and has no line (`lsNone`) and no marker draws a filled rectangle instead of a line: height = 60 % of the row height, centred on `line`'s y, full `line` width. With a line, the rectangle is drawn first and the line on top.

- [ ] **Step 1: Failing test**

```cpp
void TestMultiGraph::legendIconOfABandIsAFilledSwatch()
{
    auto* mg = new QCPMultiGraph(mPlot->xAxis, mPlot->yAxis);
    mg->setData(std::vector<double>{1, 2}, std::vector<std::vector<double>>{{0, 0}, {1, 1}});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentInLegend(1, false);
    mg->setComponentFillBetween(0, 1, QBrush(Qt::red));
    QImage img(40, 20, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QCPPainter p(&img);
    mg->drawComponentLegendIcon(&p, 0, QLineF(0, 10, 40, 10));
    p.end();
    QVERIFY(isRedish(img.pixelColor(20, 6)));
    QVERIFY(isRedish(img.pixelColor(20, 14)));
}
```

- [ ] **Step 2: Run** — FAIL (nothing drawn for an `lsNone` component).
- [ ] **Step 3: Implement** — in `drawComponentLegendIcon`, before the line: `if (auto fill = componentFill(component)) painter->fillRect(swatchRect(line), effectiveFillBrush(*fill));` with `swatchRect` = `QRectF(line.x1(), line.y1() - 6, line.length(), 12)` (12 px ≈ 60 % of the 20 px default row; read `rowHeight()` defaults to confirm).
- [ ] **Step 4: Run** — PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(multigraph): filled swatch for a band in the legend"`.

---

### Task 7: Error-bar builder, pure geometry (NeoQCP)

**Files:**
- Create: `subprojects/NeoQCP/src/plottables/multigraph-errorbars.h`, `.cpp`; add the `.cpp` to `subprojects/NeoQCP/meson.build`
- Create test: `subprojects/NeoQCP/tests/auto/test-errorbar-geometry/test-errorbar-geometry.{h,cpp}`; register in `tests/auto/meson.build` and `autotest.cpp` (`QCPTEST(TestErrorBarGeometry);`)

**Interfaces:**
- Produces (namespace `qcp::errorbars`):

```cpp
//! One bar in pixels. value/lower/upper may be NaN; resolve() has already clamped crossed
//! bounds and log-invalid bounds in data space.
struct Bar { double key; double value; double lower; double upper; };

//! A NaN bound falls back to the value (a one-sided bar). Returns false when fewer than two of
//! the three points are finite: no bar.
bool resolve(Bar& bar);

//! Stem and two caps per bar as separate NaN-separated runs (3 runs, 2 points each), for the
//! polyline helpers. Never emits infinities.
QVector<QPointF> barRuns(std::span<const Bar> bars, double capHalfWidthPx, bool keyIsVertical);

//! True when no two consecutive bars with a finite key are closer than minSpacingPx.
bool barsFit(std::span<const Bar> bars, double minSpacingPx);
```

- [ ] **Step 1: Failing tests**

```cpp
#include "test-errorbar-geometry.h"
#include "plottables/multigraph-errorbars.h"
using qcp::errorbars::Bar;
namespace { constexpr double nan = std::numeric_limits<double>::quiet_NaN(); }

void TestErrorBarGeometry::oneBarIsAStemAndTwoCaps()
{
    const std::vector<Bar> bars { {10, 50, 60, 40} };
    const auto runs = qcp::errorbars::barRuns(bars, 3, false);
    QCOMPARE(runs.size(), 3 * 2 + 2); // 3 runs of 2 points, 2 NaN separators
    QCOMPARE(runs[0], QPointF(10, 60));
    QCOMPARE(runs[1], QPointF(10, 40));
    QVERIFY(std::isnan(runs[2].x()));
    QCOMPARE(runs[3], QPointF(7, 60));
    QCOMPARE(runs[4], QPointF(13, 60));
}

void TestErrorBarGeometry::nanBoundGivesAOneSidedBar()
{
    Bar b { 10, 50, nan, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 50.0);
}

void TestErrorBarGeometry::twoNanPointsGiveNoBar()
{
    Bar b { 10, nan, nan, 40 };
    QVERIFY(!qcp::errorbars::resolve(b));
}

void TestErrorBarGeometry::missingValueStillDrawsBetweenBounds()
{
    Bar b { 10, nan, 60, 40 };
    QVERIFY(qcp::errorbars::resolve(b));
    QCOMPARE(b.lower, 60.0);
    QCOMPARE(b.upper, 40.0);
}

void TestErrorBarGeometry::verticalKeyGivesHorizontalBars()
{
    const std::vector<Bar> bars { {10, 50, 60, 40} };
    const auto runs = qcp::errorbars::barRuns(bars, 3, true);
    QCOMPARE(runs[0], QPointF(60, 10));
    QCOMPARE(runs[1], QPointF(40, 10));
}

void TestErrorBarGeometry::barsFitNeedsTheSpacing()
{
    const std::vector<Bar> wide { {0, 1, 2, 0}, {20, 1, 2, 0}, {40, 1, 2, 0} };
    const std::vector<Bar> bursty { {0, 1, 2, 0}, {2, 1, 2, 0}, {40, 1, 2, 0} };
    QVERIFY(qcp::errorbars::barsFit(wide, 18));
    QVERIFY(!qcp::errorbars::barsFit(bursty, 18)); // a mean spacing would hide this
}

void TestErrorBarGeometry::noInfinitiesEverReachTheRuns()
{
    const std::vector<Bar> bars { {10, 50, std::numeric_limits<double>::infinity(), 40} };
    for (const QPointF& p : qcp::errorbars::barRuns(bars, 3, false))
        QVERIFY(std::isnan(p.x()) || (std::isfinite(p.x()) && std::isfinite(p.y())));
}
```

- [ ] **Step 2: Run** — compile error. Expected FAIL.
- [ ] **Step 3: Implement.** `resolve`: treat non-finite (`!std::isfinite`) as missing; `lower = finite(lower) ? lower : value`, same for upper; return `finiteCount >= 2` and, when `value` is missing, leave lower/upper as they are. `barRuns`: skip bars that fail `resolve` or have a non-finite key; per bar append `stem(lower→upper)`, NaN, `cap(lower)`, NaN, `cap(upper)`, NaN between bars (none after the last); swap x/y when `keyIsVertical`. `barsFit`: consecutive finite keys, `std::abs(k1 - k0) >= minSpacingPx`.
- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestErrorBarGeometry` → PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(errorbars): pure bar builder for multigraph error bars"`.

---

### Task 8: Error bars on `QCPMultiGraph` (storage, drawing, band fallback, legend glyph)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-multigraph.{h,cpp}`
- Test: `test-multigraph.{h,cpp}`

**Interfaces:**
- Consumes: `qcp::errorbars::*` (Task 7), `qcp::fill::appendStrip` / `stripPolygons` (Task 3), `dropDecorationsBeyond` (Task 4), `drawFills` placement (Task 5).
- Produces:

```cpp
    struct ErrorBars {
        int value = -1, lower = -1, upper = -1;
        std::optional<QPen> pen;      // none = the value component's pen
        double capWidthPx = 6;
    };
    bool setComponentErrorBars(int value, int lower, int upper, double capWidthPx = 6,
                               std::optional<QPen> pen = std::nullopt);
    void clearComponentErrorBars(int value);
    [[nodiscard]] std::optional<ErrorBars> componentErrorBars(int value) const;
```

Storage: `QVector<ErrorBarsEntry> mErrorBars` with `struct ErrorBarsEntry { ErrorBars spec; qcp::ExtrusionCache bars; std::vector<float> band; bool asBand = false; };`. Validation as fills (all three columns distinct from each other where `lower == upper` is refused, `>= 0`, `< columnCount()` when a source exists). `dropDecorationsBeyond` also drops entries with any column `>= columnCount`. Setters invalidate like Task 4 (cap width or pen change must show on the next frame).

Drawing (`drawErrorBars`, called right after `drawFills`, before the line loop):
1. Skip when the value component is not `visible`.
2. Data-space resolution per row, before mapping: when `y` is finite, `lower = min(lower, y)`, `upper = max(upper, y)`; on a log value axis a bound `<= 0` maps to the axis edge pixel (reuse Task 5's `fillBaselinePixel(0)` rule, without the 2× extension: bars stop at the edge).
3. Bars only when `ds == mDataSource.get()` (after the L2 re-resolve at :999-1008) **and** `qcp::errorbars::barsFit(bars, 3 * capWidthPx)`. Then `pen = comp.selection.isEmpty() ? spec.pen.value_or(comp.pen) : comp.selectedPen` and `qcp::drawPolylineCached(painter, mParentPlot, mLayer, runs, pen, gpuOffset, clipRect(), needFreshLines, entry.bars)` (export: `drawPolylineWithGpuFallback`).
4. Otherwise a band: strip rows with `a = lower`, `b = upper` from `ds` (downsampled sources carry the bound columns), brush = the pen colour at alpha 64, drawn like a fill and cached in `entry.band`. `entry.asBand` records the mode for cache-hit frames.
5. `entry.bars.clear()` and `entry.band.clear()` in the same block that clears `mExtrusionCaches` (:1074).

Legend: `drawComponentLegendIcon` draws, for a component with error bars, a vertical stem of 10 px through `line.center()` with 6 px caps, in the bar pen, after the marker.

- [ ] **Step 1: Failing tests**

```cpp
static QCPMultiGraph* errorBarGraph(QCustomPlot* plot, int n, double spacing)
{
    std::vector<double> x(n), y(n), lo(n), hi(n);
    for (int i = 0; i < n; ++i) { x[i] = i * spacing; y[i] = 5; lo[i] = 3; hi[i] = 7; }
    auto* mg = new QCPMultiGraph(plot->xAxis, plot->yAxis);
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y), std::move(lo), std::move(hi)});
    mg->setComponentColors({Qt::red, Qt::red, Qt::red});
    mg->setComponentLineStyle(0, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(1, QCPMultiGraph::lsNone);
    mg->setComponentLineStyle(2, QCPMultiGraph::lsNone);
    mg->setComponentErrorBars(0, 1, 2);
    return mg;
}

void TestMultiGraph::sparseErrorBarsAreDrawnAsBars()
{
    errorBarGraph(mPlot, 5, 1.0);
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    const QImage img = renderOf(mPlot);
    const int xBar = int(mPlot->xAxis->coordToPixel(2));
    const int xBetween = int(mPlot->xAxis->coordToPixel(2.5));
    const int yInside = int(mPlot->yAxis->coordToPixel(4));
    QVERIFY(isRedish(img.pixelColor(xBar, yInside)));
    QVERIFY(!isRedish(img.pixelColor(xBetween, yInside)));
}

void TestMultiGraph::denseErrorBarsBecomeABand()
{
    errorBarGraph(mPlot, 2000, 0.01);
    mPlot->xAxis->setRange(0, 20);
    mPlot->yAxis->setRange(0, 10);
    const QImage img = renderOf(mPlot);
    const QColor between = img.pixelColor(int(mPlot->xAxis->coordToPixel(10.003)),
                                          int(mPlot->yAxis->coordToPixel(4)));
    QVERIFY(between.red() > between.green() + 30); // translucent red band, everywhere
}

void TestMultiGraph::errorBoundsBelowZeroOnALogAxisReachTheEdge()
{
    auto* mg = errorBarGraph(mPlot, 3, 1.0);
    std::vector<double> x{0, 1, 2}, y{1, 1, 1}, lo{-1, -1, -1}, hi{2, 2, 2};
    mg->setData(std::move(x), std::vector<std::vector<double>>{std::move(y), std::move(lo), std::move(hi)});
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->xAxis->setRange(-0.5, 2.5);
    mPlot->yAxis->setRange(0.1, 10);
    const QImage img = renderOf(mPlot);
    const QRect r = mPlot->axisRect()->rect();
    QVERIFY(isRedish(img.pixelColor(int(mPlot->xAxis->coordToPixel(1)), r.bottom() - 3)));
}

void TestMultiGraph::selectedErrorBarsUseTheSelectedPen()
{
    auto* mg = errorBarGraph(mPlot, 5, 1.0);
    auto& comp = mg->component(0);
    comp.selectedPen = QPen(Qt::blue, 2);
    mg->setComponentSelection(0, QCPDataSelection(QCPDataRange(0, 5)));
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    QImage img = renderOf(mPlot);
    const QPoint bar(int(mPlot->xAxis->coordToPixel(2)), int(mPlot->yAxis->coordToPixel(4)));
    QVERIFY(img.pixelColor(bar).blue() > 150);
    mg->setComponentSelection(0, QCPDataSelection());
    img = renderOf(mPlot);
    QVERIFY(isRedish(img.pixelColor(bar)));
}

void TestMultiGraph::errorBarsAreDroppedWhenABoundColumnDisappears()
{
    auto* mg = errorBarGraph(mPlot, 3, 1.0);
    mg->setData(std::vector<double>{0, 1}, std::vector<std::vector<double>>{{1, 1}, {0, 0}});
    QVERIFY(!mg->componentErrorBars(0).has_value());
}

void TestMultiGraph::errorBarsRejectBadColumns()
{
    auto* mg = threeColumnGraph(mPlot);
    QVERIFY(!mg->setComponentErrorBars(0, 1, 1));
    QVERIFY(!mg->setComponentErrorBars(0, 0, 2));
    QVERIFY(!mg->setComponentErrorBars(0, 1, 5));
}

void TestMultiGraph::capWidthChangeShowsOnTheNextFrame()
{
    auto* mg = errorBarGraph(mPlot, 5, 1.0);
    mPlot->xAxis->setRange(-0.5, 4.5);
    mPlot->yAxis->setRange(0, 10);
    const QPoint capEdge(int(mPlot->xAxis->coordToPixel(2)) + 8, int(mPlot->yAxis->coordToPixel(7)));
    QVERIFY(!isRedish(renderOf(mPlot).pixelColor(capEdge)));
    mg->setComponentErrorBars(0, 1, 2, 20);
    QVERIFY(isRedish(renderOf(mPlot).pixelColor(capEdge)));
}
```

`toPixmap` exports through `QPainter`, so these run without an RHI. Add one GPU check, skipped without RHI: after `replot(rpImmediateRefresh)`, `mg->mErrorBars.first().bars.isEmpty()` is false for the sparse graph and `band` is non-empty for the dense one.

- [ ] **Step 2: Run** — FAIL (no bars drawn).
- [ ] **Step 3: Implement** the storage, `drawErrorBars` with helpers `errorBarRows(const ErrorBars&, ds, begin, end)` (data → resolved pixel `Bar`s), `drawBarsAsPolylines(...)`, `drawBarsAsBand(...)`, and the legend glyph.
- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestMultiGraph`, then the full NeoQCP suite → PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(multigraph): error bars from columns, a band when dense"`.

---

### Task 9: Column-indexed decorations on SciQLopPlots graphs

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp`, `src/SciQLopMultiGraphBase.cpp` (`sync_components` :104-134)
- Modify: `SciQLopPlots/bindings/bindings.xml` (new template next to `save_raises_oserror` :55-68; `modify-function` entries in the `SciQLopMultiGraphBase` block :296)
- Create test: `tests/integration/test_fills_errorbars.py`

**Interfaces:**
- Consumes: the NeoQCP API of Tasks 1, 4, 8.
- Produces (all on `SciQLopMultiGraphBase`, none `noexcept`, invalid columns raise `std::invalid_argument` → `ValueError`):

```cpp
    void set_fill_to_value(int column, double value);
    void set_fill_between(int column, int other);
    void clear_fill(int column);
    void set_fill_color(int column, const QColor& color);
    void set_error_bars(int column, int lower, int upper, double cap_width = 6.);
    void clear_error_bars(int column);
    //! Styles for components that may not exist yet (a callback graph before its first data):
    //! applied now if the component exists, and again whenever it is created.
    void set_column_line_style(int column, GraphLineStyle style);
    void set_column_marker_shape(int column, GraphMarkerShape shape);
    void set_column_in_legend(int column, bool in_legend);
    //! The preset if one was set, else the component's flag, else true.
    bool column_in_legend(int column) const;
```

Presets: `struct ColumnPreset { std::optional<GraphLineStyle> line_style; std::optional<GraphMarkerShape> marker; std::optional<bool> in_legend; }; std::map<int, ColumnPreset> _column_presets;`. A small `apply_preset(int column)` applies through the component wrapper (`component(column)->set_line_style(...)`, `set_marker_shape(...)`, and `_multiGraph->setComponentInLegend(column, ...)`), called from the setters when `column < plottable_count()` and from `sync_components` for each newly created component. Never call `component(column)` with an out-of-range index: it clamps to the last component (`QCPAbstractPlottableWrapper.hpp:161-168`); check `column < plottable_count()` first.

Bindings template:

```xml
    <template name="void_raises_valueerror">
        try {
            %CPPSELF.%FUNCTION_NAME(%ARGUMENT_NAMES);
        } catch (const std::invalid_argument&amp; e) {
            PyErr_SetString(PyExc_ValueError, e.what());
            return nullptr;
        } catch (const std::exception&amp; e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return nullptr;
        }
    </template>
```

and for each throwing method, e.g.

```xml
        <modify-function signature="set_fill_between(int,int)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_valueerror"/>
            </inject-code>
        </modify-function>
```

(`set_fill_to_value(int,double)`, `set_fill_between(int,int)`, `set_fill_color(int,const QColor&amp;)`, `set_error_bars(int,int,int,double)`). Then the stale-wrapper cleanup from Global Constraints and a rebuild.

- [ ] **Step 1: Failing tests** (`tests/integration/test_fills_errorbars.py`)

```python
"""Fills, bands and error bars (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest
from PySide6.QtGui import QImage

from SciQLopPlots import GraphLineStyle, SciQLopPlot
from conftest import process_events

X = np.linspace(0, 10, 200)


def _render(plot, tmp_path, name="p"):
    plot.resize(600, 400)
    path = str(tmp_path / f"{name}.png")
    plot.save_png(path, 600, 400)
    return QImage(path).convertToFormat(QImage.Format_ARGB32)


def _tinted_red(c):
    # Default fills are the line colour at alpha 64: over a white background that is pink
    # (about 255, 191, 191), so test the red excess, not absolute red.
    return c.red() - c.green() > 30 and c.red() - c.blue() > 30


def _pixel(plot, x, y):
    return int(plot.x_axis().qcp_axis().coordToPixel(x)), int(plot.y_axis().qcp_axis().coordToPixel(y))


class TestColumnDecorations:
    def test_fill_to_value_colours_below_the_line(self, qtbot, plot, tmp_path):
        g = plot.line(X, np.full_like(X, 5.0), colors=["red"])
        g.set_fill_to_value(0, 0.0)
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(-5, 10)
        img = _render(plot, tmp_path)
        assert _tinted_red(img.pixelColor(*_pixel(plot, 5, 2.5)))
        assert not _tinted_red(img.pixelColor(*_pixel(plot, 5, 7.5)))

    def test_bad_columns_raise_value_error(self, plot):
        g = plot.line(X, np.column_stack([X, X]))
        with pytest.raises(ValueError):
            g.set_fill_between(0, 0)
        with pytest.raises(ValueError):
            g.set_fill_between(0, 5)
        with pytest.raises(ValueError):
            g.set_error_bars(0, 1, 1)

    def test_presets_apply_to_components_created_later(self, qtbot, plot):
        def cb(start, stop):
            x = np.linspace(start, stop, 50)
            return x, np.column_stack([np.sin(x), np.sin(x) - 1, np.sin(x) + 1])
        g = plot.line(cb)
        g.set_column_line_style(1, GraphLineStyle.NoLine)
        g.set_column_in_legend(2, False)
        g.set_fill_between(1, 2)
        plot.x_axis().set_range(0, 10)
        qtbot.waitUntil(lambda: len(g.components()) == 3, timeout=5000)
        assert g.components()[1].line_style() == GraphLineStyle.NoLine
```

If `qcp_axis()` is not exposed on axes, compute pixels from the axis rect instead (read `tests/integration/test_api_features.py::_gradient_beside_the_plot_area` for the existing approach) — check before writing the helper.

- [ ] **Step 2: Run** — build, then `pytest tests/integration/test_fills_errorbars.py` → FAIL (`AttributeError: ... set_fill_to_value`).
- [ ] **Step 3: Implement** the methods (each a few lines: validate via the NeoQCP setter's `bool`, `throw std::invalid_argument("...")` with the column numbers in the message, then `Q_EMIT replot()`), the presets, and the bindings.
- [ ] **Step 4: Run** — the new tests PASS; then the full SciQLopPlots suite (Global Constraints) → read the real pass/fail count.
- [ ] **Step 5: Commit (SciQLopPlots)**

```bash
git add include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp src/SciQLopMultiGraphBase.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_fills_errorbars.py
git commit -m "feat(graph): fills, error bars and column presets by column"
```

---

### Task 10: Component-level methods and the inspector row

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp` (after `set_visible`, signals block :155-162)
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`, `src/SciQLopGraphComponent.cpp`
- Modify: `src/SciQLopGraphComponentDelegate.cpp` (and its header) — a "Fill color" row
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopGraphComponentInterface` block :265, `void_raises_valueerror` on the throwing methods)
- Test: `tests/integration/test_fills_errorbars.py`

**Interfaces:**
- Consumes: Task 9's column methods are not used here; the component talks to `QCPMultiGraph` directly through its `MultiGraphRef`.
- Produces on `SciQLopGraphComponentInterface` (virtual; defaults `WARN_ABSTRACT_METHOD;` like the other setters, which raises `NotImplementedError` from Python):

```cpp
    virtual void set_fill_to_value(double value);
    virtual void set_fill_between(SciQLopGraphComponentInterface* other);
    virtual void clear_fill();
    virtual void set_fill_color(const QColor& color);
    virtual bool has_fill() const;
    virtual QColor fill_color() const;
    virtual void set_error_bars(SciQLopGraphComponentInterface* lower,
                                SciQLopGraphComponentInterface* upper, double cap_width = 6.);
    virtual void clear_error_bars();
    virtual int component_index() const noexcept { return -1; }
    Q_SIGNAL void fill_changed();
    Q_SIGNAL void error_bars_changed();
```

`SciQLopGraphComponent` overrides: a helper `std::optional<MultiGraphRef> multi_ref() const` (`to_variant()` holds a `MultiGraphRef`); `QCPGraph`/`QCPCurve` components call `WARN_UNSUPPORTED_FUNCTIONALITY;` and return. `set_fill_between(other)`: `other` must be a `SciQLopGraphComponent` on the same `QCPMultiGraph`, else `throw std::invalid_argument("the other component must belong to the same graph")`. `component_index()` returns `m_componentIndex`. Each setter emits its signal and `replot()`.

Inspector: in `SciQLopGraphComponentDelegate`, a `ColorDelegate` labelled "Fill color", shown only while `has_fill()`, updated on `fill_changed`, writing `set_fill_color`.

- [ ] **Step 1: Failing tests** (append)

```python
class TestComponentDecorations:
    def test_component_fill_between(self, qtbot, plot, tmp_path):
        g = plot.line(X, np.column_stack([np.full_like(X, 3.0), np.full_like(X, 7.0)]),
                      colors=["red", "red"])
        low, high = g.components()
        low.set_fill_between(high)
        assert low.has_fill() and low.component_index() == 0
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(0, 10)
        assert _tinted_red(_render(plot, tmp_path).pixelColor(*_pixel(plot, 5, 5)))
        low.clear_fill()
        assert not low.has_fill()

    def test_fill_between_another_graph_raises(self, plot):
        a = plot.line(X, np.sin(X)).components()[0]
        b = plot.line(X, np.cos(X)).components()[0]
        with pytest.raises(ValueError):
            a.set_fill_between(b)

    def test_curve_components_raise_not_implemented(self, plot):
        from SciQLopPlots import GraphType
        c = plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve).components()[0]
        with pytest.raises(NotImplementedError):
            c.set_fill_to_value(0.0)

    def test_fill_changed_is_emitted(self, qtbot, plot):
        comp = plot.line(X, np.sin(X)).components()[0]
        with qtbot.waitSignal(comp.fill_changed, timeout=1000):
            comp.set_fill_to_value(0.0)
```

- [ ] **Step 2: Run** — rebuild (stale wrapper cleanup: the interface is header-only), run → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — new tests PASS; full SciQLopPlots suite.
- [ ] **Step 5: Commit (SciQLopPlots)** — `git commit -m "feat(component): fill and error-bar methods, inspector fill colour"` with the touched files.

---

### Task 11: `plot.band()` and `plot.error_bars()` (Python factories)

**Files:**
- Modify: `SciQLopPlots/__init__.py` (new helpers after `_reject_histogram2d_kwargs` :143; attach to `SciQLopPlot`, `SciQLopTimeSeriesPlot`, `SciQLopMultiPlotPanel` inside `_patch_sciqlop_plot` or right after it, :234-237)
- Test: `tests/integration/test_fills_errorbars.py`

**Interfaces:**
- Consumes: Task 9's column methods; `cls.line(...)` (plots return a graph, panels return `(plot, graph)`).
- Produces:

```python
plot.band(x, lower, upper, name=None, color=None, **line_kwargs)     # -> graph (panel: (plot, graph))
plot.band(callback, name=None, color=None, **line_kwargs)            # callback(start, stop) -> (x, lower, upper) or None
plot.error_bars(x, y, err, name=None, color=None, cap_width=6.0, **line_kwargs)
plot.error_bars(x, y, err_minus, err_plus, ...)
plot.error_bars(callback, ...)                                        # callback -> (x, y, err) or (x, y, em, ep) or None
```

Code (module level in `__init__.py`):

```python
def _graph_of(result):
    return result[1] if isinstance(result, tuple) else result


def _as_columns(*columns):
    import numpy as np
    return np.column_stack([np.asarray(c, dtype=np.float64) for c in columns])


def _error_bounds(y, err_minus, err_plus=None):
    import numpy as np
    y = np.asarray(y, dtype=np.float64)
    try:
        em = np.broadcast_to(np.asarray(err_minus, dtype=np.float64), y.shape)
        ep = em if err_plus is None else np.broadcast_to(np.asarray(err_plus, dtype=np.float64), y.shape)
    except ValueError:
        raise ValueError(f"errors must be a scalar or match y's length ({len(y)})") from None
    return y - em, y + ep


def _band_batch(x, lower, upper):
    return x, _as_columns(lower, upper)


def _error_batch(x, y, *errors):
    return x, _as_columns(y, *_error_bounds(y, *errors))


def _wrapped_callback(callback, to_batch):
    def wrapped(start, stop):
        batch = callback(start, stop)
        if batch is None or len(batch) == 0:
            return batch
        return to_batch(*batch)
    return wrapped


def _line_data(args, to_batch, arity):
    if len(args) == 1 and callable(args[0]):
        return (_wrapped_callback(args[0], to_batch),)
    if len(args) in arity:
        return to_batch(*args)
    raise TypeError(f"expected a callback or {' or '.join(map(str, arity))} arrays, got {len(args)} arguments")


def _hide_columns(graph, columns, keep_in_legend=()):
    for c in columns:
        graph.set_column_line_style(c, GraphLineStyle.NoLine)
        if c not in keep_in_legend:
            graph.set_column_in_legend(c, False)


def _band(self, *args, name=None, color=None, **kwargs):
    data = _line_data(args, _band_batch, (3,))
    colors = [color, color] if color is not None else []
    result = self.line(*data, labels=[name or "band", "upper"], colors=colors, **kwargs)
    graph = _graph_of(result)
    _hide_columns(graph, (0, 1), keep_in_legend=(0,))
    graph.set_fill_between(0, 1)
    return result


def _error_bars(self, *args, name=None, color=None, cap_width=6.0, **kwargs):
    data = _line_data(args, _error_batch, (3, 4))
    colors = [color] * 3 if color is not None else []
    result = self.line(*data, labels=[name or "y", "lower", "upper"], colors=colors, **kwargs)
    graph = _graph_of(result)
    graph.set_column_line_style(0, GraphLineStyle.NoLine)
    graph.set_column_marker_shape(0, GraphMarkerShape.Circle)
    _hide_columns(graph, (1, 2))
    graph.set_error_bars(0, 1, 2, cap_width)
    return result


for _cls in (SciQLopPlot, SciQLopTimeSeriesPlot, SciQLopMultiPlotPanel):
    _cls.band = _band
    _cls.error_bars = _error_bars
```

Check `GraphLineStyle`, `GraphMarkerShape` are imported at the top of `__init__.py` (they come from the bindings star import) and that `"band"` / `"error_bars"` are not swallowed by `__all__` filtering (`_public_names`, :570 — they are methods, not module names, so nothing to add). These are plain Python methods, not wrappers over Qt slots, so `feedback-no-python-wrappers-on-slots` does not apply.

- [ ] **Step 1: Failing tests** (append)

```python
class TestFactories:
    def test_band_shades_between_lower_and_upper(self, qtbot, plot, tmp_path):
        plot.band(X, np.full_like(X, 3.0), np.full_like(X, 7.0), name="b", color="red")
        plot.x_axis().set_range(0, 10)
        plot.y_axis().set_range(0, 10)
        img = _render(plot, tmp_path)
        assert _tinted_red(img.pixelColor(*_pixel(plot, 5, 5)))
        assert not _tinted_red(img.pixelColor(*_pixel(plot, 5, 9)))

    def test_band_is_one_legend_row(self, plot):
        g = plot.band(X, X - 1, X + 1, name="b")
        assert g.name() == "b"
        assert g.column_in_legend(0) and not g.column_in_legend(1)

    def test_band_callback_passes_empty_batches_through(self, qtbot, plot):
        calls = []
        def cb(start, stop):
            calls.append((start, stop))
            if len(calls) == 1:
                return None
            x = np.linspace(start, stop, 20)
            return x, x - 1, x + 1
        g = plot.band(cb)
        plot.x_axis().set_range(0, 10)
        plot.x_axis().set_range(0, 11)
        qtbot.waitUntil(lambda: len(g.components()) == 2, timeout=5000)

    def test_error_bars_symmetric_and_asymmetric(self, plot):
        x = np.arange(5.0)
        g = plot.error_bars(x, x, 0.5)
        assert len(g.components()) == 3
        lo, hi = g.data()[1][:, 1], g.data()[1][:, 2]
        np.testing.assert_allclose(hi - lo, 1.0)
        g2 = plot.error_bars(x, x, 0.2, np.full(5, 0.7))
        np.testing.assert_allclose(g2.data()[1][:, 2] - x, 0.7)

    def test_error_bars_length_mismatch_raises(self, plot):
        with pytest.raises(ValueError):
            plot.error_bars(np.arange(5.0), np.arange(5.0), np.ones(3))

    def test_error_bars_callback_reaches_three_columns(self, qtbot, plot):
        g = plot.error_bars(lambda a, b: (np.linspace(a, b, 10), np.ones(10), 0.1))
        plot.x_axis().set_range(0, 10)
        qtbot.waitUntil(lambda: len(g.components()) == 3, timeout=5000)

    def test_panel_band_returns_plot_and_graph(self, qtbot, panel):
        p, g = panel.band(X, X - 1, X + 1)
        assert g is not None and p is not None

    def test_bad_arity_raises_type_error(self, plot):
        with pytest.raises(TypeError):
            plot.band(X, X)
```

- [ ] **Step 2: Run** — FAIL (`AttributeError: 'SciQLopPlot' object has no attribute 'band'`).
- [ ] **Step 3: Implement** the code above.
- [ ] **Step 4: Run** — new tests PASS; full SciQLopPlots suite.
- [ ] **Step 5: Commit (SciQLopPlots)** — `git commit -m "feat(plot): band() and error_bars() factories"`.

---

### Task 12: Guide, full verification, reviews

**Files:**
- Modify: `docs/user-guide.md` (a "Bands, fills and error bars" section next to the line-graph section; snippets there are executed by the docs tests, so they must run)
- Modify: `docs/api-review-2026-10-20.md` (Progress: task 14 done)

- [ ] **Step 1:** Write the guide section: `plot.band`, `set_fill_to_value`, `set_fill_between` (with the "same graph" rule and the virtual-product advice for two spacecraft), the uncertainty recipe (`[y, y-e, y+e]` + `set_fill_between(1, 2)`), `plot.error_bars` (and that they turn into a band when zoomed out), and the caveat that a downsampled band spans between the drawn lines.
- [ ] **Step 2:** Full NeoQCP suite (`meson test -C build`), then the full SciQLopPlots suite (both integration halves), one at a time. Read the real counts.
- [ ] **Step 3:** SciQLop's suite and the four plugins against `build-venv` (memory `reference-run-sciqlop-suite-against-build`). Expected: no change in failures.
- [ ] **Step 4:** opencode code review of the NeoQCP and SciQLopPlots commits (memory `feedback-opencode-second-opinion`, `feedback-opencode-auto`): read-only, report to a file, verify each finding against the code before acting.
- [ ] **Step 5:** Commit docs; update the API-review tracker (Claude Doc row 14 → Done) and the project memory. Do not push.
