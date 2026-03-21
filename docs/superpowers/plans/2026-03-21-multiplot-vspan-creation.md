# Multiplot VSpan Creation Mode — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable interactive drawing of `MultiPlotsVerticalSpan` items across a `SciQLopMultiPlotPanel` via click-move-click gestures, with synchronized live preview on all plots.

**Architecture:** The panel installs NeoQCP's `ItemCreator`/`ItemPositioner` callbacks on each child `QCustomPlot`. During a creation gesture, preview `QCPItemVSpan`s are created on all non-active plots and kept in sync via the positioner. On commit, raw items are removed and a proper `MultiPlotsVerticalSpan` is created. On cancel, previews are cleaned up.

**Tech Stack:** C++20, Qt6, NeoQCP (QCPItemVSpan, ItemCreator, ItemPositioner, creation mode API), Shiboken6 bindings

**Spec:** `docs/superpowers/specs/2026-03-21-multiplot-vspan-creation-design.md`

---

## File Structure

| File | Role |
|---|---|
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` | Add `SpanCreationState` struct, creation mode members, methods, signals |
| `src/SciQLopMultiPlotPanel.cpp` | Implement creation mode logic |
| `SciQLopPlots/bindings/bindings.xml` | Expose new methods/signals to Python |
| `tests/manual-tests/gallery.py` | Add creation mode toggle to Stacked Plots tab |

---

### Task 1: Add creation mode state and API to SciQLopMultiPlotPanel header

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`

- [ ] **Step 1: Add includes and forward declarations**

At top of file, add:
```cpp
#include <qcustomplot.h>  // for QCPItemVSpan, QCPRange
```

- [ ] **Step 2: Add SpanCreationState struct and members**

Add inside `SciQLopMultiPlotPanel` class, in the private section (before `protected:`):

```cpp
private:
    struct SpanCreationState {
        QCustomPlot* active_plot = nullptr;
        QList<QPointer<QCPItemVSpan>> preview_spans;
        void clear() {
            active_plot = nullptr;
            preview_spans.clear();
        }
    };

    bool m_span_creation_enabled = false;
    QColor m_span_creation_color = QColor(100, 100, 200, 80);
    SpanCreationState m_creation_state;
    QList<QMetaObject::Connection> m_creation_connections;

    void _install_span_creator(SciQLopPlot* plot);
    void _uninstall_span_creator(SciQLopPlot* plot);
    void _on_item_created(QCustomPlot* qcp, QCPAbstractItem* item);
    void _on_item_canceled(QCustomPlot* qcp);
    void _clear_preview_spans();
```

- [ ] **Step 3: Add public API methods**

Add in the `public:` section, after the existing `save_*` methods:

```cpp
    void set_span_creation_enabled(bool enabled);
    bool span_creation_enabled() const { return m_span_creation_enabled; }
    void set_span_creation_color(const QColor& color) { m_span_creation_color = color; }
    QColor span_creation_color() const { return m_span_creation_color; }
```

- [ ] **Step 4: Add signals**

Add in the signals section (before `Q_SIGNAL void panel_added`):

```cpp
    Q_SIGNAL void span_created(MultiPlotsVerticalSpan* span);
    Q_SIGNAL void span_creation_canceled();
```

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp
git commit -m "feat: add span creation mode API to SciQLopMultiPlotPanel header"
```

---

### Task 2: Implement creation mode logic in SciQLopMultiPlotPanel.cpp

**Files:**
- Modify: `src/SciQLopMultiPlotPanel.cpp`

- [ ] **Step 1: Add include for MultiPlotsVSpan**

Add at top with other includes:
```cpp
#include "SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp"
```

- [ ] **Step 2: Implement _install_span_creator**

```cpp
void SciQLopMultiPlotPanel::_install_span_creator(SciQLopPlot* plot)
{
    auto* qcp = plot->qcp_plot();

    qcp->setItemCreator([this, qcp](QCustomPlot*, QCPAxis*, QCPAxis*) -> QCPAbstractItem* {
        auto* vspan = new QCPItemVSpan(qcp);
        vspan->setPen(QPen(m_span_creation_color.darker(120)));
        vspan->setBrush(QBrush(m_span_creation_color));

        m_creation_state.active_plot = qcp;
        m_creation_state.preview_spans.clear();
        for (auto& p : plots()) {
            auto* sp = dynamic_cast<SciQLopPlot*>(p.data());
            if (!sp) continue;
            auto* other_qcp = sp->qcp_plot();
            if (other_qcp == qcp) continue;
            auto* preview = new QCPItemVSpan(other_qcp);
            preview->setPen(QPen(m_span_creation_color.darker(120)));
            preview->setBrush(QBrush(m_span_creation_color));
            m_creation_state.preview_spans.append(preview);
            other_qcp->replot(QCustomPlot::rpQueuedReplot);
        }
        return vspan;
    });

    qcp->setItemPositioner([this](QCPAbstractItem* item, double anchorKey, double,
                                   double currentKey, double) {
        double lower = std::min(anchorKey, currentKey);
        double upper = std::max(anchorKey, currentKey);
        if (auto* vspan = qobject_cast<QCPItemVSpan*>(item))
            vspan->setRange(QCPRange(lower, upper));
        for (auto& preview : m_creation_state.preview_spans) {
            if (preview) {
                preview->setRange(QCPRange(lower, upper));
                preview->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
            }
        }
    });

    m_creation_connections.append(
        connect(qcp, &QCustomPlot::itemCreated, this,
                [this, qcp](QCPAbstractItem* item) { _on_item_created(qcp, item); }));
    m_creation_connections.append(
        connect(qcp, &QCustomPlot::itemCanceled, this,
                [this, qcp]() { _on_item_canceled(qcp); }));

    qcp->setCreationModeEnabled(true);
}
```

- [ ] **Step 3: Implement _uninstall_span_creator**

```cpp
void SciQLopMultiPlotPanel::_uninstall_span_creator(SciQLopPlot* plot)
{
    auto* qcp = plot->qcp_plot();
    qcp->setCreationModeEnabled(false);
    qcp->setItemCreator(nullptr);
    qcp->setItemPositioner(nullptr);
}
```

- [ ] **Step 4: Implement _clear_preview_spans**

```cpp
void SciQLopMultiPlotPanel::_clear_preview_spans()
{
    for (auto& preview : m_creation_state.preview_spans) {
        if (preview)
            preview->parentPlot()->removeItem(preview);
    }
    m_creation_state.clear();
}
```

- [ ] **Step 5: Implement _on_item_created**

```cpp
void SciQLopMultiPlotPanel::_on_item_created(QCustomPlot* qcp, QCPAbstractItem* item)
{
    auto* vspan = qobject_cast<QCPItemVSpan*>(item);
    if (!vspan) return;

    auto qcp_range = vspan->range();
    SciQLopPlotRange range(qcp_range.lower, qcp_range.upper);

    qcp->removeItem(vspan);
    _clear_preview_spans();

    auto* mpvspan = new MultiPlotsVerticalSpan(
        this, range, m_span_creation_color, false, true, "");
    Q_EMIT span_created(mpvspan);
}
```

- [ ] **Step 6: Implement _on_item_canceled**

```cpp
void SciQLopMultiPlotPanel::_on_item_canceled(QCustomPlot*)
{
    _clear_preview_spans();
    Q_EMIT span_creation_canceled();
}
```

- [ ] **Step 7: Implement set_span_creation_enabled**

```cpp
void SciQLopMultiPlotPanel::set_span_creation_enabled(bool enabled)
{
    if (m_span_creation_enabled == enabled)
        return;
    m_span_creation_enabled = enabled;

    if (enabled) {
        for (auto& p : plots()) {
            if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
                _install_span_creator(sp);
        }
        connect(this, &SciQLopMultiPlotPanel::plot_added, this,
                [this](SciQLopPlotInterface* plot) {
                    if (m_span_creation_enabled) {
                        if (auto* sp = dynamic_cast<SciQLopPlot*>(plot))
                            _install_span_creator(sp);
                    }
                });
    } else {
        _clear_preview_spans();
        for (auto& conn : m_creation_connections)
            disconnect(conn);
        m_creation_connections.clear();
        for (auto& p : plots()) {
            if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
                _uninstall_span_creator(sp);
        }
    }
}
```

- [ ] **Step 8: Commit**

```bash
git add src/SciQLopMultiPlotPanel.cpp
git commit -m "feat: implement multiplot VSpan creation mode"
```

---

### Task 3: Expose creation mode API in Python bindings

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Add methods/signals to SciQLopMultiPlotPanel type**

Inside the `<object-type name="SciQLopMultiPlotPanel">` block (after the `<property name="name">` line at line 632), add nothing — the methods should be auto-detected by Shiboken since they're public. But we need to verify the `span_created` signal's `MultiPlotsVerticalSpan*` argument is handled correctly since that type is already declared.

Check that `MultiPlotsVerticalSpan` is declared before `SciQLopMultiPlotPanel` in the bindings file, or add a forward declaration if needed.

- [ ] **Step 2: Verify no bindings.xml changes needed**

Shiboken should auto-expose public methods and signals. If the build fails due to missing type info, add:
```xml
<!-- Inside SciQLopMultiPlotPanel object-type -->
<modify-function signature="span_created(MultiPlotsVerticalSpan*)"/>
```

- [ ] **Step 3: Commit (if changes needed)**

```bash
git add SciQLopPlots/bindings/bindings.xml
git commit -m "feat: expose span creation mode in Python bindings"
```

---

### Task 4: Add creation mode demo to gallery

**Files:**
- Modify: `tests/manual-tests/gallery.py`

- [ ] **Step 1: Modify create_stacked_tab to add creation mode toggle**

Replace the existing `create_stacked_tab` function:

```python
def create_stacked_tab():
    container = QWidget()
    layout = QVBoxLayout(container)
    layout.setContentsMargins(0, 0, 0, 0)

    btn = QPushButton("Toggle Span Creation Mode (Shift+Click to draw)")
    btn.setCheckable(True)

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    for _ in range(4):
        panel.plot(
            multicomponent_signal,
            labels=["X", "Y", "Z"],
            colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
            plot_type=PlotType.TimeSeries,
        )
    x_range = panel.plot_at(0).x_axis().range()
    MultiPlotsVerticalSpan(
        panel, x_range / 10,
        QColor(100, 100, 200, 80),
        read_only=False, visible=True,
        tool_tip="Drag across all plots",
    )

    btn.toggled.connect(panel.set_span_creation_enabled)
    panel.span_created.connect(
        lambda s: print(f"Span created: [{s.range().start():.1f}, {s.range().stop():.1f}]"))

    layout.addWidget(btn)
    layout.addWidget(panel, stretch=1)
    return container
```

- [ ] **Step 2: Update the tab registration**

Change the `tabs.addTab` line for "Stacked Plots" from:
```python
tabs.addTab(with_export_button(create_stacked_tab()), "Stacked Plots")
```
to:
```python
tabs.addTab(create_stacked_tab(), "Stacked Plots")
```

Since `create_stacked_tab` now returns a container widget (not a panel directly), `with_export_button` would nest incorrectly.

- [ ] **Step 3: Commit**

```bash
git add tests/manual-tests/gallery.py
git commit -m "feat: add span creation mode demo to gallery Stacked Plots tab"
```

---

### Task 5: Build and manual test

- [ ] **Step 1: Build the project**

```bash
meson compile -C build
```

- [ ] **Step 2: Run gallery and test creation mode**

```bash
python tests/manual-tests/gallery.py
```

1. Go to "Stacked Plots" tab
2. Click the toggle button to enable creation mode
3. Shift+click on any plot, drag, click again — verify synchronized preview on all 4 plots
4. Verify `MultiPlotsVerticalSpan` is created after commit
5. Press Escape during creation — verify cleanup
6. Verify existing drag-to-move span still works when creation mode is off

- [ ] **Step 3: Fix any issues found during testing**

- [ ] **Step 4: Final commit if fixes were needed**
