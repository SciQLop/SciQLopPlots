# Automated Testing Plan — 2026-03-07

## Goal

Create an automated test suite that exercises the Python API against compiled C++ bindings, catching:
- API contract violations (wrong return types, missing methods, bad signatures)
- Memory management issues (segfaults, use-after-free, dangling pointers)
- Signal/slot correctness (signals fire with correct arguments)
- Object lifecycle issues (parent-child ownership, destruction order)

The tests must run headless in GitHub Actions using Xvfb.

## Constraints

- Framework: `pytest` + `pytest-qt`
- No rendering/screenshot verification — only API state and behavior
- Tests run against an installed wheel (skip build in test job for now)
- Xvfb for headless Qt widget creation
- Python API surface only (this project is not meant for direct C++ use)

## Test Structure

```
tests/
  unit/                          # Existing pure-Python tests (mocks)
    conftest.py
    test_event.py
    test_properties.py
    test_pipeline.py
    test_registration.py
  integration/                   # NEW: tests against compiled bindings
    conftest.py                  # QApplication fixture, helpers, markers
    test_plot_lifecycle.py       # Create/destroy plots without crash
    test_graph_api.py            # Add graphs, set/get data, labels, colors
    test_colormap_api.py         # Colormap creation, data, gradient
    test_axis_api.py             # Axis range, log, label, visibility
    test_items_api.py            # Spans, lines, text, ellipse, pixmap
    test_panel_api.py            # MultiPlotPanel: add/remove/move plots
    test_signals.py              # Signal emission verification
    test_pipeline_integration.py # Pipeline with real Qt objects
    test_memory.py               # Ownership, destruction, use-after-free
```

## Test Categories & What They Cover

### 1. `test_plot_lifecycle.py` — Object Creation & Destruction

Verifies that all plot types can be created and destroyed without segfault.

Tests:
- Create `SciQLopPlot`, verify it's a QWidget, delete it
- Create `SciQLopTimeSeriesPlot`, verify time_axis is not None, delete it
- Create `SciQLopNDProjectionPlot`, delete it
- Create `SciQLopMultiPlotPanel`, delete it
- Create plot, add graphs, delete plot (graphs should be cleaned up)
- Create panel, add multiple plots, clear panel, delete panel
- Create panel, add plot, delete plot individually, verify panel still works
- Rapid create/destroy cycles (stress test, catches race conditions)

### 2. `test_graph_api.py` — Graph CRUD & Data

Tests:
- `plot.line(x, y)` returns `SciQLopGraphInterface` (not None)
- `plot.line(x, y, labels=["a","b"])` — verify `graph.labels() == ["a","b"]`
- `plot.line(x, y, colors=[QColor("red")])` — verify colors round-trip
- `plot.scatter(x, y)` returns non-None (catches the current P0 bug)
- `plot.parametric_curve(x, y)` returns non-None
- `graph.set_data(x, y)` then `graph.data()` returns buffers with matching shape
- `graph.set_labels(["new"])` then `graph.labels() == ["new"]`
- `graph.set_colors([QColor("blue")])` then verify
- `graph.components()` returns list of `SciQLopGraphComponentInterface`
- `graph.component(0)` and `graph.component("name")` work
- Callable-based graph: `plot.line(callback)` — verify graph is created
- `plot.plottables()` returns list containing added graphs
- `plot.plottable(0)` returns the first graph
- `plot.plottable("name")` returns by name

### 3. `test_colormap_api.py` — Colormap Specific

Tests:
- `plot.colormap(x, y, z)` returns `SciQLopColorMapInterface`
- `cmap.set_data(x, y, z)` then `cmap.data()` returns 3 buffers
- `cmap.gradient()` returns a `ColorGradient` enum value
- `cmap.set_gradient(ColorGradient.Jet)` then verify
- `cmap.set_y_log_scale(True)` then `cmap.y_log_scale() == True`
- `cmap.set_z_log_scale(True)` then `cmap.z_log_scale() == True`
- Callable-based colormap: `plot.colormap(callback)` works

### 4. `test_axis_api.py` — Axis Manipulation

Tests:
- `plot.x_axis()` / `plot.y_axis()` return `SciQLopPlotAxisInterface`
- `plot.z_axis()` returns axis (may be None if no colormap)
- `plot.x2_axis()` / `plot.y2_axis()` return axes
- `plot.time_axis()` returns axis for `SciQLopTimeSeriesPlot`
- `axis.set_range(SciQLopPlotRange(0, 10))` then `axis.range()` matches
- `axis.set_range(0.0, 10.0)` convenience overload works
- `axis.set_log(True)` then `axis.log() == True`
- `axis.set_label("X")` then `axis.label() == "X"`
- `axis.set_visible(False)` then `axis.visible() == False`
- `axis.rescale()` doesn't crash (even with no data)
- `axis.orientation()` returns a Qt.Orientation
- `SciQLopPlotRange` basics: construction, `start()`, `stop()`, `size()`, `center()`, `contains()`, `__getitem__`, `__len__`, `__eq__`, arithmetic operators

### 5. `test_items_api.py` — Overlay Items

Tests:
- `SciQLopVerticalSpan(plot, range, color)` — creation doesn't crash
- `span.range()` matches construction range
- `span.set_range(new_range)` then `span.range()` matches
- `span.set_color(QColor("red"))` then `span.color()` matches
- `span.set_visible(False)` then `span.visible() == False`
- `span.set_read_only(True)` then `span.read_only() == True`
- `span.set_tool_tip("hello")` then `span.tool_tip() == "hello"`
- Delete span while plot is alive (no crash)
- Delete plot while span exists (no crash)
- `SciQLopVerticalLine(plot, 5.0)` — creation, `position()` == 5.0
- `SciQLopHorizontalLine(plot, 3.0)` — creation, `position()` == 3.0
- `line.set_color(QColor("green"))` then `line.color()` matches
- `line.set_line_width(2.0)` then `line.line_width()` == 2.0
- `SciQLopTextItem(plot, "hello", QPointF(10, 20))` — creation
- `text.text() == "hello"`, `text.set_text("world")` round-trips
- `text.position()` matches construction position
- `SciQLopEllipseItem(plot, QRectF(0,0,10,10))` — creation
- `ellipse.bounding_rectangle()` matches
- `ellipse.set_position(QPointF(5,5))` then `ellipse.position()` matches
- `SciQLopCurvedLineItem(plot, QPointF(0,0), QPointF(10,10))` — creation
- `curved.start_position()` / `curved.stop_position()` match
- `SciQLopPixmapItem(plot, QPixmap(10,10), QRectF(0,0,10,10))` — creation doesn't crash

### 6. `test_panel_api.py` — MultiPlotPanel

Tests:
- `SciQLopMultiPlotPanel()` — default construction
- `SciQLopMultiPlotPanel(synchronize_x=True, synchronize_time=True)` — kwargs
- `panel.create_plot()` returns `SciQLopPlotInterface`
- `panel.create_plot(plot_type=PlotType.TimeSeries)` works
- `panel.add_plot(SciQLopPlot())` — adds successfully
- `panel.size()` reflects additions
- `panel.empty()` is True initially, False after add
- `panel.plot_at(0)` returns added plot
- `panel.plots()` returns list
- `panel.remove_plot(plot)` — removes, size decreases
- `panel.insert_plot(0, plot)` — inserts at position
- `panel.move_plot(0, 1)` — reorders
- `panel.clear()` — empties panel
- `panel.line(x, y)` returns `(SciQLopPlotInterface, SciQLopGraphInterface)` tuple
- `panel.colormap(x, y, z)` returns `(SciQLopPlotInterface, SciQLopColorMapInterface)` tuple
- `panel.set_x_axis_range(SciQLopPlotRange(0, 10))` propagates to child plots
- `panel.set_time_axis_range(SciQLopPlotRange(t0, t1))` propagates
- `MultiPlotsVerticalSpan(panel, range, color)` — creation
- `MultiPlotsVSpanCollection` — `create_span()`, `delete_span()`, `spans()`

### 7. `test_signals.py` — Signal Emission

Uses `qtbot.waitSignal` and `qtbot.waitSignals` from pytest-qt.

Tests:
- `axis.set_range(...)` emits `axis.range_changed`
- `span.set_range(...)` emits `span.range_changed`
- `graph.set_data(x, y)` emits `graph.data_changed`
- `plot.set_selected(True)` emits `plot.selection_changed`
- `panel.add_plot(plot)` emits `panel.plot_list_changed`
- `axis.set_log(True)` emits `axis.log_changed`
- `axis.set_label("X")` emits `axis.label_changed`
- Signal argument types are correct (e.g. `range_changed` receives `SciQLopPlotRange`)

### 8. `test_pipeline_integration.py` — Pipeline with Real Bindings

Tests:
- `graph.on.data` returns an `ObservableProperty`
- `axis.on.range` returns an `ObservableProperty`
- `span.on.range` returns an `ObservableProperty`
- `axis.on.range >> other_axis.on.range` — changing source axis range propagates
- `span.on.range >> transform >> span.on.tooltip` — tooltip updates on range change
- `graph.on.data >> transform >> other_graph.on.data` — data pipeline works
- Pipeline disconnect stops propagation
- Terminal sink (no target) fires callback

### 9. `test_memory.py` — Ownership & Destruction Safety

These tests verify no segfault occurs during object lifecycle edge cases. They use a helper that runs operations in a function scope and forces garbage collection.

Tests:
- Create plot, add graph, delete graph reference, GC, access plot — no crash
- Create plot, add graph, delete plot — graph should be invalidated
- Create panel, add plot, delete panel — all children cleaned up
- Create span on plot, delete plot, try accessing span — no crash (graceful error)
- Create pipeline between two objects, delete source object, trigger — no crash
- Create pipeline between two objects, delete target object, trigger — no crash
- Create 100 plots with graphs in a loop, delete all, GC — no leak/crash
- Create panel, add/remove plots rapidly in a loop — no crash
- Create MultiPlotsVerticalSpan, remove all plots from panel, add new ones — span updates

## conftest.py Design

```python
import pytest
import gc
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer


@pytest.fixture(scope="session")
def qapp():
    """Session-scoped QApplication (pytest-qt also provides this but we
    want explicit control)."""
    app = QApplication.instance() or QApplication([])
    yield app


@pytest.fixture
def qtbot(qapp, qtbot):
    """Re-export pytest-qt's qtbot with our qapp."""
    return qtbot


@pytest.fixture
def plot(qtbot):
    """Fresh SciQLopPlot for each test."""
    from SciQLopPlots import SciQLopPlot
    p = SciQLopPlot()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def ts_plot(qtbot):
    """Fresh SciQLopTimeSeriesPlot for each test."""
    from SciQLopPlots import SciQLopTimeSeriesPlot
    p = SciQLopTimeSeriesPlot()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def panel(qtbot):
    """Fresh SciQLopMultiPlotPanel for each test."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    p = SciQLopMultiPlotPanel()
    qtbot.addWidget(p)
    return p


@pytest.fixture
def sample_data():
    """Returns (x, y) numpy arrays for testing."""
    x = np.linspace(0, 10, 100).astype(np.float64)
    y = np.sin(x).astype(np.float64)
    return x, y


@pytest.fixture
def sample_colormap_data():
    """Returns (x, y, z) numpy arrays for colormap testing."""
    x = np.linspace(0, 10, 50).astype(np.float64)
    y = np.linspace(0, 5, 30).astype(np.float64)
    z = np.random.rand(30, 50).astype(np.float64)
    return x, y, z


def force_gc():
    """Force garbage collection and process pending Qt events."""
    gc.collect()
    QApplication.processEvents()
    gc.collect()
```

## GitHub Actions Workflow

```yaml
name: Tests

on: [push, pull_request]

jobs:
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.12'

      - name: Install system dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y xvfb libegl1 libgl1-mesa-glx

      - name: Install package
        run: pip install ".[test]"

      - name: Run unit tests
        run: pytest tests/unit/ -v

      - name: Run integration tests
        run: xvfb-run --auto-servernum pytest tests/integration/ -v --timeout=60

  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.12'

      - name: Install test dependencies
        run: pip install pytest numpy

      - name: Run unit tests
        working-directory: tests/unit
        run: |
          PYTHONPATH=../../SciQLopPlots pytest -v
```

## pyproject.toml Changes

Add optional test dependencies:

```toml
[project.optional-dependencies]
test = ["pytest", "pytest-qt", "pytest-timeout", "numpy"]
```

## Implementation Order

### Step 1: Infrastructure
- Create `tests/integration/conftest.py` with fixtures
- Add `[project.optional-dependencies] test` to `pyproject.toml`
- Update `.github/workflows/test.yml`

### Step 2: Lifecycle & Memory (highest value — catches segfaults)
- `test_plot_lifecycle.py`
- `test_memory.py`

### Step 3: Core API (catches regressions)
- `test_graph_api.py`
- `test_axis_api.py`
- `test_colormap_api.py`

### Step 4: Items & Panel
- `test_items_api.py`
- `test_panel_api.py`

### Step 5: Signals & Pipeline
- `test_signals.py`
- `test_pipeline_integration.py`

## Success Criteria

- All tests pass locally with `xvfb-run pytest tests/integration/`
- No test takes more than 10 seconds
- Tests catch the known P0 bugs (scatter returns None, etc.)
- No test depends on rendering or visual output
- Tests are independent (can run in any order)
