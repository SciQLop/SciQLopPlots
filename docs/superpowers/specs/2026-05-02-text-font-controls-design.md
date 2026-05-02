# Inspector Font Controls — Design

**Date**: 2026-05-02
**Tracking issue**: SciQLop/SciQLopPlots#53
**Follows**: PR #52 (inspector properties expansion)

## Goal

Expose the standard text-styling properties (font family, point size, bold, italic, color) for every text-bearing UI element that already has inspector controls. The current inspector lets users edit axis labels and toggle visibility of legends/contour labels, but the visual styling of those text elements is not editable from the GUI.

## Scope

### In scope (text elements with QCustomPlot font support)

| Element | Object | Underlying Qt API |
|---|---|---|
| Axis label | `SciQLopPlotAxis` | `QCPAxis::labelFont`, `QCPAxis::labelColor` |
| Axis tick labels | `SciQLopPlotAxis` | `QCPAxis::tickLabelFont`, `QCPAxis::tickLabelColor` |
| Plot legend entries | `SciQLopPlotLegend` | `QCPLegend::font` |
| Text overlay items | `SciQLopTextItem` | `QCPItemText::setFont` |

### Out of scope

- **ColorMap contour-label font** — `QCPColorMap2` has `setContourLabelEnabled(bool)` but no font getter/setter. Would require an upstream NeoQCP change. Defer.
- **Selected-state fonts** (`QCPLegend::selectedFont`) — secondary visual state, not visible until interaction. Defer.
- **Axis title** — no separate concept in QCustomPlot; the axis label IS the title. Already covered by axis-label-font.
- **Plot title** — still no plot-level title API exists. Out of scope (same as PR #52).
- **Inspector Tree row text** (the property panel itself) — Qt theme territory, not per-plot.

## Reusable widget: `FontDelegate`

A new widget under `include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp`. UX-wise it mirrors `ColorDelegate`: a single compact button shows a preview ("Aa" rendered in the current font and color), and clicking it opens a popup with the editing controls. This keeps the inspector panel compact and avoids constructing heavy widgets (notably `QFontComboBox`) until the user actually opens the picker.

```
inspector row:   Label font:  [ Aa ▾ ]

popup contents:
  [ Family ▾ ] [ Size ▴▾ ]
  [ B ] [ I ]
  Color: [ ■ ]
```

| Control (in popup) | Widget | Maps to |
|---|---|---|
| Family | `QFontComboBox` | `QFont::setFamily` |
| Point size | `QSpinBox` (range 4–72) | `QFont::setPointSize` |
| Bold | `QToolButton` (checkable) | `QFont::setBold` |
| Italic | `QToolButton` (checkable) | `QFont::setItalic` |
| Color | `ColorDelegate` | separate signal — color is per-element, not on QFont |

The popup is created lazily on first click and reused thereafter. The preview button repaints whenever the font or color changes so the inspector row always reflects current state at a glance.

**Signals:**
- `fontChanged(const QFont& font)` — emitted on family/size/bold/italic edit
- `colorChanged(const QColor& color)` — emitted on color edit (color is paired with font but stored separately on the underlying object)

**Slots (reverse path):**
- `setFont(const QFont& font)` — updates the popup widgets and the preview button without re-emitting (signal-blocked)
- `setColor(const QColor& color)` — updates the popup's ColorDelegate and the preview swatch

The widget is constructed with `(QFont initial_font, QColor initial_color, QWidget* parent)`.

### Why a popup-style picker instead of inline controls

Two reasons:
1. **Panel real estate.** Embedding `QFontComboBox` + spinbox + two toggles + color swatch inline for every axis (label + tick labels) and the legend would dominate the inspector vertically. A single preview button per font slot keeps the panel scannable.
2. **Construction cost.** `QFontComboBox` scans installed fonts. By deferring popup construction until the user clicks, we never pay that cost for axes the user never restyles.

### Why one widget that emits two signals (font + color)

The user edits "label appearance" as one logical unit, but the underlying types store font and color separately (`QCPAxis::labelFont` / `QCPAxis::labelColor`). Combining them into a single `(QFont, QColor)` struct would force both setters to fire on every edit. Two separate signals let the delegate wire each to its own setter, so editing the bold toggle only fires `set_label_font`, not `set_label_color`.

## C++ wrapper additions

### `SciQLopPlotAxis` (in `SciQLopPlotAxisInterface` for Python visibility)

| Method | Maps to |
|---|---|
| `set_label_font(QFont)` / `label_font() const` | `QCPAxis::setLabelFont` |
| `set_label_color(QColor)` / `label_color() const` | `QCPAxis::setLabelColor` |
| `set_tick_label_font(QFont)` / `tick_label_font() const` | `QCPAxis::setTickLabelFont` |
| `set_tick_label_color(QColor)` / `tick_label_color() const` | `QCPAxis::setTickLabelColor` |

Plus four `*_changed` signals (`label_font_changed`, `label_color_changed`, `tick_label_font_changed`, `tick_label_color_changed`).

### `SciQLopPlotLegend`

| Method | Maps to |
|---|---|
| `set_font(QFont)` / `font() const` | `QCPLegend::setFont` |
| `set_color(QColor)` / `color() const` | `QCPLegend::setTextColor` |

Plus `font_changed`, `color_changed` signals.

### `SciQLopTextItem`

The existing API has `set_font_size(double)`, `set_color(QColor)`, `set_font_color(QColor)`. Add:

| Method | Maps to |
|---|---|
| `set_font(QFont)` / `font() const` | `QCPItemText::setFont` (overall — replaces piecemeal size setter) |
| `set_font_family(const QString&)` | `QFont::setFamily` (convenience for Python users) |

The existing `set_font_size` / `font_size` stay for backwards compatibility.

## Inspector delegate updates

### `SciQLopPlotAxisDelegate`

Add a `Label` `QGroupBox` (with the existing `QLineEdit` for label text moving inside it, plus a `FontDelegate` for the label font/color). Add a `Tick labels` `QGroupBox` (containing the existing visibility checkbox plus a separate `FontDelegate` for the tick-label font/color).

Label font and tick-label font are deliberately **two separate `FontDelegate` widgets** — they map to independent QCPAxis getters/setters and users routinely want a larger / bolder axis label paired with smaller tick labels.

The Range group from PR #52 stays as-is.

### `SciQLopPlotDelegate`

Add a `Legend` `QGroupBox` containing the existing visibility toggle plus a `FontDelegate` for legend font + color.

### `SciQLopTextItemDelegate` (NEW)

A new `PropertyDelegateBase` subclass with: text content (`QLineEdit`) + `FontDelegate`. Registered in `Registrations.cpp` for `SciQLopTextItem`.

## Build & registration

| File | Change |
|---|---|
| `meson.build` | Add `FontDelegate.hpp` to `moc_headers`, `FontDelegate.cpp` + `SciQLopTextItemDelegate.cpp` to sources |
| `Registrations.cpp` | Add `register_type<SciQLopTextItemDelegate>()` |
| `bindings.xml` | Verify Shiboken auto-introspects the new methods (per PR #52 experience, this should work; touch `bindings.xml` to regenerate) |

## Testing

Per project convention, integration tests in `tests/manual-tests/test_inspector_properties.py`:

- `TestAxisFontControls` — for each axis-label and tick-label font/color setter:
  - initial state from model
  - widget edit → model
  - model change → widget
  - idempotent set does not emit
- `TestLegendFontControls` — same shape for `SciQLopPlotLegend::set_font` / `set_color`
- `TestTextItemDelegate` — initial dispatch + font edit propagation
- `TestFontDelegateUnit` — standalone widget test: changing the family combo emits `fontChanged` with the right family; bold toggle flips the `font().bold()` bit; color picker emits `colorChanged`; reverse path (`setFont`/`setColor`) updates widgets without re-emitting

Estimated ~25–30 new test methods.

## Risks

- **Color and font emission are paired in the user's mental model but separate on the wire**. Tests must verify that editing only the font does NOT fire `*_color_changed` (and vice versa) — otherwise users connecting to `*_color_changed` would see spurious notifications.
- **Bindings drift**: Adding signals to `SciQLopPlotAxisInterface` (so all axis types inherit them) requires the same `touch bindings.xml + meson compile` cycle as PR #52. Note in implementation plan.
- **Popup lifetime**: the lazily-constructed popup must be parented correctly so it's destroyed with the `FontDelegate`, and must be hidden (not destroyed) on close so reopening it preserves any in-progress edits. Mirror what `ColorDelegate`'s color-dialog handling does — read its source before implementing.

## Out of scope (future work)

- Contour-label font (needs upstream NeoQCP change to expose font API on `QCPColorMap2`)
- Per-tick custom fonts (only one font for all tick labels)
- Font fallback / shaping options
- Selected-state fonts (`selectedFont`, `selectedTickLabelFont` etc.)
