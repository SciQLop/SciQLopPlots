# SciQLopPlots Python API Review — 2026-03-07

## Overview

Comprehensive review of the Python API surface, covering inconsistencies, missing functionality, broken features, and a proposed roadmap for the next release. Based on analysis of all C++ headers, Python modules, bindings XML, and all test files.

---

## Project Structure Summary

### Python API Surface

- `SciQLopPlots/__init__.py` — Main entry point. Imports bindings, monkey-patches `plot()` onto 4 classes, registers 3 observable properties, sets up `.on` descriptor.
- `SciQLopPlots/pipeline.py` — Reactive pipeline: `source.on.prop >> transform >> target.on.prop`
- `SciQLopPlots/event.py` — Event object passed to pipeline transforms
- `SciQLopPlots/properties.py` — Observable property registry, `OnNamespace`, `OnDescriptor`
- `SciQLopPlots/bindings/bindings.xml` — Shiboken typesystem (defines what C++ gets exposed to Python)

### Key C++ Class Hierarchy (Python-facing)

```
SciQLopPlotInterface (QFrame)
  +-- SciQLopPlot
  |     +-- SciQLopTimeSeriesPlot
  +-- SciQLopNDProjectionPlot

SciQLopPlottableInterface (QObject)
  +-- SciQLopGraphInterface
  |     +-- SciQLopLineGraph / SciQLopLineGraphFunction
  |     +-- SciQLopCurve / SciQLopCurveFunction
  |     +-- SciQLopNDProjectionCurves / SciQLopNDProjectionCurvesFunction
  +-- SciQLopColorMapInterface
        +-- SciQLopColorMap / SciQLopColorMapFunction

SciQLopPlotAxisInterface (QObject)
  +-- SciQLopPlotDummyAxis
  +-- SciQLopPlotAxis
        +-- SciQLopPlotColorScaleAxis

SciQLopPlotPanelInterface (QScrollArea + SciQLopPlotCollectionInterface)
  +-- SciQLopMultiPlotPanel

SciQLopItemInterface (QObject)
  +-- SciQLopMovableItemInterface
  |     +-- SciQLopBoundingRectItemInterface
  |     |     +-- SciQLopTextItem
  |     |     +-- SciQLopPixmapItem
  |     +-- SciQLopPolygonItemInterface
  |     |     +-- SciQLopEllipseItem
  |     +-- SciQLopRangeItemInterface
  |           +-- SciQLopVerticalSpan
  +-- SciQLopLineItemInterface
        +-- SciQLopStraightLine
              +-- SciQLopVerticalLine
              +-- SciQLopHorizontalLine
        +-- SciQLopCurvedLineItem

MultiPlotsVerticalSpan (SciQLopMultiPlotObject)
MultiPlotsVSpanCollection (SciQLopMultiPlotObject)

SciQLopGraphComponentInterface (QObject)
  +-- SciQLopGraphComponent
```

### Enums Exposed to Python

- `GraphType`: Line, ParametricCurve, Scatter, ColorMap
- `PlotType`: BasicXY, TimeSeries, Projections
- `AxisType`: NoneAxis, TimeAxis, XAxis, YAxis, ZAxis
- `GraphMarkerShape`: NoMarker, Dot, Circle, Square, Triangle, Diamond, Star, Plus, Cross, FilledCircle, InvertedTriangle, CrossedSquare, PlusSquare, CrossedCircle, PlusCircle, Custom
- `GraphLineStyle`: NoLine, Line, StepLeft, StepRight, StepCenter
- `ColorGradient`: Grayscale, Hot, Cold, Night, Candy, Geography, Ion, Thermal, Polar, Spectrum, Jet, Hues
- `Coordinates`: Pixels, Data
- `LineTermination`: NoneTermination, Arrow, SPikeArrow, LineArrow, Circle, Square, Diamond, Bar, HalfBar, SkewedBar
- `ParameterType`: NotAParameter, Scalar, Vector, Multicomponents, Spectrogram
- `ProductsModelNodeType`: (exposed but values not listed in enums.hpp)

---

## P0 — Broken / Non-functional

### 1. `plot()` monkey-patch doesn't handle `Scatter`

**File:** `__init__.py:22-65`

The `plot_func()` inner function only routes `GraphType.Line`, `GraphType.ParametricCurve`, and `GraphType.ColorMap`. Passing `GraphType.Scatter` silently returns `None`.

```python
# Current code (broken):
def plot_func(self, callback, graph_type=None, **kwargs):
    if graph_type == GraphType.ParametricCurve:
        res = cls.parametric_curve(self, callback, **kwargs)
    if graph_type == GraphType.Line:
        res = cls.line(self, callback, **kwargs)
    if graph_type == GraphType.ColorMap:
        res = cls.colormap(self, callback, **kwargs)
    return res  # None if Scatter
```

**Impact:** Any user calling `plot(callback, graph_type=GraphType.Scatter)` gets `None` with no error.

### 2. `Pipeline.threaded()` is a no-op

**File:** `pipeline.py:114-117`

```python
def threaded(self):
    """Force this pipeline to execute transforms in a worker thread."""
    self._auto_threaded = True
    return self
```

The `_auto_threaded` flag is set but never read by the dispatch logic in `_make_dispatch()`. The threading promise is unfulfilled.

### 3. `plot()` return type inconsistency

- `SciQLopPlot.line()` returns `SciQLopGraphInterface*` (single object)
- `SciQLopMultiPlotPanel.line()` returns `QPair<SciQLopPlotInterface*, SciQLopGraphInterface*>` (tuple)

The monkey-patched `plot()` method is applied identically to both classes but doesn't account for this. The Pipeline manual test relies on tuple unpacking (`plot1, graph1 = panel.plot(...)`) which only works for the panel, not for individual plots.

### 4. Silent error swallowing

**File:** `__init__.py:24-39, 53-62`

All exceptions in `plot()` and `plot_func()` are caught and printed to stdout instead of being raised. This makes debugging extremely difficult for users.

---

## P1 — Naming & Consistency Issues

### 1. Getter naming inconsistency

| Class | Pattern | Examples |
|-------|---------|----------|
| `SciQLopVerticalSpan` | bare name | `color()`, `range()`, `tool_tip()`, `visible()`, `selected()`, `read_only()` |
| `MultiPlotsVerticalSpan` | `get_` / `is_` prefix | `get_color()`, `get_tool_tip()`, `get_id()`, `is_visible()`, `is_selected()`, `is_read_only()` |

These are the same conceptual object (vertical span) at different abstraction levels but use different naming conventions.

### 2. Signal naming inconsistency

| Signal | Convention | Location |
|--------|-----------|----------|
| `selectionChanged(bool)` | camelCase | `SciQLopVerticalSpan`, `SciQLopPlotPanelInterface`, `SciQLopMultiPlotPanel` |
| `selection_changed(bool)` | snake_case | `SciQLopPlotInterface`, `SciQLopPlottableInterface`, `MultiPlotsVerticalSpan` |
| `setSelected()` | camelCase | `SciQLopMultiPlotPanel` |
| `set_selected()` | snake_case | `SciQLopVerticalSpan`, `SciQLopPlotInterface` |

### 3. Method naming inconsistency

| Pattern | Example | Location |
|---------|---------|----------|
| `enable_legend(bool show)` | Parameter named `show` but method is `enable_` | `SciQLopPlotInterface` |
| `set_tool_tip()` / `tool_tip()` | snake_case with abbreviation | `SciQLopVerticalSpan` |
| `setToolTip()` / `toolTip()` | Qt camelCase | `impl::SciQlopItemWithToolTip` |

### 4. bindings.xml duplicate rename

**File:** `bindings.xml:159-160` — `scatter` method has `index="5"` renamed twice:

```xml
<modify-argument index="5">
    <rename to="marker"/>
</modify-argument>
<modify-argument index="5">
    <rename to="marker"/>
</modify-argument>
```

### 5. `SciQLopPlotRange` Python integration gaps

- `datetime_start()` / `datetime_stop()` use raw CPython datetime API (fragile, no timezone/sub-second handling)
- `_is_time_range` flag exists in C++ but is not accessible from Python
- `__repr__` exists in C++ but not clear if Shiboken surfaces it (not defined in bindings.xml)
- `__getitem__` / `__setitem__` have no bounds checking

---

## P2 — Missing Functionality

### 1. No `remove_graph()` / `remove_plotable()`

Graphs can be added to plots but there is no public API to remove them. The only way is to destroy the plot.

### 2. No Pythonic container protocols on panels

`SciQLopMultiPlotPanel` has `plot_at(index)` and `size()` but no `__getitem__`, `__len__`, `__iter__`, or `__contains__`. Same for `SciQLopPlotContainer`.

### 3. Items lack factory methods

Creating items requires direct construction:
```python
span = SciQLopVerticalSpan(plot, range, color, ...)
text = SciQLopTextItem(plot, "hello", QPointF(10, 20))
```

No convenience methods like `plot.add_vspan(...)`, `plot.add_text(...)`, etc.

### 4. Incomplete item wrappers

- `SciQLopPixmapItem`: No setters exposed (no `set_pixmap()`, no `position()`, no `bounding_rectangle()`)
- `SciQLopTextItem`: Missing font, color, rotation setters (only `set_text()` and `set_position()`)
- `SciQLopStraightLine`: The `moved` signal from inner `StraightLine` is not forwarded to the Python-facing wrapper
- `SciQLopCurvedLineItem`: No `set_color()` or `set_line_width()` (base class has these but they're not wired)

### 5. Few observable properties registered for pipeline

Only 3 out of dozens of available signals are registered:

| Registered | Class | Property |
|-----------|-------|----------|
| Yes | `SciQLopGraphInterface` | `data` (signal: `data_changed`, getter: `data`, setter: `set_data`) |
| Yes | `SciQLopPlotAxisInterface` | `range` (signal: `range_changed`, getter: `range`, setter: `set_range`) |
| Yes | `SciQLopVerticalSpan` | `range` (signal: `range_changed`, getter: `range`, setter: `set_range`) |
| Yes | `SciQLopVerticalSpan` | `tooltip` (no signal, getter: `tool_tip`, setter: `set_tool_tip`) |

**Missing registrations that would be valuable:**

| Class | Property | Signal | Use case |
|-------|----------|--------|----------|
| `SciQLopPlotInterface` | `x_axis_range` | `x_axis_range_changed` | React to pan/zoom |
| `SciQLopPlotInterface` | `y_axis_range` | `y_axis_range_changed` | React to pan/zoom |
| `SciQLopPlotInterface` | `time_axis_range` | `time_axis_range_changed` | React to time navigation |
| `SciQLopPlotInterface` | `selection` | `selection_changed` | React to plot selection |
| `SciQLopGraphInterface` | `labels` | `labels_changed` | React to label changes |
| `SciQLopGraphInterface` | `colors` | `colors_changed` | React to color changes |
| `SciQLopColorMapInterface` | `gradient` | — | Gradient control |
| `MultiPlotsVerticalSpan` | `range` | `range_changed` | Multi-plot span pipeline |
| `SciQLopGraphComponentInterface` | `visible` | `visible_changed` | Toggle traces |
| `SciQLopPlotPanelInterface` | `time_range` | `time_range_changed` | Panel-level time navigation |

### 6. No `MultiPlotsVerticalSpan` pipeline support

The `.on` descriptor is set up for `SciQLopGraphInterface`, `SciQLopPlotAxisInterface`, and `SciQLopVerticalSpan` — but NOT for `MultiPlotsVerticalSpan`.

### 7. Pipeline lifecycle management

- No registry of active pipelines
- No auto-disconnect when QObjects are destroyed (relies on `try/except RuntimeError` in slot)
- No way to list/inspect active pipelines
- No `Pipeline.__del__` to auto-disconnect

### 8. Missing exports in `__init__.py`

Classes available in bindings but not explicitly imported/documented:
- `SciQLopPlotPanelInterface` (needed for nested panels)
- `SciQLopPlotCollectionBehavior` (needed for custom behaviors)
- `SciQLopMultiTimeSeriesPlotPanel` (exists in headers, no test)
- `SciQLopGraphComponent` / `SciQLopGraphComponentInterface`
- `SciQLopFunctionGraph` (for `observe()` / `set_callable()`)

---

## P3 — Polish & Nice-to-Haves

### 1. No save/export API

QCustomPlot internally supports `savePng()`, `savePdf()`, `saveBmp()`, `saveJpg()`, `saveRastered()`, `toPixmap()`. None of these are exposed through the Python API.

**Single plots:** `SciQLopPlot` inherits from `QCustomPlot` (or wraps it via `qcp_plot()`), so exposing these methods to Python is straightforward — just add them to `bindings.xml` or add thin wrappers on `SciQLopPlotInterface`.

**Multi-plot panels (SVG/PDF export):** `SciQLopMultiPlotPanel` (a `QScrollArea` containing a `SciQLopPlotContainer` with multiple child `SciQLopPlot`s) has no export capability at all. Panel-level export requires compositing all child plots into a single document. Two approaches:

- *Raster (PNG/JPG):* Render via `QWidget::grab()` on the panel — simple but limited resolution and no vector output. May also miss off-screen plots in the scroll area (need to temporarily resize or iterate children).
- *Vector (SVG/PDF):* Create a `QPainter` on a `QSvgGenerator` (SVG) or `QPdfWriter` (PDF), then iterate `plots()` and render each `QCustomPlot` into a sub-region using `QCustomPlot::toPainter()` (or the internal `draw()` path that `savePdf` uses). This preserves vector quality for text, axes, and line graphs. ColorMaps would still be rasterized internally by QCP, which is expected.

Recommended approach: add `save_png()`, `save_pdf()`, `save_svg()` methods on both `SciQLopPlotInterface` and `SciQLopPlotPanelInterface`. For single plots, delegate to QCP's existing methods. For panels, implement the compositing logic in C++ (iterate children, paint into a single `QPainter`), handling layout (vertical stacking with correct spacing/margins) and scroll-area viewport expansion.

### 2. No subplot convenience

The `StackedGraphs` manual test uses raw QCP `plotLayout()` and `QCPAxisRect` for subplot creation. No Python-friendly `add_subplot()` or `figure()` style API.

### 3. No datetime helpers on axes

Time axes exist (`SciQLopTimeSeriesPlot`, `SciQLopPlotDummyAxis`) but there's no Python convenience for:
- Setting axis range with Python `datetime` objects on individual plots
- Getting axis range as Python `datetime` objects
- Formatting tick labels with datetime

### 4. Testing gaps

- **Zero automated integration tests** against compiled bindings. All unit tests use mock `FakeQObject`.
- **No tests for:** `SciQLopTextItem`, `SciQLopTracer`, `SciQLopHorizontalLine`/`SciQLopVerticalLine` (imported but never instantiated), `SciQLopMultiTimeSeriesPlotPanel`
- **Manual tests have optional deps** (`humanize`, `qtconsole`, `numba`) with no graceful fallback in some cases.

### 5. Low-level QCP API still needed

Several manual tests (`SciQLopGraph/main.py`, `StackedGraphs/main.py`) fall back to raw QCP APIs for:
- `QCP.iRangeDrag | QCP.iRangeZoom` interaction flags
- `QCPAxisRect` for multi-axis layouts
- `QCPLegend` customization
- `QCPColorGradient` / `QCPColorScale` configuration

This suggests the high-level Python wrapper is incomplete for advanced use cases.

### 6. `SciQLopPlotCollectionInterface` is not a QObject

It's a plain C++ class used via multiple inheritance. This makes Shiboken bindings tricky and prevents it from participating in Qt's signal/slot system directly.

---

## Test Coverage Matrix

| Component | Unit Test | Manual Test | Gap |
|-----------|-----------|-------------|-----|
| `SciQLopPlot` | No | Yes (All, SciQLopGraph, etc.) | No unit test |
| `SciQLopTimeSeriesPlot` | No | Yes (All, MicroSciQLop, etc.) | No unit test |
| `SciQLopMultiPlotPanel` | No | Yes (All, ColorMap, etc.) | No unit test |
| `SciQLopNDProjectionPlot` | No | Yes (Projections) | No unit test |
| `SciQLopLineGraph` | No | Yes | No unit test |
| `SciQLopCurve` | No | Yes (butterfly) | No unit test |
| `SciQLopColorMap` | No | Yes | No unit test |
| `SciQLopVerticalSpan` | No | Yes (All, Pipeline) | No unit test |
| `MultiPlotsVerticalSpan` | No | Yes (All, StressModel) | No unit test |
| `SciQLopPixmapItem` | No | Yes (All) | No unit test |
| `SciQLopEllipseItem` | No | Yes (All) | No unit test |
| `SciQLopTextItem` | No | **No** | **No coverage at all** |
| `SciQLopTracer` | No | **No** | **No coverage at all** |
| `SciQLopHorizontalLine` | No | Imported only | **No coverage** |
| `SciQLopVerticalLine` | No | Imported only | **No coverage** |
| `SciQLopCurvedLineItem` | No | **No** | **No coverage at all** |
| `Pipeline` / `Event` / `ObservableProperty` | Yes (mocks) | Yes (Pipeline) | No integration test |
| `InspectorView` / `PlotsModel` | No | Yes (All) | No unit test |
| `ProductsModel` / `ProductsView` | No | Yes (Products) | No unit test |
| `PlotDragNDropCallback` | No | Yes (DragNDrop) | No unit test |
| `SciQLopMultiTimeSeriesPlotPanel` | No | **No** | **No coverage at all** |

---

## Proposed Roadmap

### Phase 1: Fix What's Broken
1. Fix `plot()` to handle `GraphType.Scatter`
2. Fix or remove `Pipeline.threaded()` (either implement threading or remove the dead API)
3. Normalize `plot()` return types (always return a consistent type, document tuple unpacking for panels)
4. Raise exceptions instead of printing to stdout in `plot()`

### Phase 2: Naming Consistency
1. Audit all getter/setter pairs — pick one convention (bare name preferred: `color()` not `get_color()`, `visible()` not `is_visible()`)
2. Audit all signals — pick one convention (snake_case preferred: `selection_changed` not `selectionChanged`)
3. Fix `bindings.xml` duplicate rename
4. Add `SciQLopPlotRange.__repr__` and `_is_time_range` to Python

### Phase 3: Complete the Python API
1. Add `remove_graph()` / `remove_plotable()` to plots
2. Add `__len__`, `__iter__`, `__getitem__` to panels/containers
3. Add factory methods to plots: `plot.add_vspan()`, `plot.add_text()`, etc.
4. Complete item wrappers (PixmapItem setters, TextItem font/color, signal forwarding)
5. Register more observable properties for pipeline system
6. Add `MultiPlotsVerticalSpan` to pipeline system
7. Export missing classes from `__init__.py`

### Phase 4: New User Interactions
1. **Interactive zone creation on plots** — Allow users to create vertical spans (zones) directly on a plot via mouse interaction. Two possible UX approaches (TBC):
   - *Two-click*: first click sets start position, second click sets end position, span is created between them
   - *Click-drag*: click sets start, drag shows live preview, release sets end
   Needs: a mode/tool toggle (so regular pan/zoom isn't broken), visual feedback during creation (preview span with dashed borders or translucent fill), a signal emitted when creation completes (e.g. `zone_created(SciQLopPlotRange)`), and integration with `MultiPlotsVerticalSpan` for multi-panel use. Should work on both `SciQLopPlot` and `SciQLopMultiPlotPanel`. Consider a toolbar button or keyboard shortcut to enter "zone creation mode".

2. **Activity/loading indicator for graphs awaiting data** — When a graph's data provider is fetching data (e.g. from the network in SciQLop), show a visual indicator that the graph is busy. Options:
   - *Per-graph*: a small spinner, pulsing overlay, or animated hatching pattern over the plot area while a `SciQLopFunctionGraph`'s pipeline is running
   - *Per-plot*: a progress bar or spinner in the plot title bar / margin area
   - API surface needed: `SciQLopPlottableInterface::set_loading(bool)` / `loading()` signal, or automatic detection when `SimplePyCallablePipeline` has an in-flight call. Should be visible at both the individual plot level and propagate to `SciQLopMultiPlotPanel` (e.g. a subtle indicator in the panel header if any child plot is loading).
   Key files: `DataProducer/DataProducer.hpp` (pipeline already tracks calls), `SciQLopPlot.hpp` (rendering), `SciQLopPlotInterface.hpp` (API surface).

### Phase 5: Polish
1. Add save/export API: `save_png()`, `save_pdf()`, `save_svg()`, `to_pixmap()` on both `SciQLopPlotInterface` (single plot, delegates to QCP) and `SciQLopPlotPanelInterface` (panel, composites child plots via `QPainter` into `QPdfWriter`/`QSvgGenerator`/`QPixmap`)
2. Add datetime convenience methods on axes
3. Add automated integration tests against compiled bindings
4. Wrap remaining raw QCP APIs needed by advanced users
5. Add pipeline lifecycle management (registry, auto-disconnect)

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `SciQLopPlots/__init__.py` | Python entry point, monkey-patches, property registration |
| `SciQLopPlots/pipeline.py` | Reactive pipeline implementation |
| `SciQLopPlots/event.py` | Event object for pipeline transforms |
| `SciQLopPlots/properties.py` | Observable property system |
| `SciQLopPlots/bindings/bindings.xml` | Shiboken typesystem definition |
| `include/SciQLopPlots/SciQLopPlotInterface.hpp` | Base plot interface (all plot methods) |
| `include/SciQLopPlots/SciQLopPlot.hpp` | Concrete plot implementation |
| `include/SciQLopPlots/SciQLopTimeSeriesPlot.hpp` | Time-series specialization |
| `include/SciQLopPlots/SciQLopPlotAxis.hpp` | Axis interface + implementations |
| `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp` | Graph/plotable interface hierarchy |
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` | Multi-plot panel |
| `include/SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp` | Plot collection interface + behaviors |
| `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp` | Vertical span item |
| `include/SciQLopPlots/Items/SciQLopStraightLines.hpp` | Line items (vertical, horizontal) |
| `include/SciQLopPlots/Items/SciQLopShapesItems.hpp` | Ellipse, curved line items |
| `include/SciQLopPlots/Items/SciQLopTextItem.hpp` | Text overlay item |
| `include/SciQLopPlots/Items/SciQLopPixmapItem.hpp` | Pixmap overlay item |
| `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp` | Multi-plot vertical span + collection |
| `include/SciQLopPlots/SciQLopPlotRange.hpp` | Range value type |
| `include/SciQLopPlots/enums.hpp` | All enums (GraphType, PlotType, etc.) |
