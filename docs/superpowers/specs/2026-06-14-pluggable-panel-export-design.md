# Pluggable Panel Export (PDF / PNG) — Design

**Date:** 2026-06-14
**Status:** Approved (pending implementation)
**Branch target:** new branch off `main`

## Problem

Exporting a `SciQLopMultiPlotPanel` to PDF/PNG silently drops nested sub-panels.

Root cause is one defect with two faces. A nested panel is
`SciQLopMultiPlotPanel : SciQLopPlotPanelInterface : QScrollArea`; a normal plot
is `SciQLopPlotInterface : QFrame` — unrelated branches.
`SciQLopPlotContainer::plots()` (`include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp:125`)
returns only `qobject_cast<SciQLopPlotInterface*>` children, so nested panels are
excluded from `plots()`, `replot()`, and the export loops.

Symptoms (all reproduced 2026-06-14 against the installed binary):

| Scenario | `save_pdf` | `save_png` |
|---|---|---|
| sub-panel only | returns **False** (`plot_list.isEmpty()` early-out, `src/SciQLopMultiPlotPanel.cpp:557`) | **blank** image |
| top-level plot **+** sub-panel | succeeds but sub-panel **missing** | both regions render (incidental) |
| plain panel (control) | works | works |

- **PDF** `save_pdf` (`src/SciQLopMultiPlotPanel.cpp:554`) iterates `_container->plots()` then `qobject_cast<SciQLopPlot*>` → nested panels never painted.
- **PNG** `_save_panel_raster` (`src/SciQLopMultiPlotPanel.cpp:608`) uses `container->render(DrawChildren)`. A nested plot is a GPU `QRhiWidget`; with no direct `SciQLopPlotInterface` child the nested panel's `replot()` is never driven, so the RHI surface produces no frame. A sibling top-level plot incidentally cascades a replot — which is why the mixed case appears to work.

## Goal

Replace the type-specific export loops with a **capability-based recursive
walker** that renders the whole panel tree, is extensible from Python (e.g. a
matplotlib Qt-backend canvas dropped into a panel), and keeps PDF vector where
it's cheap.

## Design

### Capability interface

A small abstract mixin, discovered by RTTI cross-cast (no `Q_OBJECT`):

```cpp
enum class SciQLopExportTarget { Vector, Raster };

class SciQLopExportable
{
public:
    virtual ~SciQLopExportable() = default;
    // Paint self into `target` (page coords) on `painter`.
    virtual void export_paint(QPainter* painter, const QRect& target,
                              SciQLopExportTarget kind) = 0;
};
```

The signature takes **`QPainter*`, not `QCPPainter*`**, on purpose. The top-level
always *constructs* a `QCPPainter` (so plots get vector output), but the
interface exposes it as `QPainter*` because `QCPPainter` is not exposed to
Python while `QPainter` is. C++ implementations `static_cast` back internally; a
Python subclass sees a PySide6 `QPainter` and can `drawImage`/`drawText`/etc.

### Recursive walker

A single free function is the entire dispatch:

```cpp
void export_widget(QWidget* w, QPainter* p, const QRect& target,
                   SciQLopExportTarget kind)
{
    if (!w || !w->isVisible()) return;
    if (auto* e = dynamic_cast<SciQLopExportable*>(w))
        e->export_paint(p, target, kind);
    else
        w->render(p, target.topLeft(), QRegion(), QWidget::DrawChildren);
}
```

`QWidget::render()` is the **universal baseline**: any plain `QWidget` (MPL
canvas, `QLabel`, custom Python widget) exports for free. The interface is the
opt-in for widgets that can do *better* than a generic software render.

### Geometry mapping

Layout is preserved by mapping each child's on-screen geometry proportionally
into its parent's allocated rect, replacing the hand-rolled
`plotH = plot.height/containerH*totalH` loop. A container's `export_paint`:

```
scale = target.size / container.size
for each visible child c (layout order):
    childTarget = map(c->geometry()) scaled by `scale`, offset by target.topLeft()
    export_widget(c, painter, childTarget, kind)
```

This generalizes to vertical stacks, side-by-side, and arbitrary nesting.

### Who implements `SciQLopExportable`

- **`SciQLopPlot`** — `Vector` → `static_cast<QCPPainter*>(painter)` +
  `qcp_plot()->toPainter()` (true vector PDF). `Raster` → `replot()` then
  `grabFramebuffer()` and `drawImage` into `target`. The explicit grab fixes the
  blank nested-only PNG (no longer depends on a sibling cascading a replot).
- **`SciQLopPlotContainer`** — `export_paint` = the recurse-children step. Making
  the container exportable keeps the top-level call uniform and the
  panel/container distinction internal.
- **`SciQLopMultiPlotPanel`** — exportable via its container; recursion fixes the
  dropped nested panel in PDF and the `save_pdf`-returns-false sub-panel-only case.
- **MPL / any plain `QWidget`** — nothing; the walker's `else` branch rasterizes
  them (into PDF too — accepted; an Agg buffer cannot be vectorized).

### Export functions collapse

```cpp
bool SciQLopMultiPlotPanel::save_pdf(const QString& f, int w, int h)
{
    const int W = w>0 ? w : _container->width();
    const int H = h>0 ? h : _container->height();
    if (W<=0 || H<=0) return false;

    QPdfWriter writer(f);
    writer.setPageSize(QPageSize(QSizeF(W,H), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0,0,0,0));
    writer.setResolution(72);

    QCPPainter painter(&writer);
    if (!painter.isActive()) return false;
    painter.setMode(QCPPainter::pmVectorized);
    painter.setMode(QCPPainter::pmNoCaching);

    export_widget(_container, &painter, QRect(0,0,W,H), SciQLopExportTarget::Vector);
    painter.end();
    return true;
}
```

`_save_panel_raster` mirrors this: build the `QPixmap`, open a `QCPPainter` on it,
`export_widget(_container, &painter, rect, Raster)`, then save. The
`plot_list.isEmpty()` early-return-false is removed.

### Python extension surface

Expose `SciQLopExportable` + `SciQLopExportTarget` in `bindings.xml` as a normal
polymorphic base so a Python widget can subclass and override
`export_paint(painter, target, kind)`. The common case (MPL) needs none of this —
it's a `QWidget`, handled by the walker's `else` branch. Per the shiboken regen
quirk, touch `bindings.xml` after editing the header so the custom_target
regenerates.

## Testing (reproducer-first)

Tests land first and must fail against current `main`. Integration tests under
`tests/integration`, run from `/tmp` per the build recipe; widgets shown +
`qtbot.wait` to settle the RHI surface. A `_nonblank_fraction` pixel sampler
over the saved PNG quantifies blank regions.

| Case | PDF assert | PNG assert |
|---|---|---|
| sub-panel only | `save_pdf` → **True** (currently False) | non-blank (currently blank) |
| top-level plot **+** sub-panel | nested block present (currently dropped) | both regions non-blank |
| plain panel (control) | still works | still works |
| panel + non-plot `QWidget` (`QLabel` ≈ MPL) | label region rendered | label region rendered |

## Files touched

- **New** `include/SciQLopPlots/Export/SciQLopExportable.hpp` — interface + enum + `export_widget` walker declaration; walker body in `src/Export/SciQLopExportable.cpp` (or inline if header-only stays clean).
- `include/SciQLopPlots/MultiPlots/SciQLopPlotContainer.hpp` — implement `export_paint` (recurse children + geometry mapping).
- `include/SciQLopPlots/SciQLopPlot.hpp` + `src/SciQLopPlot.cpp` — implement `export_paint` (vector toPainter / raster grabFramebuffer).
- `src/SciQLopMultiPlotPanel.cpp` — rewrite `save_pdf` / `_save_panel_raster` to call the walker.
- `SciQLopPlots/bindings/bindings.xml` (+ `bindings.h`) — expose the interface + enum.
- `tests/integration/` — the test matrix above.

## Non-goals

- Vectorizing raster widgets (MPL Agg) in PDF.
- Changing `plots()`/axis-sync/autoscale to see nested panels — export is the
  only consumer addressed here; the recursive primitive lives in the export path.
