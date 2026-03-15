# NeoQCP Migration Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate SciQLopPlots from QCPGraph/QCPColorMap to QCPMultiGraph/QCPColorMap2 with multi-type numpy support.

**Architecture:** PyBuffer becomes dtype-aware (void* + format code). SciQLopLineGraph uses QCPMultiGraph with zero-copy viewData(). SciQLopColorMap uses QCPColorMap2 with built-in async resampling. Resampler infrastructure removed for graphs and colormaps.

**Tech Stack:** C++20, Qt6, NeoQCP (QCPMultiGraph, QCPColorMap2, QCPSoAMultiDataSource, QCPSoADataSource2D), Python/numpy, Shiboken6

**Spec:** `docs/superpowers/specs/2026-03-14-neoqcp-migration-design.md`

---

## Chunk 1: PyBuffer Multi-Type + DtypeDispatch

### Task 1: Add multi-type support to PyBuffer

**Files:**
- Modify: `src/PythonInterface.cpp` (lines 208-245: `_PyBuffer_impl::init_buffer` and `PyBuffer` accessors)
- Modify: `include/SciQLopPlots/Python/PythonInterface.hpp` (lines 266-347: `PyBuffer` class)

- [ ] **Step 1: Write failing integration test for float32 PyBuffer**

Create `tests/integration/test_multi_dtype.py`:

```python
"""Tests for multi-type numpy support."""
import pytest
import numpy as np


class TestMultiDtype:

    def test_line_float32(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = np.sin(x).astype(np.float32)
        g = plot.line(x, y)
        assert g is not None

    def test_line_int16(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = (np.sin(x) * 1000).astype(np.int16)
        g = plot.line(x, y)
        assert g is not None

    def test_line_uint8(self, plot):
        x = np.linspace(0, 10, 100).astype(np.float64)
        y = ((np.sin(x) + 1) * 127).astype(np.uint8)
        g = plot.line(x, y)
        assert g is not None

    def test_colormap_float32(self, plot, sample_colormap_data):
        x, y, z = sample_colormap_data
        z32 = z.astype(np.float32)
        g = plot.plot(x, y, z32)
        assert g is not None

    def test_line_float64_still_works(self, plot, sample_data):
        x, y = sample_data
        g = plot.line(x, y)
        assert g is not None
        data = g.data()
        assert len(data) >= 2
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && meson test` or `pytest tests/integration/test_multi_dtype.py -v`
Expected: FAIL — `_PyBuffer_impl` throws "Buffer must be double type" for non-double arrays.

- [ ] **Step 3: Modify `_PyBuffer_impl::init_buffer` to accept all numeric types**

In `src/PythonInterface.cpp`, change `init_buffer()`:

```cpp
inline void init_buffer(PyObject* obj)
{
    this->py_obj.set_obj(obj);
    {
        auto scoped_gil = PyAutoScopedGIL();
        this->is_valid = PyObject_GetBuffer(obj, &this->buffer,
                                            PyBUF_SIMPLE | PyBUF_READ | PyBUF_ANY_CONTIGUOUS
                                                | PyBUF_FORMAT)
            == 0;
    }
    if (!this->is_valid)
        throw std::runtime_error("Failed to get buffer from object");

    // Accept any numeric format code
    static constexpr std::string_view numeric_formats = "bBhHiIlLqQfd";
    if (this->buffer.format == nullptr
        || numeric_formats.find(this->buffer.format[0]) == std::string_view::npos)
        throw std::runtime_error("Buffer must be a numeric type");

    this->is_row_major = PyBuffer_IsContiguous(&this->buffer, 'C') == 1;
    if (this->buffer.ndim > 0)
    {
        this->shape.resize(this->buffer.ndim);
        std::copy_n(this->buffer.shape, std::size(this->shape), std::begin(this->shape));
    }
    else
    {
        this->shape.resize(1);
        this->shape[0] = this->buffer.len / this->buffer.itemsize;
    }
}
```

- [ ] **Step 4: Add `format_code()`, `raw_data()`, `item_size()` to PyBuffer**

In `include/SciQLopPlots/Python/PythonInterface.hpp`, add to the `PyBuffer` class (after `row_major()`):

```cpp
char format_code() const;
void* raw_data() const;
std::size_t item_size() const;
```

In `src/PythonInterface.cpp`, add implementations:

```cpp
char PyBuffer::format_code() const
{
    if (is_valid() && _impl->buffer.format)
        return _impl->buffer.format[0];
    return '\0';
}

void* PyBuffer::raw_data() const
{
    if (is_valid())
        return _impl->buffer.buf;
    return nullptr;
}

std::size_t PyBuffer::item_size() const
{
    if (is_valid())
        return _impl->buffer.itemsize;
    return 0;
}
```

- [ ] **Step 5: Make `double* data()` throw for non-double buffers**

In `src/PythonInterface.cpp`, change `PyBuffer::data()`:

```cpp
double* PyBuffer::data() const
{
    if (is_valid())
    {
        if (_impl->buffer.format[0] != 'd')
            throw std::runtime_error("PyBuffer::data() called on non-double buffer; use raw_data() + format_code()");
        return reinterpret_cast<double*>(_impl->buffer.buf);
    }
    return nullptr;
}
```

- [ ] **Step 6: Remove `std::size`/`std::cbegin`/`std::cend` specializations**

In `include/SciQLopPlots/Python/PythonInterface.hpp`, remove the `namespace std` block (lines 351-373) containing `size`, `cbegin`, `cend` for `PyBuffer`. Keep the `size` specialization for `ArrayViewBase` if used by the curve path.

- [ ] **Step 7: Build and verify existing tests still pass**

Run: `meson compile -C build && meson test -C build`
Expected: All existing float64 tests pass. The new multi-dtype tests will still fail (SciQLopLineGraph still uses the resampler which expects double). That's expected — we fix it in Task 3.

- [ ] **Step 8: Commit**

```bash
git add src/PythonInterface.cpp include/SciQLopPlots/Python/PythonInterface.hpp tests/integration/test_multi_dtype.py
git commit -m "feat(pybuffer): accept all numeric numpy dtypes, add format_code/raw_data/item_size"
```

---

### Task 2: Create DtypeDispatch utility

**Files:**
- Create: `include/SciQLopPlots/Python/DtypeDispatch.hpp`

- [ ] **Step 1: Create the dispatch header**

```cpp
#pragma once
#include <cstdint>
#include <stdexcept>
#include <type_traits>

template <typename F>
auto dispatch_dtype(char format_code, F&& func)
{
    switch (format_code)
    {
        case 'f': return func(std::type_identity<float> {});
        case 'd': return func(std::type_identity<double> {});
        case 'b': return func(std::type_identity<int8_t> {});
        case 'B': return func(std::type_identity<uint8_t> {});
        case 'h': return func(std::type_identity<int16_t> {});
        case 'H': return func(std::type_identity<uint16_t> {});
        case 'i': return func(std::type_identity<int32_t> {});
        case 'I': return func(std::type_identity<uint32_t> {});
        case 'l': return func(std::type_identity<long> {});
        case 'L': return func(std::type_identity<unsigned long> {});
        case 'q': return func(std::type_identity<long long> {});
        case 'Q': return func(std::type_identity<unsigned long long> {});
        default: throw std::runtime_error("Unsupported numpy dtype");
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add include/SciQLopPlots/Python/DtypeDispatch.hpp
git commit -m "feat: add dtype dispatch utility for numpy format codes"
```

---

## Chunk 2: SciQLopGraphComponent + SciQLopLineGraph Migration

### Task 3: Add MultiGraphRef to SciQLopGraphComponent

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`
- Modify: `src/SciQLopGraphComponent.cpp` (if it exists, check first)

The `SciQLopGraphComponent` variant `graph_or_curve` needs a `MultiGraphRef` arm.

- [ ] **Step 1: Add MultiGraphRef struct and update variant**

In `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`:

Add forward declaration and struct before the class:

```cpp
class QCPMultiGraph;

struct MultiGraphRef {
    QCPMultiGraph* graph = nullptr;
    int componentIndex = -1;
};
```

Change the variant type:

```cpp
using graph_or_curve_or_multi = std::variant<QCPGraph*, QCPCurve*, MultiGraphRef, std::monostate>;
```

Add a new constructor:

```cpp
SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex, QObject* parent = nullptr);
```

Store a component index member:

```cpp
int m_componentIndex = -1;
```

Update `to_variant()` to return `MultiGraphRef` when `m_componentIndex >= 0`:

```cpp
inline graph_or_curve_or_multi to_variant() const
{
    if (m_componentIndex >= 0)
        return MultiGraphRef{qobject_cast<QCPMultiGraph*>(m_plottable.data()), m_componentIndex};
    if (auto graph = qobject_cast<QCPGraph*>(m_plottable); graph != nullptr)
        return graph;
    else if (auto curve = qobject_cast<QCPCurve*>(m_plottable); curve != nullptr)
        return curve;
    return std::monostate();
}
```

- [ ] **Step 2: Add MultiGraphRef visitor arms to all accessors**

Each visitor-based accessor (`set_pen`, `set_line_style`, `set_marker_shape`, `set_marker_pen`, `line_style`, `marker_shape`, `marker_pen`) needs a `MultiGraphRef` arm. These delegate to `QCPMultiGraph::component(index)` and `QCPGraphComponent` fields.

Example for `set_color`:

```cpp
[color](MultiGraphRef ref) {
    auto& comp = ref.graph->component(ref.componentIndex);
    comp.pen.setColor(color);
},
```

Example for `set_visible`:

```cpp
[visible](MultiGraphRef ref) {
    ref.graph->component(ref.componentIndex).visible = visible;
},
```

Include `<plottables/plottable-multigraph.h>` in the implementation file for access to `QCPMultiGraph`.

- [ ] **Step 3: Implement the new constructor and guard the destructor**

New constructor:

```cpp
SciQLopGraphComponent::SciQLopGraphComponent(QCPMultiGraph* multiGraph, int componentIndex, QObject* parent)
    : SciQLopGraphComponentInterface(parent)
    , m_plottable(multiGraph)
    , m_componentIndex(componentIndex)
{
}
```

Guard the destructor to skip `removePlottable` for MultiGraphRef components (since all N components share the same QCPMultiGraph, which is removed by `SciQLopLineGraph::clear_graphs()` instead):

```cpp
SciQLopGraphComponent::~SciQLopGraphComponent()
{
    // For MultiGraphRef, the parent SciQLopLineGraph owns the QCPMultiGraph
    if (m_componentIndex >= 0)
        return;

    // Original cleanup for QCPGraph/QCPCurve...
    // (existing destructor body)
}
```

- [ ] **Step 4: Build to verify compilation**

Run: `meson compile -C build`
Expected: Compiles cleanly. No tests to run yet for MultiGraphRef specifically.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp src/SciQLopGraphComponent.cpp
git commit -m "feat(component): add MultiGraphRef variant for QCPMultiGraph support"
```

---

### Task 4: Update QCPAbstractPlottableWrapper for 1:N model

**Files:**
- Modify: `include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp`

- [ ] **Step 1: Deduplicate `qcp_plottables()` for 1:N model**

`qcp_plottables()` iterates `m_components` and appends `component->plottable()` for each. With MultiGraphRef, all N components point to the same `QCPMultiGraph`, producing N duplicate pointers. Fix by deduplicating:

```cpp
const QList<QCPAbstractPlottable*> qcp_plottables() const noexcept
{
    QList<QCPAbstractPlottable*> plottables;
    for (auto component : m_components)
    {
        auto p = component->plottable();
        if (p && !plottables.contains(p))
            plottables.append(p);
    }
    return plottables;
}
```

- [ ] **Step 2: Update `_set_selected` free function for QCPMultiGraph**

Add a `QCPMultiGraph*` arm:

```cpp
inline void set_selected(QCPAbstractPlottable* plottable, bool selected)
{
    if (auto graph = dynamic_cast<QCPGraph*>(plottable); graph != nullptr)
    {
        _set_selected(graph, selected);
    }
    else if (auto curve = dynamic_cast<QCPCurve*>(plottable); curve != nullptr)
    {
        _set_selected(curve, selected);
    }
    // QCPMultiGraph selection is handled per-component via SciQLopGraphComponent
}
```

- [ ] **Step 3: Build to verify**

Run: `meson compile -C build`

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp
git commit -m "feat(wrapper): update selection handling for QCPMultiGraph"
```

---

### Task 5: Migrate SciQLopLineGraph to QCPMultiGraph

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`
- Modify: `src/SciQLopLineGraph.cpp`
- Modify: `SciQLopPlots/meson.build` (remove SciQLopLineGraphResampler.cpp from sources)

This is the core migration. Replace the list-of-QCPGraph + resampler pattern with a single QCPMultiGraph + direct viewData.

- [ ] **Step 1: Rewrite SciQLopLineGraph.hpp**

Replace the header. Key changes:
- Remove `LineGraphResampler*`, `QThread*` members
- Remove `_setGraphData` slot, `_setGraphDataSig` signal
- Remove `create_resampler`, `clear_resampler`, `lines()`, `line()` methods
- Add `QCPMultiGraph* _multiGraph` member
- Add `PyBuffer _x, _y` members
- Include `<plottables/plottable-multigraph.h>` and `DtypeDispatch.hpp`

```cpp
#pragma once
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include <plottables/plottable-multigraph.h>
#include <span>

class SciQLopLineGraph : public SQPQCPAbstractPlottableWrapper
{
    QCPMultiGraph* _multiGraph = nullptr;
    PyBuffer _x, _y;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    bool _got_first_data = false;

    Q_OBJECT

    void clear_graphs(bool graph_already_removed = false);

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                              SciQLopPlotAxis* value_axis,
                              const QStringList& labels = QStringList(), QVariantMap metaData = {});

    SciQLopLineGraph(QCustomPlot* parent);

    virtual ~SciQLopLineGraph() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline std::size_t line_count() const noexcept { return _multiGraph ? _multiGraph->componentCount() : 0; }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }

    void create_graphs(const QStringList& labels);
};

class SciQLopLineGraphFunction : public SciQLopLineGraph,
                                 public SciQLopFunctionGraph
{
    Q_OBJECT

public:
    explicit SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                      SciQLopPlotAxis* value_axis, GetDataPyCallable&& callable,
                                      const QStringList& labels, QVariantMap metaData = {});

    virtual ~SciQLopLineGraphFunction() override = default;
};
```

- [ ] **Step 2: Rewrite SciQLopLineGraph.cpp**

Key implementation changes:

```cpp
#include "SciQLopPlots/Plotables/SciQLopLineGraph.hpp"
#include <vector>

void SciQLopLineGraph::create_graphs(const QStringList& labels)
{
    if (_multiGraph)
    {
        clear_graphs();
    }
    _multiGraph = new QCPMultiGraph(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _multiGraph->setComponentNames(labels);

    // Create SciQLopGraphComponent wrappers for each component
    for (int i = 0; i < _multiGraph->componentCount(); ++i)
    {
        auto component = new SciQLopGraphComponent(_multiGraph, i, this);
        component->set_name(labels.value(i));
        _register_component(component);
    }
}

void SciQLopLineGraph::clear_graphs(bool graph_already_removed)
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

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                   SciQLopPlotAxis* value_axis, const QStringList& labels,
                                   QVariantMap metaData)
    : SQPQCPAbstractPlottableWrapper("Line", metaData, parent)
    , _keyAxis{key_axis}
    , _valueAxis{value_axis}
{
    if (!labels.isEmpty())
        create_graphs(labels);
}

SciQLopLineGraph::SciQLopLineGraph(QCustomPlot* parent)
    : SQPQCPAbstractPlottableWrapper("Line", {}, parent)
    , _keyAxis{nullptr}
    , _valueAxis{nullptr}
{
}

SciQLopLineGraph::~SciQLopLineGraph()
{
    clear_graphs();
}

void SciQLopLineGraph::set_data(PyBuffer x, PyBuffer y)
{
    if (!_multiGraph || !x.is_valid() || !y.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");

    _x = x;
    _y = y;

    const auto* keys = static_cast<const double*>(x.raw_data());
    const int n = static_cast<int>(x.flat_size());

    dispatch_dtype(y.format_code(), [&](auto tag) {
        using V = typename decltype(tag)::type;
        const auto* values = static_cast<const V*>(y.raw_data());

        if (y.ndim() == 1)
        {
            std::vector<std::span<const V>> columns{std::span<const V>(values, n)};
            _multiGraph->viewData<double, V>(std::span<const double>(keys, n), std::move(columns));
        }
        else
        {
            const auto n_cols = y.size(1);
            if (y.row_major())
            {
                // Row-major: columns are strided, must copy
                std::vector<std::vector<V>> owned_columns(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                {
                    owned_columns[col].resize(n);
                    for (int row = 0; row < n; ++row)
                        owned_columns[col][row] = values[row * n_cols + col];
                }
                // Copy keys too since setData takes ownership
                std::vector<double> owned_keys(keys, keys + n);
                std::vector<std::vector<V>> cols_move;
                cols_move.reserve(n_cols);
                for (auto& col : owned_columns)
                    cols_move.push_back(std::move(col));
                _multiGraph->setData(std::move(owned_keys), std::move(cols_move));
            }
            else
            {
                // Column-major: each column is contiguous
                std::vector<std::span<const V>> columns;
                columns.reserve(n_cols);
                for (std::size_t col = 0; col < n_cols; ++col)
                    columns.emplace_back(values + col * n, n);
                _multiGraph->viewData<double, V>(std::span<const double>(keys, n), std::move(columns));
            }
        }
    });

    Q_EMIT this->replot();
    if (!_got_first_data && n > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }
    Q_EMIT data_changed(x, y);
}

QList<PyBuffer> SciQLopLineGraph::data() const noexcept
{
    return {_x, _y};
}

void SciQLopLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (_multiGraph)
            _multiGraph->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (_multiGraph)
            _multiGraph->setValueAxis(qcp_axis->qcp_axis());
    }
}

SciQLopLineGraphFunction::SciQLopLineGraphFunction(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                                   SciQLopPlotAxis* value_axis,
                                                   GetDataPyCallable&& callable,
                                                   const QStringList& labels, QVariantMap metaData)
    : SciQLopLineGraph{parent, key_axis, value_axis, labels, metaData}
    , SciQLopFunctionGraph(std::move(callable), this, 2)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}
```

- [ ] **Step 3: Remove SciQLopLineGraphResampler from meson.build**

In `SciQLopPlots/meson.build`, remove from **both** `moc_headers` and `sources`:
- From `moc_headers`: `project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp'` (line 96)
- From `sources`: `'../src/SciQLopLineGraphResampler.cpp'` (line 192)

- [ ] **Step 4: Build and run existing tests**

Run: `meson compile -C build && meson test -C build`
Expected: Existing graph tests pass (`test_graph_api.py`). The multi-dtype tests from Task 1 should now also pass for line graphs.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp src/SciQLopLineGraph.cpp SciQLopPlots/meson.build
git commit -m "feat(linegraph): migrate to QCPMultiGraph with zero-copy viewData"
```

---

## Chunk 3: SciQLopColorMap Migration

### Task 6: Migrate SciQLopColorMap to QCPColorMap2

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp`
- Modify: `src/SciQLopColorMap.cpp`
- Modify: `SciQLopPlots/meson.build` (remove SciQLopColorMapResampler.cpp)

- [ ] **Step 1: Rewrite SciQLopColorMap.hpp**

Key changes:
- Replace `QCPColorMap*` with `QCPColorMap2*`
- Remove `ColormapResampler*`, `QThread*`, `_data_swap_mutex`
- Remove `_setGraphData` slot, `_resample` method
- Add `PyBuffer _x, _y, _z` and `shared_ptr<QCPAbstractDataSource2D> _dataSource`
- Include `<plottables/plottable-colormap2.h>` and `DtypeDispatch.hpp`

```cpp
#pragma once
#include "SciQLopPlots/DataProducer/DataProducer.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/Python/DtypeDispatch.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/qcp_enums.hpp"
#include "QCPAbstractPlottableWrapper.hpp"
#include "SciQLopPlots/enums.hpp"
#include <plottables/plottable-colormap2.h>
#include <datasource/soa-datasource-2d.h>
#include <memory>
#include <span>

class SciQLopColorMap : public SciQLopColorMapInterface
{
    bool _got_first_data = false;
    bool _selected = false;

    QTimer* _icon_update_timer;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    SciQLopPlotColorScaleAxis* _colorScaleAxis;
    QPointer<QCPColorMap2> _cmap;
    bool _auto_scale_y = false;

    // Lifetime wrapper: PyBuffers + data source share lifetime.
    // The shared_ptr ensures numpy memory stays alive as long as
    // QCPColorMap2's async resampler holds a reference to the source.
    struct DataSourceWithBuffers {
        PyBuffer x, y, z;
        std::shared_ptr<QCPAbstractDataSource2D> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    Q_OBJECT
    inline QCustomPlot* _plot() const { return qobject_cast<QCustomPlot*>(this->parent()); }

    void _cmap_got_destroyed();

    inline QCPPlottableLegendItem* _legend_item()
    {
        if (_cmap)
        {
            auto plot = _plot();
            return plot->legend->itemWithPlottable(_cmap.data());
        }
        return nullptr;
    }

public:
    Q_ENUMS(FractionStyle)
    explicit SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis, SciQLopPlotAxis* yAxis,
                             SciQLopPlotColorScaleAxis* zAxis, const QString& name,
                             QVariantMap metaData = {});
    virtual ~SciQLopColorMap() override;

    inline virtual QString layer() const noexcept override
    {
        if (_cmap)
            return _cmap->layer()->name();
        return QString();
    }

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline QCPColorMap2* colorMap() const { return _cmap; }

    void set_auto_scale_y(bool auto_scale_y);
    inline bool auto_scale_y() const { return _auto_scale_y; }

    inline virtual ColorGradient gradient() const noexcept override
    {
        return _colorScaleAxis->color_gradient();
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        _colorScaleAxis->set_color_gradient(gradient);
    }

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    inline virtual void set_name(const QString& name) noexcept override
    {
        this->setObjectName(name);
        if (_cmap)
            _cmap->setName(name);
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }
    virtual SciQLopPlotAxisInterface* z_axis() const noexcept override { return _colorScaleAxis; }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void auto_scale_y_changed(bool);

private:
    ::DataOrder _dataOrder = DataOrder::RowMajor;
};

class SciQLopColorMapFunction : public SciQLopColorMap, public SciQLopFunctionGraph
{
    Q_OBJECT

    inline Q_SLOT void _set_data(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        SciQLopColorMap::set_data(x, y, z);
    }

public:
    explicit SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                     SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                     GetDataPyCallable&& callable, const QString& name);

    virtual ~SciQLopColorMapFunction() override = default;
};
```

- [ ] **Step 2: Rewrite SciQLopColorMap.cpp**

```cpp
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"
#include "SciQLopPlots/constants.hpp"

void SciQLopColorMap::_cmap_got_destroyed()
{
    _cmap = nullptr;
}

SciQLopColorMap::SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                 SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                 const QString& name, QVariantMap metaData)
    : SciQLopColorMapInterface(metaData, parent)
    , _icon_update_timer{new QTimer(this)}
    , _keyAxis{xAxis}
    , _valueAxis{yAxis}
    , _colorScaleAxis{zAxis}
{
    _cmap = new QCPColorMap2(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _cmap->setLayer(Constants::LayersNames::ColorMap);
    connect(_cmap, &QCPColorMap2::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
    SciQLopColorMap::set_gradient(ColorGradient::Jet);
    SciQLopColorMap::set_name(name);

    _icon_update_timer->setInterval(1000);
    _icon_update_timer->setSingleShot(true);

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopColorMap::set_selected);
    }
}

SciQLopColorMap::~SciQLopColorMap()
{
    if (_cmap)
        _plot()->removePlottable(_cmap);
}

void SciQLopColorMap::set_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    if (!_cmap || !x.is_valid() || !y.is_valid() || !z.is_valid())
        return;

    if (x.format_code() != 'd')
        throw std::runtime_error("Keys (x) must be float64");

    const auto* x_ptr = static_cast<const double*>(x.raw_data());
    const int nx = static_cast<int>(x.flat_size());

    dispatch_dtype(y.format_code(), [&](auto y_tag) {
        dispatch_dtype(z.format_code(), [&](auto z_tag) {
            using Y = typename decltype(y_tag)::type;
            using Z = typename decltype(z_tag)::type;
            const auto* y_ptr = static_cast<const Y*>(y.raw_data());
            const auto* z_ptr = static_cast<const Z*>(z.raw_data());
            const int ny = static_cast<int>(y.flat_size());
            const int nz = static_cast<int>(z.flat_size());

            auto source = std::make_shared<QCPSoADataSource2D<
                std::span<const double>, std::span<const Y>, std::span<const Z>>>(
                std::span<const double>(x_ptr, nx),
                std::span<const Y>(y_ptr, ny),
                std::span<const Z>(z_ptr, nz));

            // Bundle PyBuffers with data source so numpy memory stays alive
            // as long as QCPColorMap2's async resampler holds a shared_ptr
            _dataHolder = std::make_shared<DataSourceWithBuffers>(
                DataSourceWithBuffers{x, y, z, source});
            _cmap->setDataSource(source);
        });
    });

    if (!_got_first_data && nx > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }

    if (_auto_scale_y && _dataHolder && _dataHolder->source)
    {
        bool found = false;
        auto yRange = _dataHolder->source->yRange(found);
        if (found)
            _valueAxis->qcp_axis()->setRange(yRange);
    }

    Q_EMIT data_changed(x, y, z);
}

QList<PyBuffer> SciQLopColorMap::data() const noexcept
{
    if (_dataHolder)
        return {_dataHolder->x, _dataHolder->y, _dataHolder->z};
    return {};
}

void SciQLopColorMap::set_auto_scale_y(bool auto_scale_y)
{
    _auto_scale_y = auto_scale_y;
    Q_EMIT auto_scale_y_changed(auto_scale_y);
}

void SciQLopColorMap::set_selected(bool selected) noexcept
{
    if (_selected != selected)
    {
        _selected = selected;
        auto legend_item = _legend_item();
        if (legend_item && legend_item->selected() != selected)
            legend_item->setSelected(selected);
        emit selection_changed(selected);
    }
}

bool SciQLopColorMap::selected() const noexcept
{
    return _selected;
}

void SciQLopColorMap::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (_cmap)
            _cmap->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopColorMap::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (_cmap)
            _cmap->setValueAxis(qcp_axis->qcp_axis());
    }
}

SciQLopColorMapFunction::SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                                 SciQLopPlotAxis* yAxis,
                                                 SciQLopPlotColorScaleAxis* zAxis,
                                                 GetDataPyCallable&& callable, const QString& name)
    : SciQLopColorMap{parent, xAxis, yAxis, zAxis, name}
    , SciQLopFunctionGraph(std::move(callable), this, 3)
{
    this->set_range({parent->xAxis->range().lower, parent->xAxis->range().upper});
}
```

- [ ] **Step 3: Remove SciQLopColorMapResampler from meson.build**

In `SciQLopPlots/meson.build`, remove from **both** `moc_headers` and `sources`:
- From `moc_headers`: `project_source_root + '/include/SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp'` (line 98)
- From `sources`: `'../src/SciQLopColorMapResampler.cpp'` (line 194)

- [ ] **Step 4: Build and run all tests**

Run: `meson compile -C build && meson test -C build`
Expected: All existing colormap tests pass (`test_colormap_api.py`). Multi-dtype colormap test passes.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopColorMap.hpp src/SciQLopColorMap.cpp SciQLopPlots/meson.build
git commit -m "feat(colormap): migrate to QCPColorMap2 with built-in async resampling"
```

---

## Chunk 4: Cleanup + Bindings + Final Verification

### Task 7: Delete unused resampler files

**Files:**
- Delete: `include/SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp`
- Delete: `include/SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp`
- Delete: `src/SciQLopLineGraphResampler.cpp`
- Delete: `src/SciQLopColorMapResampler.cpp`

- [ ] **Step 1: Delete the files**

```bash
rm include/SciQLopPlots/Plotables/Resamplers/SciQLopLineGraphResampler.hpp
rm include/SciQLopPlots/Plotables/Resamplers/SciQLopColorMapResampler.hpp
rm src/SciQLopLineGraphResampler.cpp
rm src/SciQLopColorMapResampler.cpp
```

- [ ] **Step 2: Build to verify nothing else includes them**

Run: `meson compile -C build`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add -u
git commit -m "chore: remove unused line graph and colormap resampler files"
```

---

### Task 8: Update Shiboken bindings

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `SciQLopPlots/bindings/bindings.h`

This task depends on what's currently exposed. Check `bindings.xml` for PyBuffer-related entries and update as needed.

- [ ] **Step 1: Check current bindings for PyBuffer**

Read `SciQLopPlots/bindings/bindings.xml` and search for `PyBuffer`, `data()`, `ArrayView`.

- [ ] **Step 2: Update bindings.xml**

Add `format_code()`, `raw_data()`, `item_size()` to PyBuffer if exposed. Remove any double-specific accessors that were removed. Update any references from `QCPColorMap` to `QCPColorMap2` or `QCPGraph` to `QCPMultiGraph` in rejection rules.

- [ ] **Step 3: Update bindings.h if needed**

Ensure `bindings.h` includes the new NeoQCP headers (`plottable-multigraph.h`, `plottable-colormap2.h`) if any types are referenced in the typesystem.

- [ ] **Step 4: Build the full Python wheel**

Run: `pip install . --no-build-isolation` (or `meson compile -C build` if doing dev build)
Expected: Shiboken generates wrappers successfully.

- [ ] **Step 5: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml SciQLopPlots/bindings/bindings.h
git commit -m "fix(bindings): update typesystem for QCPMultiGraph/QCPColorMap2 migration"
```

---

### Task 9: Final integration test run

- [ ] **Step 1: Run all integration tests**

```bash
cd build && meson test --print-errorlogs
```

Or:

```bash
pytest tests/integration/ -v
```

Expected: All tests pass, including:
- `test_graph_api.py` — all graph creation, data, properties tests
- `test_colormap_api.py` — all colormap tests
- `test_multi_dtype.py` — float32, int16, uint8 data types
- `test_memory.py` — no memory leaks
- `test_signals.py` — signals still fire correctly

- [ ] **Step 2: Run a manual test to visually verify**

```bash
python tests/manual-tests/SciQLopGraph/main.py
python tests/manual-tests/SciQLopColorMap/main.py
```

Expected: Plots render correctly.

- [ ] **Step 3: Final commit if any fixups were needed**

```bash
git add -u
git commit -m "fix: address integration test issues from NeoQCP migration"
```
