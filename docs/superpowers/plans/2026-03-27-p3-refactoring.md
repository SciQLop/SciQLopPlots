# P3 Refactoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deduplicate ~500+ lines of repeated code across ColorMap/Histogram2D and VSpan/HSpan/RSpan class families, extract shared helpers, and fix naming inconsistencies.

**Architecture:** Extract concrete intermediate base classes for the two duplicated hierarchies (colormap plotables and span items). Use a free function for the axis-setter pattern and a base-class helper for the first-data pattern. Rename bare-accessor methods on MultiPlotsVerticalSpan for consistency.

**Tech Stack:** C++20, Qt6, Shiboken6 bindings (no new bindings needed), Meson build system.

---

## File Map

### New files

| File | Responsibility |
|------|---------------|
| `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp` | Base class for ColorMap/Histogram2D — shared members and methods |
| `src/SciQLopColorMapBase.cpp` | Out-of-line implementations for SciQLopColorMapBase |
| `include/SciQLopPlots/Plotables/AxisHelpers.hpp` | `apply_axis()` free function for set_x/y_axis dedup |
| `include/SciQLopPlots/Items/impl/SpanBase.hpp` | Shared impl-layer span methods |
| `src/SpanBase.cpp` | Out-of-line implementations for impl::SpanBase |
| `include/SciQLopPlots/Items/SciQLopSpanBase.hpp` | Shared wrapper-layer span methods |
| `src/SciQLopSpanBase.cpp` | Out-of-line implementations for SciQLopSpanBase |

### Modified files

| File | Changes |
|------|---------|
| `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp` | Add `_got_first_data` + `check_first_data()` to SciQLopPlottableInterface |
| `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp` | Inherit SciQLopColorMapBase, remove duplicated members/methods |
| `src/SciQLopColorMap.cpp` | Remove implementations now in base |
| `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp` | Inherit SciQLopColorMapBase, remove duplicated members/methods |
| `src/SciQLopHistogram2D.cpp` | Remove implementations now in base |
| `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp` | Remove `_got_first_data` member |
| `src/SciQLopLineGraph.cpp` | Use `check_first_data()` and `apply_axis()` |
| `include/SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp` | Remove `_got_first_data` member |
| `src/SciQLopSingleLineGraph.cpp` | Use `check_first_data()` and `apply_axis()` |
| `src/SciQLopCurve.cpp` | Use `apply_axis()` |
| `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp` | Inherit SciQLopSpanBase, remove shared methods |
| `src/SciQLopVerticalSpan.cpp` | Remove shared impl methods, inherit SpanBase |
| `include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp` | Inherit SciQLopSpanBase, remove shared methods |
| `src/SciQLopHorizontalSpan.cpp` | Remove shared impl methods, inherit SpanBase |
| `include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp` | Inherit SciQLopSpanBase, remove shared methods |
| `src/SciQLopRectangularSpan.cpp` | Remove shared impl methods, inherit SpanBase |
| `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp` | Rename get_*/is_* to bare accessors |
| `src/MultiPlotsVSpan.cpp` | Update caller of get_id() |
| `SciQLopPlots/bindings/bindings.xml` | Update property getter names for MultiPlotsVerticalSpan |
| `tests/integration/test_panel_api.py` | Update test calls to new accessor names |
| `SciQLopPlots/meson.build` | Add new .cpp files to build |

---

## Task 1: REFAC-04 — Add `_got_first_data` + `check_first_data()` to base

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp:45-48,180-182`
- Modify: `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp:39`
- Modify: `include/SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp:44`
- Modify: `src/SciQLopLineGraph.cpp:130-134`
- Modify: `src/SciQLopSingleLineGraph.cpp:102-106`

- [ ] **Step 1: Add `_got_first_data` and `check_first_data()` to `SciQLopPlottableInterface`**

In `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp`, add to the existing protected section before the `Q_OBJECT_BINDABLE_PROPERTY` line (before line 181):

```cpp
    bool _got_first_data = false;

    void check_first_data(std::size_t n)
    {
        if (!_got_first_data && n > 0)
        {
            _got_first_data = true;
            Q_EMIT request_rescale();
        }
    }
```

The protected section currently starts at line 180. Insert after `protected:` (line 180) and before the `Q_OBJECT_BINDABLE_PROPERTY` macro (line 181).

- [ ] **Step 2: Remove `_got_first_data` from `SciQLopLineGraph`**

In `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`, delete line 39:

```cpp
    bool _got_first_data = false;
```

- [ ] **Step 3: Replace inline pattern with `check_first_data()` in `SciQLopLineGraph`**

In `src/SciQLopLineGraph.cpp`, replace lines 130-134:

```cpp
    if (!_got_first_data && n > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }
```

with:

```cpp
    check_first_data(n);
```

- [ ] **Step 4: Remove `_got_first_data` from `SciQLopSingleLineGraph`**

In `include/SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp`, delete line 44:

```cpp
    bool _got_first_data = false;
```

- [ ] **Step 5: Replace inline pattern with `check_first_data()` in `SciQLopSingleLineGraph`**

In `src/SciQLopSingleLineGraph.cpp`, replace lines 102-106:

```cpp
    if (!_got_first_data && n > 0)
    {
        _got_first_data = true;
        Q_EMIT request_rescale();
    }
```

with:

```cpp
    check_first_data(n);
```

- [ ] **Step 6: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation. No behavioral changes.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp \
        include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp \
        include/SciQLopPlots/Plotables/SciQLopSingleLineGraph.hpp \
        src/SciQLopLineGraph.cpp \
        src/SciQLopSingleLineGraph.cpp
git commit -m "refactor(REFAC-04): extract check_first_data() into SciQLopPlottableInterface"
```

---

## Task 2: REFAC-03 — Extract `apply_axis()` free function

**Files:**
- Create: `include/SciQLopPlots/Plotables/AxisHelpers.hpp`
- Modify: `src/SciQLopLineGraph.cpp:144-162`
- Modify: `src/SciQLopSingleLineGraph.cpp:118-136`
- Modify: `src/SciQLopCurve.cpp:106-130`

- [ ] **Step 1: Create `AxisHelpers.hpp`**

Create `include/SciQLopPlots/Plotables/AxisHelpers.hpp`:

```cpp
#pragma once
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotAxisInterface.hpp"

inline void apply_axis(SciQLopPlotAxis*& stored,
                       SciQLopPlotAxisInterface* axis,
                       auto&& apply_to_plottable)
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        stored = qcp_axis;
        apply_to_plottable(qcp_axis->qcp_axis());
    }
}
```

- [ ] **Step 2: Refactor `SciQLopLineGraph::set_x_axis` and `set_y_axis`**

In `src/SciQLopLineGraph.cpp`, add include at top:

```cpp
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
```

Replace lines 144-162:

```cpp
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
```

with:

```cpp
void SciQLopLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setKeyAxis(a); });
}

void SciQLopLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_multiGraph) _multiGraph->setValueAxis(a); });
}
```

- [ ] **Step 3: Refactor `SciQLopSingleLineGraph::set_x_axis` and `set_y_axis`**

In `src/SciQLopSingleLineGraph.cpp`, add include at top:

```cpp
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
```

Replace lines 118-136:

```cpp
void SciQLopSingleLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (_graph)
            _graph->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopSingleLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (_graph)
            _graph->setValueAxis(qcp_axis->qcp_axis());
    }
}
```

with:

```cpp
void SciQLopSingleLineGraph::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) { if (_graph) _graph->setKeyAxis(a); });
}

void SciQLopSingleLineGraph::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) { if (_graph) _graph->setValueAxis(a); });
}
```

- [ ] **Step 4: Refactor `SciQLopCurve::set_x_axis` and `set_y_axis`**

In `src/SciQLopCurve.cpp`, add include at top:

```cpp
#include "SciQLopPlots/Plotables/AxisHelpers.hpp"
```

Replace lines 106-130:

```cpp
void SciQLopCurve::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_keyAxis = qcp_axis;
        for (auto plottable : m_components)
        {
            auto curve = qobject_cast<QCPCurve*>(plottable->plottable());
            curve->setKeyAxis(qcp_axis->qcp_axis());
        }
    }
}

void SciQLopCurve::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        this->_valueAxis = qcp_axis;
        for (auto plottable : m_components)
        {
            auto curve = qobject_cast<QCPCurve*>(plottable->plottable());
            curve->setValueAxis(qcp_axis->qcp_axis());
        }
    }
}
```

with:

```cpp
void SciQLopCurve::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_keyAxis, axis, [this](auto* a) {
        for (auto p : m_components)
            qobject_cast<QCPCurve*>(p->plottable())->setKeyAxis(a);
    });
}

void SciQLopCurve::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    apply_axis(_valueAxis, axis, [this](auto* a) {
        for (auto p : m_components)
            qobject_cast<QCPCurve*>(p->plottable())->setValueAxis(a);
    });
}
```

- [ ] **Step 5: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation.

- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/Plotables/AxisHelpers.hpp \
        src/SciQLopLineGraph.cpp \
        src/SciQLopSingleLineGraph.cpp \
        src/SciQLopCurve.cpp
git commit -m "refactor(REFAC-03): extract apply_axis() free function for set_x/y_axis dedup"
```

---

## Task 3: REFAC-01 — Extract `SciQLopColorMapBase`

**Files:**
- Create: `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp`
- Create: `src/SciQLopColorMapBase.cpp`
- Modify: `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp`
- Modify: `src/SciQLopColorMap.cpp`
- Modify: `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp`
- Modify: `src/SciQLopHistogram2D.cpp`
- Modify: `SciQLopPlots/meson.build:206`

- [ ] **Step 1: Create `SciQLopColorMapBase.hpp`**

Create `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp`:

```cpp
#pragma once
#include "SciQLopPlots/Plotables/SciQLopGraphInterface.hpp"
#include "SciQLopPlots/SciQLopPlotAxis.hpp"
#include "SciQLopPlots/SciQLopPlotColorScaleAxis.hpp"
#include "SciQLopPlots/constants.hpp"

#include <QCPAbstractPlottable>
#include <QCustomPlot>
#include <QPointer>

class SciQLopColorMapBase : public SciQLopColorMapInterface
{
    Q_OBJECT

protected:
    bool _selected = false;

    SciQLopPlotAxis* _keyAxis;
    SciQLopPlotAxis* _valueAxis;
    SciQLopPlotColorScaleAxis* _colorScaleAxis;

    inline QCustomPlot* _plot() const
    {
        return qobject_cast<QCustomPlot*>(this->parent());
    }

    virtual QCPAbstractPlottable* plottable() const = 0;

    QCPPlottableLegendItem* _legend_item();

public:
    SciQLopColorMapBase(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                        SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                        const QString& name, QVariantMap metaData = {});
    virtual ~SciQLopColorMapBase() override;

    inline virtual QString layer() const noexcept override
    {
        if (auto p = plottable(); p)
            return p->layer()->name();
        return {};
    }

    inline virtual QString name() const noexcept override { return objectName(); }

    inline virtual void set_name(const QString& name) noexcept override
    {
        if (auto p = plottable(); p)
            p->setName(name);
        setObjectName(name);
        Q_EMIT name_changed(name);
    }

    // gradient()/set_gradient() stay in derived classes — QCPColorMap2 and
    // QCPHistogram2D have type-specific colorGradient() APIs that can't be
    // called through QCPAbstractPlottable*.

    virtual void set_selected(bool selected) noexcept override;
    virtual bool selected() const noexcept override;

    virtual bool busy() const noexcept override
    {
        if (auto p = plottable(); p)
            return p->busy();
        return false;
    }

    virtual void set_busy(bool busy) noexcept override
    {
        if (auto p = plottable(); p)
            p->setBusy(busy);
    }

    virtual void set_x_axis(SciQLopPlotAxisInterface* axis) noexcept override;
    virtual void set_y_axis(SciQLopPlotAxisInterface* axis) noexcept override;

    inline virtual SciQLopPlotAxisInterface* x_axis() const noexcept override { return _keyAxis; }
    inline virtual SciQLopPlotAxisInterface* y_axis() const noexcept override { return _valueAxis; }
    inline virtual SciQLopPlotAxisInterface* z_axis() const noexcept override { return _colorScaleAxis; }
};
```

- [ ] **Step 2: Create `SciQLopColorMapBase.cpp`**

Create `src/SciQLopColorMapBase.cpp`:

```cpp
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"

SciQLopColorMapBase::SciQLopColorMapBase(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                         SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                         const QString& name, QVariantMap metaData)
    : SciQLopColorMapInterface(std::move(metaData), parent)
    , _keyAxis { xAxis }
    , _valueAxis { yAxis }
    , _colorScaleAxis { zAxis }
{
}

SciQLopColorMapBase::~SciQLopColorMapBase()
{
    if (auto p = plottable(); p)
        _plot()->removePlottable(p);
}

QCPPlottableLegendItem* SciQLopColorMapBase::_legend_item()
{
    if (auto p = plottable(); p)
    {
        auto plot = _plot();
        return plot->legend->itemWithPlottable(p);
    }
    return nullptr;
}

void SciQLopColorMapBase::set_selected(bool selected) noexcept
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

bool SciQLopColorMapBase::selected() const noexcept
{
    return _selected;
}

void SciQLopColorMapBase::set_x_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _keyAxis = qcp_axis;
        if (auto p = plottable(); p)
            p->setKeyAxis(qcp_axis->qcp_axis());
    }
}

void SciQLopColorMapBase::set_y_axis(SciQLopPlotAxisInterface* axis) noexcept
{
    if (auto qcp_axis = dynamic_cast<SciQLopPlotAxis*>(axis))
    {
        _valueAxis = qcp_axis;
        if (auto p = plottable(); p)
            p->setValueAxis(qcp_axis->qcp_axis());
    }
}
```

- [ ] **Step 3: Refactor `SciQLopColorMap` to inherit from `SciQLopColorMapBase`**

In `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp`:

Replace the class declaration and remove all members/methods that moved to the base. The class should now look like:

```cpp
#pragma once
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"
#include "SciQLopPlots/enums.hpp"

#include <QCPColorMap2>
#include <QPointer>
#include <memory>

class SciQLopColorMap : public SciQLopColorMapBase
{
    Q_OBJECT

    QPointer<QCPColorMap2> _cmap;
    bool _auto_scale_y = false;

    struct DataSourceWithBuffers
    {
        PyBuffer x, y, z;
        std::shared_ptr<QCPAbstractDataSource2D> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    ::DataOrder _dataOrder = DataOrder::RowMajor;

    void _cmap_got_destroyed();

protected:
    QCPAbstractPlottable* plottable() const override
    {
        return _cmap.data();
    }

public:
    explicit SciQLopColorMap(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                             SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                             const QString& name, QVariantMap metaData = {});
    virtual ~SciQLopColorMap() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y, PyBuffer z) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline QCPColorMap2* colorMap() const { return _cmap.data(); }

    void set_auto_scale_y(bool auto_scale_y);
    inline bool auto_scale_y() const { return _auto_scale_y; }

    inline virtual ColorGradient gradient() const noexcept override
    {
        return _cmap ? static_cast<ColorGradient>(_cmap->colorGradient()) : ColorGradient::Jet;
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        if (_cmap)
            _cmap->setColorGradient(static_cast<QCPColorGradient::GradientPreset>(gradient));
    }

    inline virtual void set_y_log_scale(bool y_log_scale) noexcept override
    {
        if (_cmap)
            _cmap->setYLogScale(y_log_scale);
    }

    inline virtual bool y_log_scale() const noexcept override
    {
        return _cmap ? _cmap->yLogScale() : false;
    }

    inline virtual void set_z_log_scale(bool z_log_scale) noexcept override
    {
        if (_cmap)
            _cmap->setZLogScale(z_log_scale);
    }

    inline virtual bool z_log_scale() const noexcept override
    {
        return _cmap ? _cmap->zLogScale() : false;
    }

#ifndef BINDINGS_H
    Q_SIGNAL void auto_scale_y_changed(bool);
#endif
};

class SciQLopColorMapFunction : public SciQLopColorMap, public SciQLopFunctionGraph
{
    Q_OBJECT
public:
    SciQLopColorMapFunction(QCustomPlot* parent, SciQLopPlotAxis* x_axis,
                            SciQLopPlotAxis* y_axis, SciQLopPlotColorScaleAxis* z_axis,
                            GetDataPyCallable&& callable, const QString& name,
                            QVariantMap metaData = {});
    virtual ~SciQLopColorMapFunction() override = default;
};
```

- [ ] **Step 4: Simplify `SciQLopColorMap.cpp`**

In `src/SciQLopColorMap.cpp`, the constructor no longer initializes axis members or connects common signals — those are in the base. The constructor now:

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
    : SciQLopColorMapBase(parent, xAxis, yAxis, zAxis, name, std::move(metaData))
{
    _cmap = new QCPColorMap2(_keyAxis->qcp_axis(), _valueAxis->qcp_axis());
    _cmap->setLayer(Constants::LayersNames::ColorMap);
    connect(_cmap, &QCPColorMap2::destroyed, this, &SciQLopColorMap::_cmap_got_destroyed);
    connect(_cmap, &QCPAbstractPlottable::busyChanged,
            this, &SciQLopPlottableInterface::busy_changed);
    SciQLopColorMap::set_gradient(ColorGradient::Jet);
    SciQLopColorMap::set_name(name);

    if (auto legend_item = _legend_item(); legend_item)
    {
        connect(legend_item, &QCPAbstractLegendItem::selectionChanged, this,
                &SciQLopColorMap::set_selected);
    }
}

SciQLopColorMap::~SciQLopColorMap()
{
    // prevent double-removal: clear _cmap so base destructor's plottable() returns null
    if (_cmap)
    {
        auto plot = _plot();
        auto cmap = _cmap.data();
        _cmap = nullptr;
        plot->removePlottable(cmap);
    }
}
```

Remove the `set_selected()`, `selected()`, `set_x_axis()`, `set_y_axis()` implementations — they're now in the base.

Keep `set_data()` and `data()` as-is, but replace the inline `_got_first_data` pattern (lines 99-103) with:

```cpp
    check_first_data(nx);
```

Also remove the `_got_first_data` member from the header (line 37 of original).

- [ ] **Step 5: Refactor `SciQLopHistogram2D` similarly**

In `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp`, change to inherit from `SciQLopColorMapBase` and remove all shared members/methods. The class should now look like:

```cpp
#pragma once
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"
#include "SciQLopPlots/Python/PythonInterface.hpp"

#include <QCPHistogram2D>
#include <QPointer>
#include <memory>

class SciQLopHistogram2D : public SciQLopColorMapBase
{
    Q_OBJECT

    QPointer<QCPHistogram2D> _hist;

    struct DataSourceWithBuffers
    {
        PyBuffer x, y;
        std::shared_ptr<QCPAbstractDataSource> source;
    };
    std::shared_ptr<DataSourceWithBuffers> _dataHolder;

    void _hist_got_destroyed();

protected:
    QCPAbstractPlottable* plottable() const override
    {
        return _hist.data();
    }

public:
    explicit SciQLopHistogram2D(QCustomPlot* parent, SciQLopPlotAxis* xAxis,
                                SciQLopPlotAxis* yAxis, SciQLopPlotColorScaleAxis* zAxis,
                                const QString& name, int key_bins = 100,
                                int value_bins = 100, QVariantMap metaData = {});
    virtual ~SciQLopHistogram2D() override;

    Q_SLOT virtual void set_data(PyBuffer x, PyBuffer y) override;
    virtual QList<PyBuffer> data() const noexcept override;

    inline QCPHistogram2D* histogram() const { return _hist.data(); }

    inline virtual ColorGradient gradient() const noexcept override
    {
        return _hist ? static_cast<ColorGradient>(_hist->colorGradient()) : ColorGradient::Jet;
    }

    inline virtual void set_gradient(ColorGradient gradient) noexcept override
    {
        if (_hist)
            _hist->setColorGradient(static_cast<QCPColorGradient::GradientPreset>(gradient));
    }

    void set_bins(int key_bins, int value_bins);
    int key_bins() const;
    int value_bins() const;

    void set_normalization(int normalization);
    int normalization() const;
};
```

- [ ] **Step 6: Simplify `SciQLopHistogram2D.cpp`**

Same pattern as step 4. Constructor calls `SciQLopColorMapBase(...)`. Remove `set_selected()`, `selected()`, `set_x_axis()`, `set_y_axis()` implementations. Keep `set_data()` with its own `_got_first_data` logic (async pipeline variant — uses the inherited `_got_first_data` member directly, does NOT call `check_first_data()`).

Destructor same pattern as ColorMap — clear `_hist` before base destructor runs:

```cpp
SciQLopHistogram2D::~SciQLopHistogram2D()
{
    if (_hist)
    {
        auto plot = _plot();
        auto hist = _hist.data();
        _hist = nullptr;
        plot->removePlottable(hist);
    }
}
```

- [ ] **Step 7: Add `SciQLopColorMapBase.cpp` to `meson.build`**

In `SciQLopPlots/meson.build`, add after line 206 (`'../src/SciQLopColorMap.cpp'`):

```
            '../src/SciQLopColorMapBase.cpp',
```

- [ ] **Step 8: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation. The `gradient()`/`set_gradient()` methods in the base use QCPAbstractPlottable properties — verify at compile time that these resolve. If not, keep them as inline overrides in each derived class instead.

- [ ] **Step 9: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp \
        src/SciQLopColorMapBase.cpp \
        include/SciQLopPlots/Plotables/SciQLopColorMap.hpp \
        src/SciQLopColorMap.cpp \
        include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp \
        src/SciQLopHistogram2D.cpp \
        SciQLopPlots/meson.build
git commit -m "refactor(REFAC-01): extract SciQLopColorMapBase for ColorMap/Histogram2D dedup"
```

---

## Task 4: REFAC-02 — Extract span base classes

**Files:**
- Create: `include/SciQLopPlots/Items/impl/SpanBase.hpp`
- Create: `src/SpanBase.cpp`
- Create: `include/SciQLopPlots/Items/SciQLopSpanBase.hpp`
- Create: `src/SciQLopSpanBase.cpp`
- Modify: `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`
- Modify: `src/SciQLopVerticalSpan.cpp`
- Modify: `include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp`
- Modify: `src/SciQLopHorizontalSpan.cpp`
- Modify: `include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp`
- Modify: `src/SciQLopRectangularSpan.cpp`
- Modify: `SciQLopPlots/meson.build`

### Subtask 4a: impl::SpanBase

- [ ] **Step 1: Create `impl/SpanBase.hpp`**

Create `include/SciQLopPlots/Items/impl/SpanBase.hpp`:

```cpp
#pragma once
#include "SciQLopPlots/Items/SciQLopPlotItem.hpp"

#include <QCPAbstractSpanItem>
#include <QColor>
#include <QString>

namespace impl {

class SpanBase : public SciQlopItemWithToolTip
{
protected:
    QCPAbstractSpanItem* _span_item;

public:
    explicit SpanBase(QCPAbstractSpanItem* item);

    void set_color(const QColor& color);
    [[nodiscard]] QColor color() const noexcept;

    void set_borders_color(const QColor& color);
    [[nodiscard]] QColor borders_color() const noexcept;

    void set_visible(bool visible);

    void set_borders_tool_tip(const QString& tool_tip);

    void replot(bool immediate = false);
};

} // namespace impl
```

- [ ] **Step 2: Create `SpanBase.cpp`**

Create `src/SpanBase.cpp`:

```cpp
#include "SciQLopPlots/Items/impl/SpanBase.hpp"

namespace impl {

SpanBase::SpanBase(QCPAbstractSpanItem* item)
    : _span_item { item }
{
}

void SpanBase::set_color(const QColor& color)
{
    _span_item->setBrush(QBrush(color));
    _span_item->setSelectedBrush(QBrush(color));
    auto pen = _span_item->pen();
    auto inv_color = QColor(255 - color.red(), 255 - color.green(), 255 - color.blue());
    pen.setColor(inv_color);
    _span_item->setSelectedPen(pen);
    pen.setColor(color);
    _span_item->setPen(pen);
}

QColor SpanBase::color() const noexcept
{
    return _span_item->brush().color();
}

void SpanBase::set_borders_color(const QColor& color)
{
    auto pen = _span_item->borderPen();
    pen.setColor(color);
    pen.setWidthF(3.);
    _span_item->setBorderPen(pen);
}

QColor SpanBase::borders_color() const noexcept
{
    return _span_item->borderPen().color();
}

void SpanBase::set_visible(bool visible)
{
    _span_item->setVisible(visible);
}

void SpanBase::set_borders_tool_tip(const QString& tool_tip)
{
    SciQlopItemWithToolTip::setToolTip(tool_tip);
}

void SpanBase::replot(bool immediate)
{
    if (immediate)
        _span_item->layer()->replot();
    else
        _span_item->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

} // namespace impl
```

- [ ] **Step 3: Refactor `impl::VerticalSpan` to use SpanBase**

In `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`, change impl class (around line 36):

```cpp
class VerticalSpan : public QCPItemVSpan, public impl::SpanBase
```

Remove from `impl::VerticalSpan` declarations: `set_visible`, `set_color`, `color`, `set_borders_color`, `borders_color`, `set_borders_tool_tip`, `replot`. Keep: constructor, `set_range`, `range`, `select_lower_border`, `select_upper_border`, signals.

The constructor should initialize SpanBase:

In `src/SciQLopVerticalSpan.cpp`, change constructor (line 27):

```cpp
impl::VerticalSpan::VerticalSpan(QCustomPlot* plot, SciQLopPlotRange horizontal_range,
                                  bool do_not_replot, bool immediate_replot)
    : QCPItemVSpan(plot)
    , SpanBase(this)
{
```

Remove implementations of: `set_visible` (line 48), `set_color` (lines 65-73), `color` (line 75), `set_borders_color` (lines 77-80), `borders_color` (lines 82-85), `set_borders_tool_tip` (lines 87-90).

- [ ] **Step 4: Refactor `impl::HorizontalSpan` to use SpanBase**

Same pattern as step 3. In `include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp`:

```cpp
class HorizontalSpan : public QCPItemHSpan, public impl::SpanBase
```

In `src/SciQLopHorizontalSpan.cpp`, change constructor:

```cpp
impl::HorizontalSpan::HorizontalSpan(QCustomPlot* plot, SciQLopPlotRange vertical_range,
                                       bool do_not_replot, bool immediate_replot)
    : QCPItemHSpan(plot)
    , SpanBase(this)
{
```

Remove same method implementations as step 3.

- [ ] **Step 5: Refactor `impl::RectangularSpan` to use SpanBase**

In `include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp`:

```cpp
class RectangularSpan : public QCPItemRSpan, public impl::SpanBase
```

In `src/SciQLopRectangularSpan.cpp`, change constructor:

```cpp
impl::RectangularSpan::RectangularSpan(QCustomPlot* plot, SciQLopPlotRange key_range,
                                        SciQLopPlotRange value_range, bool do_not_replot,
                                        bool immediate_replot)
    : QCPItemRSpan(plot)
    , SpanBase(this)
{
```

Remove same method implementations.

- [ ] **Step 6: Build and verify impl layer compiles**

Run: `meson compile -C build`
Expected: Clean compilation. The `SpanBase(this)` idiom passes the derived QCP item as `QCPAbstractSpanItem*` — verify the implicit upcast compiles.

### Subtask 4b: SciQLopSpanBase

- [ ] **Step 7: Create `SciQLopSpanBase.hpp`**

`SciQLopSpanBase` is a plain helper class (NOT a QObject, NOT a base class for inheritance). Each span wrapper holds a `SciQLopSpanBase _base` member and delegates shared methods to it. This avoids diamond inheritance with the existing `SciQLopRangeItemInterface`/`SciQLopMovableItemInterface` hierarchy.

Create `include/SciQLopPlots/Items/SciQLopSpanBase.hpp`:

```cpp
#pragma once
#include "SciQLopPlots/Items/impl/SpanBase.hpp"

#include <QPointer>

class QCPAbstractItem;
class QCustomPlot;

class SciQLopSpanBase
{
protected:
    impl::SpanBase* _span_base;
    QPointer<QCPAbstractItem> _base_item;

public:
    SciQLopSpanBase(impl::SpanBase* span_base, QCPAbstractItem* item);
    ~SciQLopSpanBase();

    QCustomPlot* parentPlot() const noexcept;

    void set_visible(bool visible) noexcept;
    bool visible() const noexcept;

    void set_color(const QColor& color);
    QColor color() const;

    void set_borders_color(const QColor& color);
    QColor borders_color() const noexcept;

    void set_selected(bool selected);
    bool selected() const noexcept;

    void set_read_only(bool read_only);
    bool read_only() const noexcept;

    void set_tool_tip(const QString& tool_tip) noexcept;
    QString tool_tip() const noexcept;

    void replot();
};
```

- [ ] **Step 8: Create `SciQLopSpanBase.cpp`**

Create `src/SciQLopSpanBase.cpp`:

```cpp
#include "SciQLopPlots/Items/SciQLopSpanBase.hpp"

#include <QCustomPlot>

SciQLopSpanBase::SciQLopSpanBase(impl::SpanBase* span_base, QCPAbstractItem* item)
    : _span_base { span_base }
    , _base_item { item }
{
}

SciQLopSpanBase::~SciQLopSpanBase()
{
    if (_base_item)
    {
        auto plot = _base_item->parentPlot();
        if (plot)
            plot->removeItem(_base_item);
    }
}

QCustomPlot* SciQLopSpanBase::parentPlot() const noexcept
{
    return _base_item ? _base_item->parentPlot() : nullptr;
}

void SciQLopSpanBase::set_visible(bool visible) noexcept
{
    if (_base_item)
        _span_base->set_visible(visible);
}

bool SciQLopSpanBase::visible() const noexcept
{
    return _base_item ? _base_item->visible() : false;
}

void SciQLopSpanBase::set_color(const QColor& color)
{
    if (_base_item)
        _span_base->set_color(color);
}

QColor SciQLopSpanBase::color() const
{
    return _base_item ? _span_base->color() : QColor {};
}

void SciQLopSpanBase::set_borders_color(const QColor& color)
{
    if (_base_item)
        _span_base->set_borders_color(color);
}

QColor SciQLopSpanBase::borders_color() const noexcept
{
    return _base_item ? _span_base->borders_color() : QColor {};
}

void SciQLopSpanBase::set_selected(bool selected)
{
    if (_base_item)
        _base_item->setSelected(selected);
}

bool SciQLopSpanBase::selected() const noexcept
{
    return _base_item ? _base_item->selected() : false;
}

void SciQLopSpanBase::set_read_only(bool read_only)
{
    if (_base_item)
        _base_item->setMovable(!read_only);
}

bool SciQLopSpanBase::read_only() const noexcept
{
    return _base_item ? !_base_item->movable() : true;
}

void SciQLopSpanBase::set_tool_tip(const QString& tool_tip) noexcept
{
    if (_base_item)
    {
        _base_item->setToolTip(tool_tip);
        _span_base->set_borders_tool_tip(tool_tip);
    }
}

QString SciQLopSpanBase::tool_tip() const noexcept
{
    return _base_item ? _base_item->toolTip() : QString {};
}

void SciQLopSpanBase::replot()
{
    if (_base_item)
        _span_base->replot();
}
```

- [ ] **Step 9: Refactor `SciQLopVerticalSpan` to use `SciQLopSpanBase` member**

In `SciQLopVerticalSpan`:

```cpp
class SciQLopVerticalSpan : public SciQLopRangeItemInterface
{
    Q_OBJECT
    QPointer<impl::VerticalSpan> _impl;
    SciQLopSpanBase _base;  // helper, NOT a base class
    ...
```

Constructor:

```cpp
SciQLopVerticalSpan::SciQLopVerticalSpan(SciQLopPlot* plot, SciQLopPlotRange horizontal_range,
    QColor color, bool read_only, bool visible, const QString& tool_tip)
        : SciQLopRangeItemInterface { plot }
        , _impl { new impl::VerticalSpan { plot->qcp_plot(), horizontal_range, true } }
        , _base { _impl.data(), _impl.data() }
{
    _base.set_color(color);
    _base.set_read_only(read_only);
    _base.set_visible(visible);
    _base.set_tool_tip(tool_tip);
    _impl->set_borders_color(color.lighter());
    // ... signal connections for range, selection, border selection, delete
    _impl->replot();
}
```

Methods delegate to `_base`:

```cpp
void set_visible(bool visible) noexcept override { _base.set_visible(visible); }
bool visible() const noexcept override { return _base.visible(); }
void set_color(const QColor& color) { _base.set_color(color); }
// ... etc
```

Remove the inline destructor body — `_base` destructor handles item removal.

Apply the same pattern to `SciQLopHorizontalSpan` and `SciQLopRectangularSpan`.

- [ ] **Step 10: Refactor `SciQLopHorizontalSpan` to use `SciQLopSpanBase` member**

Same pattern as step 9 — add `SciQLopSpanBase _base` member, delegate shared methods, keep range-specific methods.

- [ ] **Step 11: Refactor `SciQLopRectangularSpan` to use `SciQLopSpanBase` member**

Same pattern. Note: `SciQLopRectangularSpan` inherits `SciQLopMovableItemInterface` (not `SciQLopRangeItemInterface`), so its constructor calls `SciQLopMovableItemInterface { plot }`.

- [ ] **Step 12: Add new .cpp files to `meson.build`**

In `SciQLopPlots/meson.build`, add after the span .cpp entries (around line 201):

```
            '../src/SpanBase.cpp',
            '../src/SciQLopSpanBase.cpp',
```

- [ ] **Step 13: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation.

- [ ] **Step 14: Commit**

```bash
git add include/SciQLopPlots/Items/impl/SpanBase.hpp \
        src/SpanBase.cpp \
        include/SciQLopPlots/Items/SciQLopSpanBase.hpp \
        src/SciQLopSpanBase.cpp \
        include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp \
        src/SciQLopVerticalSpan.cpp \
        include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp \
        src/SciQLopHorizontalSpan.cpp \
        include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp \
        src/SciQLopRectangularSpan.cpp \
        SciQLopPlots/meson.build
git commit -m "refactor(REFAC-02): extract SpanBase and SciQLopSpanBase for span dedup"
```

---

## Task 5: REFAC-05 — Rename MultiPlotsVerticalSpan accessors

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp:84,88,102,120,134,148`
- Modify: `src/MultiPlotsVSpan.cpp:188`
- Modify: `SciQLopPlots/bindings/bindings.xml:657-662`
- Modify: `tests/integration/test_panel_api.py:177,179,188`

- [ ] **Step 1: Rename methods in header**

In `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp`:

| Line | Old | New |
|------|-----|-----|
| 84 | `QString get_id() const` | `QString id() const` |
| 88 | `bool is_selected() const noexcept` | `bool selected() const noexcept` |
| 102 | `QColor get_color() const` | `QColor color() const` |
| 120 | `bool is_visible() const noexcept` | `bool visible() const noexcept` |
| 134 | `QString get_tool_tip() const noexcept` | `QString tool_tip() const noexcept` |
| 148 | `bool is_read_only() const noexcept` | `bool read_only() const noexcept` |

- [ ] **Step 2: Update caller in `MultiPlotsVSpan.cpp`**

In `src/MultiPlotsVSpan.cpp`, line 188, change:

```cpp
if (span && span->get_id() == id)
```

to:

```cpp
if (span && span->id() == id)
```

- [ ] **Step 3: Update bindings.xml property getters**

In `SciQLopPlots/bindings/bindings.xml`, update lines 657-662:

| Line | Old `get=` | New `get=` |
|------|-----------|-----------|
| 657 | `get="get_color"` | `get="color"` |
| 658 | `get="is_visible"` | `get="visible"` |
| 659 | `get="is_selected"` | `get="selected"` |
| 660 | `get="get_tool_tip"` | `get="tool_tip"` |
| 661 | `get="is_read_only"` | `get="read_only"` |
| 662 | `get="get_id"` | `get="id"` |

- [ ] **Step 4: Update test code**

In `tests/integration/test_panel_api.py`:

Line 177: `assert span.is_visible()` → `assert span.visible`
Line 179: `assert not span.is_visible()` → `assert not span.visible`
Line 188: `assert span.get_color().red() == 255` → `assert span.color.red() == 255`

Note: With the Shiboken `<property>` declarations using `generate-getsetdef="yes"`, these are accessed as Python properties (no parentheses), not method calls. The test code may already be using property syntax — verify before editing.

- [ ] **Step 5: Build and verify**

Run: `meson compile -C build`
Expected: Clean compilation.

- [ ] **Step 6: Run tests**

Run: `QT_QPA_PLATFORM=offscreen python -m pytest tests/integration/test_panel_api.py -v`
Expected: All tests pass with renamed accessors.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp \
        src/MultiPlotsVSpan.cpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_panel_api.py
git commit -m "refactor(REFAC-05): rename MultiPlotsVerticalSpan accessors to bare style"
```

---

## Task 6: Final verification

- [ ] **Step 1: Full rebuild**

```bash
meson setup build --wipe --buildtype=debug && meson compile -C build
```

- [ ] **Step 2: Run all tests**

```bash
cd /tmp && QT_QPA_PLATFORM=offscreen python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/ -v
```

- [ ] **Step 3: Verify no regressions in manual gallery**

```bash
meson test -C build
```
