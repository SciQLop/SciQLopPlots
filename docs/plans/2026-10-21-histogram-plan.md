# 1-D Histogram Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A 1-D histogram that bins values in the background (shared edges across columns, stable while panning), pre-binned bars, and the factories `plot.histogram()`, `plot.bars()`, `panel.histogram()` that follow the panel's time range.

**Architecture:** NeoQCP gets pure binning functions (`qcp::algo::bin1d` & co.) and a `QCPHistogram` plottable on `QCPAsyncPipeline<QCPAbstractMultiDataSource, qcp::algo::Histogram1D>`, modelled on `QCPHistogram2D`. SciQLopPlots wraps it as `SciQLopHistogram` (+ `Function`, `Remote`), modelled on `SciQLopHistogram2D`; callback histograms observe the plot's hidden time axis instead of its x axis.

**Tech Stack:** C++20, Qt 6.11.2 (QRhi), NeoQCP, Shiboken6, pytest + pytest-qt, QtTest.

**Spec:** `docs/plans/2026-10-21-bands-errorbars-histogram-design.md`, section "1-D histogram". Independent of `docs/plans/2026-10-21-fills-errorbars-plan.md`; either can go first.

## Global Constraints

- One build or test run at a time, in the foreground, waited out. Never background a build. `-j 4` maximum. This applies to subagents too.
- SciQLopPlots build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`.
- SciQLopPlots tests from `/tmp`: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`. Full suite = `tests/unit tests/fuzzing tests/resampler tests/tracing` plus `tests/integration` in two halves (each under 590 s). Never `tests/`.
- NeoQCP: `cd subprojects/NeoQCP && ../../.venv/bin/meson compile -C build -j 4 && QT_QPA_PLATFORM=offscreen QT_LOGGING_RULES="*.debug=true" QCP_TEST_CLASS=<TestClass> ./build/tests/auto/auto-tests`; full: `../../.venv/bin/meson test -C build` with the same environment.
- New bound classes, enums or members: `rm build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.cpp build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.h && .venv/bin/meson setup --reconfigure build-venv` before building.
- NeoQCP commits in `subprojects/NeoQCP`, SciQLopPlots commits in the top repo, explicit pathspecs (untracked `qt.ini` and `subprojects/NeoQCP/subprojects/jkqtmathtext/` are not ours). No push without the user's explicit consent.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- `std::invalid_argument` becomes `ValueError` only through a `bindings.xml` injection (template `void_raises_valueerror`; if the fills plan has not added it yet, add it as in that plan's Task 9: a `try { %CPPSELF.%FUNCTION_NAME(%ARGUMENT_NAMES); }` with `std::invalid_argument` → `PyExc_ValueError`, other `std::exception` → `PyExc_RuntimeError`).
- Spec values: default `bins = 64`; `Normalization {Count, Density, Probability}`; `Style {Bars, Step}`; default style `Bars` for one column, `Step` + light fill (alpha 64) for several unless `style` is given; layer `Main`; `log_bins=True` sets the x axis to log.

## Review Focus

1. **An empty fetch** (callback returns zero-length arrays, or every value is NaN) after a non-empty one: the histogram shows no bars, auto-scale does not crash or jump to 0..1, and the next non-empty batch shows again. Test in Task 3.
2. **A single repeated value** (flat data, e.g. a saturated sensor): one visible bar around the value, not a zero-width range or a division by zero in `Density`. Test in Task 2.
3. **`range=` given and most values outside it**: those values are not counted (like `numpy.histogram`), not piled into the edge bins by `BinAxis::index`'s clamp. Test in Task 2.
4. **Integer `y` from the callback** (counts, flags): binned like floats. Test in Task 5.
5. **A histogram in a panel next to time series**: panning the time series refetches the histogram's data, and the histogram's own x axis (values) is not dragged along by the panel's time synchronisation. Test in Task 6.

---

### Task 1: One hidden time axis on projection plots

`SciQLopPlot`'s constructor already creates `m_time_axis` (`src/SciQLopPlot.cpp:843-845`). `SciQLopNDProjectionPlot`'s constructor makes a second one and orphans the first (`src/SciQLopNDProjectionPlot.cpp:97-99`). The histogram relies on this axis, so fix it first.

**Files:**
- Modify: `src/SciQLopNDProjectionPlot.cpp:97-99`
- Test: `tests/integration/test_projection_time_axis.py` (create)

- [ ] **Step 1: Failing test**

```python
"""The projection plot has exactly one hidden time axis (found while designing the histogram)."""
from PySide6.QtCore import Qt

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlotDummyAxis


def test_projection_plot_has_one_time_axis(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    own = proj.findChildren(SciQLopPlotDummyAxis, options=Qt.FindDirectChildrenOnly)
    assert len(own) == 1
    assert own[0] is proj.time_axis()


def test_projection_time_axis_still_drives_the_signal(qtbot):
    proj = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(proj)
    with qtbot.waitSignal(proj.time_axis_range_changed, timeout=1000):
        proj.time_axis().set_range(0.0, 10.0)
```

If `SciQLopNDProjectionPlot` is not the right class name in Python or `SciQLopPlotDummyAxis` is not bound, check `bindings.xml` and adapt the test to the bound names first.

- [ ] **Step 2: Run** — the first test FAILS with 2 children.
- [ ] **Step 3: Implement** — delete the three lines; the base class's axis and connection remain. Check the projection's own uses of `m_time_axis` (grep `m_time_axis` in `src/SciQLopNDProjectionPlot.cpp`): it must be the base member, not a shadowing one in `SciQLopNDProjectionPlot.hpp:51`. If the header redeclares `m_time_axis`, remove that declaration too.
- [ ] **Step 4: Run** — PASS; then the projection tests (`pytest tests/integration -k projection`).
- [ ] **Step 5: Commit** — `git commit -m "fix(projection): one hidden time axis, not two"`.

---

### Task 2: 1-D binning, pure functions (NeoQCP)

**Files:**
- Create: `subprojects/NeoQCP/src/datasource/histogram-binner-1d.h` (header-only, like `histogram-binner.h`)
- Create test: `subprojects/NeoQCP/tests/auto/test-histogram1d/test-histogram1d.{h,cpp}`; register in `tests/auto/meson.build` and `autotest.cpp` (`QCPTEST(TestHistogram1D);`)

**Interfaces:**
- Consumes: `qcp::algo::BinAxis`, `qcp::algo::expandIfFlat` (`src/datasource/histogram-binner.h`), `QCPAbstractMultiDataSource`.
- Produces (namespace `qcp::algo`):

```cpp
struct Histogram1D {
    std::vector<double> edges;                 // bins + 1, increasing
    std::vector<std::vector<double>> heights;  // one vector of `bins` per column
    std::vector<int> counted;                  // values counted per column (for normalization)
    bool log = false;
};
enum class HistogramNormalization { Count, Density, Probability };

//! Finite values of every column (positive only when positiveOnly); nullopt when there are none.
std::optional<QCPRange> finiteValueBounds(const QCPAbstractMultiDataSource& src, bool positiveOnly);
//! bins + 1 edges, evenly spaced in linear or log10 space.
std::vector<double> binEdges(QCPRange range, int bins, bool log);
//! Counts finite values inside [range.lower, range.upper] of every column into shared bins.
//! Values outside the range are not counted. Log: non-positive values are skipped.
Histogram1D bin1d(const QCPAbstractMultiDataSource& src, QCPRange range, int bins, bool log);
//! Count: unchanged. Probability: count / N. Density: count / (N * width of that bin).
void normalize(Histogram1D& h, HistogramNormalization norm);
//! Auto-range policy: the current range if bounds fit inside it, else their union (edges only widen).
QCPRange widenedRange(std::optional<QCPRange> current, QCPRange bounds);
```

- [ ] **Step 1: Failing tests**

```cpp
#include "test-histogram1d.h"
#include "datasource/histogram-binner-1d.h"
#include "datasource/soa-multi-datasource.h"

using namespace qcp::algo;
namespace {
using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
Src source(std::vector<std::vector<double>> columns)
{
    std::vector<double> keys(columns.front().size());
    for (std::size_t i = 0; i < keys.size(); ++i) keys[i] = double(i);
    return Src(std::move(keys), std::move(columns));
}
constexpr double nan = std::numeric_limits<double>::quiet_NaN();
}

void TestHistogram1D::columnsShareEdges()
{
    const auto src = source({{0, 1, 2, 3}, {0, 0, 0, 3}});
    const auto h = bin1d(src, QCPRange(0, 4), 4, false);
    QCOMPARE(h.edges.size(), std::size_t(5));
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1, 1, 1}));
    QCOMPARE(h.heights[1], (std::vector<double>{3, 0, 0, 1}));
}

void TestHistogram1D::nanAndInfinityAreSkipped()
{
    const auto src = source({{1, nan, std::numeric_limits<double>::infinity(), 1}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, false);
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::valuesOutsideAFixedRangeAreNotCounted()
{
    const auto src = source({{-10, 0.5, 1.5, 10}});
    const auto h = bin1d(src, QCPRange(0, 2), 2, false);
    QCOMPARE(h.heights[0], (std::vector<double>{1, 1}));
    QCOMPARE(h.counted[0], 2);
}

void TestHistogram1D::logBinsSkipNonPositiveValues()
{
    const auto src = source({{-1, 0, 1, 10, 100}});
    const auto h = bin1d(src, QCPRange(1, 100), 2, true);
    QCOMPARE(h.edges[1], 10.0);
    QCOMPARE(h.counted[0], 3);
}

void TestHistogram1D::flatDataGetsAVisibleBar()
{
    const auto src = source({{5, 5, 5}});
    auto bounds = finiteValueBounds(src, false);
    QVERIFY(bounds.has_value());
    expandIfFlat(*bounds, false);
    const auto h = bin1d(src, *bounds, 4, false);
    double total = 0;
    for (double v : h.heights[0]) total += v;
    QCOMPARE(total, 3.0);
}

void TestHistogram1D::normalizationSums()
{
    const auto src = source({{0.5, 1.5, 1.5, 3.5}});
    auto probability = bin1d(src, QCPRange(0, 4), 4, false);
    normalize(probability, HistogramNormalization::Probability);
    double p = 0;
    for (double v : probability.heights[0]) p += v;
    QCOMPARE(p, 1.0);

    auto density = bin1d(src, QCPRange(0, 8), 4, false); // width 2
    normalize(density, HistogramNormalization::Density);
    double integral = 0;
    for (std::size_t b = 0; b < 4; ++b)
        integral += density.heights[0][b] * (density.edges[b + 1] - density.edges[b]);
    QCOMPARE(integral, 1.0);
}

void TestHistogram1D::densityUsesEachLogBinsOwnWidth()
{
    const auto src = source({{2, 20}});
    auto h = bin1d(src, QCPRange(1, 100), 2, true); // widths 9 and 90
    normalize(h, HistogramNormalization::Density);
    QCOMPARE(h.heights[0][0], 1.0 / (2 * 9));
    QCOMPARE(h.heights[0][1], 1.0 / (2 * 90));
}

void TestHistogram1D::emptyOrAllNanHasNoBounds()
{
    QVERIFY(!finiteValueBounds(source({{nan, nan}}), false).has_value());
}

void TestHistogram1D::edgesOnlyWiden()
{
    QCOMPARE(widenedRange(QCPRange(0, 10), QCPRange(2, 8)), QCPRange(0, 10));
    QCOMPARE(widenedRange(QCPRange(0, 10), QCPRange(-5, 8)), QCPRange(-5, 10));
    QCOMPARE(widenedRange(std::nullopt, QCPRange(2, 8)), QCPRange(2, 8));
}
```

- [ ] **Step 2: Run** — compile error (missing header). FAIL.
- [ ] **Step 3: Implement.** `bin1d`: `const auto axis = BinAxis::make(range, bins, log);` then per column, per row: skip `!std::isfinite(v)`, `log && v <= 0`, `v < range.lower || v > range.upper` (this check is what keeps `BinAxis::index`'s clamp from piling outside values into the edge bins); `++heights[c][axis.index(v)]; ++counted[c]`. `binEdges` in log: `10^(log10(lo) + i * span / bins)`. `normalize`: divide by `counted[c]` (skip columns with 0) and for Density also by `edges[b+1] - edges[b]`. `finiteValueBounds`: min/max over all columns with the same skips.
- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestHistogram1D` → PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(histogram): 1-D binning with shared edges"`.

---

### Task 3: `QCPHistogram` plottable: data, background binning, edges, ranges (NeoQCP)

**Files:**
- Create: `subprojects/NeoQCP/src/plottables/plottable-histogram.h`, `.cpp`; add to `subprojects/NeoQCP/meson.build` and to the umbrella header that includes `plottable-histogram2d.h` (grep for it: `qcustomplot.h` or `src/plottables.h`)
- Modify: `subprojects/NeoQCP/src/datasource/async-pipeline.h:229-234` (one alias)
- Create test: `subprojects/NeoQCP/tests/auto/test-histogram1d/test-histogram-plottable.{h,cpp}` (`TestHistogramPlottable`, registered like Task 2)

**Interfaces:**
- Consumes: Task 2.
- Produces:

```cpp
using QCPHistogram1DPipeline = QCPAsyncPipeline<QCPAbstractMultiDataSource, qcp::algo::Histogram1D>;

class QCP_LIB_DECL QCPHistogram : public QCPAbstractPlottable
{
    Q_OBJECT
public:
    enum Style { sBars, sStep };
    explicit QCPHistogram(QCPAxis* keyAxis, QCPAxis* valueAxis);
    ~QCPHistogram() override;

    void setDataSource(std::shared_ptr<QCPAbstractMultiDataSource> source);   // values to bin
    void setBinned(std::vector<double> edges, std::vector<std::vector<double>> heights); // no binning
    void setBins(int bins);                             // <= 0 ignored (SciQLopPlots raises first)
    void setFixedRange(std::optional<QCPRange> range);  // nullopt = auto, edges only widen
    void setNormalization(qcp::algo::HistogramNormalization norm);
    void setStyle(Style style);
    void setColumnColors(const QList<QColor>& colors);
    void resetEdges();                                  // forget the auto range (next data sets it)
    [[nodiscard]] const qcp::algo::Histogram1D* histogram() const; // the drawn result, or nullptr
    QCPHistogram1DPipeline& pipeline();

    QCPRange getKeyRange(bool& found, QCP::SignDomain = QCP::sdBoth) const override;
    QCPRange getValueRange(bool& found, QCP::SignDomain = QCP::sdBoth, const QCPRange& = QCPRange()) const override;
    double selectTest(const QPointF& pos, bool onlySelectable, QVariant* details = nullptr) const override;

Q_SIGNALS:
    void edgesChanged();
    void histogramReady();   // after every new result, binned or pre-binned
protected:
    void draw(QCPPainter* painter) override;            // Task 4
    void drawLegendIcon(QCPPainter* painter, const QRectF& rect) const override; // Task 4
    bool pipelineBusy() const override;
    bool canProduceContent() const override;
private:
    std::shared_ptr<QCPAbstractMultiDataSource> mDataSource;
    std::shared_ptr<qcp::algo::Histogram1D> mResult;    // written by both paths; draw reads only this
    std::optional<QCPRange> mFixedRange, mAutoRange;
    int mBins = 64;
    qcp::algo::HistogramNormalization mNormalization = qcp::algo::HistogramNormalization::Count;
    Style mStyle = sBars;
    QList<QColor> mColors;
    QCPHistogram1DPipeline mPipeline;
    void installTransform();
    void takeResult(std::shared_ptr<qcp::algo::Histogram1D> result);
};
```

Rules:
- `installTransform` mirrors `QCPHistogram2D::installTransform` (`plottable-histogram2d.cpp:50-69`): a `TransformKind::ViewportIndependent` lambda capturing **by value** `bins`, `log` (= key axis is logarithmic), `normalization`, `fixed` (`mFixedRange`) and `autoRange` (`mAutoRange`). The job runs on a worker thread, so it never reads members. `takeResult` updates `mAutoRange` and calls `installTransform()` again, on the GUI thread; `setTransform` does not restart the pipeline (`async-pipeline.h:123-127`), so this cannot loop. A job already running with the old range is harmless: its result is widened again by the next one. The lambda computes `range = fixed ? *fixed : widenedRange(autoRange, *finiteValueBounds(src, log))` (no bounds → return `nullptr`), `expandIfFlat(range, log)`, `bin1d`, `normalize`.
- `takeResult(nullptr)` (no finite value: the transform returned nothing) resets `mResult`, emits `histogramReady()`, leaves `mAutoRange` and `edgesChanged` alone.
- On `pipeline().finished`, `takeResult(pipeline().result())`: when the edges differ from the previous result's, store the range in `mAutoRange` (auto mode only) and emit `edgesChanged()`; always emit `histogramReady()` and `mLayer->replot`-mark like `QCPHistogram2D` does.
- `setBinned` validates `edges.size() == heights[c].size() + 1` for every column (else ignore and `qWarning`), builds a `Histogram1D` directly and calls `takeResult`. It never touches the pipeline: with no transform, `pipeline().result()` returns nothing (`async-pipeline.h:139-152`), which is why `draw` reads `mResult`.
- Any setter that changes binning (`setBins`, `setFixedRange`, `setNormalization`, `resetEdges`, key axis scale change) reinstalls the transform and restarts the pipeline (mirror `QCPHistogram2D::refreshBinning`, connected to the key axis `scaleTypeChanged`).
- Export (`painter->modes().testFlag(QCPPainter::pmNoCaching)`) with no `mResult` and a data source: `mPipeline.runSynchronously(...)` then `takeResult` (mirror `QCPHistogram2D::draw`, `plottable-histogram2d.cpp:284-342`).
- `getKeyRange`: `edges.front()..edges.back()`, `found = false` without a result. `getValueRange`: `0..max(height)`; with `inSignDomain == sdPositive` (log value axis), lower = smallest positive height; `found = false` when there is no result or every height is 0 on a log axis. Never dereference a null `mResult`.
- `selectTest`: returns -1 in this task; Task 4 implements it with `barRects` (distance 0 inside a bar of any column).

- [ ] **Step 1: Failing tests**

```cpp
#include "test-histogram-plottable.h"
#include "qcustomplot.h"
#include "plottables/plottable-histogram.h"
#include "datasource/soa-multi-datasource.h"

namespace {
using Src = QCPSoAMultiDataSource<std::vector<double>, std::vector<double>>;
std::shared_ptr<Src> values(std::vector<double> v)
{
    std::vector<double> keys(v.size());
    for (std::size_t i = 0; i < keys.size(); ++i) keys[i] = double(i);
    return std::make_shared<Src>(std::move(keys), std::vector<std::vector<double>>{std::move(v)});
}
bool waitForResult(QCPHistogram* h)
{
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    return ready.count() > 0 || ready.wait(5000);
}
}

void TestHistogramPlottable::init() { mPlot = new QCustomPlot(); mPlot->resize(400, 300); }
void TestHistogramPlottable::cleanup() { delete mPlot; mPlot = nullptr; }

void TestHistogramPlottable::binsInTheBackground()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBins(4);
    h->setDataSource(values({0, 1, 2, 3, 4}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->heights[0].size(), std::size_t(4));
}

void TestHistogramPlottable::preBinnedNeedsNoPipeline()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2}, {{3, 5}});
    QVERIFY(h->histogram());
    bool found = false;
    QCOMPARE(h->getValueRange(found).upper, 5.0);
    QVERIFY(found);
}

void TestHistogramPlottable::preBinnedWithBadEdgesIsIgnored()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1}, {{3, 5}});
    QVERIFY(!h->histogram());
}

void TestHistogramPlottable::autoEdgesStayWhenNewDataFitsInside()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    const auto first = h->histogram()->edges;
    QSignalSpy moved(h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({2, 8}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges, first);
    QCOMPARE(moved.count(), 0);
}

void TestHistogramPlottable::autoEdgesWidenForDataOutside()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    QSignalSpy moved(h, &QCPHistogram::edgesChanged);
    h->setDataSource(values({-5, 8}));
    QVERIFY(waitForResult(h));
    QCOMPARE(h->histogram()->edges.front(), -5.0);
    QCOMPARE(moved.count(), 1);
}

void TestHistogramPlottable::emptyDataAfterDataHasNoRangeAndNoCrash()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 10}));
    QVERIFY(waitForResult(h));
    QSignalSpy ready(h, &QCPHistogram::histogramReady);
    h->setDataSource(values({std::numeric_limits<double>::quiet_NaN()}));
    QVERIFY(ready.wait(5000));
    QVERIFY(!h->histogram());
    bool found = true;
    h->getValueRange(found);
    QVERIFY(!found);
    mPlot->rescaleAxes();
    mPlot->replot(QCustomPlot::rpImmediateRefresh);
    h->setDataSource(values({1, 2}));
    QVERIFY(waitForResult(h));
    QVERIFY(h->histogram());
}

void TestHistogramPlottable::exportBinsSynchronously()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setDataSource(values({0, 1, 2, 3}));
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    QVERIFY(h->histogram()); // drawn without waiting for the pipeline
    Q_UNUSED(img);
}

void TestHistogramPlottable::logValueAxisRangeSkipsZeroHeights()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{0, 4, 16}});
    bool found = false;
    const QCPRange r = h->getValueRange(found, QCP::sdPositive);
    QVERIFY(found);
    QCOMPARE(r.lower, 4.0);
}
```

- [ ] **Step 2: Run** — compile error, then (once the class exists) every test except the two pre-binned ones FAILS. `preBinnedNeedsNoPipeline` and `preBinnedWithBadEdgesIsIgnored` are contract tests: green as soon as `setBinned` exists.
- [ ] **Step 3: Implement** per the rules. `draw()` and `drawLegendIcon()` stay empty in this commit; Task 4 fills them in next. Nothing outside NeoQCP's tests uses `QCPHistogram` until then.
- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestHistogramPlottable` → PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(histogram): QCPHistogram, background binning and stable edges"`.

---

### Task 4: Drawing the histogram (NeoQCP)

**Files:**
- Modify: `subprojects/NeoQCP/src/plottables/plottable-histogram.cpp`
- Create: `subprojects/NeoQCP/src/plottables/histogram-geometry.h` (pure helpers)
- Test: `test-histogram-plottable.{h,cpp}`

**Interfaces:**
- Produces (namespace `qcp::histogram`):

```cpp
//! One column's bars as pixel rectangles; zero-height bars on a log value axis have
//! top == base (they are left out of the outline).
struct BarRect { double left, right, base, top; };
std::vector<BarRect> barRects(const qcp::algo::Histogram1D& h, int column,
                              const QCPAxis* keyAxis, const QCPAxis* valueAxis);
//! Bars: one closed 5-point run per bar, NaN-separated (no diagonals between bars).
QVector<QPointF> barOutlines(std::span<const BarRect> bars, bool keyIsVertical);
//! Step: the top profile only, one run per gap-free stretch of bars.
QVector<QPointF> stepOutline(std::span<const BarRect> bars, bool keyIsVertical);
//! Triangles (6 floats per vertex) filling each bar.
void appendBarQuads(std::span<const BarRect> bars, bool keyIsVertical,
                    const std::array<float, 4>& rgba, std::vector<float>& out);
```

Rules:
- Base pixel: `valueAxis->coordToPixel(0)`; on a log value axis the axis-rect edge pushed out by `2 × axisRect height` (same rule as the fills plan, Task 5, rule 2; read `QCPGraph::getFillBasePoint`, `plottable-graph.cpp:1702-1760`).
- Colours: `mColors[c]`, defaulting to the theme's plottable colour cycle (look at how `QCPMultiGraph` picks `sDefaultColors`). `Bars`: fill = colour at alpha 255 for one column, outline = colour darkened by 20 %. `Step` (default for several columns): fill = colour at alpha 64, outline = colour.
- GPU: one `prl->addPlottable(fillVerts, strokeVerts, clipRect(), dpr, outHeight, 0, 0, painter->opacity())` per column, `strokeVerts` from `QCPLineExtruder::extrudePolyline(outline, penWidth, colour)`. Export / no RHI: `painter->drawRect` per bar and `drawPolyline` per run (split at NaN with `qcp::drawPolylineSplitNaN` if exposed, else split locally).
- Legend icon: one swatch per column across `rect`'s width (like `QCPMultiGraph::drawLegendIcon` splits by component).
- `selectTest`: distance 0 when `pos` is inside any column's `barRects`, else -1 (`onlySelectable && !mSelectable` → -1). Test it: a click inside a bar hits, a click above it misses.
- Outlines are extruded every frame without a cache. Add `// simplify: re-extruded per frame; fine for the hundreds of bins histograms use. Cache with qcp::ExtrusionCache (drawPolylineCached) if a 10^5-bin case appears.`

- [ ] **Step 1: Failing tests**

```cpp
static bool isRedish(QColor c) { return c.red() > 150 && c.green() < 120 && c.blue() < 120; }

void TestHistogramPlottable::barsAreDrawnUpToTheirHeight()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setColumnColors({Qt::red});
    h->setBinned({0, 1, 2}, {{3, 6}});
    mPlot->xAxis->setRange(0, 2);
    mPlot->yAxis->setRange(0, 8);
    const QImage img = mPlot->toPixmap(400, 300).toImage();
    const auto px = [&](double x, double y) {
        return img.pixelColor(int(mPlot->xAxis->coordToPixel(x)), int(mPlot->yAxis->coordToPixel(y)));
    };
    QVERIFY(isRedish(px(0.5, 2)));
    QVERIFY(!isRedish(px(0.5, 4.5)));
    QVERIFY(isRedish(px(1.5, 4.5)));
}

void TestHistogramPlottable::outlineRunsDoNotJoinBars()
{
    const std::vector<qcp::histogram::BarRect> bars { {0, 10, 100, 50}, {10, 20, 100, 20} };
    const auto runs = qcp::histogram::barOutlines(bars, false);
    QCOMPARE(runs.size(), 5 + 1 + 5);
    QVERIFY(std::isnan(runs[5].x()));
}

void TestHistogramPlottable::stepOutlineIsTheTopProfile()
{
    const std::vector<qcp::histogram::BarRect> bars { {0, 10, 100, 50}, {10, 20, 100, 20} };
    const auto run = qcp::histogram::stepOutline(bars, false);
    QCOMPARE(run.first(), QPointF(0, 100));
    QCOMPARE(run.last(), QPointF(20, 100));
    QVERIFY(run.contains(QPointF(10, 50)) && run.contains(QPointF(10, 20)));
}

void TestHistogramPlottable::emptyBinsOnALogAxisStayOutOfTheOutline()
{
    auto* h = new QCPHistogram(mPlot->xAxis, mPlot->yAxis);
    h->setBinned({0, 1, 2, 3}, {{4, 0, 4}});
    mPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);
    mPlot->yAxis->setRange(1, 10);
    const auto bars = qcp::histogram::barRects(*h->histogram(), 0, mPlot->xAxis, mPlot->yAxis);
    QCOMPARE(bars[1].top, bars[1].base);
    for (const QPointF& p : qcp::histogram::stepOutline(bars, false))
        QVERIFY(std::isnan(p.x()) || std::isfinite(p.y()));
}
```

- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — `QCP_TEST_CLASS=TestHistogramPlottable`, then the full NeoQCP suite → PASS.
- [ ] **Step 5: Commit (NeoQCP)** — `git commit -m "feat(histogram): bars and step drawing, GPU and export"`.

---

### Task 5: `SciQLopHistogram` wrappers, enums, bindings (SciQLopPlots)

**Files:**
- Create: `include/SciQLopPlots/Plotables/SciQLopHistogram.hpp`, `src/SciQLopHistogram.cpp`; add the `.cpp` and the header to `SciQLopPlots/meson.build` next to `SciQLopHistogram2D`
- Modify: `include/SciQLopPlots/enums.hpp` (`HistogramNormalization`, `HistogramStyle`, `GraphType::Histogram`), `include/SciQLopPlots/qcp_enums.hpp` (conversions `to_qcp(HistogramNormalization) -> qcp::algo::HistogramNormalization` and `to_qcp(HistogramStyle) -> QCPHistogram::Style`, next to the existing `GraphLineStyle` conversions there)
- Modify: `SciQLopPlots/bindings/bindings.h` (include), `bindings.xml` (object types, enums, `void_raises_valueerror` on `set_bins` and `set_bin_range`)
- Test: `tests/integration/test_histogram1d.py` (create)

**Interfaces:**
- Consumes: `QCPHistogram` (Tasks 3-4).
- Produces:

```cpp
enum class HistogramNormalization { Count, Density, Probability };
enum class HistogramStyle { Bars, Step };

class SciQLopHistogram : public SciQLopGraphInterface
{
    Q_OBJECT
public:
    explicit SciQLopHistogram(QCustomPlot* parent, SciQLopPlotAxis* xAxis, SciQLopPlotAxis* yAxis,
                              const QString& name, int bins = 64, QVariantMap metaData = {});
    Q_SLOT void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override; // y binned, x ignored
    void set_values(SciQLopPyBuffer values);                             // plot.histogram(values)
    void set_binned(SciQLopPyBuffer edges, SciQLopPyBuffer heights);     // plot.bars(...)
    void set_bins(int bins);                                             // <= 0: invalid_argument
    int bins() const noexcept;
    //! Not set_range: SciQLopFunctionGraph observers drive a set_range(SciQLopPlotRange) slot
    //! with the fetch window; the bin range is a different thing.
    void set_bin_range(double lower, double upper);                      // lower >= upper, or log with lower <= 0: invalid_argument
    void clear_bin_range();
    void reset_edges();                                                  // forget auto edges
    QList<double> bin_edges() const;                                     // empty before the first result
    QList<QList<double>> bin_heights() const;                            // one list per column
    void set_log_bins(bool log);                                         // sets the x axis to log
    void set_normalization(HistogramNormalization n);
    void set_style(HistogramStyle s);
    QCPHistogram* histogram() const;
    Q_SIGNAL void bins_changed();                                        // edges moved
};
class SciQLopHistogramFunction : public SciQLopHistogram, public SciQLopFunctionGraph { ... };
class SciQLopHistogramRemote : public SciQLopHistogram, public SciQLopRemoteGraph { ... };
```

Model the class on two existing ones: `SciQLopHistogram2D` (`include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp`, `src/SciQLopHistogram2D.cpp`: `set_data` dtype dispatch into a span-backed source kept alive by an aliased `shared_ptr`, first-result rescale through a `SingleShotConnection`, destructor removal, the `Function`/`Remote` variants, `set_busy` in the remote variant) and `SciQLopNDProjectionCurves` (a `SciQLopGraphInterface` subclass: which virtuals a graph must implement). Differences:
- `set_values(values)` has no x: it synthesises keys `0..N-1` (a `std::vector<double>` kept in the data holder), because a multi source needs sorted keys for `findBegin`/`keyRange`.
- The source is a `QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>` over `y` (1-D: one column; 2-D row-major: build column spans or use `QCPRowMajorMultiDataSource` exactly like `SciQLopMultiGraphBase::build_data_source`, `src/SciQLopMultiGraphBase.cpp:46-102`). Keys = `x` (ignored by the binning, needed by the source type).
- Rescale on `histogramReady` the first time, and on every `edgesChanged` (connect to `request_rescale`), not only once.
- Layer `Constants::LayersNames::Main`.
- `set_log_bins(on)` calls `_keyAxis->set_log(on)`; the NeoQCP plottable re-bins on the axis scale change (wire the wrapper's `log_changed` like `SciQLopHistogram2D` does, because `SciQLopPlotAxis::set_log` blocks `scaleTypeChanged`).

- [ ] **Step 1: Failing tests**

```python
"""1-D histogram (docs/plans/2026-10-21-bands-errorbars-histogram-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (HistogramNormalization, HistogramStyle, SciQLopHistogram,
                          SciQLopPlot)


class TestHistogramClass:
    def test_bins_must_be_positive(self, plot):
        h = plot.histogram(np.random.default_rng(0).normal(size=100))
        with pytest.raises(ValueError):
            h.set_bins(0)

    def test_bad_range_raises(self, plot):
        h = plot.histogram(np.arange(10.0))
        with pytest.raises(ValueError):
            h.set_bin_range(5.0, 1.0)
        h.set_log_bins(True)
        with pytest.raises(ValueError):
            h.set_bin_range(0.0, 10.0)

    def test_log_bins_set_the_x_axis_to_log(self, plot):
        h = plot.histogram(np.arange(1.0, 100.0))
        h.set_log_bins(True)
        assert plot.x_axis().log()

    def test_integer_values_are_binned(self, qtbot, plot):
        h = plot.histogram(np.array([1, 1, 2, 3], dtype=np.int32))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
        assert sum(h.bin_heights()[0]) == 4
```

- [ ] **Step 2: Run** — build (wrapper cleanup), tests FAIL (`ImportError: HistogramNormalization`).
- [ ] **Step 3: Implement** the class, the enums (registered like `GraphLineStyle` in `bindings.xml`), the factories' C++ side is Task 6 — for this task's tests to run, add only `SciQLopPlot::histogram(const SciQLopPyBuffer& values, ...)` (Task 6 adds the rest).
- [ ] **Step 4: Run** — PASS; full SciQLopPlots suite.
- [ ] **Step 5: Commit (SciQLopPlots)** — `git commit -m "feat(histogram): SciQLopHistogram over QCPHistogram"`.

---

### Task 6: Factories, time-axis sync, Python dispatch (SciQLopPlots)

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (next to `histogram2d` :385-403), `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp` (next to `add_histogram2d` :179-260 and :916-935), `include/SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp` (:366-380), `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` (:111-120), `src/SciQLopMultiPlotPanel.cpp` (:505-540). Follow every place `histogram2d(` appears (`grep -rn "histogram2d(" include src`) and add the 1-D twin.
- Modify: `SciQLopPlots/__init__.py` (`_HISTOGRAM_KWARGS` and guards next to :140-150; dispatch in `plot_func` :152-174 and `_plot` :191-231; `takes_name` :183)
- Test: `tests/integration/test_histogram1d.py`

**Interfaces:**
- Consumes: Task 5.
- Produces (C++, on `SciQLopPlotInterface` with `SciQLopPlot` and panel overrides):

```cpp
    virtual SciQLopHistogram* histogram(const SciQLopPyBuffer& values, QString name = "Histogram",
                                        int bins = 64, QVariantMap metaData = {});
    virtual SciQLopHistogram* histogram(GetDataPyCallable callable, QString name = "Histogram",
                                        int bins = 64, QObject* sync_with = nullptr,
                                        QVariantMap metaData = {});
    virtual SciQLopHistogram* bars(const SciQLopPyBuffer& edges, const SciQLopPyBuffer& heights,
                                   QString name = "Bars", QVariantMap metaData = {});
```

(the panel versions return `QPair<SciQLopPlotInterface*, SciQLopHistogram*>` and create a `SciQLopPlot`, like `_plot_histogram2d<SciQLopPlot>`). `bars`: `edges` must have one more element than `heights` has rows, else `std::invalid_argument`.

Time sync: in the callable factory, `graph->observe(sync_with ? sync_with : time_axis())` — **not** `_connect_callable_sync`, which observes `x_axis()` (`src/SciQLopPlot.cpp:1362-1375`), the value axis here.

Python (`__init__.py`): `plot.histogram(values_or_callback, bins=64, range=None, log_bins=False, normalization=HistogramNormalization.Count, style=None, name=None)` is a Python method that calls the C++ factory, then `set_bin_range(*range)` / `set_log_bins` / `set_normalization` / `set_style` (style `None` = the one/several-column default). Keep the C++ method before replacing it (`_cpp_histogram = {cls: cls.histogram for cls in (...)}`) and call that; the panel's returns `(plot, histogram)`, so configure `_graph_of(result)`. The fills plan (Task 11) owns `_graph_of`; if it has not landed, add it here exactly as written there, once, and the fills plan then reuses it instead of redefining it. Not a Qt slot, so this is allowed. `_HISTOGRAM_KWARGS = ("bins", "range", "log_bins", "normalization", "style")`, rejected (TypeError) on every other dispatch path, like `_reject_histogram2d_kwargs`. `GraphType.Histogram` dispatches only in `plot_func` (1 argument); in `_plot` with 2 arguments it raises `TypeError("GraphType.Histogram takes one argument (values or a callback); use plot.bars(edges, heights) for pre-binned data")`. Add `GraphType.Histogram` to `takes_name`.

- [ ] **Step 1: Failing tests** (append)

```python
from SciQLopPlots import GraphType


class TestFactories:
    def test_plot_dispatch_one_argument(self, qtbot, plot):
        h = plot.plot(np.arange(10.0), graph_type=GraphType.Histogram, name="h", bins=5)
        assert isinstance(h, SciQLopHistogram) and h.bins() == 5 and h.name() == "h"

    def test_two_arguments_are_refused(self, plot):
        with pytest.raises(TypeError):
            plot.plot(np.arange(10.0), np.arange(10.0), graph_type=GraphType.Histogram)

    def test_histogram_kwargs_on_a_line_are_refused(self, plot):
        with pytest.raises(TypeError):
            plot.plot(np.arange(10.0), np.arange(10.0), bins=32)

    def test_bars_from_edges_and_heights(self, qtbot, plot):
        b = plot.bars(np.array([0.0, 1.0, 2.0]), np.array([3.0, 5.0]))
        assert list(b.bin_heights()[0]) == [3.0, 5.0]
        with pytest.raises(ValueError):
            plot.bars(np.array([0.0, 1.0]), np.array([3.0, 5.0]))

    def test_range_excludes_outside_values(self, qtbot, plot):
        h = plot.histogram(np.array([-10.0, 0.5, 1.5, 10.0]), bins=2, range=(0.0, 2.0))
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
        assert sum(h.bin_heights()[0]) == 2

    def test_panel_histogram_follows_the_time_range(self, qtbot, panel):
        fetched = []

        def cb(start, stop):
            fetched.append((start, stop))
            x = np.linspace(start, stop, 100)
            return x, np.sin(x)

        panel.plot(np.linspace(0, 100, 1000), np.cos(np.linspace(0, 100, 1000)))
        hplot, h = panel.histogram(cb)
        panel.set_time_axis_range(10.0, 20.0)
        qtbot.waitUntil(lambda: any(abs(a - 10.0) < 1e-9 for a, _ in fetched), timeout=5000)
        x_range = hplot.x_axis().range()
        assert not (abs(x_range.start() - 10.0) < 1e-9 and abs(x_range.stop() - 20.0) < 1e-9)

    def test_empty_fetch_then_data(self, qtbot, plot):
        calls = []

        def cb(start, stop):
            calls.append(1)
            if len(calls) == 1:
                return np.array([], dtype=np.float64), np.array([], dtype=np.float64)
            return np.linspace(start, stop, 10), np.arange(10.0)

        h = plot.histogram(cb)
        plot.time_axis().set_range(0.0, 1.0)
        plot.time_axis().set_range(0.0, 2.0)
        qtbot.waitUntil(lambda: len(h.bin_heights()) == 1, timeout=5000)
```

(`panel.set_time_axis_range(double, double)` exists, `SciQLopMultiPlotPanel.hpp:275`; `SciQLopPlotRange` has `start()` / `stop()`, `SciQLopPlotRange.hpp:134-136`.)

- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** — PASS; full SciQLopPlots suite.
- [ ] **Step 5: Commit (SciQLopPlots)** — `git commit -m "feat(histogram): plot/panel factories, time-axis sync, dispatch"`.

---

### Task 7: Guide, full verification, reviews

- [ ] **Step 1:** `docs/user-guide.md`: a "Histograms" section: `plot.histogram(values)`, the callback form and that it follows the panel's time range (a histogram-only panel needs `panel.set_time_axis_range`), `bins`, `range` (outside values are not counted), `log_bins`, normalization, style, `plot.bars(edges, heights)`, and that auto edges only widen (`reset_edges` if exposed). Snippets are executed by the docs tests.
- [ ] **Step 2:** Full NeoQCP suite, then the full SciQLopPlots suite (two integration halves), one at a time; read the real counts.
- [ ] **Step 3:** SciQLop's suite and the four plugins against `build-venv` (memory `reference-run-sciqlop-suite-against-build`).
- [ ] **Step 4:** opencode code review (read-only, report to a file), verify each finding before acting.
- [ ] **Step 5:** Commit docs; API review Progress (task 17 done), tracker row 17 → Done, project memory. Do not push.
