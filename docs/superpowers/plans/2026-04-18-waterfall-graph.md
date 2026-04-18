# Waterfall Graph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose `QCPWaterfallGraph` as a first-class SciQLopPlots plotable at parity with line/scatter/colormap — unified `plot()` dispatch, Inspector delegate, reactive `.on.*` properties, raw-value tooltip, function variant — while extracting a shared `SciQLopMultiGraphBase` from `SciQLopLineGraph` without changing line-graph behavior.

**Architecture:** Refactor `SciQLopLineGraph` to inherit from a new `SciQLopMultiGraphBase` that owns the `QCPMultiGraph*`, `PyBuffer _x/_y`, dataGuard, buffer-dispatch, and per-column `SciQLopGraphComponent` creation. Add `SciQLopWaterfallGraph` (wraps `QCPWaterfallGraph`, a `QCPMultiGraph` subclass) deriving from the same base, plus `SciQLopWaterfallGraphFunction` for reactive data sources. Wire Python entry points (`plot.plot(..., graph_type=Waterfall)`, `add_waterfall(...)`), Inspector delegate (`SciQLopWaterfallDelegate`), and reactive properties (`.on.gain`, `.on.normalize`, etc.). Tooltip path in `SciQLopCrosshair` gets a `qobject_cast<SciQLopWaterfallGraph*>` branch that calls the wrapper's `raw_value_at` to show pre-transform signal amplitudes.

**Tech Stack:** C++20, Qt6, Shiboken6 bindings, Meson build, pytest + pytest-qt for integration tests.

**Spec:** `docs/superpowers/specs/2026-04-18-waterfall-graph-design.md`

---

## Build & Test Commands (reused throughout)

**Build:**
```bash
PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:/var/home/jeandet/Documents/prog/SciQLop/.venv/bin:/home/jeandet/.local/bin:/usr/bin:/usr/local/bin:$PATH" meson compile -C build
```

**Install built .so into SciQLop venv (fast path after meson compile):**
```bash
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

**Run tests (from outside project dir, via SciQLop venv, real display, no offscreen):**
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py -v
```

**Full regression suite:**
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/ -v
```

---

## File Map (what gets created / modified)

**New files:**
- `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp` — extracted shared base for line/waterfall
- `src/SciQLopMultiGraphBase.cpp`
- `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp` — wrapper + function variant
- `src/SciQLopWaterfallGraph.cpp`
- `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp`
- `src/SciQLopWaterfallDelegate.cpp`
- `tests/integration/test_waterfall.py`
- `tests/manual-tests/basics/waterfall.py`

**Modified files:**
- `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`, `src/SciQLopLineGraph.cpp` — inherit from base, drop duplicated code
- `include/SciQLopPlots/SciQLopPlotInterface.hpp` — `waterfall(...)` entry points
- `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp` — `add_waterfall(...)` + `plot_impl` routing
- `include/SciQLopPlots/enums.hpp` — `WaterfallOffsetMode`, `GraphType::Waterfall`
- `include/SciQLopPlots/SciQLopCrosshair.hpp`, `src/SciQLopCrosshair.cpp` — raw-value tooltip branch
- `SciQLopPlots/bindings/bindings.xml` — new object-types, enum, GraphType entry
- `SciQLopPlots/bindings/bindings.h` — new `#include`s
- `SciQLopPlots/meson.build` — sources, moc_headers
- `SciQLopPlots/__init__.py` — plot dispatch + reactive properties
- `src/Registrations.cpp` — TypeDescriptor + delegate registration
- `tests/manual-tests/gallery.py` — new waterfall demo entry

---

## Task Dependency Overview

```
Task 1 (base skeleton) → Task 2 (move logic, regression gate)
Task 2 → Task 3 (enums)
Task 3 → Task 4 (waterfall class) → Task 5 (knobs) → Task 6 (raw_value_at)
Task 6 → Task 7 (bindings/registrations)
Task 7 → Task 8 (function variant)
Task 8 → Task 9 (add_waterfall + plot_impl)
Task 9 → Task 10 (PlotInterface::waterfall)
Task 10 → Task 11 (Python plot dispatch)
Task 11 → Task 12 (reactive .on.*)
Task 12 → Task 13 (tooltip wiring)
Task 13 → Task 14 (Inspector delegate) → Task 15 (register delegate)
Task 15 → Task 16 (demo) → Task 17 (gallery link)
Task 17 → Task 18 (unity + full regression)
```

---

## Task 1: Create `SciQLopMultiGraphBase` skeleton (empty)

**Goal:** Introduce the base class as a mechanical extraction step — declaration only, no behavior moved yet. This isolates bindings/meson/unity-build surprises from the behavioral refactor in Task 2.

**Files:**
- Create: `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp`
- Create: `src/SciQLopMultiGraphBase.cpp`
- Modify: `SciQLopPlots/meson.build` (add to `headers` and `sources`)

- [ ] **Step 1: Create the header**

Write `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp`:
```cpp
#pragma once
#include "SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>
#include <span>

class SciQLopMultiGraphBase : public SQPQCPAbstractPlottableWrapper
{
    Q_OBJECT
protected:
    QCPMultiGraph* _multiGraph = nullptr;
    PyBuffer _x, _y;
    std::shared_ptr<void> _dataHolder;
    QStringList _pendingLabels;
    SciQLopPlotAxis* _keyAxis = nullptr;
    SciQLopPlotAxis* _valueAxis = nullptr;

    virtual QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) = 0;

    void clear_graphs(bool graph_already_removed = false);
    void build_data_source(const PyBuffer& x, const PyBuffer& y);
    void sync_components();

public:
    explicit SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                                   SciQLopPlotAxis* key_axis, SciQLopPlotAxis* value_axis,
                                   const QStringList& labels, QVariantMap metaData);
    ~SciQLopMultiGraphBase() override;

    Q_SLOT void set_data(PyBuffer x, PyBuffer y) override;
    QList<PyBuffer> data() const noexcept override;

    std::size_t line_count() const noexcept
    {
        return _multiGraph ? _multiGraph->componentCount() : 0;
    }

    void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graphs(const QStringList& labels);
};
```

- [ ] **Step 2: Create stub .cpp (all bodies throw/no-op for now; Task 2 fills them in)**

Write `src/SciQLopMultiGraphBase.cpp`:
```cpp
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"

SciQLopMultiGraphBase::SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                                             SciQLopPlotAxis* key_axis,
                                             SciQLopPlotAxis* value_axis,
                                             const QStringList& labels, QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper(type_label, metaData, parent)
    , _pendingLabels{labels}
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
}

SciQLopMultiGraphBase::~SciQLopMultiGraphBase() { clear_graphs(); }

void SciQLopMultiGraphBase::clear_graphs(bool) { /* filled in Task 2 */ }
void SciQLopMultiGraphBase::build_data_source(const PyBuffer&, const PyBuffer&) { /* Task 2 */ }
void SciQLopMultiGraphBase::sync_components() { /* Task 2 */ }
void SciQLopMultiGraphBase::set_data(PyBuffer, PyBuffer) { /* Task 2 */ }
QList<PyBuffer> SciQLopMultiGraphBase::data() const noexcept { return {}; }
void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface*) noexcept { /* Task 2 */ }
void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface*) noexcept { /* Task 2 */ }
void SciQLopMultiGraphBase::create_graphs(const QStringList&) { /* Task 2 */ }
```

- [ ] **Step 3: Add to meson.build**

In `SciQLopPlots/meson.build`, find the `headers` list (around line 96) and insert:
```meson
project_source_root + '/include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp',
```
Find the corresponding sources list (around line 210) and insert:
```meson
'../src/SciQLopMultiGraphBase.cpp',
```
Since the class has `Q_OBJECT`, it must also go in `moc_headers` (same file). Grep `moc_headers` to find the list and add the header path there too.

- [ ] **Step 4: Build — must compile clean**

Run: `PATH="..." meson compile -C build` (use the full PATH from the Build & Test Commands section)
Expected: build succeeds, no new warnings related to MultiGraphBase.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp \
        src/SciQLopMultiGraphBase.cpp SciQLopPlots/meson.build
git commit -m "refactor(plotables): add empty SciQLopMultiGraphBase skeleton"
```

---

## Task 2: Move data/buffer logic into `SciQLopMultiGraphBase`, reparent `SciQLopLineGraph`

**Goal:** Move the `set_data` buffer dispatch, dataGuard, component-sync from `SciQLopLineGraph` into the base. `SciQLopLineGraph` becomes a thin derived class. Existing tests for line graph must remain green — this is the load-bearing regression gate.

**Files:**
- Modify: `src/SciQLopMultiGraphBase.cpp` (fill in bodies)
- Modify: `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`
- Modify: `src/SciQLopLineGraph.cpp`

- [ ] **Step 1: Write regression snapshot test for line graph**

Create `tests/integration/test_line_graph_regression.py`:
```python
"""Snapshot behavior of SciQLopLineGraph to gate the MultiGraphBase refactor."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot, SciQLopLineGraph, GraphType


@pytest.fixture(scope="module")
def app():
    inst = QApplication.instance()
    return inst if inst else QApplication([])


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def test_line_graph_2d_creates_component_per_column(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.column_stack([np.sin(x), np.cos(x), np.sin(2 * x)]).astype(np.float64)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert isinstance(g, SciQLopLineGraph)
    assert g.line_count() == 3


def test_line_graph_data_round_trip(plot):
    x = np.linspace(0, 1, 50).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    data = g.data()
    assert len(data) == 2
    np.testing.assert_array_equal(np.array(data[0]), x)
    np.testing.assert_array_equal(np.array(data[1]), y)


def test_line_graph_row_major_2d(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.ascontiguousarray(np.column_stack([np.sin(x), np.cos(x)]))  # row-major
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 2


def test_line_graph_column_major_2d(plot):
    x = np.linspace(0, 1, 100).astype(np.float64)
    y = np.asfortranarray(np.column_stack([np.sin(x), np.cos(x)]))  # col-major
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 2


def test_line_graph_float32_y(plot):
    x = np.linspace(0, 1, 50).astype(np.float64)
    y = np.sin(x).astype(np.float32)
    g = plot.plot(x, y, graph_type=GraphType.Line)
    assert g.line_count() == 1
```

- [ ] **Step 2: Run snapshot test against pre-refactor state — must PASS**

Build + install:
```bash
PATH="..." meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```
Run: `cd /var/home/jeandet/Documents/prog && LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_line_graph_regression.py -v`
Expected: 5 passed. This confirms the baseline before any code moves.

- [ ] **Step 3: Fill in `SciQLopMultiGraphBase.cpp` bodies**

Replace the stubs in `src/SciQLopMultiGraphBase.cpp`:
```cpp
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponent.hpp"
#include <datasource/row-major-multi-datasource.h>
#include <datasource/soa-multi-datasource.h>
#include <vector>

SciQLopMultiGraphBase::SciQLopMultiGraphBase(const QString& type_label, QCustomPlot* parent,
                                             SciQLopPlotAxis* key_axis,
                                             SciQLopPlotAxis* value_axis,
                                             const QStringList& labels, QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper(type_label, metaData, parent)
    , _pendingLabels{labels}
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
}

SciQLopMultiGraphBase::~SciQLopMultiGraphBase() { clear_graphs(); }

void SciQLopMultiGraphBase::create_graphs(const QStringList& labels)
{
    if (_multiGraph)
        clear_graphs();

    _multiGraph = create_multi_graph(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    connect(_multiGraph, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
    if (!labels.isEmpty())
        _pendingLabels = labels;
}

void SciQLopMultiGraphBase::clear_graphs(bool graph_already_removed)
{
    clear_plottables();
    if (_multiGraph && !graph_already_removed)
    {
        auto plot = _multiGraph->parentPlot();
        if (plot)
            plot->removePlottable(_multiGraph);
    }
    _multiGraph = nullptr;
}

void SciQLopMultiGraphBase::build_data_source(const PyBuffer& x, const PyBuffer& y)
{
    const auto* keys = static_cast<const double*>(x.raw_data());
    const int n = static_cast<int>(x.flat_size());

    if (n > 0)
        m_data_range = SciQLopPlotRange(keys[0], keys[n - 1]);
    else
        m_data_range = SciQLopPlotRange();

    struct Buffers { PyBuffer x, y; };
    auto guard = std::make_shared<Buffers>(Buffers{x, y});

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            auto source = std::make_shared<
                QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                std::span<const double>(keys, n), std::move(columns), guard);
            _multiGraph->setDataSource(std::move(source));
        }
        else
        {
            const auto n_cols = y.size(1);
            if (y.row_major())
            {
                auto source = std::make_shared<QCPRowMajorMultiDataSource<double, V>>(
                    std::span<const double>(keys, n),
                    values, n, static_cast<int>(n_cols), static_cast<int>(n_cols), guard);
                _multiGraph->setDataSource(std::move(source));
            }
            else
            {
                std::vector<std::span<const V>> columns;
                columns.reserve(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                    columns.emplace_back(values + col * n, n);
                auto source = std::make_shared<
                    QCPSoAMultiDataSource<std::span<const double>, std::span<const V>>>(
                    std::span<const double>(keys, n), std::move(columns), guard);
                _multiGraph->setDataSource(std::move(source));
            }
        }
    });
    _dataHolder = std::move(guard);
}

void SciQLopMultiGraphBase::sync_components()
{
    const int nComponents = _multiGraph->componentCount();
    if (static_cast<int>(plottable_count()) < nComponents)
    {
        if (!_pendingLabels.isEmpty())
            _multiGraph->setComponentNames(_pendingLabels);
        for (int i = static_cast<int>(plottable_count()); i < nComponents; ++i)
        {
            auto component = new SciQLopGraphComponent(_multiGraph, i, this);
            if (i < _pendingLabels.size())
                component->set_name(_pendingLabels.value(i));
            _register_component(component);
        }
        _pendingLabels.clear();
    }
}

void SciQLopMultiGraphBase::set_data(PyBuffer x, PyBuffer y)
{
    if (!_multiGraph || !x.is_valid() || !y.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");

    _x = x;
    _y = y;

    build_data_source(x, y);
    sync_components();

    Q_EMIT this->replot();
    check_first_data(static_cast<int>(x.flat_size()));
    Q_EMIT data_changed(x, y);
    Q_EMIT data_changed();
}

QList<PyBuffer> SciQLopMultiGraphBase::data() const noexcept
{
    return {_x, _y};
}

void SciQLopMultiGraphBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopMultiGraphBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
}
```

- [ ] **Step 4: Simplify `SciQLopLineGraph.hpp` to derive from the base**

Replace `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp` with:
```cpp
#pragma once
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>

class SciQLopLineGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPMultiGraph(keyAxis, valueAxis);
    }

public:
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(),
                              QVariantMap metaData = {});
    ~SciQLopLineGraph() override = default;
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels, QVariantMap metaData = {});
    ~SciQLopLineGraphFunction() override = default;
};
```

- [ ] **Step 5: Reduce `src/SciQLopLineGraph.cpp`**

Replace with:
```cpp
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis, const QStringList& labels,
                                   QVariantMap metaData)
    : SciQLopMultiGraphBase("Line", parent, key_axis, value_axis, labels, metaData)
{
    create_graphs(labels);
}

SciQLopLineGraphFunction::SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                   SciQLopPlotAxis* value_axis,
                                                   GetDataPyCallable&& callable,
                                                   const QStringList& labels,
                                                   QVariantMap metaData)
    : SciQLopLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}
```

- [ ] **Step 6: Build**

Run: `PATH="..." meson compile -C build && cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/`
Expected: build succeeds.

- [ ] **Step 7: Regression — full line/graph test suite must still pass**

Run:
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_line_graph_regression.py \
    SciQLopPlots/tests/integration/test_graph_api.py \
    SciQLopPlots/tests/integration/test_signals.py \
    SciQLopPlots/tests/integration/test_bug_reproducers.py \
    SciQLopPlots/tests/integration/test_p0_bugs.py \
    SciQLopPlots/tests/integration/test_p1_bugs.py \
    -v
```
Expected: all pass. If any fails, fix the base class before continuing — do NOT proceed to Task 3.

- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp \
        src/SciQLopLineGraph.cpp src/SciQLopMultiGraphBase.cpp \
        tests/integration/test_line_graph_regression.py
git commit -m "refactor(plotables): extract SciQLopMultiGraphBase from SciQLopLineGraph

Buffer dispatch, dataGuard, and per-column component creation move to the
base class; SciQLopLineGraph becomes a thin derived class that only
supplies the concrete QCPMultiGraph factory. Public API unchanged;
regression suite test_line_graph_regression.py added to snapshot behavior."
```

---

## Task 3: Add `WaterfallOffsetMode` enum and `GraphType::Waterfall`

**Files:**
- Modify: `include/SciQLopPlots/enums.hpp`

- [ ] **Step 1: Add enum + GraphType entry**

In `include/SciQLopPlots/enums.hpp`, after the existing `GraphType` enum (around line 85):
```cpp
enum class GraphType
{
    Line,
    ParametricCurve,
    Scatter,
    ColorMap,
    Waterfall
};
Q_DECLARE_METATYPE(GraphType);

enum class WaterfallOffsetMode
{
    Uniform,
    Custom
};
Q_DECLARE_METATYPE(WaterfallOffsetMode);
```

(Replace the existing `GraphType` block; add the `WaterfallOffsetMode` block right below.)

- [ ] **Step 2: Build to catch enum exhaustiveness errors**

Run: `PATH="..." meson compile -C build`
Expected: build succeeds (the existing `plot_impl` switch has a `default: throw`, so missing case is non-fatal but will be addressed in Task 9).

- [ ] **Step 3: Commit**

```bash
git add include/SciQLopPlots/enums.hpp
git commit -m "feat(enums): add GraphType::Waterfall and WaterfallOffsetMode"
```

---

## Task 4: `SciQLopWaterfallGraph` — construction and basic `set_data`

**Goal:** Introduce the wrapper class, inheriting from the base. No knobs yet — just verify the class constructs, accepts data, and renders via QCPWaterfallGraph's default settings.

**Files:**
- Create: `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`
- Create: `src/SciQLopWaterfallGraph.cpp`
- Modify: `SciQLopPlots/meson.build`

- [ ] **Step 1: Write the failing test**

Create `tests/integration/test_waterfall.py`:
```python
"""Integration tests for SciQLopWaterfallGraph."""
import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot, SciQLopWaterfallGraph


@pytest.fixture(scope="module")
def app():
    inst = QApplication.instance()
    return inst if inst else QApplication([])


@pytest.fixture()
def plot(app):
    p = SciQLopPlot()
    yield p
    del p


def _make_2d(n=100, cols=3):
    x = np.linspace(0, 1, n).astype(np.float64)
    y = np.column_stack([np.sin(x * (k + 1)) for k in range(cols)]).astype(np.float64)
    return x, y


class TestWaterfallConstruction:
    def test_add_waterfall_returns_correct_type(self, plot):
        wf = plot.add_waterfall("test")
        assert isinstance(wf, SciQLopWaterfallGraph)

    def test_set_data_2d(self, plot):
        wf = plot.add_waterfall("test", labels=["c0", "c1", "c2"])
        x, y = _make_2d()
        wf.set_data(x, y)
        assert wf.line_count() == 3
```

- [ ] **Step 2: Run the test — must FAIL (SciQLopWaterfallGraph not exported yet)**

Run: `cd /var/home/jeandet/Documents/prog && LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py -v`
Expected: `ImportError` or `AttributeError` for `SciQLopWaterfallGraph` / `add_waterfall`.

- [ ] **Step 3: Create the header**

Write `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`:
```cpp
#pragma once
#include "SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp"
#include "SciQLopPlots/enums.hpp"
#include <plottables/plottable-waterfall.h>

class SciQLopWaterfallGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPWaterfallGraph(keyAxis, valueAxis);
    }

    QCPWaterfallGraph* waterfall_graph() const noexcept
    {
        return static_cast<QCPWaterfallGraph*>(_multiGraph);
    }

public:
    explicit SciQLopWaterfallGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis,
                                   const QStringList& labels = QStringList(),
                                   QVariantMap metaData = {});
    ~SciQLopWaterfallGraph() override = default;
};
```

- [ ] **Step 4: Create the .cpp**

Write `src/SciQLopWaterfallGraph.cpp`:
```cpp
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"

SciQLopWaterfallGraph::SciQLopWaterfallGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                             SciQLopPlotAxis* value_axis,
                                             const QStringList& labels, QVariantMap metaData)
    : SciQLopMultiGraphBase("Waterfall", parent, key_axis, value_axis, labels, metaData)
{
    create_graphs(labels);
}
```

- [ ] **Step 5: Add to meson.build (headers + moc_headers + sources)**

In `SciQLopPlots/meson.build`:
- Add to `headers`: `project_source_root + '/include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp'`
- Add to `moc_headers`: same path (class has `Q_OBJECT`)
- Add to sources: `'../src/SciQLopWaterfallGraph.cpp'`

(`add_waterfall` method on `SciQLopPlot` comes in Task 9 — for now the test cannot construct via that factory; wait for Task 9 before the test passes. Mark this task done once the class compiles; the test will pass later.)

- [ ] **Step 6: Build — must compile clean**

Run: `PATH="..." meson compile -C build`
Expected: build succeeds.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp \
        src/SciQLopWaterfallGraph.cpp SciQLopPlots/meson.build \
        tests/integration/test_waterfall.py
git commit -m "feat(plotables): SciQLopWaterfallGraph class (construction only)"
```

---

## Task 5: Waterfall knobs — `offset_mode`, `uniform_spacing`, `offsets`, `normalize`, `gain`

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`
- Modify: `src/SciQLopWaterfallGraph.cpp`

- [ ] **Step 1: Extend the header with signals/slots/getters**

Replace the class body in `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp` with:
```cpp
class SciQLopWaterfallGraph : public SciQLopMultiGraphBase
{
    Q_OBJECT
protected:
    QCPMultiGraph* create_multi_graph(QCPAxis* keyAxis, QCPAxis* valueAxis) override
    {
        return new QCPWaterfallGraph(keyAxis, valueAxis);
    }

    QCPWaterfallGraph* waterfall_graph() const noexcept
    {
        return static_cast<QCPWaterfallGraph*>(_multiGraph);
    }

public:
    explicit SciQLopWaterfallGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis,
                                   const QStringList& labels = QStringList(),
                                   QVariantMap metaData = {});
    ~SciQLopWaterfallGraph() override = default;

    Q_SLOT void set_offset_mode(WaterfallOffsetMode mode);
    Q_SLOT void set_uniform_spacing(double spacing);
    Q_SLOT void set_offsets(const QVector<double>& offsets);
    Q_SLOT void set_normalize(bool enabled);
    Q_SLOT void set_gain(double gain);

    WaterfallOffsetMode offset_mode() const;
    double uniform_spacing() const;
    QVector<double> offsets() const;
    bool normalize() const;
    double gain() const;

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void offset_mode_changed(WaterfallOffsetMode);
    Q_SIGNAL void uniform_spacing_changed(double);
    Q_SIGNAL void offsets_changed(QVector<double>);
    Q_SIGNAL void normalize_changed(bool);
    Q_SIGNAL void gain_changed(double);
};
```

- [ ] **Step 2: Implement setters/getters in .cpp**

Append to `src/SciQLopWaterfallGraph.cpp`:
```cpp
static QCPWaterfallGraph::OffsetMode to_qcp(WaterfallOffsetMode mode)
{
    return mode == WaterfallOffsetMode::Uniform
               ? QCPWaterfallGraph::omUniform
               : QCPWaterfallGraph::omCustom;
}

static WaterfallOffsetMode from_qcp(QCPWaterfallGraph::OffsetMode mode)
{
    return mode == QCPWaterfallGraph::omUniform
               ? WaterfallOffsetMode::Uniform
               : WaterfallOffsetMode::Custom;
}

void SciQLopWaterfallGraph::set_offset_mode(WaterfallOffsetMode mode)
{
    if (auto* w = waterfall_graph())
    {
        if (from_qcp(w->offsetMode()) == mode)
            return;
        w->setOffsetMode(to_qcp(mode));
        Q_EMIT offset_mode_changed(mode);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_uniform_spacing(double spacing)
{
    if (auto* w = waterfall_graph())
    {
        if (w->uniformSpacing() == spacing)
            return;
        w->setUniformSpacing(spacing);
        Q_EMIT uniform_spacing_changed(spacing);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_offsets(const QVector<double>& offsets)
{
    if (auto* w = waterfall_graph())
    {
        const auto n = static_cast<int>(line_count());
        if (n > 0 && offsets.size() != n)
            throw std::invalid_argument(
                "offsets length must match number of traces");
        if (w->offsets() == offsets)
            return;
        w->setOffsets(offsets);
        Q_EMIT offsets_changed(offsets);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_normalize(bool enabled)
{
    if (auto* w = waterfall_graph())
    {
        if (w->normalize() == enabled)
            return;
        w->setNormalize(enabled);
        Q_EMIT normalize_changed(enabled);
        Q_EMIT replot();
    }
}

void SciQLopWaterfallGraph::set_gain(double gain)
{
    if (auto* w = waterfall_graph())
    {
        if (w->gain() == gain)
            return;
        w->setGain(gain);
        Q_EMIT gain_changed(gain);
        Q_EMIT replot();
    }
}

WaterfallOffsetMode SciQLopWaterfallGraph::offset_mode() const
{
    return waterfall_graph() ? from_qcp(waterfall_graph()->offsetMode())
                              : WaterfallOffsetMode::Uniform;
}

double SciQLopWaterfallGraph::uniform_spacing() const
{
    return waterfall_graph() ? waterfall_graph()->uniformSpacing() : 1.0;
}

QVector<double> SciQLopWaterfallGraph::offsets() const
{
    return waterfall_graph() ? waterfall_graph()->offsets() : QVector<double>{};
}

bool SciQLopWaterfallGraph::normalize() const
{
    return waterfall_graph() ? waterfall_graph()->normalize() : true;
}

double SciQLopWaterfallGraph::gain() const
{
    return waterfall_graph() ? waterfall_graph()->gain() : 1.0;
}
```

- [ ] **Step 3: Write tests (will pass once Task 9 wires `add_waterfall`)**

Append to `tests/integration/test_waterfall.py`:
```python
from SciQLopPlots import WaterfallOffsetMode


class TestWaterfallKnobs:
    def test_default_offset_mode_is_uniform(self, plot):
        wf = plot.add_waterfall("w")
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform

    def test_set_offset_mode_switches(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_offset_mode(WaterfallOffsetMode.Custom)
        assert wf.offset_mode() == WaterfallOffsetMode.Custom

    def test_set_uniform_spacing(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_uniform_spacing(2.5)
        assert wf.uniform_spacing() == 2.5

    def test_set_offsets_wrong_len_raises(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x, y = _make_2d(cols=3)
        wf.set_data(x, y)
        with pytest.raises(ValueError):
            wf.set_offsets([0.0, 1.0])  # too short

    def test_set_offsets_correct_len(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x, y = _make_2d(cols=3)
        wf.set_data(x, y)
        wf.set_offsets([0.0, 1.0, 2.5])
        assert list(wf.offsets()) == [0.0, 1.0, 2.5]

    def test_set_normalize(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_normalize(False)
        assert wf.normalize() is False

    def test_set_gain(self, plot):
        wf = plot.add_waterfall("w")
        wf.set_gain(2.0)
        assert wf.gain() == 2.0

    def test_set_gain_negative_ok(self, plot):
        """Negative gain flips polarity — valid, not an error."""
        wf = plot.add_waterfall("w")
        wf.set_gain(-1.0)
        assert wf.gain() == -1.0
```

- [ ] **Step 4: Build**

Run: `PATH="..." meson compile -C build && cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/`
Expected: build succeeds (no Python tests exercise the knobs yet — Task 7 handles the bindings XML, Task 9 wires `add_waterfall`).

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp \
        src/SciQLopWaterfallGraph.cpp tests/integration/test_waterfall.py
git commit -m "feat(waterfall): offset_mode/spacing/offsets/normalize/gain knobs + signals"
```

---

## Task 6: `raw_value_at` for tooltip readback

**Goal:** Provide a function that returns the pre-transform buffer amplitude at a given key, for `SciQLopCrosshair` to call later (Task 13).

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`
- Modify: `src/SciQLopWaterfallGraph.cpp`

- [ ] **Step 1: Add declaration to header**

Inside the `SciQLopWaterfallGraph` `public:` block, above the signals, add:
```cpp
double raw_value_at(int component, double key) const;
```

- [ ] **Step 2: Implement the method**

Append to `src/SciQLopWaterfallGraph.cpp`:
```cpp
#include <algorithm>
#include <cmath>

double SciQLopWaterfallGraph::raw_value_at(int component, double key) const
{
    if (!_x.is_valid() || !_y.is_valid())
        return std::nan("");

    const auto* keys = static_cast<const double*>(_x.raw_data());
    const int n = static_cast<int>(_x.flat_size());
    if (n == 0 || component < 0)
        return std::nan("");

    // Binary search for nearest key index (sorted keys assumed, same as line graph).
    auto it = std::lower_bound(keys, keys + n, key);
    int idx;
    if (it == keys + n) idx = n - 1;
    else if (it == keys) idx = 0;
    else
    {
        idx = static_cast<int>(it - keys);
        if (idx > 0 && std::abs(*(it - 1) - key) < std::abs(*it - key))
            idx = idx - 1;
    }

    double result = std::nan("");
    dispatch_dtype(_y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(_y.raw_data());
        if (_y.ndim() == 1)
        {
            if (component == 0) result = static_cast<double>(values[idx]);
        }
        else
        {
            const auto n_cols = static_cast<int>(_y.size(1));
            if (component >= n_cols) return;
            if (_y.row_major())
                result = static_cast<double>(values[idx * n_cols + component]);
            else
                result = static_cast<double>(values[component * n + idx]);
        }
    });
    return result;
}
```

- [ ] **Step 3: Write tests**

Append to `tests/integration/test_waterfall.py`:
```python
class TestWaterfallRawValueAt:
    def test_raw_value_at_row_major(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.ascontiguousarray(np.column_stack([x, 2 * x, 3 * x]))  # row-major
        wf.set_data(x, y)
        # At key=0.5 (idx 5), component 1 should be 2*0.5 = 1.0
        assert wf.raw_value_at(1, 0.5) == pytest.approx(1.0)

    def test_raw_value_at_column_major(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1", "c2"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.asfortranarray(np.column_stack([x, 2 * x, 3 * x]))
        wf.set_data(x, y)
        assert wf.raw_value_at(2, 0.5) == pytest.approx(1.5)

    def test_raw_value_at_normalize_still_returns_raw(self, plot):
        wf = plot.add_waterfall("w", labels=["c0"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.sin(x * 3.14159).astype(np.float64)
        wf.set_data(x, y)
        wf.set_normalize(True)
        wf.set_gain(100.0)
        # Raw value is pre-normalize, pre-gain — should match raw sin.
        assert wf.raw_value_at(0, 0.5) == pytest.approx(np.sin(0.5 * 3.14159))

    def test_raw_value_at_out_of_range_is_finite(self, plot):
        """Binary search clamps to array bounds, so out-of-range keys map to
        nearest endpoint — a finite value."""
        wf = plot.add_waterfall("w", labels=["c0"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.ones_like(x)
        wf.set_data(x, y)
        assert wf.raw_value_at(0, 999.0) == 1.0   # clamped to last
        assert wf.raw_value_at(0, -999.0) == 1.0  # clamped to first

    def test_raw_value_at_invalid_component_is_nan(self, plot):
        wf = plot.add_waterfall("w", labels=["c0", "c1"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.column_stack([x, 2 * x]).astype(np.float64)
        wf.set_data(x, y)
        import math
        assert math.isnan(wf.raw_value_at(5, 0.5))  # component out of range
```

- [ ] **Step 4: Build**

Run: `PATH="..." meson compile -C build && cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp \
        src/SciQLopWaterfallGraph.cpp tests/integration/test_waterfall.py
git commit -m "feat(waterfall): raw_value_at for pre-transform tooltip readback"
```

---

## Task 7: Python bindings — bindings.xml, bindings.h, Registrations.cpp

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `SciQLopPlots/bindings/bindings.h`
- Modify: `src/Registrations.cpp`

- [ ] **Step 1: Add object-types + enum to bindings.xml**

In `SciQLopPlots/bindings/bindings.xml`, in the enum-type block (grep for `<enum-type name="GraphType"`):
```xml
<enum-type name="WaterfallOffsetMode"/>
```

In the object-type block, near the line-graph entries (grep for `<object-type name="SciQLopLineGraph"`):
```xml
<object-type name="SciQLopMultiGraphBase" parent-management="yes">
    <property name="name" type="QString" get="objectName" set="setObjectName" generate-getsetdef="yes"/>
</object-type>
<object-type name="SciQLopWaterfallGraph" parent-management="yes">
    <property name="name" type="QString" get="objectName" set="setObjectName" generate-getsetdef="yes"/>
</object-type>
```

(The function variant `SciQLopWaterfallGraphFunction` comes in Task 8.)

- [ ] **Step 2: Add includes to bindings.h**

In `SciQLopPlots/bindings/bindings.h`, next to existing plotable includes:
```cpp
#include <SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp>
#include <SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp>
```

- [ ] **Step 3: Register TypeDescriptor in `src/Registrations.cpp`**

Add the header include at top:
```cpp
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"
```

In the `register_all_types()` function, after the `SciQLopLineGraph`-equivalent block (pattern-match on `SciQLopGraphInterface` registration block around line 135), insert:
```cpp
types.register_type<SciQLopWaterfallGraph>({
    .parent_type = TypeDescriptor<SciQLopMultiGraphBase>(),
    .children = {TypeDescriptor<SciQLopGraphComponentInterface>()},
});
```

(The delegate registration — `delegates.register_type<SciQLopWaterfallDelegate>()` — is added in Task 15.)

- [ ] **Step 4: Build**

Run: `PATH="..." meson compile -C build && cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/`
Expected: Shiboken regenerates wrappers; build succeeds.

- [ ] **Step 5: Sanity import test**

Run:
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -c "from SciQLopPlots import SciQLopWaterfallGraph, WaterfallOffsetMode; print('ok', WaterfallOffsetMode.Uniform)"
```
Expected: `ok WaterfallOffsetMode.Uniform`.

- [ ] **Step 6: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml SciQLopPlots/bindings/bindings.h \
        src/Registrations.cpp
git commit -m "feat(bindings): expose SciQLopWaterfallGraph + WaterfallOffsetMode to Python"
```

---

## Task 8: `SciQLopWaterfallGraphFunction` (reactive callable variant)

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`
- Modify: `src/SciQLopWaterfallGraph.cpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Append function-variant class to header**

Add to `include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp`, after `SciQLopWaterfallGraph`:
```cpp
class SciQLopWaterfallGraphFunction : public SciQLopWaterfallGraph,
                                       public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    explicit SciQLopWaterfallGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                           SciQLopPlotAxis* value_axis,
                                           GetDataPyCallable&& callable,
                                           const QStringList& labels = QStringList(),
                                           QVariantMap metaData = {});
    ~SciQLopWaterfallGraphFunction() override = default;
};
```

- [ ] **Step 2: Implement the function-variant constructor**

Append to `src/SciQLopWaterfallGraph.cpp`:
```cpp
SciQLopWaterfallGraphFunction::SciQLopWaterfallGraphFunction(QCustomPlot* parent,
                                                             SciQLopPlotAxis* key_axis,
                                                             SciQLopPlotAxis* value_axis,
                                                             GetDataPyCallable&& callable,
                                                             const QStringList& labels,
                                                             QVariantMap metaData)
    : SciQLopWaterfallGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}
```

- [ ] **Step 3: Register in bindings.xml**

Add next to other function-variant object-types:
```xml
<object-type name="SciQLopWaterfallGraphFunction" parent-management="yes">
    <property name="name" type="QString" get="objectName" set="setObjectName" generate-getsetdef="yes"/>
</object-type>
```

- [ ] **Step 4: Build**

Run: `PATH="..." meson compile -C build && cp ...`
Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp \
        src/SciQLopWaterfallGraph.cpp SciQLopPlots/bindings/bindings.xml
git commit -m "feat(waterfall): function variant for reactive callable data source"
```

---

## Task 9: `SciQLopPlot::add_waterfall(...)` and `plot_impl` routing

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp`
- Modify: `src/SciQLopPlot.cpp`

- [ ] **Step 1: Add declarations to header**

In `include/SciQLopPlots/SciQLopPlot.hpp`, near `add_histogram2d` (around line 96), add:
```cpp
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"
// ...
SciQLopWaterfallGraph* add_waterfall(const QString& name,
                                     const QStringList& labels = {},
                                     const QList<QColor>& colors = {});

SciQLopWaterfallGraphFunction* add_waterfall(GetDataPyCallable callable,
                                              const QString& name,
                                              const QStringList& labels = {},
                                              const QList<QColor>& colors = {});
```

(We keep the `add_waterfall` API narrow — offsets/normalize/gain set via setters post-construction. The `plot()` Python dispatch handles the full kwargs in Task 11.)

- [ ] **Step 2: Implement in .cpp**

Add to `src/SciQLopPlot.cpp` (near `add_histogram2d` implementation):
```cpp
SciQLopWaterfallGraph* SciQLopPlot::add_waterfall(const QString& name,
                                                   const QStringList& labels,
                                                   const QList<QColor>& colors)
{
    auto* wf = add_plottable<SciQLopWaterfallGraph>(labels);
    if (wf)
    {
        wf->set_name(name);
        if (!colors.isEmpty())
            wf->set_colors(colors);
    }
    return wf;
}

SciQLopWaterfallGraphFunction* SciQLopPlot::add_waterfall(GetDataPyCallable callable,
                                                          const QString& name,
                                                          const QStringList& labels,
                                                          const QList<QColor>& colors)
{
    auto* wf = add_plottable<SciQLopWaterfallGraphFunction>(std::move(callable), labels);
    if (wf)
    {
        wf->set_name(name);
        if (!colors.isEmpty())
            wf->set_colors(colors);
        _connect_callable_sync(wf, nullptr);
    }
    return wf;
}
```

- [ ] **Step 3: Add `Waterfall` branch to `plot_impl` (data variant)**

In `src/SciQLopPlot.cpp` line ~733, find the `switch (graph_type)` inside the data-variant `plot_impl`. Add a case:
```cpp
case GraphType::Waterfall:
    plottable = m_impl->add_plottable<SciQLopWaterfallGraph>(labels, metaData);
    break;
```

- [ ] **Step 4: Add `Waterfall` branch to `plot_impl` (callable variant)**

In the callable-variant `plot_impl` (line ~823), add:
```cpp
case GraphType::Waterfall:
    plottable = m_impl->add_plottable<SciQLopWaterfallGraphFunction>(
        std::move(callable), labels, metaData);
    break;
```

- [ ] **Step 5: Build + run waterfall tests — now should actually pass**

Run:
```bash
PATH="..." meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py -v
```
Expected: all tests from Tasks 4, 5, 6 pass.

- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp
git commit -m "feat(plot): add_waterfall() + Waterfall branch in plot_impl dispatch"
```

---

## Task 10: `SciQLopPlotInterface::waterfall(...)` entry points

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp`

- [ ] **Step 1: Add `waterfall()` overloads**

In `include/SciQLopPlots/SciQLopPlotInterface.hpp`, after the `colormap(callable, ...)` method (around line 346), add:
```cpp
inline virtual SciQLopGraphInterface*
waterfall(const PyBuffer& x, const PyBuffer& y, QStringList labels = QStringList(),
          QList<QColor> colors = QList<QColor>(),
          QVariantMap metaData = {})
{
    return plot_impl(x, y, labels, colors, ::GraphType::Waterfall,
                     ::GraphMarkerShape::NoMarker, metaData);
}

inline virtual SciQLopGraphInterface*
waterfall(GetDataPyCallable callable, QStringList labels = QStringList(),
          QList<QColor> colors = QList<QColor>(),
          QObject* sync_with = nullptr, QVariantMap metaData = {})
{
    return plot_impl(callable, labels, colors, ::GraphType::Waterfall,
                     ::GraphMarkerShape::NoMarker, sync_with, metaData);
}
```

- [ ] **Step 2: Build**

Run: `PATH="..." meson compile -C build && cp ...`
Expected: build succeeds.

- [ ] **Step 3: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp
git commit -m "feat(interface): waterfall(x,y) and waterfall(callable) entry points"
```

---

## Task 11: Python `plot()` dispatch — `Waterfall` branch + kwarg resolution

**Files:**
- Modify: `SciQLopPlots/__init__.py`

- [ ] **Step 1: Extend `plot_func` and `plot` with a Waterfall branch**

Replace the `_patch_sciqlop_plot` function in `SciQLopPlots/__init__.py` with:
```python
def _apply_waterfall_kwargs(wf, offsets=None, normalize=True, gain=1.0):
    """Resolve polymorphic `offsets` kwarg then apply waterfall-specific knobs."""
    import numpy as np
    from .SciQLopPlotsBindings import WaterfallOffsetMode

    if offsets is None:
        wf.set_offset_mode(WaterfallOffsetMode.Uniform)
        wf.set_uniform_spacing(1.0)
    elif isinstance(offsets, (int, float)):
        wf.set_offset_mode(WaterfallOffsetMode.Uniform)
        wf.set_uniform_spacing(float(offsets))
    else:
        arr = np.asarray(offsets, dtype=np.float64).ravel()
        wf.set_offset_mode(WaterfallOffsetMode.Custom)
        wf.set_offsets(arr.tolist())

    wf.set_normalize(bool(normalize))
    wf.set_gain(float(gain))
    return wf


def _patch_sciqlop_plot(cls):
    def plot_func(self, callback, graph_type=None,
                  offsets=None, normalize=True, gain=1.0, **kwargs):
        kwargs = {k: v for k, v in kwargs.items() if v is not None}
        if graph_type == GraphType.ParametricCurve:
            return cls.parametric_curve(self, callback, **kwargs)
        elif graph_type == GraphType.Line:
            return cls.line(self, callback, **kwargs)
        elif graph_type == GraphType.Scatter:
            return cls.scatter(self, callback, **kwargs)
        elif graph_type == GraphType.ColorMap:
            return cls.colormap(self, callback, **kwargs)
        elif graph_type == GraphType.Waterfall:
            wf = cls.waterfall(self, callback, **kwargs)
            return _apply_waterfall_kwargs(wf, offsets=offsets,
                                           normalize=normalize, gain=gain)
        raise ValueError(f"unsupported graph_type {graph_type!r} for single-arg plot()")

    def plot(self, *args, name=None, labels=None, colors=None, graph_type=None,
             offsets=None, normalize=True, gain=1.0, **kwargs):
        graph_type = graph_type or GraphType.Line
        kwargs = _merge_kwargs(kwargs, name=name, labels=labels, colors=colors)
        if (graph_type == GraphType.ParametricCurve) and (len(args) in (1, 2, 4)) and not callable(args[0]):
            return cls.parametric_curve(self, *args, **kwargs)
        if len(args) == 1:
            return plot_func(self, *args, graph_type=graph_type,
                             offsets=offsets, normalize=normalize, gain=gain, **kwargs)
        if len(args) == 2:
            if graph_type == GraphType.Line:
                return cls.line(self, *args, **kwargs)
            if graph_type == GraphType.Scatter:
                return cls.scatter(self, *args, **kwargs)
            if graph_type == GraphType.ColorMap:
                return cls.colormap(self, *args, **kwargs)
            if graph_type == GraphType.Waterfall:
                wf = cls.waterfall(self, *args, **kwargs)
                return _apply_waterfall_kwargs(wf, offsets=offsets,
                                               normalize=normalize, gain=gain)
            raise ValueError(f"unsupported graph_type {graph_type!r} for 2-arg plot()")
        if len(args) == 3:
            return cls.colormap(self, *args, **kwargs)
        raise ValueError(f"only 1, 2 or 3 arguments are supported, got {len(args)}")

    cls.plot = plot
    return cls
```

- [ ] **Step 2: Write tests**

Append to `tests/integration/test_waterfall.py`:
```python
from SciQLopPlots import GraphType


class TestWaterfallPlotDispatch:
    def test_plot_graph_type_waterfall(self, plot):
        x, y = _make_2d(cols=4)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, labels=["a", "b", "c", "d"])
        assert isinstance(wf, SciQLopWaterfallGraph)
        assert wf.line_count() == 4

    def test_offsets_float_sets_uniform_mode(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, offsets=2.5,
                       labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform
        assert wf.uniform_spacing() == 2.5

    def test_offsets_array_sets_custom_mode(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall,
                       offsets=[0.0, 1.5, 4.0], labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Custom
        assert list(wf.offsets()) == [0.0, 1.5, 4.0]

    def test_offsets_none_default(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall, labels=["a", "b", "c"])
        assert wf.offset_mode() == WaterfallOffsetMode.Uniform
        assert wf.uniform_spacing() == 1.0

    def test_offsets_array_wrong_len_raises(self, plot):
        x, y = _make_2d(cols=3)
        with pytest.raises(ValueError):
            plot.plot(x, y, graph_type=GraphType.Waterfall,
                      offsets=[0.0, 1.0], labels=["a", "b", "c"])  # too short

    def test_normalize_gain_kwargs(self, plot):
        x, y = _make_2d(cols=3)
        wf = plot.plot(x, y, graph_type=GraphType.Waterfall,
                       normalize=False, gain=3.0, labels=["a", "b", "c"])
        assert wf.normalize() is False
        assert wf.gain() == 3.0

    def test_callable_variant(self, plot):
        from SciQLopPlots import SciQLopWaterfallGraphFunction

        def cb(start, stop):
            n = 50
            x = np.linspace(start, stop, n).astype(np.float64)
            y = np.column_stack([np.sin(x), np.cos(x)]).astype(np.float64)
            return x, y

        wf = plot.plot(cb, graph_type=GraphType.Waterfall, labels=["a", "b"])
        assert isinstance(wf, SciQLopWaterfallGraphFunction)
```

- [ ] **Step 3: Build + run tests**

Run: `PATH="..." meson compile -C build && cp ... && cd /var/home/jeandet/Documents/prog && LD_LIBRARY_PATH=... SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py -v`
Expected: all new dispatch tests pass.

- [ ] **Step 4: Commit**

```bash
git add SciQLopPlots/__init__.py tests/integration/test_waterfall.py
git commit -m "feat(python): plot(graph_type=Waterfall) with offsets/normalize/gain kwargs"
```

---

## Task 12: Reactive `.on.*` properties

**Files:**
- Modify: `SciQLopPlots/__init__.py`

- [ ] **Step 1: Register properties**

In `SciQLopPlots/__init__.py`, after the existing `register_property` calls for `SciQLopGraphInterface`, add:
```python
from .SciQLopPlotsBindings import SciQLopWaterfallGraph

register_property(
    SciQLopWaterfallGraph, "offset_mode",
    signal_name="offset_mode_changed",
    getter_name="offset_mode", setter_name="set_offset_mode",
    property_type="enum",
)
register_property(
    SciQLopWaterfallGraph, "spacing",
    signal_name="uniform_spacing_changed",
    getter_name="uniform_spacing", setter_name="set_uniform_spacing",
    property_type="float",
)
register_property(
    SciQLopWaterfallGraph, "offsets",
    signal_name="offsets_changed",
    getter_name="offsets", setter_name="set_offsets",
    property_type="array",
)
register_property(
    SciQLopWaterfallGraph, "normalize",
    signal_name="normalize_changed",
    getter_name="normalize", setter_name="set_normalize",
    property_type="bool",
)
register_property(
    SciQLopWaterfallGraph, "gain",
    signal_name="gain_changed",
    getter_name="gain", setter_name="set_gain",
    property_type="float",
)

SciQLopWaterfallGraph.on = OnDescriptor()
```

- [ ] **Step 2: Write reactive-pipeline test**

Append to `tests/integration/test_waterfall.py`:
```python
class TestWaterfallReactive:
    def test_on_gain_emits(self, plot):
        from PySide6.QtCore import QEventLoop, QTimer

        wf = plot.add_waterfall("w")
        received = []
        wf.gain_changed.connect(lambda v: received.append(v))
        wf.set_gain(2.5)
        QApplication.processEvents()
        assert received == [2.5]

    def test_on_gain_pipeline(self, plot):
        wf_src = plot.add_waterfall("src")
        wf_dst = plot.add_waterfall("dst")
        wf_src.on.gain >> wf_dst.on.gain
        wf_src.set_gain(3.14)
        QApplication.processEvents()
        assert wf_dst.gain() == 3.14

    def test_on_normalize_pipeline(self, plot):
        wf_src = plot.add_waterfall("src")
        wf_dst = plot.add_waterfall("dst")
        wf_src.on.normalize >> wf_dst.on.normalize
        wf_src.set_normalize(False)
        QApplication.processEvents()
        assert wf_dst.normalize() is False
```

- [ ] **Step 3: Build + test**

Run: `PATH="..." meson compile -C build && cp ...` — no C++ changed here, only Python, but keep for consistency; then run:
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py::TestWaterfallReactive -v
```
Expected: all pass.

- [ ] **Step 4: Commit**

```bash
git add SciQLopPlots/__init__.py tests/integration/test_waterfall.py
git commit -m "feat(waterfall): register reactive .on.* properties"
```

---

## Task 13: Tooltip — wire `SciQLopCrosshair` to call `raw_value_at`

**Files:**
- Modify: `src/SciQLopCrosshair.cpp`

**Context:** `SciQLopCrosshair::build_tooltip_html(key, pixelPos)` at `src/SciQLopCrosshair.cpp:176` iterates `m_plot->plottableCount()` raw QCP plottables. Inside it, a `qobject_cast<QCPMultiGraph*>(plottable)` branch (line 208) formats one line per component using `locator.componentValues()[c]`. For `QCPWaterfallGraph` (a `QCPMultiGraph` subclass) those values are *post-transform* (offset + normalize + gain applied by `QCPWaterfallDataAdapter`). We need to intercept this branch for the waterfall case and format with pre-transform values via the SciQLop wrapper's `raw_value_at`.

To find the SciQLop wrapper from the raw QCP plottable, walk the plot's children: each `SciQLopWaterfallGraph` is a `QObject` child of its parent `QCustomPlot` and owns a `QCPWaterfallGraph` via `_multiGraph`. The findChildren-based lookup matches what other wrapper-aware code paths in this repo do.

- [ ] **Step 1: Add include + helper at top of the .cpp**

In `src/SciQLopCrosshair.cpp`, add near the existing includes:
```cpp
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"
```

Above `build_tooltip_html`, add a private helper:
```cpp
namespace
{
SciQLopWaterfallGraph* find_waterfall_wrapper(QCustomPlot* plot, QCPWaterfallGraph* raw)
{
    for (auto* w : plot->findChildren<SciQLopWaterfallGraph*>())
    {
        // Accessor: waterfall_graph() returns the owned QCPWaterfallGraph*.
        if (w->waterfall_graph() == raw)
            return w;
    }
    return nullptr;
}
}
```

`waterfall_graph()` is currently `protected` in Task 4/5. Promote it to `public` in `SciQLopWaterfallGraph.hpp` (the `waterfall_graph()` accessor only — `create_multi_graph` stays protected):

```cpp
public:
    QCPWaterfallGraph* waterfall_graph() const noexcept
    {
        return static_cast<QCPWaterfallGraph*>(_multiGraph);
    }
```
(Move this block from `protected:` to `public:`; remove the duplicate from the protected section.)

- [ ] **Step 2: Inside the `QCPMultiGraph` branch (line 208), intercept waterfall case**

Change the branch at `src/SciQLopCrosshair.cpp:208-225` from:
```cpp
if (auto* mg = qobject_cast<QCPMultiGraph*>(plottable))
{
    if (!locator.locateAtKey(mg, plKey, plValue))
        continue;
    const auto& vals = locator.componentValues();
    for (int c = 0; c < mg->componentCount(); ++c)
    {
        const auto& comp = mg->component(c);
        if (!comp.visible)
            continue;
        if (c >= vals.size() || std::isnan(vals[c]))
            continue;
        const QString color = comp.pen.color().name();
        lines += QString::fromStdString(
            fmt::format("<span style='color:{};'>&#9632;</span> {}: <b>{:.6g}</b><br/>",
                color.toStdString(), comp.name.toStdString(), vals[c]));
    }
}
```
to:
```cpp
if (auto* mg = qobject_cast<QCPMultiGraph*>(plottable))
{
    if (!locator.locateAtKey(mg, plKey, plValue))
        continue;
    const auto& vals = locator.componentValues();

    // Resolve the SciQLop wrapper for waterfall pre-transform readback.
    auto* wfRaw = qobject_cast<QCPWaterfallGraph*>(mg);
    SciQLopWaterfallGraph* wfWrapper = nullptr;
    if (wfRaw)
        wfWrapper = find_waterfall_wrapper(m_plot, wfRaw);

    for (int c = 0; c < mg->componentCount(); ++c)
    {
        const auto& comp = mg->component(c);
        if (!comp.visible)
            continue;
        double readout;
        if (wfWrapper)
        {
            readout = wfWrapper->raw_value_at(c, plKey);
            if (std::isnan(readout))
                continue;
        }
        else
        {
            if (c >= vals.size() || std::isnan(vals[c]))
                continue;
            readout = vals[c];
        }
        const QString color = comp.pen.color().name();
        lines += QString::fromStdString(
            fmt::format("<span style='color:{};'>&#9632;</span> {}: <b>{:.6g}</b><br/>",
                color.toStdString(), comp.name.toStdString(), readout));
    }
}
```

Non-waterfall `QCPMultiGraph` (i.e. line graphs) keeps the existing `vals[c]` path verbatim. Only waterfall is intercepted. The `else` branch (non-multi-graph) is unchanged.

- [ ] **Step 3: Write integration-level tooltip tests**

Append to `tests/integration/test_waterfall.py`:
```python
class TestWaterfallTooltip:
    """The crosshair tooltip is driven by mouse events + HTML rendering. The
    integration-level guarantee we test here is that `raw_value_at` (the
    function the crosshair calls internally) returns the pre-transform buffer
    value, not the post-normalize/gain/offset value that `vals[c]` would
    carry."""

    def test_raw_value_is_pre_transform(self, plot):
        wf = plot.add_waterfall("w", labels=["c0"])
        x = np.linspace(0, 1, 11).astype(np.float64)
        y = np.full_like(x, 2.0)
        wf.set_data(x, y)
        wf.set_normalize(True)     # would scale post-transform Y
        wf.set_gain(10.0)          # would scale post-transform Y
        wf.set_uniform_spacing(5.0)  # would shift post-transform Y
        # Raw readback bypasses all three transforms.
        assert wf.raw_value_at(0, 0.5) == pytest.approx(2.0)
```

- [ ] **Step 4: Build + full regression**

Run:
```bash
PATH="..." meson compile -C build && cp ... && \
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_waterfall.py \
    SciQLopPlots/tests/integration/test_graph_api.py -v
```
Expected: all pass. Line-graph tooltips (the non-waterfall `QCPMultiGraph` path) are untouched — that's why the `wfWrapper` intercept is guarded on `qobject_cast<QCPWaterfallGraph*>`.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp \
        src/SciQLopCrosshair.cpp tests/integration/test_waterfall.py
git commit -m "feat(crosshair): route waterfall tooltip through raw_value_at"
```

---

## Task 14: `SciQLopWaterfallDelegate` (Inspector)

**Files:**
- Create: `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp`
- Create: `src/SciQLopWaterfallDelegate.cpp`
- Modify: `SciQLopPlots/meson.build`

- [ ] **Step 1: Study `SciQLopColorMapDelegate` for the pattern**

Read `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp` and `src/SciQLopColorMapDelegate.cpp`. Note: class derives from `PropertyDelegate`, builds QWidgets, binds signals. Copy this structure.

- [ ] **Step 2: Create the header**

Write `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp`:
```cpp
#pragma once
#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"
#include "SciQLopPlots/Plotables/SciQLopWaterfallGraph.hpp"

class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QWidget;
class QScrollArea;
class QVBoxLayout;

class SciQLopWaterfallDelegate : public PropertyDelegateBase
{
    Q_OBJECT
    QPointer<SciQLopWaterfallGraph> m_target;

    QComboBox* m_modeCombo = nullptr;
    QDoubleSpinBox* m_spacingSpin = nullptr;
    QScrollArea* m_offsetsScroll = nullptr;
    QVBoxLayout* m_offsetsLayout = nullptr;
    QList<QDoubleSpinBox*> m_offsetsSpins;
    QCheckBox* m_normalizeCheck = nullptr;
    QDoubleSpinBox* m_gainSpin = nullptr;

    void rebuild_offsets_editor();
    void refresh_mode_enabled_state();

public:
    explicit SciQLopWaterfallDelegate(SciQLopWaterfallGraph* target, QWidget* parent = nullptr);
};
```

- [ ] **Step 3: Implement the delegate**

Write `src/SciQLopWaterfallDelegate.cpp`:
```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>

SciQLopWaterfallDelegate::SciQLopWaterfallDelegate(SciQLopWaterfallGraph* target, QWidget* parent)
    : PropertyDelegateBase(parent)
    , m_target(target)
{
    auto* outer = new QVBoxLayout(this);

    auto* modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel("Offset mode"));
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Uniform", QVariant::fromValue(WaterfallOffsetMode::Uniform));
    m_modeCombo->addItem("Custom", QVariant::fromValue(WaterfallOffsetMode::Custom));
    m_modeCombo->setCurrentIndex(target->offset_mode() == WaterfallOffsetMode::Uniform ? 0 : 1);
    modeRow->addWidget(m_modeCombo);
    outer->addLayout(modeRow);

    auto* spacingRow = new QHBoxLayout();
    spacingRow->addWidget(new QLabel("Uniform spacing"));
    m_spacingSpin = new QDoubleSpinBox;
    m_spacingSpin->setRange(-1e9, 1e9);
    m_spacingSpin->setDecimals(4);
    m_spacingSpin->setValue(target->uniform_spacing());
    spacingRow->addWidget(m_spacingSpin);
    outer->addLayout(spacingRow);

    outer->addWidget(new QLabel("Offsets"));
    m_offsetsScroll = new QScrollArea;
    m_offsetsScroll->setWidgetResizable(true);
    auto* offsetsHost = new QWidget;
    m_offsetsLayout = new QVBoxLayout(offsetsHost);
    m_offsetsScroll->setWidget(offsetsHost);
    m_offsetsScroll->setMaximumHeight(120);
    outer->addWidget(m_offsetsScroll);
    rebuild_offsets_editor();

    auto* normRow = new QHBoxLayout();
    m_normalizeCheck = new QCheckBox("Normalize per trace");
    m_normalizeCheck->setChecked(target->normalize());
    normRow->addWidget(m_normalizeCheck);
    outer->addLayout(normRow);

    auto* gainRow = new QHBoxLayout();
    gainRow->addWidget(new QLabel("Gain"));
    m_gainSpin = new QDoubleSpinBox;
    m_gainSpin->setRange(-1e6, 1e6);
    m_gainSpin->setDecimals(3);
    m_gainSpin->setValue(target->gain());
    gainRow->addWidget(m_gainSpin);
    outer->addLayout(gainRow);

    refresh_mode_enabled_state();

    // Bind widgets → model.
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                if (!m_target) return;
                m_target->set_offset_mode(m_modeCombo->currentData().value<WaterfallOffsetMode>());
                refresh_mode_enabled_state();
            });
    connect(m_spacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { if (m_target) m_target->set_uniform_spacing(v); });
    connect(m_normalizeCheck, &QCheckBox::toggled,
            this, [this](bool v) { if (m_target) m_target->set_normalize(v); });
    connect(m_gainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) { if (m_target) m_target->set_gain(v); });

    // Bind model → widgets (one-way refresh on model change).
    connect(target, &SciQLopWaterfallGraph::offset_mode_changed,
            this, [this](WaterfallOffsetMode m) {
                m_modeCombo->setCurrentIndex(m == WaterfallOffsetMode::Uniform ? 0 : 1);
                refresh_mode_enabled_state();
            });
    connect(target, &SciQLopWaterfallGraph::uniform_spacing_changed,
            this, [this](double v) { m_spacingSpin->setValue(v); });
    connect(target, &SciQLopWaterfallGraph::offsets_changed,
            this, [this](const QVector<double>&) { rebuild_offsets_editor(); });
    connect(target, &SciQLopWaterfallGraph::normalize_changed,
            this, [this](bool v) { m_normalizeCheck->setChecked(v); });
    connect(target, &SciQLopWaterfallGraph::gain_changed,
            this, [this](double v) { m_gainSpin->setValue(v); });
    connect(target, &SciQLopGraphInterface::data_changed,
            this, [this]() { rebuild_offsets_editor(); });
}

void SciQLopWaterfallDelegate::rebuild_offsets_editor()
{
    // Clear prior widgets.
    for (auto* s : m_offsetsSpins) s->deleteLater();
    m_offsetsSpins.clear();

    if (!m_target) return;
    const auto current = m_target->offsets();
    const int n = static_cast<int>(m_target->line_count());
    QVector<double> padded = current;
    while (padded.size() < n) padded.append(0.0);
    padded.resize(n);

    for (int i = 0; i < n; ++i)
    {
        auto* spin = new QDoubleSpinBox;
        spin->setRange(-1e9, 1e9);
        spin->setDecimals(4);
        spin->setValue(padded[i]);
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, i](double) {
                    if (!m_target) return;
                    QVector<double> v;
                    v.reserve(m_offsetsSpins.size());
                    for (auto* s : m_offsetsSpins) v.append(s->value());
                    m_target->set_offsets(v);
                });
        m_offsetsLayout->addWidget(spin);
        m_offsetsSpins.append(spin);
    }
}

void SciQLopWaterfallDelegate::refresh_mode_enabled_state()
{
    if (!m_target) return;
    const bool uniform = m_target->offset_mode() == WaterfallOffsetMode::Uniform;
    m_spacingSpin->setEnabled(uniform);
    m_offsetsScroll->setEnabled(!uniform);
}
```

(Base class `PropertyDelegateBase` is confirmed from `include/SciQLopPlots/Inspector/PropertyDelegateBase.hpp`, matching `SciQLopColorMapDelegate`'s pattern.)

- [ ] **Step 4: Add to meson.build**

Add the header to `headers` + `moc_headers`, and the cpp to sources. Path:
```meson
project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp',
'../src/SciQLopWaterfallDelegate.cpp',
```

- [ ] **Step 5: Build**

Run: `PATH="..." meson compile -C build`
Expected: build succeeds.

- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp \
        src/SciQLopWaterfallDelegate.cpp SciQLopPlots/meson.build
git commit -m "feat(inspector): SciQLopWaterfallDelegate for runtime knob editing"
```

---

## Task 15: Register the delegate

**Files:**
- Modify: `src/Registrations.cpp`

- [ ] **Step 1: Include the delegate header**

At top of `src/Registrations.cpp`:
```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopWaterfallDelegate.hpp"
```

- [ ] **Step 2: Register in the delegates block**

Find the existing `delegates.register_type<SciQLopColorMapDelegate>();` call (around line 206) and add below it:
```cpp
delegates.register_type<SciQLopWaterfallDelegate>();
```

- [ ] **Step 3: Build + inspector-model test regression**

Run:
```bash
PATH="..." meson compile -C build && cp ... && \
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_inspector_model.py -v
```
Expected: passes.

- [ ] **Step 4: Commit**

```bash
git add src/Registrations.cpp
git commit -m "feat(inspector): register SciQLopWaterfallDelegate"
```

---

## Task 16: Gallery demo — channel stack + record section

**Files:**
- Create: `tests/manual-tests/basics/waterfall.py`

- [ ] **Step 1: Write the demo script**

Write `tests/manual-tests/basics/waterfall.py`:
```python
"""Waterfall graph demo — channel stack and record section."""
import sys
import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QApplication, QWidget, QVBoxLayout, QHBoxLayout,
                                QTabWidget, QSlider, QCheckBox, QLabel)
from SciQLopPlots import SciQLopPlot, GraphType, WaterfallOffsetMode


def make_channel_stack_tab():
    n_channels = 8
    n_samples = 2000
    t = np.linspace(0, 10, n_samples).astype(np.float64)
    rng = np.random.default_rng(42)
    y = np.zeros((n_samples, n_channels), dtype=np.float64)
    for i in range(n_channels):
        f = 0.5 + 0.3 * i
        y[:, i] = np.sin(2 * np.pi * f * t) + 0.15 * rng.standard_normal(n_samples)

    widget = QWidget()
    layout = QVBoxLayout(widget)
    plot = SciQLopPlot()
    layout.addWidget(plot)

    wf = plot.plot(t, y, graph_type=GraphType.Waterfall,
                   offsets=2.5, normalize=True, gain=1.0,
                   labels=[f"ch{i}" for i in range(n_channels)])

    gain_row = QHBoxLayout()
    gain_row.addWidget(QLabel("Gain"))
    gain = QSlider(Qt.Horizontal)
    gain.setRange(10, 500)
    gain.setValue(100)
    gain_label = QLabel("1.00")
    def on_gain(v):
        g = v / 100.0
        wf.set_gain(g)
        gain_label.setText(f"{g:.2f}")
    gain.valueChanged.connect(on_gain)
    gain_row.addWidget(gain)
    gain_row.addWidget(gain_label)
    layout.addLayout(gain_row)

    norm = QCheckBox("Normalize per trace")
    norm.setChecked(True)
    norm.toggled.connect(wf.set_normalize)
    layout.addWidget(norm)
    return widget


def make_record_section_tab():
    n_traces = 16
    n_samples = 4000
    t = np.linspace(0, 20, n_samples).astype(np.float64)
    rng = np.random.default_rng(7)
    distances = np.sort(rng.uniform(0, 100, n_traces))
    y = np.zeros((n_samples, n_traces), dtype=np.float64)
    for i in range(n_traces):
        onset = distances[i] / 5.0  # P-wave-like arrival
        pulse = np.exp(-((t - onset) / 0.3) ** 2) * np.sin(2 * np.pi * 2 * (t - onset))
        y[:, i] = pulse + 0.05 * rng.standard_normal(n_samples)

    widget = QWidget()
    layout = QVBoxLayout(widget)
    plot = SciQLopPlot()
    layout.addWidget(plot)

    wf = plot.plot(t, y, graph_type=GraphType.Waterfall,
                   offsets=distances.tolist(),
                   normalize=False, gain=3.0,
                   labels=[f"stn{i:02d}" for i in range(n_traces)])
    return widget


def main():
    app = QApplication.instance() or QApplication(sys.argv)
    tabs = QTabWidget()
    tabs.addTab(make_channel_stack_tab(), "Channel stack")
    tabs.addTab(make_record_section_tab(), "Record section")
    tabs.resize(900, 700)
    tabs.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Smoke-run**

Run: `cd /var/home/jeandet/Documents/prog && LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib SciQLop/.venv/bin/python SciQLopPlots/tests/manual-tests/basics/waterfall.py`
Expected: window opens with two tabs; both render without errors; gain slider and normalize checkbox work; hovering a trace shows a tooltip with the raw value at the cursor key.

- [ ] **Step 3: Commit**

```bash
git add tests/manual-tests/basics/waterfall.py
git commit -m "test(manual): waterfall demo — channel stack + record section tabs"
```

---

## Task 17: Link demo into gallery.py

**Files:**
- Modify: `tests/manual-tests/gallery.py`

- [ ] **Step 1: Inspect existing gallery entries**

Run: `grep -n "histogram2d\|colormap\|import" tests/manual-tests/gallery.py | head -20`
Note the import + registration pattern for existing demos.

- [ ] **Step 2: Add waterfall entry**

Following the same pattern as `histogram2d`, add a `waterfall` import and register the two tabs (or a single entry that spawns the tabbed window). Match the existing style exactly.

- [ ] **Step 3: Smoke-run gallery**

Run: `cd /var/home/jeandet/Documents/prog && LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib SciQLop/.venv/bin/python SciQLopPlots/tests/manual-tests/gallery.py`
Expected: gallery lists "waterfall"; clicking opens the demo.

- [ ] **Step 4: Commit**

```bash
git add tests/manual-tests/gallery.py
git commit -m "test(gallery): link waterfall demo"
```

---

## Task 18: Final regression + unity build check

**Goal:** Prove the whole thing passes end-to-end and doesn't regress anything.

- [ ] **Step 1: Full integration suite**

Run:
```bash
cd /var/home/jeandet/Documents/prog && \
LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/ -v
```
Expected: all pass. If any regressions surface in tests unrelated to waterfall, stop and investigate — this is the no-breakage gate.

- [ ] **Step 2: Unity build check**

Run:
```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots && \
meson setup build-unity --buildtype=debug --unity=on --force-fallback-for=NeoQCP && \
PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:/var/home/jeandet/Documents/prog/SciQLop/.venv/bin:/home/jeandet/.local/bin:/usr/bin:/usr/local/bin:$PATH" \
meson compile -C build-unity
```
Expected: unity build succeeds. Known pitfalls to watch for (per `recent_work.md` and `dsp_module.md`):
- `auto` vs `decltype` extern mismatch in header-defined templates.
- Variables/lambdas named `emit` colliding with Qt macro.

If unity fails: fix the collision (rename, add explicit `decltype`, etc.) rather than disabling unity.

- [ ] **Step 3: Exported-symbol diff on SciQLopLineGraph**

Compare ABI before/after the base refactor:
```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots && \
nm -D --demangle build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
    | grep -E "SciQLopLineGraph" | sort > /tmp/linegraph-post.txt && \
git stash && \
meson compile -C build && \
nm -D --demangle build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
    | grep -E "SciQLopLineGraph" | sort > /tmp/linegraph-pre.txt && \
git stash pop && \
meson compile -C build && \
diff /tmp/linegraph-pre.txt /tmp/linegraph-post.txt
```
Expected: diff shows only expected inheritance-related symbol changes (base-class function pointers), no public signature changes. If `set_data`, `data`, `x_axis`, `y_axis`, `line_count`, `create_graphs` now resolve to the base class that's expected; any removed public signature is a regression.

- [ ] **Step 4: Commit final state (if any cleanup needed)**

If unity build required fixes, commit them:
```bash
git add -u
git commit -m "fix(build): unity build compatibility for SciQLopWaterfallGraph"
```

Otherwise, this task is a verification step with no commit.

---

## Acceptance Checklist

Before declaring the feature done, verify all of these:

- [ ] `plot.plot(x, y, graph_type=GraphType.Waterfall, offsets=<float|array|None>, normalize=<bool>, gain=<float>)` works for 2D y of any dtype.
- [ ] `plot.plot(callback, graph_type=GraphType.Waterfall, ...)` returns a `SciQLopWaterfallGraphFunction`.
- [ ] `plot.add_waterfall(name, labels, colors)` works and the returned object accepts `set_data`, `set_offsets`, `set_normalize`, `set_gain`, `set_uniform_spacing`, `set_offset_mode`.
- [ ] All five reactive properties (`offset_mode`, `spacing`, `offsets`, `normalize`, `gain`) work in pipelines via `.on.<name>`.
- [ ] Inspector shows the waterfall with editable controls for all five knobs.
- [ ] Hovering a waterfall trace shows a tooltip with the raw signal amplitude (not the offset-adjusted displayed Y).
- [ ] Full `tests/integration/` suite passes — including `test_line_graph_regression.py` and all pre-existing tests.
- [ ] Unity build passes.
- [ ] Gallery demo (`tests/manual-tests/gallery.py`) lists and runs the waterfall demo.
