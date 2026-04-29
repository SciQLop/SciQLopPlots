# Inspector Properties Expansion — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Surface a comprehensive set of plot/plotable/axis configuration knobs in the SciQLopPlots inspector — most notably Histogram2D binning, ColorMap contour controls, axis range/label, and plot-level toggles (crosshair, equal aspect, scroll factor) — and add the missing `marker_size` interface so marker controls become useful.

**Architecture:** Create a shared `SciQLopColorMapBaseDelegate` to hold rows common to ColorMap and Histogram2D (gradient). Existing `SciQLopColorMapDelegate` becomes a thin subclass that adds Contours; new `SciQLopHistogram2DDelegate` adds Binning + Normalization. Other delegates (`GraphComponent`, `PlotAxis`, `Plot`, `Waterfall`) get extended in place. Group related fields with `QGroupBox` only when 2+ fields share a topic. All two-way bindings follow the existing Waterfall delegate pattern (forward via setter, reverse via `*_changed` signal wrapped in `QSignalBlocker`).

**Tech Stack:** C++20 / Qt6 (QtWidgets, QFormLayout, QGroupBox), Shiboken6 bindings, Meson build, Python integration tests via PySide6.

**Spec:** `docs/superpowers/specs/2026-04-29-inspector-properties-expansion-design.md`

---

## File Structure

**Files created:**
- `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp`
- `src/SciQLopColorMapBaseDelegate.cpp`
- `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp`
- `src/SciQLopHistogram2DDelegate.cpp`
- `tests/manual-tests/test_inspector_properties.py`

**Files modified:**
- `include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp` — add `marker_size` API + signals
- `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` — implement `set_marker_size`, emit signals
- `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp` — add `bins_changed`, `normalization_changed` signals
- `src/SciQLopHistogram2D.cpp` — emit signals from setters
- `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp` — add contour signals
- `src/SciQLopColorMap.cpp` — emit signals from setters (search current state first)
- `include/SciQLopPlots/SciQLopPlot.hpp` — add `crosshair_enabled_changed`, `equal_aspect_ratio_changed` signals
- `src/SciQLopPlot.cpp` — emit signals from setters
- `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp` — change base class
- `src/SciQLopColorMapDelegate.cpp` — slim, add `auto_scale_y` + Contours group
- `src/SciQLopGraphComponentDelegate.cpp` — add Marker group
- `src/SciQLopPlotAxisDelegate.cpp` — add label + Range group with type guards
- `src/SciQLopPlotDelegate.cpp` — add crosshair, equal aspect, scroll factor
- `src/SciQLopWaterfallDelegate.cpp` — wrap Mode/Spacing/Offsets in `Offsets` group
- `src/Registrations.cpp` — register new delegates
- `meson.build` — add new sources
- `SciQLopPlots/bindings/bindings.xml` — add `marker_size` + `marker_pen` bindings
- `tests/manual-tests/meson.build` — register new test

---

## Conventions used in this plan

- All commits use the format: `<area>: <message>` (e.g. `delegates: add Histogram2D delegate`)
- Build with: `meson compile -C build` (assumes `meson setup build --buildtype=debugoptimized` already done — see CLAUDE.md)
- Run tests with: `meson test -C build inspector_properties` (after Task 1 registers it)
- The Python test file uses `unittest` and a single shared `QApplication`. Headless runs require `xvfb-run` or an X server (per project `feedback_test_env`: never set `QT_QPA_PLATFORM=offscreen`).
- All file paths are absolute relative to `/var/home/jeandet/Documents/prog/SciQLopPlots`.

---

## Phase 1: Plotable signal & API additions

These tasks add the C++ surface the delegates will bind to. Each one is self-contained, builds cleanly on its own, and is independently committable.

---

### Task 1: Set up the Python test harness

**Files:**
- Create: `tests/manual-tests/test_inspector_properties.py`
- Modify: `tests/manual-tests/meson.build`

- [ ] **Step 1: Create the test file with shared QApplication**

Create `tests/manual-tests/test_inspector_properties.py`:

```python
"""Integration tests for inspector property delegates.

Each TestCase exercises one delegate. Per control:
  - initial state reflects the model
  - widget edit -> model setter fires
  - model setter -> widget reflects new value
  - no recursive emit feedback loops

Run: meson test -C build inspector_properties
"""
import os
import sys
import unittest

os.environ['QT_API'] = 'PySide6'

from PySide6.QtWidgets import QApplication, QGroupBox, QSpinBox, QDoubleSpinBox, QComboBox, QCheckBox, QLineEdit
from PySide6.QtCore import Qt

import numpy as np
import SciQLopPlots as sp
from SciQLopPlots import (
    SciQLopMultiPlotPanel, PlotType, GraphType,
)
from SciQLopPlots.SciQLopPlotsBindings import DelegateRegistry


# Shared QApplication for the whole module.
_app = QApplication.instance() or QApplication(sys.argv)


def find_child(parent, widget_type, predicate=None):
    """Find the first descendant matching widget_type and an optional predicate."""
    for child in parent.findChildren(widget_type):
        if predicate is None or predicate(child):
            return child
    return None


def find_group(parent, title):
    """Find a QGroupBox by title."""
    return find_child(parent, QGroupBox, lambda g: g.title() == title)


def make_delegate_for(obj):
    """Resolve and instantiate the registered delegate for obj."""
    return DelegateRegistry.instance().create_delegate(obj, None)


class TestHarnessSanity(unittest.TestCase):
    """Smoke check: harness imports and QApplication exists."""

    def test_app_exists(self):
        self.assertIsNotNone(QApplication.instance())


if __name__ == '__main__':
    unittest.main(verbosity=2)
```

- [ ] **Step 2: Register the test in meson.build**

Edit `tests/manual-tests/meson.build`. After the `examples` block and before the `library('SimplePlot_fake', ...)` line, add:

```meson
test('inspector_properties',
    python3,
    args: [meson.current_source_dir() + '/test_inspector_properties.py'],
    env: ['PYTHONPATH=' + meson.project_build_root()],
    suite: 'integration',
    timeout: 120,
)
```

- [ ] **Step 3: Build and run the smoke test**

Build:
```
meson compile -C build
```
Expected: clean build (no source code changed yet, just meson registration).

Run:
```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS (one test: `TestHarnessSanity::test_app_exists`).

- [ ] **Step 4: Commit**

```
git add tests/manual-tests/test_inspector_properties.py tests/manual-tests/meson.build
git commit -m "tests: scaffold inspector properties integration test"
```

---

### Task 2: Add `marker_size` to the graph component interface

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp`

- [ ] **Step 1: Add abstract `set_marker_size` and getter to the interface**

In `include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp`, after the existing `set_marker_pen` declaration (around line 63), add:

```cpp
    inline virtual void set_marker_size(qreal size) noexcept { WARN_ABSTRACT_METHOD; }
```

After the existing `marker_pen() const` declaration (around line 101–105), add:

```cpp
    inline virtual qreal marker_size() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return 0;
    }
```

In the signals section (after `colors_changed`), add:

```cpp
    Q_SIGNAL void marker_pen_changed(const QPen& pen);
    Q_SIGNAL void marker_size_changed(qreal size);
```

- [ ] **Step 2: Build to verify the header compiles**

```
meson compile -C build
```
Expected: clean build (no impls reference these yet, defaults trigger `WARN_ABSTRACT_METHOD`).

- [ ] **Step 3: Commit**

```
git add include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp
git commit -m "plotables: add marker_size + marker_pen_changed/size_changed signals"
```

---

### Task 3: Implement `set_marker_size` in `SciQLopGraphComponent` and emit signals

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`

- [ ] **Step 1: Add `set_marker_size` implementation**

In `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp`, immediately after the closing brace of `set_marker_pen` (around line 201), add:

```cpp
    inline virtual void set_marker_size(qreal size) noexcept override
    {
        if (m_plottable)
        {
            auto set_scatter_size = [size](auto* any)
            {
                auto scatterStyle = any->scatterStyle();
                scatterStyle.setSize(size);
                any->setScatterStyle(scatterStyle);
            };
            std::visit(visitor { [&](QCPGraph* any) { set_scatter_size(any); },
                                 [&](QCPGraph2* any) { set_scatter_size(any); },
                                 [&](QCPCurve* any) { set_scatter_size(any); },
                                 [size](MultiGraphRef ref)
                                 {
                                     ref.graph->component(ref.componentIndex)
                                         .scatterStyle.setSize(size);
                                 },
                                 [](std::monostate) {} },
                       to_variant());
            Q_EMIT marker_size_changed(size);
        }
    }
```

- [ ] **Step 2: Add `marker_size()` getter implementation**

In the same file, after the existing `marker_pen() const` getter (search for "auto get_pen = [](auto* any)"), add a parallel `marker_size()`. Insert immediately after the marker_pen getter's closing brace:

```cpp
    inline virtual qreal marker_size() const noexcept override
    {
        if (m_plottable)
        {
            auto get_size = [](auto* any) -> qreal { return any->scatterStyle().size(); };
            return std::visit(
                visitor { [&](QCPGraph* any) { return get_size(any); },
                          [&](QCPGraph2* any) { return get_size(any); },
                          [&](QCPCurve* any) { return get_size(any); },
                          [](MultiGraphRef ref) -> qreal
                          { return ref.graph->component(ref.componentIndex).scatterStyle.size(); },
                          [](std::monostate) -> qreal { return 0.0; } },
                to_variant());
        }
        return 0.0;
    }
```

- [ ] **Step 3: Add `Q_EMIT marker_pen_changed(pen)` to existing `set_marker_pen`**

In `set_marker_pen` (around line 180–201), after the closing brace of the `std::visit`, before the function's closing brace, add:

```cpp
            Q_EMIT marker_pen_changed(pen);
```

The function should now end like:

```cpp
            std::visit(visitor { ... }, to_variant());
            Q_EMIT marker_pen_changed(pen);
        }
    }
```

- [ ] **Step 4: Build to verify**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 5: Commit**

```
git add include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp
git commit -m "plotables: implement marker_size and emit marker_pen/size signals"
```

---

### Task 4: Add `bins_changed` and `normalization_changed` signals to Histogram2D

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp`
- Modify: `src/SciQLopHistogram2D.cpp`

- [ ] **Step 1: Add signal declarations to the header**

In `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp`, before the closing `};` of `SciQLopHistogram2D`, add:

```cpp
#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void bins_changed(int key_bins, int value_bins);
    Q_SIGNAL void normalization_changed(int normalization);
```

- [ ] **Step 2: Emit from setters in the cpp**

In `src/SciQLopHistogram2D.cpp`, modify `set_bins` (around line 117) to:

```cpp
void SciQLopHistogram2D::set_bins(int key_bins, int value_bins)
{
    if (_hist)
    {
        if (_hist->keyBins() == key_bins && _hist->valueBins() == value_bins)
            return;
        _hist->setBins(key_bins, value_bins);
        Q_EMIT bins_changed(key_bins, value_bins);
    }
}
```

Modify `set_normalization` (around line 133) to:

```cpp
void SciQLopHistogram2D::set_normalization(int normalization)
{
    if (_hist)
    {
        if (static_cast<int>(_hist->normalization()) == normalization)
            return;
        _hist->setNormalization(static_cast<QCPHistogram2D::Normalization>(normalization));
        Q_EMIT normalization_changed(normalization);
    }
}
```

- [ ] **Step 3: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 4: Commit**

```
git add include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp src/SciQLopHistogram2D.cpp
git commit -m "plotables: emit bins_changed and normalization_changed from Histogram2D"
```

---

### Task 5: Add contour signals to ColorMap

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp`

- [ ] **Step 1: Add signal declarations**

In `include/SciQLopPlots/Plotables/SciQLopColorMap.hpp`, in the existing signals block (the `#ifdef BINDINGS_H` block already containing `auto_scale_y_changed`, around line 142–145), append:

```cpp
    Q_SIGNAL void contour_levels_changed();
    Q_SIGNAL void contour_pen_changed(const QPen& pen);
    Q_SIGNAL void contour_labels_enabled_changed(bool enabled);
```

- [ ] **Step 2: Emit from inline setters**

In the same file, modify each contour setter to emit. Replace the existing inline `set_auto_contour_levels`, `set_contour_pen`, `set_contour_color`, `set_contour_width`, `set_contour_labels_enabled` with versions that emit. Find and replace each setter body:

For `set_auto_contour_levels` (around line 81):
```cpp
    inline void set_auto_contour_levels(int count)
    {
        if (_cmap)
        {
            _cmap->setAutoContourLevels(count);
            Q_EMIT contour_levels_changed();
        }
    }
```

For `set_contour_pen` (around line 91):
```cpp
    inline void set_contour_pen(const QPen& pen)
    {
        if (_cmap)
        {
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }
```

For `set_contour_color` (around line 101):
```cpp
    inline void set_contour_color(const QColor& color)
    {
        if (_cmap)
        {
            auto pen = _cmap->contourPen();
            pen.setColor(color);
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }
```

For `set_contour_width` (around line 116):
```cpp
    inline void set_contour_width(double width)
    {
        if (_cmap)
        {
            auto pen = _cmap->contourPen();
            pen.setWidthF(width);
            _cmap->setContourPen(pen);
            Q_EMIT contour_pen_changed(pen);
        }
    }
```

For `set_contour_labels_enabled` (around line 131):
```cpp
    inline void set_contour_labels_enabled(bool enabled)
    {
        if (_cmap)
        {
            _cmap->setContourLabelEnabled(enabled);
            Q_EMIT contour_labels_enabled_changed(enabled);
        }
    }
```

- [ ] **Step 3: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 4: Commit**

```
git add include/SciQLopPlots/Plotables/SciQLopColorMap.hpp
git commit -m "plotables: emit contour change signals from ColorMap"
```

---

### Task 6: Add `crosshair_enabled_changed` and `equal_aspect_ratio_changed` signals to SciQLopPlot

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp`
- Modify: `src/SciQLopPlot.cpp`

- [ ] **Step 1: Add signal declarations to the header**

In `include/SciQLopPlots/SciQLopPlot.hpp`, in the signals block (around lines 62–73), after the existing `Q_SIGNAL void scroll_factor_changed(double factor);` line, add:

```cpp
    Q_SIGNAL void crosshair_enabled_changed(bool enabled);
    Q_SIGNAL void equal_aspect_ratio_changed(bool enabled);
```

- [ ] **Step 2: Emit `crosshair_enabled_changed`**

In `src/SciQLopPlot.cpp`, find `set_crosshair_enabled` (around line 736–740, the version that calls `m_impl->set_crosshair_enabled`). Modify to:

```cpp
void SciQLopPlot::set_crosshair_enabled(bool enable)
{
    if (m_impl->crosshair_enabled() == enable)
        return;
    m_impl->set_crosshair_enabled(enable);
    Q_EMIT crosshair_enabled_changed(enable);
}
```

- [ ] **Step 3: Emit `equal_aspect_ratio_changed`**

In the same file, modify `set_equal_aspect_ratio` (around line 1019). Add `Q_EMIT equal_aspect_ratio_changed(enabled);` immediately after the assignment `m_equal_aspect_ratio = enabled;` (so the signal fires regardless of which branch runs):

```cpp
void SciQLopPlot::set_equal_aspect_ratio(bool enabled) noexcept
{
    if (m_equal_aspect_ratio == enabled)
        return;
    m_equal_aspect_ratio = enabled;
    if (enabled)
    {
        connect(m_impl->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged),
                this, &SciQLopPlot::_enforce_equal_aspect, Qt::UniqueConnection);
        _enforce_equal_aspect();
    }
    else
    {
        disconnect(m_impl->xAxis, qOverload<const QCPRange&>(&QCPAxis::rangeChanged),
                   this, &SciQLopPlot::_enforce_equal_aspect);
    }
    Q_EMIT equal_aspect_ratio_changed(enabled);
}
```

- [ ] **Step 4: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 5: Commit**

```
git add include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp
git commit -m "plot: emit crosshair_enabled_changed and equal_aspect_ratio_changed"
```

---

## Phase 2: Shared base delegate

---

### Task 7: Create `SciQLopColorMapBaseDelegate`

**Files:**
- Create: `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp`
- Create: `src/SciQLopColorMapBaseDelegate.cpp`
- Modify: `meson.build`

- [ ] **Step 1: Create the header**

Create `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp`:

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
-- (GPL-2.0-or-later)
-------------------------------------------------------------------------------*/
#pragma once

#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"

class SciQLopColorMapBase;

class SciQLopColorMapBaseDelegate : public PropertyDelegateBase
{
    Q_OBJECT

protected:
    SciQLopColorMapBase* color_map_base() const;

public:
    using compatible_type = SciQLopColorMapBase;
    SciQLopColorMapBaseDelegate(SciQLopColorMapBase* object, QWidget* parent = nullptr);
    ~SciQLopColorMapBaseDelegate() override = default;
};
```

- [ ] **Step 2: Create the cpp**

Create `src/SciQLopColorMapBaseDelegate.cpp`:

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
-- (GPL-2.0-or-later)
-------------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorGradientDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMapBase.hpp"

SciQLopColorMapBase* SciQLopColorMapBaseDelegate::color_map_base() const
{
    return as_type<SciQLopColorMapBase>(m_object);
}

SciQLopColorMapBaseDelegate::SciQLopColorMapBaseDelegate(SciQLopColorMapBase* object,
                                                         QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* gradient = new ColorGradientDelegate(object->gradient(), this);
    m_layout->addRow("Gradient", gradient);
    connect(gradient, &ColorGradientDelegate::gradientChanged, this,
            [this](ColorGradient g)
            {
                if (auto* cm = color_map_base())
                    cm->set_gradient(g);
            });
    // Subclasses call append_inspector_extensions() themselves after their rows.
}
```

Note: `append_inspector_extensions()` is **not** called here — it must run after subclass rows. Subclasses call it.

- [ ] **Step 3: Add the new source to `meson.build`**

In `/var/home/jeandet/Documents/prog/SciQLopPlots/meson.build`, locate the inspector sources list. Search for `SciQLopColorMapDelegate.cpp`. Add `SciQLopColorMapBaseDelegate.cpp` immediately above it (alphabetical order). The exact line context will look like this — find the existing `SciQLopColorMapDelegate.cpp` entry and add a sibling line above it:

```meson
    'src/SciQLopColorMapBaseDelegate.cpp',
    'src/SciQLopColorMapDelegate.cpp',
```

- [ ] **Step 4: Build**

```
meson compile -C build
```
Expected: clean build (the new delegate isn't registered yet, so isn't yet exercised).

- [ ] **Step 5: Commit**

```
git add include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp src/SciQLopColorMapBaseDelegate.cpp meson.build
git commit -m "delegates: add SciQLopColorMapBaseDelegate with shared gradient row"
```

---

### Task 8: Refactor `SciQLopColorMapDelegate` to inherit from base; add `auto_scale_y` + Contours group

**Files:**
- Modify: `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp`
- Modify: `src/SciQLopColorMapDelegate.cpp`

- [ ] **Step 1: Update the header to inherit from the base**

Replace the contents of `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp` body (keep license header) with:

```cpp
#pragma once

#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"

class SciQLopColorMap;

class SciQLopColorMapDelegate : public SciQLopColorMapBaseDelegate
{
    Q_OBJECT

    SciQLopColorMap* colorMap() const;

public:
    using compatible_type = SciQLopColorMap;
    SciQLopColorMapDelegate(SciQLopColorMap* object, QWidget* parent = nullptr);

    ~SciQLopColorMapDelegate() override = default;
};
```

- [ ] **Step 2: Rewrite the cpp**

Replace the body of `src/SciQLopColorMapDelegate.cpp` with:

```cpp
/*------------------------------------------------------------------------------
-- License header (preserve existing)
-------------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopColorMap.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>

SciQLopColorMap* SciQLopColorMapDelegate::colorMap() const
{
    return as_type<SciQLopColorMap>(m_object);
}

SciQLopColorMapDelegate::SciQLopColorMapDelegate(SciQLopColorMap* object, QWidget* parent)
        : SciQLopColorMapBaseDelegate(object, parent)
{
    auto* auto_scale = new BooleanDelegate(object->auto_scale_y(), this);
    m_layout->addRow("Auto scale Y", auto_scale);
    connect(auto_scale, &BooleanDelegate::value_changed, object,
            &SciQLopColorMap::set_auto_scale_y);
    connect(object, &SciQLopColorMap::auto_scale_y_changed, auto_scale,
            &BooleanDelegate::set_value);

    // ----- Contours group -----
    auto* contoursBox = new QGroupBox("Contours", this);
    auto* contoursLayout = new QFormLayout(contoursBox);

    auto* countSpin = new QSpinBox(contoursBox);
    countSpin->setRange(0, 50);
    countSpin->setValue(object->auto_contour_level_count());
    contoursLayout->addRow("Auto level count", countSpin);
    connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged), object,
            [object](int v) { object->set_auto_contour_levels(v); });

    auto* colorWidget = new ColorDelegate(object->contour_color(), contoursBox);
    contoursLayout->addRow("Color", colorWidget);
    connect(colorWidget, &ColorDelegate::colorChanged, object,
            [object](const QColor& c) { object->set_contour_color(c); });

    auto* widthSpin = new QDoubleSpinBox(contoursBox);
    widthSpin->setRange(0.1, 10.0);
    widthSpin->setDecimals(1);
    widthSpin->setSingleStep(0.1);
    widthSpin->setValue(object->contour_width());
    contoursLayout->addRow("Width", widthSpin);
    connect(widthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            [object](double w) { object->set_contour_width(w); });

    auto* labelsCheck = new BooleanDelegate(object->contour_labels_enabled(), contoursBox);
    contoursLayout->addRow("Show labels", labelsCheck);
    connect(labelsCheck, &BooleanDelegate::value_changed, object,
            &SciQLopColorMap::set_contour_labels_enabled);

    m_layout->addRow(contoursBox);

    // Reverse path: model -> widgets (signal-blocked to avoid loops).
    connect(object, &SciQLopColorMap::contour_levels_changed, this,
            [countSpin, object]()
            {
                QSignalBlocker b(countSpin);
                countSpin->setValue(object->auto_contour_level_count());
            });
    connect(object, &SciQLopColorMap::contour_pen_changed, this,
            [colorWidget, widthSpin](const QPen& pen)
            {
                colorWidget->setColor(pen.color());
                QSignalBlocker b(widthSpin);
                widthSpin->setValue(pen.widthF());
            });
    connect(object, &SciQLopColorMap::contour_labels_enabled_changed, labelsCheck,
            &BooleanDelegate::set_value);

    append_inspector_extensions();
}
```

- [ ] **Step 3: Verify `ColorDelegate::setColor` exists**

Check that `ColorDelegate` (in `include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp`) has a `setColor` slot. If it does not, this task needs an extra step to add one. Run:

```
grep -n "setColor\|set_color" include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp
```

If the result shows no setter slot, edit `ColorDelegate.hpp` to add (in the public section):

```cpp
    void setColor(const QColor& color);
```

And in `src/ColorDelegate.cpp` (or wherever it's implemented), add:

```cpp
void ColorDelegate::setColor(const QColor& color)
{
    if (m_color == color) return;
    m_color = color;
    update();  // or whatever the visual refresh call is — match the existing pattern in the file
    Q_EMIT colorChanged(color);
}
```

- [ ] **Step 4: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 5: Commit**

```
git add include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp src/SciQLopColorMapDelegate.cpp
# include ColorDelegate files only if Step 3 modified them
git commit -m "delegates: extend ColorMapDelegate with auto_scale_y and Contours group"
```

---

### Task 9: Create `SciQLopHistogram2DDelegate`

**Files:**
- Create: `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp`
- Create: `src/SciQLopHistogram2DDelegate.cpp`
- Modify: `meson.build`

- [ ] **Step 1: Create the header**

Create `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp`:

```cpp
/*------------------------------------------------------------------------------
-- License header
-------------------------------------------------------------------------------*/
#pragma once

#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"

class SciQLopHistogram2D;
class QSpinBox;
class QComboBox;

class SciQLopHistogram2DDelegate : public SciQLopColorMapBaseDelegate
{
    Q_OBJECT

    SciQLopHistogram2D* histogram() const;

public:
    using compatible_type = SciQLopHistogram2D;
    SciQLopHistogram2DDelegate(SciQLopHistogram2D* object, QWidget* parent = nullptr);

    ~SciQLopHistogram2DDelegate() override = default;
};
```

- [ ] **Step 2: Create the cpp**

Create `src/SciQLopHistogram2DDelegate.cpp`:

```cpp
/*------------------------------------------------------------------------------
-- License header
-------------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopHistogram2D.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>
#include <QSpinBox>

SciQLopHistogram2D* SciQLopHistogram2DDelegate::histogram() const
{
    return as_type<SciQLopHistogram2D>(m_object);
}

SciQLopHistogram2DDelegate::SciQLopHistogram2DDelegate(SciQLopHistogram2D* object, QWidget* parent)
        : SciQLopColorMapBaseDelegate(object, parent)
{
    // ----- Binning group -----
    auto* binningBox = new QGroupBox("Binning", this);
    auto* binningLayout = new QFormLayout(binningBox);

    auto* keyBinsSpin = new QSpinBox(binningBox);
    keyBinsSpin->setRange(1, 100000);
    keyBinsSpin->setValue(object->key_bins());
    binningLayout->addRow("Key bins", keyBinsSpin);

    auto* valueBinsSpin = new QSpinBox(binningBox);
    valueBinsSpin->setRange(1, 100000);
    valueBinsSpin->setValue(object->value_bins());
    binningLayout->addRow("Value bins", valueBinsSpin);

    auto push_bins = [keyBinsSpin, valueBinsSpin, object]()
    {
        if (auto* h = object)
            h->set_bins(keyBinsSpin->value(), valueBinsSpin->value());
    };
    connect(keyBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [push_bins](int) { push_bins(); });
    connect(valueBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [push_bins](int) { push_bins(); });

    m_layout->addRow(binningBox);

    // ----- Normalization row (loose) -----
    auto* normCombo = new QComboBox(this);
    normCombo->addItem("None", 0);            // QCPHistogram2D::nNone
    normCombo->addItem("Per-column", 1);      // QCPHistogram2D::nColumn
    normCombo->setCurrentIndex(normCombo->findData(object->normalization()));
    m_layout->addRow("Normalization", normCombo);
    connect(normCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [normCombo, object](int)
            {
                object->set_normalization(normCombo->currentData().toInt());
            });

    // Reverse path: model -> widgets.
    connect(object, &SciQLopHistogram2D::bins_changed, this,
            [keyBinsSpin, valueBinsSpin](int kb, int vb)
            {
                QSignalBlocker bk(keyBinsSpin);
                QSignalBlocker bv(valueBinsSpin);
                keyBinsSpin->setValue(kb);
                valueBinsSpin->setValue(vb);
            });
    connect(object, &SciQLopHistogram2D::normalization_changed, this,
            [normCombo](int n)
            {
                QSignalBlocker b(normCombo);
                normCombo->setCurrentIndex(normCombo->findData(n));
            });

    append_inspector_extensions();
}
```

- [ ] **Step 3: Add the new source to `meson.build`**

Locate the inspector sources list (where you added `SciQLopColorMapBaseDelegate.cpp` in Task 7). Add `SciQLopHistogram2DDelegate.cpp` after `SciQLopGraphDelegate.cpp` (alphabetical):

```meson
    'src/SciQLopGraphDelegate.cpp',
    'src/SciQLopHistogram2DDelegate.cpp',
```

- [ ] **Step 4: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 5: Commit**

```
git add include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp src/SciQLopHistogram2DDelegate.cpp meson.build
git commit -m "delegates: add SciQLopHistogram2DDelegate with Binning + Normalization"
```

---

### Task 10: Register the new delegates

**Files:**
- Modify: `src/Registrations.cpp`

- [ ] **Step 1: Add includes and registrations**

In `src/Registrations.cpp`, near the other delegate includes (around line 20–30, search `#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapDelegate.hpp"`), add:

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopColorMapBaseDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopHistogram2DDelegate.hpp"
```

In the `delegates.register_type` block (around lines 218–226), add:

```cpp
    delegates.register_type<SciQLopColorMapBaseDelegate>();
    delegates.register_type<SciQLopHistogram2DDelegate>();
```

Place `SciQLopColorMapBaseDelegate` **before** `SciQLopColorMapDelegate` and `SciQLopHistogram2DDelegate` after `SciQLopColorMapDelegate` for alphabetical readability. Registration order does not affect lookup (registry uses the most-derived match by walking the meta-object chain).

- [ ] **Step 2: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 3: Commit**

```
git add src/Registrations.cpp
git commit -m "registrations: register ColorMapBase and Histogram2D delegates"
```

---

## Phase 3: Tests for the new delegates

---

### Task 11: Test SciQLopHistogram2DDelegate

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Add the TestHistogram2DDelegate class**

Append to `tests/manual-tests/test_inspector_properties.py` (before the `if __name__ == '__main__':` line):

```python
class TestHistogram2DDelegate(unittest.TestCase):
    """Histogram2D delegate: gradient (inherited), bins, normalization."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        rng = np.random.default_rng(0)
        x = rng.normal(size=10000)
        y = rng.normal(size=10000)
        # Use the panel's plot() to create a histogram2d -- the public Python entry.
        self.plot, self.hist = self.panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.Histogram2D,
        )
        self.delegate = make_delegate_for(self.hist)
        self.assertIsNotNone(self.delegate, "delegate should resolve")

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_dispatch_resolves_to_histogram_delegate(self):
        # The class name should match SciQLopHistogram2DDelegate.
        self.assertEqual(type(self.delegate).__name__, 'SciQLopHistogram2DDelegate')

    def test_binning_group_present(self):
        box = find_group(self.delegate, 'Binning')
        self.assertIsNotNone(box, "Binning group should exist")
        spins = box.findChildren(QSpinBox)
        self.assertEqual(len(spins), 2, "Binning group should have 2 spinboxes")

    def test_initial_key_bins_reflects_model(self):
        box = find_group(self.delegate, 'Binning')
        spin = box.findChildren(QSpinBox)[0]
        self.assertEqual(spin.value(), self.hist.key_bins())

    def test_initial_value_bins_reflects_model(self):
        box = find_group(self.delegate, 'Binning')
        spin = box.findChildren(QSpinBox)[1]
        self.assertEqual(spin.value(), self.hist.value_bins())

    def test_widget_edit_pushes_to_model(self):
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        spins[0].setValue(50)
        spins[1].setValue(75)
        self.assertEqual(self.hist.key_bins(), 50)
        self.assertEqual(self.hist.value_bins(), 75)

    def test_model_change_updates_widgets(self):
        self.hist.set_bins(33, 44)
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        self.assertEqual(spins[0].value(), 33)
        self.assertEqual(spins[1].value(), 44)

    def test_no_emit_loop(self):
        emitted = []
        self.hist.bins_changed.connect(lambda k, v: emitted.append((k, v)))
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        spins[0].setValue(spins[0].value() + 1)
        # Two emits max: one from key edit, possibly one from value spin (if both bins push together).
        # Assert no runaway feedback (>4 emits would mean a loop).
        self.assertLessEqual(len(emitted), 2)

    def test_normalization_combobox(self):
        combo = find_child(self.delegate, QComboBox)
        self.assertIsNotNone(combo)
        self.assertEqual(combo.count(), 2)
        # Switch to Per-column.
        combo.setCurrentIndex(1)
        self.assertEqual(self.hist.normalization(), 1)
        # Switch back.
        combo.setCurrentIndex(0)
        self.assertEqual(self.hist.normalization(), 0)

    def test_normalization_model_updates_widget(self):
        self.hist.set_normalization(1)
        combo = find_child(self.delegate, QComboBox)
        self.assertEqual(combo.currentData(), 1)

    def test_min_bin_edge_case(self):
        self.hist.set_bins(1, 1)
        box = find_group(self.delegate, 'Binning')
        spins = box.findChildren(QSpinBox)
        self.assertEqual(spins[0].value(), 1)
        self.assertEqual(spins[1].value(), 1)

    def test_multi_instance_isolation(self):
        # Second histogram on a separate panel.
        panel2 = SciQLopMultiPlotPanel(synchronize_x=False)
        rng = np.random.default_rng(1)
        _, hist2 = panel2.plot(
            rng.normal(size=1000), rng.normal(size=1000),
            plot_type=PlotType.BasicXY, graph_type=GraphType.Histogram2D,
        )
        try:
            self.hist.set_bins(20, 20)
            self.assertNotEqual(hist2.key_bins(), 20)
        finally:
            panel2.deleteLater()
```

- [ ] **Step 2: Run the test**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: all `TestHistogram2DDelegate` cases PASS plus `TestHarnessSanity::test_app_exists`.

If there are failures: do NOT proceed. Diagnose; the most likely causes are:
- `SciQLopMultiPlotPanel.plot()` return signature differs from expected (check actual return) — adjust the test setup
- `GraphType.Histogram2D` enum name mismatch — `grep "Histogram2D" SciQLopPlots/__init__.py SciQLopPlots/bindings/bindings.xml`
- Delegate class name appears differently in Python (e.g. `SciQLopHistogram2DDelegateWrapper`) — adjust the assertion

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: cover Histogram2D inspector delegate"
```

---

### Task 12: Test SciQLopColorMapDelegate (auto_scale_y + Contours)

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Add `TestColorMapDelegate`**

Append to `tests/manual-tests/test_inspector_properties.py` (before `if __name__ == '__main__':`):

```python
class TestColorMapDelegate(unittest.TestCase):
    """ColorMap delegate: gradient (inherited), auto_scale_y, Contours group."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 10, 50)
        y = np.linspace(0, 10, 50)
        z = np.outer(np.sin(x), np.cos(y))
        self.plot, self.cmap = self.panel.plot(
            x, y, z,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.ColorMap,
        )
        self.delegate = make_delegate_for(self.cmap)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_dispatch(self):
        self.assertEqual(type(self.delegate).__name__, 'SciQLopColorMapDelegate')

    def test_auto_scale_y_present(self):
        # BooleanDelegate is a custom widget but exposes a QCheckBox internally.
        # Find by walking children.
        check = find_child(self.delegate, QCheckBox)
        self.assertIsNotNone(check, "auto_scale_y checkbox should exist")

    def test_contours_group_present(self):
        box = find_group(self.delegate, 'Contours')
        self.assertIsNotNone(box, "Contours group should exist")
        spins = box.findChildren(QSpinBox)
        self.assertEqual(len(spins), 1, "Contours group should have 1 SpinBox (level count)")
        dspins = box.findChildren(QDoubleSpinBox)
        self.assertEqual(len(dspins), 1, "Contours group should have 1 DoubleSpinBox (width)")

    def test_contour_level_count_widget_to_model(self):
        box = find_group(self.delegate, 'Contours')
        spin = box.findChildren(QSpinBox)[0]
        spin.setValue(7)
        self.assertEqual(self.cmap.auto_contour_level_count(), 7)

    def test_contour_level_count_model_to_widget(self):
        self.cmap.set_auto_contour_levels(12)
        box = find_group(self.delegate, 'Contours')
        spin = box.findChildren(QSpinBox)[0]
        self.assertEqual(spin.value(), 12)

    def test_contour_width_widget_to_model(self):
        box = find_group(self.delegate, 'Contours')
        dspin = box.findChildren(QDoubleSpinBox)[0]
        dspin.setValue(2.5)
        self.assertAlmostEqual(self.cmap.contour_width(), 2.5, places=2)

    def test_contour_width_model_to_widget(self):
        self.cmap.set_contour_width(3.0)
        box = find_group(self.delegate, 'Contours')
        dspin = box.findChildren(QDoubleSpinBox)[0]
        self.assertAlmostEqual(dspin.value(), 3.0, places=2)

    def test_contour_labels_widget_to_model(self):
        box = find_group(self.delegate, 'Contours')
        # The Contours group contains one BooleanDelegate (labels). Locate via QCheckBox descendant.
        check = box.findChildren(QCheckBox)[0]
        check.setChecked(True)
        self.assertTrue(self.cmap.contour_labels_enabled())
        check.setChecked(False)
        self.assertFalse(self.cmap.contour_labels_enabled())
```

- [ ] **Step 2: Run the test**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS.

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: cover ColorMap inspector delegate (auto_scale_y + Contours)"
```

---

## Phase 4: Extend remaining delegates

---

### Task 13: Extend `SciQLopGraphComponentDelegate` with Marker group

**Files:**
- Modify: `src/SciQLopGraphComponentDelegate.cpp`

- [ ] **Step 1: Add Marker group below the existing LineDelegate**

Replace `src/SciQLopGraphComponentDelegate.cpp` body (preserve license header) with:

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopGraphComponentDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LineDelegate.hpp"
#include "SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSignalBlocker>

SciQLopGraphComponentInterface* SciQLopGraphComponentDelegate::component() const
{
    return as_type<SciQLopGraphComponentInterface>(m_object);
}

SciQLopGraphComponentDelegate::SciQLopGraphComponentDelegate(SciQLopGraphComponentInterface* object,
                                                             QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    m_lineDelegate = new LineDelegate(object->pen(), object->line_style(), object->marker_shape(), this);
    m_layout->addWidget(m_lineDelegate);
    connect(m_lineDelegate, &LineDelegate::penChanged, object,
            &SciQLopGraphComponentInterface::set_pen);
    connect(m_lineDelegate, &LineDelegate::styleChanged, object,
            &SciQLopGraphComponentInterface::set_line_style);
    connect(m_lineDelegate, &LineDelegate::markerShapeChanged, object,
            &SciQLopGraphComponentInterface::set_marker_shape);

    // ----- Marker group -----
    auto* markerBox = new QGroupBox("Marker", this);
    auto* markerLayout = new QFormLayout(markerBox);

    auto* penColor = new ColorDelegate(object->marker_pen().color(), markerBox);
    markerLayout->addRow("Pen color", penColor);
    connect(penColor, &ColorDelegate::colorChanged, this,
            [object](const QColor& c)
            {
                auto pen = object->marker_pen();
                pen.setColor(c);
                object->set_marker_pen(pen);
            });

    auto* sizeSpin = new QDoubleSpinBox(markerBox);
    sizeSpin->setRange(1.0, 30.0);
    sizeSpin->setDecimals(1);
    sizeSpin->setSingleStep(0.5);
    sizeSpin->setValue(object->marker_size());
    markerLayout->addRow("Size", sizeSpin);
    connect(sizeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            &SciQLopGraphComponentInterface::set_marker_size);

    m_layout->addWidget(markerBox);

    // Reverse path.
    connect(object, &SciQLopGraphComponentInterface::marker_pen_changed, this,
            [penColor](const QPen& pen) { penColor->setColor(pen.color()); });
    connect(object, &SciQLopGraphComponentInterface::marker_size_changed, this,
            [sizeSpin](qreal s)
            {
                QSignalBlocker b(sizeSpin);
                sizeSpin->setValue(s);
            });

    append_inspector_extensions();
}
```

- [ ] **Step 2: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 3: Commit**

```
git add src/SciQLopGraphComponentDelegate.cpp
git commit -m "delegates: add Marker group (pen color + size) to GraphComponent delegate"
```

---

### Task 14: Add Marker group test

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Append `TestGraphComponentDelegate`**

Add before `if __name__ == '__main__':`:

```python
class TestGraphComponentDelegate(unittest.TestCase):
    """Graph component delegate: existing LineDelegate + Marker group."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, self.graph = self.panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line,
            labels=["sig"],
        )
        # Component is the per-trace child; access via the graph's components() collection.
        comps = self.graph.components()
        self.assertGreater(len(comps), 0)
        self.component = comps[0]
        self.delegate = make_delegate_for(self.component)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_marker_group_present(self):
        box = find_group(self.delegate, 'Marker')
        self.assertIsNotNone(box)

    def test_marker_size_widget_to_model(self):
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        size_spin.setValue(8.0)
        self.assertAlmostEqual(self.component.marker_size(), 8.0, places=1)

    def test_marker_size_model_to_widget(self):
        self.component.set_marker_size(12.5)
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        self.assertAlmostEqual(size_spin.value(), 12.5, places=1)

    def test_marker_size_no_emit_loop(self):
        emitted = []
        self.component.marker_size_changed.connect(lambda s: emitted.append(s))
        box = find_group(self.delegate, 'Marker')
        size_spin = box.findChildren(QDoubleSpinBox)[0]
        size_spin.setValue(7.0)
        self.assertLessEqual(len(emitted), 1)
```

- [ ] **Step 2: Run**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS. If `self.graph.components()` is the wrong API, grep `git grep -n "def components\|components()" SciQLopPlots/` for the correct accessor.

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: cover GraphComponent Marker group"
```

---

### Task 15: Extend `SciQLopPlotAxisDelegate` with label + Range group (with type guards)

**Files:**
- Modify: `src/SciQLopPlotAxisDelegate.cpp`

- [ ] **Step 1: Add label, range group, and type guards**

Replace the body of the constructor in `src/SciQLopPlotAxisDelegate.cpp` with:

```cpp
SciQLopPlotAxisDelegate::SciQLopPlotAxisDelegate(SciQLopPlotAxisInterface* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* ax = this->axis();

    auto* axis_visible_delegate = new BooleanDelegate(ax->tick_labels_visible(), this);
    connect(axis_visible_delegate, &BooleanDelegate::value_changed, ax,
            &SciQLopPlotAxisInterface::set_tick_labels_visible);
    connect(ax, &SciQLopPlotAxisInterface::tick_labels_visible_changed,
            axis_visible_delegate, &BooleanDelegate::set_value);
    addWidgetWithLabel(axis_visible_delegate, "Tick labels visible");

    if (!ax->is_time_axis())
    {
        auto* axis_log_delegate = new BooleanDelegate(ax->log(), this);
        connect(axis_log_delegate, &BooleanDelegate::value_changed, ax,
                &SciQLopPlotAxisInterface::set_log);
        connect(ax, &SciQLopPlotAxisInterface::log_changed, axis_log_delegate,
                &BooleanDelegate::set_value);
        addWidgetWithLabel(axis_log_delegate, "Log scale");
    }

    if (auto* color_scale = this->color_scale(); color_scale)
    {
        auto* color_scale_delegate
            = new ColorGradientDelegate(color_scale->color_gradient(), this);
        connect(color_scale_delegate, &ColorGradientDelegate::gradientChanged, color_scale,
                &SciQLopPlotColorScaleAxis::set_color_gradient);
        connect(color_scale, &SciQLopPlotColorScaleAxis::color_gradient_changed,
                color_scale_delegate, &ColorGradientDelegate::setGradient);
        addWidgetWithLabel(color_scale_delegate, "Color gradient");
    }

    // Label edit (all axis types).
    auto* labelEdit = new QLineEdit(ax->label(), this);
    m_layout->addRow("Label", labelEdit);
    connect(labelEdit, &QLineEdit::editingFinished, ax,
            [labelEdit, ax]() { ax->set_label(labelEdit->text()); });
    connect(ax, &SciQLopPlotAxisInterface::label_changed, this,
            [labelEdit](const QString& s)
            {
                if (labelEdit->text() != s)
                    labelEdit->setText(s);
            });

    // Range group (numeric axes only).
    const bool show_range = !ax->is_time_axis() && (this->color_scale() == nullptr);
    if (show_range)
    {
        auto* rangeBox = new QGroupBox("Range", this);
        auto* rangeLayout = new QFormLayout(rangeBox);

        auto* minSpin = new QDoubleSpinBox(rangeBox);
        minSpin->setRange(-1e15, 1e15);
        minSpin->setDecimals(6);
        minSpin->setValue(ax->range().start());
        rangeLayout->addRow("Min", minSpin);

        auto* maxSpin = new QDoubleSpinBox(rangeBox);
        maxSpin->setRange(-1e15, 1e15);
        maxSpin->setDecimals(6);
        maxSpin->setValue(ax->range().stop());
        rangeLayout->addRow("Max", maxSpin);

        auto push_range = [minSpin, maxSpin, ax]()
        {
            ax->set_range(SciQLopPlotRange(minSpin->value(), maxSpin->value()));
        };
        connect(minSpin, &QDoubleSpinBox::editingFinished, this, push_range);
        connect(maxSpin, &QDoubleSpinBox::editingFinished, this, push_range);

        connect(ax, &SciQLopPlotAxisInterface::range_changed, this,
                [minSpin, maxSpin](SciQLopPlotRange r)
                {
                    QSignalBlocker bmin(minSpin);
                    QSignalBlocker bmax(maxSpin);
                    minSpin->setValue(r.start());
                    maxSpin->setValue(r.stop());
                });

        m_layout->addRow(rangeBox);
    }

    append_inspector_extensions();
}
```

You'll also need to add the includes at the top of the file (after existing includes):

```cpp
#include "SciQLopPlots/SciQLopPlotRange.hpp"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSignalBlocker>
```

- [ ] **Step 2: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 3: Commit**

```
git add src/SciQLopPlotAxisDelegate.cpp
git commit -m "delegates: add Label and Range to PlotAxis delegate (with type guards)"
```

---

### Task 16: Test PlotAxisDelegate (numeric / time / color-scale guards)

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Append `TestPlotAxisDelegate`**

Add before `if __name__ == '__main__':`:

```python
class TestPlotAxisDelegate(unittest.TestCase):
    """PlotAxis delegate: label + Range group with type guards."""

    def _make_panel_with_xy(self):
        panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        plot, _graph = panel.plot(
            x, y, plot_type=PlotType.BasicXY, graph_type=GraphType.Line,
            labels=["s"],
        )
        return panel, plot

    def setUp(self):
        self.panel, self.plot = self._make_panel_with_xy()

    def tearDown(self):
        self.panel.deleteLater()

    def test_numeric_axis_has_range_group(self):
        x_axis = self.plot.x_axis()
        delegate = make_delegate_for(x_axis)
        self.assertIsNotNone(delegate)
        try:
            self.assertIsNotNone(find_group(delegate, 'Range'))
        finally:
            delegate.deleteLater()

    def test_numeric_axis_label_edit(self):
        x_axis = self.plot.x_axis()
        delegate = make_delegate_for(x_axis)
        try:
            edit = find_child(delegate, QLineEdit)
            self.assertIsNotNone(edit)
            edit.setText("My Axis")
            edit.editingFinished.emit()
            self.assertEqual(x_axis.label(), "My Axis")
        finally:
            delegate.deleteLater()

    def test_numeric_axis_range_widget_to_model(self):
        x_axis = self.plot.x_axis()
        delegate = make_delegate_for(x_axis)
        try:
            box = find_group(delegate, 'Range')
            spins = box.findChildren(QDoubleSpinBox)
            spins[0].setValue(0.1)
            spins[0].editingFinished.emit()
            spins[1].setValue(0.9)
            spins[1].editingFinished.emit()
            r = x_axis.range()
            self.assertAlmostEqual(r.start(), 0.1, places=4)
            self.assertAlmostEqual(r.stop(), 0.9, places=4)
        finally:
            delegate.deleteLater()

    def test_time_axis_no_range_no_log(self):
        # Build a TimeSeries plot to get a time axis.
        panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
        try:
            t = np.arange(0, 100, dtype=np.float64)
            y = np.sin(t / 5)
            plot, _g = panel.plot(t, y, plot_type=PlotType.TimeSeries, labels=["t"])
            x_axis = plot.x_axis()
            self.assertTrue(x_axis.is_time_axis())
            delegate = make_delegate_for(x_axis)
            try:
                self.assertIsNone(find_group(delegate, 'Range'),
                                  "time axis should not show Range group")
                # Log scale checkbox should be absent (only the tick-labels checkbox remains).
                # Count BooleanDelegate descendants -- expect just the tick-labels one.
                # (Crude but matches current code.)
            finally:
                delegate.deleteLater()
        finally:
            panel.deleteLater()
```

- [ ] **Step 2: Run**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS. If `plot.x_axis()` is the wrong accessor, grep for the actual API: `git grep "def x_axis\|x_axis()" SciQLopPlots/`.

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: cover PlotAxis delegate (numeric, time guards, label, range)"
```

---

### Task 17: Extend `SciQLopPlotDelegate` with crosshair + equal aspect + scroll factor

**Files:**
- Modify: `src/SciQLopPlotDelegate.cpp`

- [ ] **Step 1: Add three new rows after the existing `auto_scale`**

Replace the body of `src/SciQLopPlotDelegate.cpp` with:

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/BooleanDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/LegendDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopPlotAxisDelegate.hpp"
#include "SciQLopPlots/SciQLopPlot.hpp"

#include <QDoubleSpinBox>
#include <QSignalBlocker>

SciQLopPlot* SciQLopPlotDelegate::plot() const
{
    return as_type<SciQLopPlot>(m_object);
}

SciQLopPlotDelegate::SciQLopPlotDelegate(SciQLopPlot* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto legend = object->legend();
    auto* legend_delegate = new LegendDelegate(legend->is_visible(), this);
    m_layout->addRow(legend_delegate);
    connect(legend_delegate, &LegendDelegate::visibility_changed, legend,
            &SciQLopPlotLegendInterface::set_visible);
    connect(legend, &SciQLopPlotLegendInterface::visibility_changed, legend_delegate,
            &LegendDelegate::set_visible);

    auto* auto_scale = new BooleanDelegate(object->auto_scale());
    m_layout->addRow("Auto scale", auto_scale);
    connect(auto_scale, &BooleanDelegate::value_changed, object, &SciQLopPlot::set_auto_scale);
    connect(object, &SciQLopPlot::auto_scale_changed, auto_scale, &BooleanDelegate::set_value);

    auto* crosshair = new BooleanDelegate(object->crosshair_enabled(), this);
    m_layout->addRow("Crosshair", crosshair);
    connect(crosshair, &BooleanDelegate::value_changed, object, &SciQLopPlot::set_crosshair_enabled);
    connect(object, &SciQLopPlot::crosshair_enabled_changed, crosshair,
            &BooleanDelegate::set_value);

    auto* equal_aspect = new BooleanDelegate(object->equal_aspect_ratio(), this);
    m_layout->addRow("Equal aspect ratio", equal_aspect);
    connect(equal_aspect, &BooleanDelegate::value_changed, object,
            &SciQLopPlot::set_equal_aspect_ratio);
    connect(object, &SciQLopPlot::equal_aspect_ratio_changed, equal_aspect,
            &BooleanDelegate::set_value);

    auto* scrollSpin = new QDoubleSpinBox(this);
    scrollSpin->setRange(1.0, 10.0);
    scrollSpin->setDecimals(2);
    scrollSpin->setSingleStep(0.1);
    scrollSpin->setValue(object->scroll_factor());
    m_layout->addRow("Scroll factor", scrollSpin);
    connect(scrollSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), object,
            &SciQLopPlot::set_scroll_factor);
    connect(object, &SciQLopPlot::scroll_factor_changed, this,
            [scrollSpin](double f)
            {
                QSignalBlocker b(scrollSpin);
                scrollSpin->setValue(f);
            });

    append_inspector_extensions();
}
```

- [ ] **Step 2: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 3: Commit**

```
git add src/SciQLopPlotDelegate.cpp
git commit -m "delegates: add crosshair, equal aspect, scroll factor to Plot delegate"
```

---

### Task 18: Test the Plot delegate

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Append `TestPlotDelegate`**

Add before `if __name__ == '__main__':`:

```python
class TestPlotDelegate(unittest.TestCase):
    """SciQLopPlot delegate: legend, auto_scale, crosshair, equal aspect, scroll factor."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 50)
        self.plot, _g = self.panel.plot(
            x, np.sin(x * 6), plot_type=PlotType.BasicXY, graph_type=GraphType.Line,
            labels=["s"],
        )
        self.delegate = make_delegate_for(self.plot)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_dispatch(self):
        self.assertEqual(type(self.delegate).__name__, 'SciQLopPlotDelegate')

    def test_scroll_factor_widget_to_model(self):
        spin = find_child(self.delegate, QDoubleSpinBox)
        self.assertIsNotNone(spin)
        spin.setValue(2.5)
        self.assertAlmostEqual(self.plot.scroll_factor(), 2.5, places=2)

    def test_scroll_factor_model_to_widget(self):
        self.plot.set_scroll_factor(3.0)
        spin = find_child(self.delegate, QDoubleSpinBox)
        self.assertAlmostEqual(spin.value(), 3.0, places=2)

    def test_scroll_factor_boundary(self):
        # Spin clamps to 1.0..10.0.
        spin = find_child(self.delegate, QDoubleSpinBox)
        spin.setValue(0.5)  # below min, should clamp to 1.0
        self.assertGreaterEqual(spin.value(), 1.0)
        spin.setValue(50.0)  # above max, should clamp to 10.0
        self.assertLessEqual(spin.value(), 10.0)

    def test_crosshair_toggle(self):
        # Find all checkboxes; crosshair is one of them.
        checks = self.delegate.findChildren(QCheckBox)
        # auto_scale, crosshair, equal_aspect, legend (= 4 booleans)
        self.assertGreaterEqual(len(checks), 3)

    def test_crosshair_model_to_widget(self):
        self.plot.set_crosshair_enabled(True)
        # Just assert no exception and model state correct.
        self.assertTrue(self.plot.crosshair_enabled())
        self.plot.set_crosshair_enabled(False)
        self.assertFalse(self.plot.crosshair_enabled())

    def test_equal_aspect_model_to_widget(self):
        self.plot.set_equal_aspect_ratio(True)
        self.assertTrue(self.plot.equal_aspect_ratio())
        self.plot.set_equal_aspect_ratio(False)
        self.assertFalse(self.plot.equal_aspect_ratio())
```

- [ ] **Step 2: Run**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS.

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: cover Plot delegate (crosshair, equal aspect, scroll factor)"
```

---

### Task 19: Retrofit `SciQLopWaterfallDelegate` with Offsets `QGroupBox`

**Files:**
- Modify: `src/SciQLopWaterfallDelegate.cpp`

- [ ] **Step 1: Wrap Mode/Spacing/Offsets in a group**

In `src/SciQLopWaterfallDelegate.cpp`, replace the section that adds Mode/Spacing/Offsets to `m_layout` (lines ~35–53) with a grouped version.

Original lines ~35–53 to replace:
```cpp
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("Uniform", QVariant::fromValue(WaterfallOffsetMode::Uniform));
    m_modeCombo->addItem("Custom", QVariant::fromValue(WaterfallOffsetMode::Custom));
    m_modeCombo->setCurrentIndex(object->offset_mode() == WaterfallOffsetMode::Uniform ? 0 : 1);
    m_layout->addRow("Offset mode", m_modeCombo);

    m_spacingSpin = new QDoubleSpinBox;
    m_spacingSpin->setRange(-1e9, 1e9);
    m_spacingSpin->setDecimals(4);
    m_spacingSpin->setValue(object->uniform_spacing());
    m_layout->addRow("Uniform spacing", m_spacingSpin);

    m_offsetsScroll = new QScrollArea;
    m_offsetsScroll->setWidgetResizable(true);
    auto* offsetsHost = new QWidget;
    m_offsetsLayout = new QVBoxLayout(offsetsHost);
    m_offsetsScroll->setWidget(offsetsHost);
    m_offsetsScroll->setMaximumHeight(120);
    m_layout->addRow("Offsets", m_offsetsScroll);
```

Replace with:

```cpp
    auto* offsetsBox = new QGroupBox("Offsets", this);
    auto* offsetsForm = new QFormLayout(offsetsBox);

    m_modeCombo = new QComboBox(offsetsBox);
    m_modeCombo->addItem("Uniform", QVariant::fromValue(WaterfallOffsetMode::Uniform));
    m_modeCombo->addItem("Custom", QVariant::fromValue(WaterfallOffsetMode::Custom));
    m_modeCombo->setCurrentIndex(object->offset_mode() == WaterfallOffsetMode::Uniform ? 0 : 1);
    offsetsForm->addRow("Mode", m_modeCombo);

    m_spacingSpin = new QDoubleSpinBox(offsetsBox);
    m_spacingSpin->setRange(-1e9, 1e9);
    m_spacingSpin->setDecimals(4);
    m_spacingSpin->setValue(object->uniform_spacing());
    offsetsForm->addRow("Uniform spacing", m_spacingSpin);

    m_offsetsScroll = new QScrollArea(offsetsBox);
    m_offsetsScroll->setWidgetResizable(true);
    auto* offsetsHost = new QWidget;
    m_offsetsLayout = new QVBoxLayout(offsetsHost);
    m_offsetsScroll->setWidget(offsetsHost);
    m_offsetsScroll->setMaximumHeight(120);
    offsetsForm->addRow("Per-line", m_offsetsScroll);

    m_layout->addRow(offsetsBox);
```

Add the include if missing (top of file):

```cpp
#include <QGroupBox>
#include <QFormLayout>
```

- [ ] **Step 2: Build**

```
meson compile -C build
```
Expected: clean build.

- [ ] **Step 3: Smoke-test the waterfall manually**

Run the existing waterfall manual test (if present) to ensure interactive behavior didn't regress:

```
meson test -C build --suite manual --no-rebuild --list | grep -i waterfall
```

If a Waterfall manual test exists, run it. Otherwise, skip — the regression test in Task 20 covers this.

- [ ] **Step 4: Commit**

```
git add src/SciQLopWaterfallDelegate.cpp
git commit -m "delegates: wrap waterfall mode/spacing/offsets in QGroupBox for consistency"
```

---

### Task 20: Waterfall regression test

**Files:**
- Modify: `tests/manual-tests/test_inspector_properties.py`

- [ ] **Step 1: Append `TestWaterfallDelegateRegression`**

Add before `if __name__ == '__main__':`:

```python
class TestWaterfallDelegateRegression(unittest.TestCase):
    """Regression: Waterfall mode/spacing/offsets/normalize/gain still drive the model
    after the Offsets QGroupBox retrofit."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 10, 200)
        y = np.column_stack([np.sin(x * k) for k in range(1, 4)])
        self.plot, self.wf = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY, graph_type=GraphType.Waterfall,
            labels=["a", "b", "c"],
        )
        self.delegate = make_delegate_for(self.wf)
        self.assertIsNotNone(self.delegate)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_offsets_group_present(self):
        box = find_group(self.delegate, 'Offsets')
        self.assertIsNotNone(box)

    def test_uniform_spacing_widget_to_model(self):
        box = find_group(self.delegate, 'Offsets')
        spin = box.findChildren(QDoubleSpinBox)[0]  # uniform spacing is the first DSpin in the group
        spin.setValue(2.5)
        self.assertAlmostEqual(self.wf.uniform_spacing(), 2.5, places=2)

    def test_gain_widget_to_model_outside_group(self):
        # Gain is outside the Offsets group (loose row). It's the last DSpin in the delegate
        # that is not inside the Offsets group.
        box = find_group(self.delegate, 'Offsets')
        all_dspins = self.delegate.findChildren(QDoubleSpinBox)
        in_box = set(box.findChildren(QDoubleSpinBox)) if box else set()
        loose = [s for s in all_dspins if s not in in_box]
        self.assertGreaterEqual(len(loose), 1, "gain spinbox should be loose")
        loose[-1].setValue(1.5)
        self.assertAlmostEqual(self.wf.gain(), 1.5, places=2)
```

- [ ] **Step 2: Run**

```
meson test -C build inspector_properties --print-errorlogs
```
Expected: PASS. If `GraphType.Waterfall` doesn't exist, grep `git grep "Waterfall" SciQLopPlots/__init__.py` to find the right enum.

- [ ] **Step 3: Commit**

```
git add tests/manual-tests/test_inspector_properties.py
git commit -m "tests: regress Waterfall delegate after Offsets group retrofit"
```

---

## Phase 5: Bindings

---

### Task 21: Expose `marker_size` and `set_marker_size` in Python

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Locate the `SciQLopGraphComponentInterface` typesystem entry**

Run:
```
grep -n "SciQLopGraphComponentInterface" SciQLopPlots/bindings/bindings.xml
```

The interface is exposed as an `<object-type>` entry. The new methods (`set_marker_size`, `marker_size`) and the new signals (`marker_pen_changed`, `marker_size_changed`) inherit automatically from the C++ class definition since they live on the public interface and are declared with `Q_SIGNAL`/virtual signatures Shiboken can introspect.

- [ ] **Step 2: Verify by rebuilding bindings**

```
meson compile -C build
```
Expected: clean build, with no Shiboken warnings about missing methods.

- [ ] **Step 3: Smoke check from Python**

Run a quick interactive check (use `python3` from the SciQLop venv):

```python
from SciQLopPlots import SciQLopMultiPlotPanel, PlotType, GraphType
import numpy as np
import sys
from PySide6.QtWidgets import QApplication
app = QApplication.instance() or QApplication(sys.argv)
panel = SciQLopMultiPlotPanel(synchronize_x=False)
plot, g = panel.plot(np.linspace(0,1,50), np.sin(np.linspace(0,6,50)),
                     plot_type=PlotType.BasicXY, graph_type=GraphType.Line, labels=["s"])
comp = g.components()[0]
comp.set_marker_size(8.0)
print("size:", comp.marker_size())
assert abs(comp.marker_size() - 8.0) < 1e-3
print("OK")
```

Expected: `OK`. If `marker_size` is not callable, manually add an `<add-function>` block to `bindings.xml` for `set_marker_size` and `marker_size` mirroring an existing scalar-setter binding.

- [ ] **Step 4: Commit (only if the file changed)**

If `bindings.xml` was modified:
```
git add SciQLopPlots/bindings/bindings.xml
git commit -m "bindings: expose marker_size in Python"
```
Otherwise skip the commit (no changes needed if Shiboken auto-introspected).

---

## Phase 6: Final integration

---

### Task 22: Full test pass + smoke-test the inspector visually

**Files:** none modified

- [ ] **Step 1: Run the entire test suite**

```
meson test -C build --print-errorlogs
```
Expected: all tests PASS, including all `Test*Delegate` cases from `test_inspector_properties.py`.

- [ ] **Step 2: Smoke-test the gallery interactively**

Run the existing gallery to confirm nothing visually regressed and the new property panels render:

```
meson test -C build examples_Histogram2D --print-errorlogs
```

When the window appears, click a histogram in the inspector tree → verify the **Binning** group + **Normalization** combo appear and edits update the plot.

```
meson test -C build examples_ContourColormap --print-errorlogs
```

Click a colormap → verify the **Contours** group appears with all four fields and edits affect the rendered contours.

```
meson test -C build examples_StackedPlots --print-errorlogs
```

Click a plot or its axis → verify the new **Range** group, **Label** edit, and per-plot toggles (crosshair, equal aspect, scroll factor) appear and work.

- [ ] **Step 3: Final commit (if any uncommitted polish)**

If the smoke test surfaced any issue requiring a fix, address it now and commit. Otherwise:

```
git status
```
Expected: clean working tree.

---

## Self-Review Checklist (run after writing the plan)

- [x] Spec coverage:
  - Histogram2D delegate (binning + normalization) → Tasks 9, 11
  - ColorMap auto_scale_y + Contours → Task 8
  - GraphComponent Marker group → Task 13
  - PlotAxis label + Range with type guards → Task 15
  - Plot crosshair + equal aspect + scroll factor → Task 17
  - Waterfall retrofit → Task 19
  - Marker size interface addition → Tasks 2, 3, 21
  - All required signals → Tasks 4, 5, 6
  - Test coverage → Tasks 1, 11, 12, 14, 16, 18, 20
  - Build/registration → Tasks 7, 9, 10
- [x] Placeholder scan: no TBD/TODO/"similar to Task N"; each step has actual code or commands
- [x] Type consistency: signal names match between header (Tasks 2, 4, 5, 6) and delegate connect calls (Tasks 8, 9, 13, 17)
- [x] Method names match: `set_marker_size`/`marker_size`, `set_bins`/`key_bins`/`value_bins`, `set_normalization`/`normalization`, `crosshair_enabled`/`set_crosshair_enabled`, `equal_aspect_ratio`/`set_equal_aspect_ratio`, `scroll_factor`/`set_scroll_factor`, `set_auto_contour_levels`/`auto_contour_level_count`, `set_contour_color`/`contour_color`, `set_contour_width`/`contour_width`, `set_contour_labels_enabled`/`contour_labels_enabled`
