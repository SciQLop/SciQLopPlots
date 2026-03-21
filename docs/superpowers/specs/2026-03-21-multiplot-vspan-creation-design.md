# Multiplot VSpan Creation Mode — Design Spec

Interactive drawing of `MultiPlotsVerticalSpan` items across a `SciQLopMultiPlotPanel` via click-move-click gestures, with synchronized live preview on all plots.

## Motivation

SciQLop's event catalog workflow currently requires clicking a button to add a span in the middle of the view, then manually editing it. Drawing the range directly on the plot is more natural and faster. The multiplot case is especially important: a single gesture should create a span visible on all plots in the panel.

## API

### SciQLopMultiPlotPanel

```cpp
void set_span_creation_enabled(bool enabled);
bool span_creation_enabled() const;
void set_span_creation_color(const QColor& color);  // optional, default: QColor(100, 100, 200, 80)
QColor span_creation_color() const;

// Signals
void span_created(MultiPlotsVerticalSpan* span);
void span_creation_canceled();
```

### SciQLopPlot (minor)

Forward NeoQCP creation API for single-plot use cases:

```cpp
void set_creation_mode_enabled(bool enabled);
bool creation_mode_enabled() const;
```

## Interaction Model

Uses NeoQCP's item creation mode (Shift+click quick creation or toggled batch mode).

1. `set_span_creation_enabled(true)` → panel installs `ItemCreator`, `ItemPositioner`, and enables `setCreationModeEnabled(true)` on every child `QCustomPlot`
2. User clicks on any plot → `ItemCreator` creates a `QCPItemVSpan` on that plot (NeoQCP manages it) AND creates preview `QCPItemVSpan` on every other plot with the same color
3. Mouse moves → `ItemPositioner` updates the range on ALL preview spans simultaneously
4. Second click (commit) → NeoQCP emits `itemCreated` → panel reads range from raw VSpan, removes it + all previews, creates a `MultiPlotsVerticalSpan`, emits `span_created`
5. Escape/right-click (cancel) → NeoQCP emits `itemCanceled` → panel removes all previews, emits `span_creation_canceled`

After commit in batch mode: stays in creation mode, ready for next span.

## Internal State

```cpp
struct SpanCreationState {
    QCustomPlot* active_plot = nullptr;
    QList<QPointer<QCPItemVSpan>> preview_spans;  // one per non-active plot
};
```

Held as a member of `SciQLopMultiPlotPanel`. Reset on commit/cancel.

## Implementation

### Enabling creation mode

When `set_span_creation_enabled(true)`:

1. Store `m_span_creation_enabled = true` and `m_span_creation_color`
2. For each child `SciQLopPlot`, call `installSpanCreator(plot)`:
   - Get `QCustomPlot* qcp = plot->qcp_plot()`
   - Set `qcp->setItemCreator(creator_lambda)` — the lambda creates a `QCPItemVSpan` and preview spans
   - Set `qcp->setItemPositioner(positioner_lambda)` — the lambda syncs all preview spans
   - Connect `qcp->itemCreated` → `onItemCreated(qcp, item)`
   - Connect `qcp->itemCanceled` → `onItemCanceled(qcp)`
   - Call `qcp->setCreationModeEnabled(true)`
3. Connect `plot_added` → install creator on new plots
4. Connect `plot_removed` → uninstall creator, clean up any in-progress preview

When `set_span_creation_enabled(false)`:

1. For each child plot: `setCreationModeEnabled(false)`, `setItemCreator(nullptr)`, `setItemPositioner(nullptr)`, disconnect signals
2. Cancel any in-progress creation
3. `m_span_creation_enabled = false`

### ItemCreator lambda

```cpp
auto creator = [this, qcp](QCustomPlot* plot, QCPAxis* keyAxis, QCPAxis* valueAxis) -> QCPAbstractItem* {
    auto* vspan = new QCPItemVSpan(plot);
    vspan->setPen(QPen(m_span_creation_color.darker(120)));
    vspan->setBrush(QBrush(m_span_creation_color));

    // Create preview spans on all other plots
    m_creation_state.active_plot = qcp;
    m_creation_state.preview_spans.clear();
    for (auto& p : plots()) {
        auto* other_qcp = static_cast<SciQLopPlot*>(p.data())->qcp_plot();
        if (other_qcp == qcp) continue;
        auto* preview = new QCPItemVSpan(other_qcp);
        preview->setPen(QPen(m_span_creation_color.darker(120)));
        preview->setBrush(QBrush(m_span_creation_color));
        m_creation_state.preview_spans.append(preview);
        other_qcp->replot(QCustomPlot::rpQueuedReplot);
    }

    return vspan;
};
```

### ItemPositioner lambda

```cpp
auto positioner = [this](QCPAbstractItem* item, double anchorKey, double, double currentKey, double) {
    // Position the main item (VSpan uses key axis only)
    double lower = std::min(anchorKey, currentKey);
    double upper = std::max(anchorKey, currentKey);
    if (auto* vspan = qobject_cast<QCPItemVSpan*>(item)) {
        vspan->setRange(lower, upper);
    }
    // Sync all preview spans
    for (auto& preview : m_creation_state.preview_spans) {
        if (preview) {
            preview->setRange(lower, upper);
            preview->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
    }
};
```

### onItemCreated

```cpp
void SciQLopMultiPlotPanel::onItemCreated(QCustomPlot* qcp, QCPAbstractItem* item) {
    auto* vspan = qobject_cast<QCPItemVSpan*>(item);
    if (!vspan) return;

    SciQLopPlotRange range(vspan->range().lower, vspan->range().upper);

    // Remove the raw NeoQCP item
    qcp->removeItem(vspan);

    // Remove all preview spans
    for (auto& preview : m_creation_state.preview_spans) {
        if (preview)
            preview->parentPlot()->removeItem(preview);
    }
    m_creation_state = {};

    // Create the proper MultiPlotsVerticalSpan
    auto* mpvspan = new MultiPlotsVerticalSpan(
        this, range, m_span_creation_color, false, true, "");
    emit span_created(mpvspan);
}
```

### onItemCanceled

```cpp
void SciQLopMultiPlotPanel::onItemCanceled(QCustomPlot*) {
    for (auto& preview : m_creation_state.preview_spans) {
        if (preview)
            preview->parentPlot()->removeItem(preview);
    }
    m_creation_state = {};
    emit span_creation_canceled();
}
```

### Dynamic plot management

- `plot_added`: if creation mode is active, call `installSpanCreator(newPlot)`. If a creation is in-progress, add a preview span to the new plot.
- `plot_removed`: if the removed plot has an in-progress preview, remove it from `m_creation_state.preview_spans`. Disconnect signals.

## Files to Modify

| File | Change |
|---|---|
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` | Add `SpanCreationState` struct, creation mode members, new methods and signals |
| `src/SciQLopMultiPlotPanel.cpp` | Implement `set_span_creation_enabled`, `installSpanCreator`, `onItemCreated`, `onItemCanceled` |
| `include/SciQLopPlots/SciQLopPlot.hpp` | Add `set_creation_mode_enabled`/`creation_mode_enabled` forwarding |
| `src/SciQLopPlot.cpp` | Implement forwarding methods |
| `SciQLopPlots/bindings/bindings.xml` | Expose new methods/signals to Python |
| `tests/manual-tests/gallery.py` | Add creation mode toggle button to "Stacked Plots" tab |

## Gallery Demo

Modify the existing "Stacked Plots" tab to add a toggle button:

```python
def create_stacked_tab():
    container = QWidget()
    layout = QVBoxLayout(container)
    btn = QPushButton("Toggle Span Creation Mode")
    btn.setCheckable(True)
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    # ... existing plot setup ...
    btn.toggled.connect(panel.set_span_creation_enabled)
    panel.span_created.connect(lambda s: print(f"Span created: {s.range()}"))
    layout.addWidget(btn)
    layout.addWidget(panel, stretch=1)
    return container
```

## Scope

**In scope:** Panel-level creation mode for VSpans, synchronized preview during gesture, batch mode, integration with existing `MultiPlotsVerticalSpan` and `MultiPlotsVSpanCollection`.

**Out of scope:** HSpan/RSpan creation, single-plot creation mode (trivial forwarding only), catalog integration (SciQLop concern), undo/redo, custom creator callbacks on the panel.
