# Pluggable Panel Export (PDF / PNG) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `SciQLopMultiPlotPanel` PDF/PNG export render nested sub-panels and arbitrary plain widgets (e.g. matplotlib Qt canvases), via a capability-based recursive walker instead of the type-specific `plots()` loops.

**Architecture:** A tiny `SciQLopExportable` interface (one method, `export_paint(QPainter*, QRect, kind)`) discovered by RTTI cross-cast. A free function `export_widget()` dispatches to the interface when present, else falls back to `QWidget::render()` (the universal baseline that handles MPL and any plain widget for free). `SciQLopPlot` implements it for true-vector PDF (`toPainter`) and GPU-correct raster (`toPixmap`); `SciQLopPlotContainer` and `SciQLopMultiPlotPanel` implement it to recurse, mapping each child's on-screen geometry into its allocated rect. `save_pdf`/`save_png` collapse to a single walker call.

**Tech Stack:** C++20, Qt6 (QWidget/QPainter/QPdfWriter), NeoQCP (`QCustomPlot::toPainter`/`toPixmap`, `QCPPainter`), Meson, Python integration tests via pytest-qt.

**Spec:** `docs/superpowers/specs/2026-06-14-pluggable-panel-export-design.md`

---

## Build & test commands (from CLAUDE.md recipe)

All build/test steps in this plan use this preamble. **Never** run a bare `meson setup build`.

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Compile: `$VENV/bin/meson compile -C build-venv`
Run a test from `/tmp` (so the in-tree source dir doesn't shadow the build):
```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_export_api.py -q
```

---

## File Structure

- **New** `include/SciQLopPlots/Export/SciQLopExportable.hpp` — `SciQLopExportTarget` enum, `SciQLopExportable` interface, `export_widget()` declaration. One responsibility: the export capability contract + dispatcher.
- **New** `src/Export/SciQLopExportable.cpp` — `export_widget()` body (dispatch + generic scaled `render()` fallback).
- `include/SciQLopPlots/SciQLopPlot.hpp` + `src/SciQLopPlot.cpp` — `SciQLopPlot` gains `SciQLopExportable` base + `export_paint`.
- `include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp` + `src/SciQLopPlotContainer.cpp` — `SciQLopPlotContainer` gains the base + recursive `export_paint`.
- `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` + `src/SciQLopMultiPlotPanel.cpp` — panel gains the base + forwarding `export_paint`; `save_pdf`/`_save_panel_raster` rewritten to call the walker.
- `SciQLopPlots/meson.build` — add `../src/Export/SciQLopExportable.cpp` to `sources`.
- `tests/integration/test_export_api.py` — new `TestNestedPanelExport` class (extend existing file).
- `SciQLopPlots/bindings/bindings.xml` + `bindings.h` — **(optional, Task 7)** expose the interface for Python opt-in.

---

## Task 1: Reproducer tests (must fail on current build)

**Files:**
- Modify: `tests/integration/test_export_api.py` (append at end)

- [ ] **Step 1: Add a blank-fraction helper + nested-export tests**

Append to `tests/integration/test_export_api.py`:

```python
def _nonblank_fraction(png_path):
    """Fraction of pixels that differ from the top-left (background) pixel."""
    img = QImage(str(png_path))
    assert not img.isNull()
    bg = img.pixel(0, 0)
    w, h = img.width(), img.height()
    if w == 0 or h == 0:
        return 0.0
    differing = 0
    total = 0
    # sample a grid to keep it fast
    step = max(1, min(w, h) // 64)
    for yy in range(0, h, step):
        for xx in range(0, w, step):
            total += 1
            if img.pixel(xx, yy) != bg:
                differing += 1
    return differing / total if total else 0.0


class TestNestedPanelExport:
    """Nested sub-panels must appear in PDF/PNG export (backlog 2026-06-14)."""

    @pytest.fixture
    def nested_only(self, qtbot):
        """Parent panel whose only content is a sub-panel with one plot."""
        panel = SciQLopMultiPlotPanel()
        panel.resize(800, 600)
        qtbot.addWidget(panel)
        sub = SciQLopMultiPlotPanel()
        panel.add_panel(sub)
        x = np.linspace(0, 10, 100).astype(np.float64)
        sub.plot(x, np.sin(x), labels=["nested"])
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        return panel

    @pytest.fixture
    def top_plus_nested(self, qtbot):
        """Top-level plot AND a sub-panel with a plot."""
        panel = SciQLopMultiPlotPanel()
        panel.resize(800, 600)
        qtbot.addWidget(panel)
        x = np.linspace(0, 10, 100).astype(np.float64)
        panel.plot(x, np.cos(x), labels=["top"])
        sub = SciQLopMultiPlotPanel()
        panel.add_panel(sub)
        sub.plot(x, np.sin(x), labels=["nested"])
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        return panel

    def test_nested_only_pdf_succeeds(self, nested_only, tmp_path):
        path = tmp_path / "nested.pdf"
        assert nested_only.save_pdf(str(path)) is True
        assert path.stat().st_size > 0

    def test_nested_only_png_nonblank(self, nested_only, tmp_path):
        path = tmp_path / "nested.png"
        assert nested_only.save_png(str(path)) is True
        assert _nonblank_fraction(path) > 0.02

    def test_top_plus_nested_png_nonblank(self, top_plus_nested, tmp_path):
        path = tmp_path / "mixed.png"
        assert top_plus_nested.save_png(str(path)) is True
        # both regions present -> a meaningful chunk of non-background pixels
        assert _nonblank_fraction(path) > 0.05

    def test_top_plus_nested_pdf_has_nested(self, top_plus_nested, tmp_path):
        # nested-included PDF is materially larger than a single-plot PDF
        nested_path = tmp_path / "mixed.pdf"
        assert top_plus_nested.save_pdf(str(nested_path)) is True
        single = SciQLopMultiPlotPanel()
        single.resize(800, 600)
        x = np.linspace(0, 10, 100).astype(np.float64)
        single.plot(x, np.cos(x), labels=["top"])
        single_path = tmp_path / "single.pdf"
        assert single.save_pdf(str(single_path)) is True
        assert nested_path.stat().st_size > single_path.stat().st_size


class TestPlainWidgetExport:
    """A non-plot QWidget child renders via the QWidget::render fallback.

    This is the matplotlib-canvas path: a plain QWidget added to the panel via
    the public `addWidget(QWidget*)` (auto-exposed by shiboken) exports for free.
    """

    def test_label_region_rendered(self, qtbot, tmp_path):
        from PySide6.QtWidgets import QLabel
        from PySide6.QtCore import Qt
        panel = SciQLopMultiPlotPanel()
        panel.resize(400, 300)
        qtbot.addWidget(panel)
        label = QLabel("EXPORT_ME")
        label.setStyleSheet("background:#ff0000;")
        label.setAlignment(Qt.AlignCenter)
        panel.addWidget(label)  # any QWidget goes into the container
        panel.show()
        qtbot.waitExposed(panel)
        qtbot.wait(200)
        path = tmp_path / "label.png"
        assert panel.save_png(str(path)) is True
        assert _nonblank_fraction(path) > 0.02
```

- [ ] **Step 2: Run the new tests against the CURRENT build — confirm they FAIL**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_export_api.py::TestNestedPanelExport -q
```
Expected: `test_nested_only_pdf_succeeds` FAILS (returns False), `test_nested_only_png_nonblank` FAILS (blank), `test_top_plus_nested_pdf_has_nested` FAILS (nested dropped → not larger). This proves the bug.

- [ ] **Step 3: Commit the failing tests**

```bash
git add tests/integration/test_export_api.py
git commit -m "test: reproduce nested sub-panel export drop (pdf/png)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Export capability interface + walker

**Files:**
- Create: `include/SciQLopPlots/Export/SciQLopExportable.hpp`
- Create: `src/Export/SciQLopExportable.cpp`
- Modify: `SciQLopPlots/meson.build` (sources list, after line 206)

- [ ] **Step 1: Write the header**

Create `include/SciQLopPlots/Export/SciQLopExportable.hpp`:

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
-- (GPL v2+ — same header block as the rest of the tree)
----------------------------------------------------------------------------*/
#pragma once
#include <QPainter>
#include <QRect>

class QWidget;

enum class SciQLopExportTarget
{
    Vector, // painting onto a vector device (QPdfWriter): keep plots vectorial
    Raster  // painting onto a raster device (QPixmap): rasterize each widget
};

// Capability mixin: a widget that can paint a higher-fidelity representation of
// itself than a generic QWidget::render() (true vector, GPU framebuffer, or a
// recursive sub-tree) implements this. Discovered by dynamic_cast cross-cast,
// so it is intentionally NOT a QObject.
class SciQLopExportable
{
public:
    virtual ~SciQLopExportable() = default;

    // Paint self into `target` (device/page coordinates) on `painter`.
    virtual void export_paint(QPainter* painter, const QRect& target,
                              SciQLopExportTarget kind)
        = 0;
};

// Paint any widget into `target`. Uses SciQLopExportable when the widget
// implements it; otherwise scales QWidget::render() into `target` — which
// renders plain widgets (labels, matplotlib Qt canvases, ...) for free.
void export_widget(QWidget* w, QPainter* painter, const QRect& target,
                   SciQLopExportTarget kind);
```

- [ ] **Step 2: Write the walker implementation**

Create `src/Export/SciQLopExportable.cpp`:

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
-- (GPL v2+ — same header block as the rest of the tree)
----------------------------------------------------------------------------*/
#include "SciQLopPlots/Export/SciQLopExportable.hpp"

#include <QRegion>
#include <QWidget>

void export_widget(QWidget* w, QPainter* painter, const QRect& target,
                   SciQLopExportTarget kind)
{
    if (!w || !w->isVisible() || target.isEmpty())
        return;

    if (auto* exportable = dynamic_cast<SciQLopExportable*>(w))
    {
        exportable->export_paint(painter, target, kind);
        return;
    }

    // Generic fallback: scale the widget's software render into `target`.
    const int ww = w->width();
    const int wh = w->height();
    if (ww <= 0 || wh <= 0)
        return;

    painter->save();
    painter->translate(target.topLeft());
    painter->scale(static_cast<double>(target.width()) / ww,
                   static_cast<double>(target.height()) / wh);
    w->render(painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter->restore();
}
```

- [ ] **Step 3: Add the source to meson**

In `SciQLopPlots/meson.build`, immediately after the line `'../src/SciQLopPlotContainer.cpp',` (line 206), add:

```meson
            '../src/Export/SciQLopExportable.cpp',
```

- [ ] **Step 4: Reconfigure + compile (new source file requires reconfigure)**

```bash
$VENV/bin/meson setup --reconfigure build-venv
$VENV/bin/meson compile -C build-venv
```
Expected: clean build (the new TU compiles; nothing uses it yet — no behavior change).

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Export/SciQLopExportable.hpp src/Export/SciQLopExportable.cpp SciQLopPlots/meson.build
git commit -m "feat(export): add SciQLopExportable interface + export_widget walker

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: SciQLopPlot implements export_paint

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp` (class decl ~line 223; `qcp_plot()` at line 302)
- Modify: `src/SciQLopPlot.cpp`

- [ ] **Step 1: Add the base + method declaration to the header**

In `include/SciQLopPlots/SciQLopPlot.hpp`, add the include near the other SciQLopPlots includes at the top of the file:

```cpp
#include "SciQLopPlots/Export/SciQLopExportable.hpp"
```

Change the class declaration (the public `SciQLopPlot`, currently `class SciQLopPlot : public SciQLopPlotInterface`) to also inherit the interface:

```cpp
class SciQLopPlot : public SciQLopPlotInterface, public SciQLopExportable
```

Add the override inside the class's `public:` section (near `qcp_plot()`):

```cpp
    void export_paint(QPainter* painter, const QRect& target,
                      SciQLopExportTarget kind) override;
```

- [ ] **Step 2: Implement in the .cpp**

In `src/SciQLopPlot.cpp`, ensure these includes are present near the top (add any missing):

```cpp
#include "SciQLopPlots/Export/SciQLopExportable.hpp"
#include <qcustomplot.h>   // for QCPPainter; match the include style already used in this file
```

> If `src/SciQLopPlot.cpp` already includes the NeoQCP core header (it uses `QCustomPlot`), reuse that include; do not add a duplicate. Verify the exact spelling with `grep -n "include" src/SciQLopPlot.cpp | grep -i "qcustomplot\|core.h\|QCPPainter"`.

Add the implementation:

```cpp
void SciQLopPlot::export_paint(QPainter* painter, const QRect& target,
                               SciQLopExportTarget kind)
{
    auto* qcp = qcp_plot();
    if (!qcp || target.isEmpty())
        return;

    painter->save();
    painter->translate(target.topLeft());
    if (kind == SciQLopExportTarget::Vector)
        qcp->toPainter(static_cast<QCPPainter*>(painter), target.width(), target.height());
    else
        painter->drawPixmap(0, 0, qcp->toPixmap(target.width(), target.height()));
    painter->restore();
}
```

> Why `static_cast` is safe: the top-level export entry points (Task 5) always construct a `QCPPainter` as the painter, so for the `Vector` path the dynamic object is genuinely a `QCPPainter`.

- [ ] **Step 3: Compile**

```bash
$VENV/bin/meson compile -C build-venv
```
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp
git commit -m "feat(export): SciQLopPlot implements export_paint (vector/raster)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: SciQLopPlotContainer recursive export_paint

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp` (class decl line 30; `child_widgets()` decl line 139)
- Modify: `src/SciQLopPlotContainer.cpp` (`child_widgets()` impl at line 135)

- [ ] **Step 1: Add base + declaration to the header**

In `include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp`, add the include after the existing includes (after line 25 `#include "SciQLopPlotCollection.hpp"`):

```cpp
#include "SciQLopPlots/Export/SciQLopExportable.hpp"
```

Change the class declaration:

```cpp
class SciQLopPlotContainer : public QSplitter,
                             public SciQLopPlotCollectionInterface,
                             public SciQLopExportable
```

Add the declaration in the `public:` section (e.g. after `child_widgets()` at line 139):

```cpp
    void export_paint(QPainter* painter, const QRect& target,
                      SciQLopExportTarget kind) override;
```

- [ ] **Step 2: Implement in the .cpp**

In `src/SciQLopPlotContainer.cpp`, add the implementation after `child_widgets()` (after line 143). The walker is declared in the interface header — add `#include "SciQLopPlots/Export/SciQLopExportable.hpp"` near the top if not pulled in transitively:

```cpp
void SciQLopPlotContainer::export_paint(QPainter* painter, const QRect& target,
                                        SciQLopExportTarget kind)
{
    const int cw = width();
    const int ch = height();
    if (cw <= 0 || ch <= 0 || target.isEmpty())
        return;

    const double sx = static_cast<double>(target.width()) / cw;
    const double sy = static_cast<double>(target.height()) / ch;

    for (auto* child : child_widgets())
    {
        if (!child || !child->isVisible())
            continue;
        const QRect g = child->geometry();
        const QRect child_target(target.x() + qRound(g.x() * sx),
                                 target.y() + qRound(g.y() * sy),
                                 qRound(g.width() * sx), qRound(g.height() * sy));
        export_widget(child, painter, child_target, kind);
    }
}
```

- [ ] **Step 3: Compile**

```bash
$VENV/bin/meson compile -C build-venv
```
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp src/SciQLopPlotContainer.cpp
git commit -m "feat(export): SciQLopPlotContainer recurses children in export_paint

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Panel forwards + save_pdf/save_png rewrite

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` (class decl line 48; `_container` line 52; save_* decls lines 294-297)
- Modify: `src/SciQLopMultiPlotPanel.cpp` (`save_pdf` line 554; `_save_panel_raster` line 608)

- [ ] **Step 1: Add base + export_paint declaration to the panel header**

In `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, add the include with the other includes at the top:

```cpp
#include "SciQLopPlots/Export/SciQLopExportable.hpp"
```

Change the class declaration (currently `class SciQLopMultiPlotPanel : public SciQLopPlotPanelInterface`):

```cpp
class SciQLopMultiPlotPanel : public SciQLopPlotPanelInterface, public SciQLopExportable
```

Add the declaration in the `public:` section (near the `save_*` declarations, ~line 294):

```cpp
    void export_paint(QPainter* painter, const QRect& target,
                      SciQLopExportTarget kind) override;
```

- [ ] **Step 2: Implement forwarding + rewrite the two export functions**

In `src/SciQLopMultiPlotPanel.cpp` add the include near the top (after line 41 block):

```cpp
#include "SciQLopPlots/Export/SciQLopExportable.hpp"
```

Add the forwarding implementation (place it next to the other panel methods, e.g. after `child_widgets()` at line 162):

```cpp
void SciQLopMultiPlotPanel::export_paint(QPainter* painter, const QRect& target,
                                         SciQLopExportTarget kind)
{
    export_widget(_container, painter, target, kind);
}
```

Replace the entire body of `save_pdf` (lines 554-606) with:

```cpp
bool SciQLopMultiPlotPanel::save_pdf(const QString& filename, int width, int height)
{
    const int totalW = (width > 0) ? width : _container->width();
    const int totalH = (height > 0) ? height : _container->height();
    if (totalW <= 0 || totalH <= 0)
        return false;

    QPdfWriter writer(filename);
    writer.setPageSize(QPageSize(QSizeF(totalW, totalH), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(72);

    QCPPainter painter(&writer);
    if (!painter.isActive())
        return false;
    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNoCaching);

    export_widget(_container, &painter, QRect(0, 0, totalW, totalH),
                  SciQLopExportTarget::Vector);
    painter.end();
    return true;
}
```

Replace the entire body of `_save_panel_raster` (lines 608-627) with:

```cpp
static bool _save_panel_raster(QWidget* container, const QString& filename,
                               const char* format, int width, int height,
                               double scale, int quality)
{
    if (!container)
        return false;

    const auto fullSize = container->sizeHint().expandedTo(container->size());
    const int targetW = (width > 0) ? width : static_cast<int>(fullSize.width() * scale);
    const int targetH = (height > 0) ? height : static_cast<int>(fullSize.height() * scale);
    if (targetW <= 0 || targetH <= 0)
        return false;

    QPixmap pixmap(targetW, targetH);
    pixmap.fill(Qt::transparent);

    QCPPainter painter(&pixmap);
    if (!painter.isActive())
        return false;
    export_widget(container, &painter, QRect(0, 0, targetW, targetH),
                  SciQLopExportTarget::Raster);
    painter.end();

    if (pixmap.isNull())
        return false;
    return pixmap.save(filename, format, quality);
}
```

> `QCPPainter` is already available — `src/SciQLopMultiPlotPanel.cpp` includes the NeoQCP plot headers (`SciQLopPlots/SciQLopPlot.hpp` pulls it). If the compiler can't find `QCPPainter`, add the same NeoQCP core include that `SciQLopPlot.cpp` uses (confirmed in Task 3 Step 2).

- [ ] **Step 3: Compile**

```bash
$VENV/bin/meson compile -C build-venv
```
Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp
git commit -m "feat(export): panel save_pdf/save_png use recursive export walker

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Verify — reproducer tests pass + no regressions

**Files:** none (verification only)

- [ ] **Step 1: Run the nested-export reproducers — expect PASS**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_export_api.py -q
```
Expected: the whole `test_export_api.py` file PASSES (the previously-failing `TestNestedPanelExport` cases now pass; all pre-existing export tests still pass). Read the final `N passed` line and the exit code — do not infer from a grep.

- [ ] **Step 2: Run the broader integration suite for regressions**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```
Expected: no new failures vs. `main` baseline. If a pre-existing flaky test fails, re-run it in isolation to confirm it is unrelated.

- [ ] **Step 3: Commit (only if any test needed adjusting; otherwise skip)**

Tests already committed in Task 1; nothing to commit here unless a test needed correction. If so:
```bash
git add tests/integration/test_export_api.py
git commit -m "test: finalize nested-panel export assertions

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7 (optional / stretch): Expose interface to Python

Deliver only if a concrete need exists for a **Python** widget to produce better-than-`render()` output (e.g. a Python class doing vector drawing). The matplotlib motivating case needs **none** of this — an MPL `FigureCanvasQTAgg` is a plain `QWidget` already handled by the walker's fallback. Skipping this task leaves the bug fix complete.

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.h` (add include)
- Modify: `SciQLopPlots/bindings/bindings.xml` (expose enum + object/value type)

- [ ] **Step 1: Add the header include**

In `SciQLopPlots/bindings/bindings.h`, add alongside the other includes:

```cpp
#include <SciQLopPlots/Export/SciQLopExportable.hpp>
```

- [ ] **Step 2: Expose the enum and the interface type**

In `SciQLopPlots/bindings/bindings.xml`, add near the other `<enum-type>` entries (around line 16):

```xml
    <enum-type name="SciQLopExportTarget"/>
```

And add an object-type for the interface (near the MultiPlots type declarations):

```xml
    <object-type name="SciQLopExportable"/>
```

- [ ] **Step 3: Touch bindings.xml mtime + reconfigure (shiboken custom_target doesn't track headers)**

```bash
touch SciQLopPlots/bindings/bindings.xml
$VENV/bin/meson setup --reconfigure build-venv
$VENV/bin/meson compile -C build-venv
```
Expected: shiboken regenerates; clean build. If a stale wrapper fails to regenerate, `rm` the offending `build-venv/SciQLopPlots/bindings/.../*_wrapper.cpp` and reconfigure (per CLAUDE.md).

- [ ] **Step 4: Add a Python override test**

Append to `tests/integration/test_export_api.py`:

```python
class TestPythonExportable:
    def test_python_subclass_export_paint_is_called(self, qtbot, tmp_path):
        from SciQLopPlots import SciQLopExportable, SciQLopExportTarget  # noqa: F401
        # A Python QWidget subclass that also implements SciQLopExportable
        # should have export_paint invoked instead of the generic fallback.
        # (Concrete assertion depends on the final binding shape; assert the
        # symbols import and a panel export still succeeds with such a child.)
        assert SciQLopExportable is not None
```

> The concrete multiple-inheritance (QWidget + SciQLopExportable) test should be fleshed out against the actual generated binding; if shiboken cannot express the cross-cast, drop this task and document the interface as C++-only in the spec.

- [ ] **Step 5: Run + commit**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_export_api.py -q
```
```bash
git add SciQLopPlots/bindings/bindings.h SciQLopPlots/bindings/bindings.xml tests/integration/test_export_api.py
git commit -m "feat(export): expose SciQLopExportable to Python (opt-in)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes / gotchas

- **`QCPPainter` requirement:** both export entry points construct a `QCPPainter` (not a plain `QPainter`) so `SciQLopPlot::export_paint`'s `static_cast<QCPPainter*>` is valid in the `Vector` path and harmless in `Raster`.
- **`toPixmap` on a GPU `QRhiWidget`:** NeoQCP's `QCustomPlot::toPixmap` owns the framebuffer grab; this replaces the old `QWidget::render` path that captured nothing for GPU plots — the fix for the blank nested-only PNG.
- **Geometry mapping** preserves the on-screen layout (vertical splitter today; generalizes to any layout/nesting) by scaling each child's `geometry()` into its allocated rect.
- **Do not** change `plots()` / axis-sync / autoscale to see nested panels — out of scope; the recursive primitive lives only in the export path.
