# Test & Demo Overhaul Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the story-driven UI fuzzing framework from SciQLop to SciQLopPlots, clean up and modernize the demo examples, and add interaction-level integration tests.

**Architecture:** Three independent workstreams: (1) a reusable fuzzing framework (`tests/fuzzing/`) with actions for plot/graph/item lifecycle and axis manipulation, tested via both scripted stories and Hypothesis stateful fuzzing; (2) restructured demos with shared data generators, graceful dependency fallbacks, and a gallery launcher; (3) new integration tests for mouse-driven interactions and stress testing.

**Tech Stack:** Python 3.10+, pytest, pytest-qt, hypothesis, PySide6, numpy, SciQLopPlots bindings

---

## File Structure

### New files to create

```
tests/fuzzing/
  __init__.py                    # empty
  actions.py                     # @ui_action, ActionRegistry, StoryRunner, settle(), run_action()
  story.py                       # Step, Story (narrative + reproducer rendering)
  model.py                       # PlotModel dataclass
  introspect.py                  # pure query functions against SciQLopPlots widgets
  plot_actions.py                # create/remove plot, add/remove to panel
  graph_actions.py               # add line, add colormap, remove graph, set data
  item_actions.py                # add/remove vertical span, move span
  axis_actions.py                # set range, toggle log scale
  conftest.py                    # story_runner fixture, fuzzing_reports_dir
  test_workflows.py              # scripted story tests
  test_fuzzing.py                # Hypothesis state machine test

tests/manual-tests/
  common.py                      # MODIFY: graceful qtconsole fallback
  demo_data.py                   # NEW: shared data generators
  gallery.py                     # NEW: launcher listing all demos
  basics/
    __init__.py
    line_graph.py                # NEW: modernized from SciQLopGraph/main.py
    colormap.py                  # NEW: simplified from SciQLopColorMap/main.py
    vertical_spans.py            # NEW: from SciQLopVerticalSpan/main.py
    parametric_curve.py          # NEW: butterfly curve extracted from All/main.py
  advanced/
    __init__.py
    pipeline.py                  # MOVE: Pipeline/main.py (already clean)
    drag_and_drop.py             # MOVE+SIMPLIFY: DragNDrop/main.py
    stacked_plots.py             # NEW: time-series stacking with multi-plot span
    products.py                  # MOVE: Products/main.py
    projections.py               # MOVE: Projections/main.py (requires speasy)
  stress/
    __init__.py
    stress_model.py              # MOVE+SIMPLIFY: StressModel/main.py
    profiling.py                 # MOVE: ForProfiling/main.py
  real_data/
    __init__.py
    micro_sciqlop.py             # MOVE: MicroSciQLop/main.py (requires speasy)

tests/integration/
  test_stress.py                 # NEW: automated stress test (from StressModel logic)
```

### Files to remove (after migration)

```
tests/manual-tests/SciQLopGraph/main.py      # replaced by basics/line_graph.py
tests/manual-tests/SciQLopColorMap/main.py    # replaced by basics/colormap.py
tests/manual-tests/SciQLopVerticalSpan/main.py # replaced by basics/vertical_spans.py
tests/manual-tests/StackedGraphs/main.py      # uses internal QCP API, not public API
tests/manual-tests/DrawingAPI/main.py          # tests QPainter internals, not plotting API
tests/manual-tests/All/main.py                 # monolith, replaced by individual demos
tests/manual-tests/GigaPoints/main.py          # merged into stress/profiling.py
tests/manual-tests/ForProfiling/main.py         # moved
tests/manual-tests/StressModel/main.py          # moved
tests/manual-tests/Pipeline/main.py             # moved
tests/manual-tests/DragNDrop/main.py            # moved
tests/manual-tests/MicroSciQLop/main.py         # moved
tests/manual-tests/Products/main.py             # moved
tests/manual-tests/Projections/main.py          # moved
```

---

## Chunk 1: Story-Driven Fuzzing Framework (Core)

### Task 1: Port generic framework files (story.py, actions.py)

These are the reusable core — no SciQLopPlots-specific code.

**Files:**
- Create: `tests/fuzzing/__init__.py`
- Create: `tests/fuzzing/story.py`
- Create: `tests/fuzzing/actions.py`

- [ ] **Step 1: Create `tests/fuzzing/__init__.py`**

```python
# empty
```

- [ ] **Step 2: Create `tests/fuzzing/story.py`**

Port from SciQLop's `tests/fuzzing/story.py` — this is generic, no changes needed:

```python
from __future__ import annotations

from dataclasses import dataclass
from textwrap import indent
from typing import Any


@dataclass
class Step:
    action_name: str
    args: dict[str, str]
    narrate_template: str
    result: Any = None
    error: Exception | None = None

    @property
    def narrative(self) -> str:
        fmt_kwargs = {**self.args}
        if self.result is not None:
            fmt_kwargs["result"] = self.result
        return self.narrate_template.format(**fmt_kwargs)

    def as_code(self) -> str:
        args_str = ", ".join(f"{k}={v!r}" for k, v in self.args.items())
        return f"actions.{self.action_name}({args_str})"


class Story:
    def __init__(self):
        self.steps: list[Step] = []

    def record(self, step: Step):
        self.steps.append(step)

    def narrative(self) -> str:
        return "\n".join(
            f"{i + 1}. {step.narrative}" for i, step in enumerate(self.steps)
        )

    def reproducer(self) -> str:
        lines = [step.as_code() for step in self.steps]
        body = indent("\n".join(lines), "    ") if lines else "    pass"
        return f"def test_reproducer(panel, qtbot):\n{body}"
```

- [ ] **Step 3: Create `tests/fuzzing/actions.py`**

Port from SciQLop, adapted to SciQLopPlots (no `main_window` — use `panel` as the root widget):

```python
from __future__ import annotations

import inspect
import time
from dataclasses import dataclass
from typing import Any, Callable

from hypothesis.stateful import (
    Bundle,
    RuleBasedStateMachine,
    rule,
    precondition,
    initialize,
)
from hypothesis import settings
from PySide6.QtWidgets import QApplication

from tests.fuzzing.model import PlotModel
from tests.fuzzing.story import Step, Story


@dataclass
class ActionMeta:
    narrate: str
    model_update: Callable
    verify: Callable
    precondition: Callable | None = None
    target: str | None = None
    bundles: dict[str, str] | None = None
    strategies: dict[str, Any] | None = None
    settle_timeout_ms: int = 50


def ui_action(
    *,
    narrate: str,
    model_update: Callable,
    verify: Callable,
    precondition: Callable | None = None,
    target: str | None = None,
    bundles: dict[str, str] | None = None,
    strategies: dict[str, Any] | None = None,
    settle_timeout_ms: int = 50,
):
    meta = ActionMeta(
        narrate=narrate,
        model_update=model_update,
        verify=verify,
        precondition=precondition,
        target=target,
        bundles=bundles,
        strategies=strategies,
        settle_timeout_ms=settle_timeout_ms,
    )

    def decorator(fn):
        fn._ui_meta = meta
        return fn

    return decorator


def settle(timeout_ms: int = 50):
    app = QApplication.instance()
    if app is None:
        return
    deadline = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < deadline:
        app.processEvents()
        time.sleep(0.001)


def _bind_kwargs(fn: Callable, kwargs: dict[str, Any]) -> dict[str, Any]:
    params = set(inspect.signature(fn).parameters.keys())
    return {k: v for k, v in kwargs.items() if k in params}


def run_action(fn: Callable, panel, model: PlotModel, story: Story, **kwargs) -> Any:
    meta: ActionMeta = fn._ui_meta

    try:
        result = fn(panel, model, **kwargs)
    except Exception as e:
        narrate_args = {k: str(v) for k, v in kwargs.items()}
        step = Step(
            action_name=fn.__name__,
            args=narrate_args,
            narrate_template=meta.narrate,
            error=e,
        )
        story.record(step)
        _dump_story(story)
        raise

    if isinstance(result, dict):
        cb_kwargs = {**kwargs, **result, "result": result}
        narrate_args = {k: str(v) for k, v in result.items()}
    else:
        cb_kwargs = {**kwargs, "result": result}
        narrate_args = {k: str(v) for k, v in kwargs.items()}
        if result is not None:
            narrate_args["result"] = str(result)

    step = Step(
        action_name=fn.__name__,
        args=narrate_args,
        narrate_template=meta.narrate,
        result=result if not isinstance(result, dict) else None,
    )
    story.record(step)

    settle(meta.settle_timeout_ms)

    model_kwargs = _bind_kwargs(meta.model_update, cb_kwargs)
    meta.model_update(model, **model_kwargs)

    verify_kwargs = _bind_kwargs(meta.verify, cb_kwargs)
    try:
        ok = meta.verify(panel, model, **verify_kwargs)
        if ok is False:
            raise AssertionError(f"Verification failed after {fn.__name__}")
    except Exception as e:
        story.steps[-1].error = e
        _dump_story(story)
        raise

    return result


class StoryRunner:
    def __init__(self, panel):
        self.panel = panel
        self.model = PlotModel()
        self.story = Story()

    def run(self, action_fn: Callable, **kwargs) -> Any:
        return run_action(action_fn, self.panel, self.model, self.story, **kwargs)

    def cleanup(self):
        self.panel.clear()
        settle()


class ActionRegistry:
    def __init__(self):
        self.actions: list[Callable] = []

    def register(self, fn: Callable) -> Callable:
        if not hasattr(fn, "_ui_meta"):
            raise ValueError(f"{fn.__name__} must be decorated with @ui_action")
        self.actions.append(fn)
        return fn

    def build_state_machine(
        self,
        name: str = "UIFuzzTest",
        *,
        max_examples: int = 50,
        stateful_step_count: int = 20,
    ) -> type:
        registry = self
        bundles_map: dict[str, Bundle] = {}

        for action_fn in registry.actions:
            meta: ActionMeta = action_fn._ui_meta
            if meta.target and meta.target not in bundles_map:
                bundles_map[meta.target] = Bundle(meta.target)

        class_dict: dict[str, Any] = {}
        class_dict.update(bundles_map)

        @initialize()
        def _init_model(self):
            self._model = PlotModel()
            self._story = Story()

        class_dict["_init_model"] = _init_model

        def teardown(self):
            if self._story.steps:
                last = self._story.steps[-1]
                if last.error is not None:
                    _dump_story(self._story)
            self.__class__.panel.clear()
            settle()

        class_dict["teardown"] = teardown

        for action_fn in registry.actions:
            meta: ActionMeta = action_fn._ui_meta
            method_name = action_fn.__name__

            rule_kwargs: dict[str, Any] = {}
            if meta.target:
                rule_kwargs["target"] = bundles_map[meta.target]

            for param_name, bundle_name in (meta.bundles or {}).items():
                rule_kwargs[param_name] = bundles_map[bundle_name]
            for param_name, strategy in (meta.strategies or {}).items():
                rule_kwargs[param_name] = strategy

            def make_rule_method(fn, fn_meta):
                def rule_method(self, **kwargs):
                    return run_action(
                        fn, self.__class__.panel,
                        self._model, self._story, **kwargs,
                    )
                return rule_method

            method = make_rule_method(action_fn, meta)
            method.__name__ = method_name

            if meta.precondition:
                user_precond = meta.precondition
                method = precondition(
                    lambda self, _p=user_precond: _p(self._model)
                )(method)

            method = rule(**rule_kwargs)(method)
            class_dict[method_name] = method

        sm_class = type(name, (RuleBasedStateMachine,), class_dict)

        sm_class.TestCase.settings = settings(
            max_examples=max_examples,
            stateful_step_count=stateful_step_count,
            deadline=None,
        )

        return sm_class


def _dump_story(story: Story):
    import os
    from datetime import datetime

    narrative = story.narrative()
    reproducer = story.reproducer()

    print("\n=== FAILURE STORY ===")
    print(narrative)
    print("\n=== REPRODUCER ===")
    print(reproducer)
    print("=== END ===\n")

    reports_dir = os.environ.get("SCIQLOP_PLOTS_TEST_REPORTS", "test-reports")
    os.makedirs(reports_dir, exist_ok=True)
    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    with open(os.path.join(reports_dir, f"story-{timestamp}.txt"), "w") as f:
        f.write(narrative)
    with open(os.path.join(reports_dir, f"story-{timestamp}.py"), "w") as f:
        f.write(reproducer)
```

- [ ] **Step 4: Verify story.py and actions.py parse correctly**

Run: `python -c "import ast; ast.parse(open('tests/fuzzing/story.py').read()); ast.parse(open('tests/fuzzing/actions.py').read()); print('OK')"`
Expected: `OK`

- [ ] **Step 5: Commit**

```bash
git add tests/fuzzing/__init__.py tests/fuzzing/story.py tests/fuzzing/actions.py
git commit -m "feat: port story-driven UI fuzzing framework core from SciQLop"
```

---

### Task 2: Create PlotModel and introspect helpers

**Important API note:** `SciQLopPlotInterface` has no `.name` property — it is a `QFrame` subclass. Graphs (`SciQLopGraphInterface`) have `.name`. Plots are tracked by **index** (their position in `panel.plots()`). The model uses integer indices, not name strings.

**Files:**
- Create: `tests/fuzzing/model.py`
- Create: `tests/fuzzing/introspect.py`

- [ ] **Step 1: Create `tests/fuzzing/model.py`**

SciQLopPlots domain model — tracks plot count, graph counts per plot index, spans, and axis state:

```python
from __future__ import annotations

from dataclasses import dataclass, field


@dataclass
class PlotModel:
    plot_count: int = 0
    graph_counts: dict[int, int] = field(default_factory=dict)
    span_counts: dict[int, int] = field(default_factory=dict)
    log_scales: dict[int, bool] = field(default_factory=dict)

    @property
    def has_plots(self) -> bool:
        return self.plot_count > 0

    @property
    def total_graph_count(self) -> int:
        return sum(self.graph_counts.values())

    @property
    def has_graphs(self) -> bool:
        return self.total_graph_count > 0

    def add_plot(self):
        idx = self.plot_count
        self.plot_count += 1
        self.graph_counts[idx] = 0
        self.span_counts[idx] = 0

    def remove_last_plot(self):
        if self.plot_count <= 0:
            return
        idx = self.plot_count - 1
        self.graph_counts.pop(idx, None)
        self.span_counts.pop(idx, None)
        self.log_scales.pop(idx, None)
        self.plot_count -= 1

    def reset(self):
        self.plot_count = 0
        self.graph_counts.clear()
        self.span_counts.clear()
        self.log_scales.clear()
```

- [ ] **Step 2: Create `tests/fuzzing/introspect.py`**

Pure query functions against live SciQLopPlots widgets:

```python
from __future__ import annotations


def count_plots(panel) -> int:
    return panel.size()


def graph_count_on(panel, plot_index: int) -> int:
    if plot_index >= panel.size():
        return 0
    plot = panel.plot_at(plot_index)
    return len(plot.plottables())


def axis_range(panel, plot_index: int) -> tuple[float, float] | None:
    if plot_index >= panel.size():
        return None
    plot = panel.plot_at(plot_index)
    r = plot.x_axis().range()
    return (r.start(), r.stop())
```

- [ ] **Step 3: Verify both files parse**

Run: `python -c "import ast; ast.parse(open('tests/fuzzing/model.py').read()); ast.parse(open('tests/fuzzing/introspect.py').read()); print('OK')"`
Expected: `OK`

- [ ] **Step 4: Commit**

```bash
git add tests/fuzzing/model.py tests/fuzzing/introspect.py
git commit -m "feat: add PlotModel and introspection helpers for fuzzing"
```

---

### Task 3: Define initial action vocabulary

**Files:**
- Create: `tests/fuzzing/plot_actions.py`
- Create: `tests/fuzzing/graph_actions.py`
- Create: `tests/fuzzing/item_actions.py`
- Create: `tests/fuzzing/axis_actions.py`

- [ ] **Step 1: Create `tests/fuzzing/plot_actions.py`**

Uses `panel.create_plot()` and tracks plots by count (not by name — plots have no `.name` property). The `add_plot` action returns the plot index for use by other actions.

```python
from __future__ import annotations

from tests.fuzzing.actions import ui_action, ActionRegistry
from tests.fuzzing.introspect import count_plots

registry = ActionRegistry()


@registry.register
@ui_action(
    target="plot_indices",
    narrate="Added plot at index {result}",
    model_update=lambda model: model.add_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def add_plot(panel, model):
    from SciQLopPlots import PlotType
    panel.create_plot(plot_type=PlotType.TimeSeries)
    return panel.size() - 1


@registry.register
@ui_action(
    precondition=lambda model: model.has_plots,
    narrate="Removed last plot (was at index {result})",
    model_update=lambda model: model.remove_last_plot(),
    verify=lambda panel, model: count_plots(panel) == model.plot_count,
)
def remove_last_plot(panel, model):
    idx = panel.size() - 1
    plot = panel.plot_at(idx)
    panel.remove_plot(plot)
    return idx
```

- [ ] **Step 2: Create `tests/fuzzing/graph_actions.py`**

Actions use `plot_indices` bundle from `plot_actions` to select which plot to operate on. Access the plot via `panel.plot_at(plot_index)`.

```python
from __future__ import annotations

import numpy as np
from PySide6.QtGui import QColorConstants

from tests.fuzzing.actions import ui_action
from tests.fuzzing.introspect import graph_count_on


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added line graph to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: (
        graph_count_on(panel, plot_index) == model.graph_counts.get(plot_index, 0)
    ),
)
def add_line_graph(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    x = np.linspace(0, 10, 200, dtype=np.float64)
    y = np.sin(x + np.random.uniform(0, 2 * np.pi)).astype(np.float64)
    plot.plot(x, y, labels=["line"], colors=[QColorConstants.Red])


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added colormap to plot {plot_index}",
    model_update=lambda model, plot_index: model.graph_counts.__setitem__(
        plot_index, model.graph_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model, plot_index: (
        graph_count_on(panel, plot_index) == model.graph_counts.get(plot_index, 0)
    ),
    settle_timeout_ms=100,
)
def add_colormap(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    x = np.linspace(0, 10, 50, dtype=np.float64)
    y = np.linspace(0, 5, 30, dtype=np.float64)
    z = np.random.rand(30, 50).astype(np.float64)
    plot.plot(x, y, z, name="cmap")


@ui_action(
    precondition=lambda model: model.has_graphs,
    bundles={"plot_index": "plot_indices"},
    narrate="Set new data on first graph of plot {plot_index}",
    model_update=lambda model: None,
    verify=lambda panel, model: True,
)
def set_graph_data(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    plottables = plot.plottables()
    if plottables:
        x = np.linspace(0, 10, 200, dtype=np.float64)
        y = np.sin(x + np.random.uniform(0, 2 * np.pi)).astype(np.float64)
        plottables[0].set_data(x, y)
```

- [ ] **Step 3: Create `tests/fuzzing/item_actions.py`**

```python
from __future__ import annotations

from PySide6.QtGui import QColor

from tests.fuzzing.actions import ui_action


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Added vertical span to plot {plot_index}",
    model_update=lambda model, plot_index: model.span_counts.__setitem__(
        plot_index, model.span_counts.get(plot_index, 0) + 1
    ),
    verify=lambda panel, model: True,
)
def add_vertical_span(panel, model, plot_index):
    from SciQLopPlots import SciQLopVerticalSpan, SciQLopPlotRange

    plot = panel.plot_at(plot_index)
    r = plot.x_axis().range()
    center = r.center()
    span_range = SciQLopPlotRange(center - r.size() * 0.1, center + r.size() * 0.1)
    SciQLopVerticalSpan(
        plot, span_range,
        QColor(100, 100, 200, 80),
        read_only=False, visible=True,
        tool_tip="test span",
    )
```

- [ ] **Step 4: Create `tests/fuzzing/axis_actions.py`**

```python
from __future__ import annotations

from hypothesis import strategies as st

from tests.fuzzing.actions import ui_action
from tests.fuzzing.introspect import axis_range


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    strategies={
        "start": st.floats(min_value=-1e6, max_value=1e6, allow_nan=False),
        "stop": st.floats(min_value=-1e6, max_value=1e6, allow_nan=False),
    },
    narrate="Set axis range on plot {plot_index} to ({start}, {stop})",
    model_update=lambda model: None,
    verify=lambda panel, model, plot_index, start, stop: _range_close(
        axis_range(panel, plot_index),
        (min(start, stop), max(start, stop)),
    ),
    settle_timeout_ms=100,
)
def set_axis_range(panel, model, plot_index, start, stop):
    from SciQLopPlots import SciQLopPlotRange
    plot = panel.plot_at(plot_index)
    plot.x_axis().set_range(SciQLopPlotRange(start, stop))
    return {"plot_index": plot_index, "start": start, "stop": stop}


@ui_action(
    precondition=lambda model: model.has_plots,
    bundles={"plot_index": "plot_indices"},
    narrate="Toggled log scale on plot {plot_index} y-axis",
    model_update=lambda model, plot_index: model.log_scales.__setitem__(
        plot_index, not model.log_scales.get(plot_index, False)
    ),
    verify=lambda panel, model: True,
)
def toggle_y_log_scale(panel, model, plot_index):
    plot = panel.plot_at(plot_index)
    current = plot.y_axis().log()
    plot.y_axis().set_log(not current)


def _range_close(actual, expected, tol=1.0):
    if actual is None:
        return True
    return abs(actual[0] - expected[0]) < tol and abs(actual[1] - expected[1]) < tol
```

- [ ] **Step 5: Verify all action files parse**

Run: `python -c "import ast; [ast.parse(open(f'tests/fuzzing/{f}').read()) for f in ['plot_actions.py','graph_actions.py','item_actions.py','axis_actions.py']]; print('OK')"`
Expected: `OK`

- [ ] **Step 6: Commit**

```bash
git add tests/fuzzing/plot_actions.py tests/fuzzing/graph_actions.py tests/fuzzing/item_actions.py tests/fuzzing/axis_actions.py
git commit -m "feat: define initial UI action vocabulary for plot fuzzing"
```

---

### Task 4: Create conftest, workflow tests, and fuzzer test

**Files:**
- Create: `tests/fuzzing/conftest.py`
- Create: `tests/fuzzing/test_workflows.py`
- Create: `tests/fuzzing/test_fuzzing.py`

- [ ] **Step 1: Create `tests/fuzzing/conftest.py`**

Note: `fuzzing_panel` is module-scoped so `test_fuzzing.py` can use it in a module-scoped `fuzzer_class` fixture. The `story_runner` fixture creates a fresh panel per-test for workflow tests.

```python
import os
import pytest

from tests.fuzzing.actions import StoryRunner


@pytest.fixture(scope="session", autouse=True)
def fuzzing_reports_dir():
    reports_dir = os.environ.get("SCIQLOP_PLOTS_TEST_REPORTS", "test-reports")
    os.makedirs(reports_dir, exist_ok=True)


@pytest.fixture(scope="module")
def fuzzing_panel(qtbot):
    """Module-scoped panel for Hypothesis fuzzer (reused across examples)."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    return panel


@pytest.fixture
def story_panel(qtbot):
    """Function-scoped panel for scripted story tests (fresh each test)."""
    from SciQLopPlots import SciQLopMultiPlotPanel
    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(panel)
    return panel


@pytest.fixture
def story_runner(story_panel):
    runner = StoryRunner(story_panel)
    yield runner
    runner.cleanup()
```

- [ ] **Step 2: Create `tests/fuzzing/test_workflows.py`**

Scripted story tests exercising common user workflows. Actions return plot indices (integers), not names.

```python
"""Scripted story tests — human-readable workflows with automatic narrative on failure."""
from tests.fuzzing.plot_actions import add_plot, remove_last_plot
from tests.fuzzing.graph_actions import add_line_graph, add_colormap, set_graph_data
from tests.fuzzing.item_actions import add_vertical_span


def test_create_plot_and_add_line(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    assert story_runner.model.graph_counts[plot_idx] == 1


def test_create_plot_add_colormap(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_colormap, plot_index=plot_idx)
    assert story_runner.model.graph_counts[plot_idx] == 1


def test_create_remove_plot(story_runner):
    story_runner.run(add_plot)
    story_runner.run(remove_last_plot)
    assert story_runner.model.plot_count == 0


def test_multiple_plots_with_graphs(story_runner):
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=p0)
    story_runner.run(add_line_graph, plot_index=p1)
    story_runner.run(add_colormap, plot_index=p0)
    assert story_runner.model.plot_count == 2
    assert story_runner.model.graph_counts[p0] == 2
    assert story_runner.model.graph_counts[p1] == 1


def test_add_span_to_plot(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    story_runner.run(add_vertical_span, plot_index=plot_idx)
    assert story_runner.model.span_counts[plot_idx] == 1


def test_set_data_on_graph(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    story_runner.run(set_graph_data, plot_index=plot_idx)


def test_add_and_remove_multiple_plots(story_runner):
    for _ in range(5):
        story_runner.run(add_plot)
    for _ in range(3):
        story_runner.run(remove_last_plot)
    assert story_runner.model.plot_count == 2
```

- [ ] **Step 3: Create `tests/fuzzing/test_fuzzing.py`**

Creates a fresh `ActionRegistry` combining all actions (avoids mutating `plot_actions.registry`).

```python
"""Hypothesis-based stateful fuzzing of SciQLopPlots UI."""
import pytest
from hypothesis.stateful import run_state_machine_as_test

from tests.fuzzing.actions import ActionRegistry
from tests.fuzzing.plot_actions import add_plot, remove_last_plot
from tests.fuzzing.graph_actions import add_line_graph, add_colormap, set_graph_data
from tests.fuzzing.item_actions import add_vertical_span
from tests.fuzzing.axis_actions import set_axis_range, toggle_y_log_scale

# Build a combined registry for the fuzzer — does not mutate any module-level registry
fuzzer_registry = ActionRegistry()
for action in [
    add_plot, remove_last_plot,
    add_line_graph, add_colormap, set_graph_data,
    add_vertical_span,
    set_axis_range, toggle_y_log_scale,
]:
    fuzzer_registry.register(action)

SciQLopPlotsFuzzer = fuzzer_registry.build_state_machine(
    name="SciQLopPlotsFuzzer",
    max_examples=10,
    stateful_step_count=10,
)


@pytest.fixture(scope="module")
def fuzzer_class(fuzzing_panel, qtbot):
    SciQLopPlotsFuzzer.panel = fuzzing_panel
    SciQLopPlotsFuzzer.qtbot = qtbot
    yield SciQLopPlotsFuzzer


def test_ui_fuzzing(fuzzer_class):
    run_state_machine_as_test(fuzzer_class)
```

- [ ] **Step 4: Run the workflow tests**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/fuzzing/test_workflows.py -v --timeout=60`
Expected: All tests PASS (may need to adjust introspect helpers based on actual API — see "API adjustment" note below)

- [ ] **Step 5: Run the fuzzer test**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/fuzzing/test_fuzzing.py -v --timeout=120`
Expected: PASS (Hypothesis explores 10 random sequences of up to 10 actions)

- [ ] **Step 6: Commit**

```bash
git add tests/fuzzing/conftest.py tests/fuzzing/test_workflows.py tests/fuzzing/test_fuzzing.py
git commit -m "feat: add scripted workflow tests and Hypothesis fuzzer for plots"
```

**API reference (verified from integration tests):**
- `panel.size()` — returns number of plots in panel
- `panel.plot_at(index)` — returns `SciQLopPlotInterface` at given index
- `panel.plots()` — returns list of all plots
- `panel.create_plot(plot_type=PlotType.TimeSeries)` — creates and adds a new plot
- `panel.add_plot(plot)` / `panel.remove_plot(plot)` — add/remove existing plot
- `panel.clear()` — remove all plots
- `panel.plot(x, y, ...)` — high-level API, returns `(plot, graph)` tuple
- `plot.plottables()` — returns list of `SciQLopGraphInterface` on the plot
- `plot.x_axis().range()` / `plot.x_axis().set_range(SciQLopPlotRange(...))` — axis manipulation
- `plot.y_axis().log()` / `plot.y_axis().set_log(bool)` — log scale
- `graph.name` — graph name (available on `SciQLopGraphInterface`, NOT on plots)
- `graph.set_data(x, y)` — update graph data

---

## Chunk 2: Demo Cleanup & Modernization

### Task 5: Create shared infrastructure (common.py fallback + demo_data.py)

**Files:**
- Modify: `tests/manual-tests/common.py`
- Create: `tests/manual-tests/demo_data.py`

- [ ] **Step 1: Add qtconsole fallback to `common.py`**

Replace the entire file content with:

```python
from PySide6.QtWidgets import (
    QMainWindow, QApplication, QTabWidget, QDockWidget,
)
from PySide6.QtCore import Qt
import sys
import os
import platform

from SciQLopPlots import PropertiesPanel

try:
    from qtconsole.rich_jupyter_widget import RichJupyterWidget
    from qtconsole.inprocess import QtInProcessKernelManager
    HAS_QTCONSOLE = True
except ImportError:
    HAS_QTCONSOLE = False

os.environ['QT_API'] = 'PySide6'

if platform.system() == 'Linux':
    os.environ['QT_QPA_PLATFORM'] = os.environ.get("QT_QPA_PLATFORM", 'xcb')


def fix_name(name):
    return name.replace(" ", "_").replace(":", "_").replace("/", "_")


class Tabs(QTabWidget):
    def __init__(self, parent):
        QTabWidget.__init__(self, parent)
        self.setMouseTracking(True)
        self.setTabPosition(QTabWidget.TabPosition.North)
        self.setTabShape(QTabWidget.TabShape.Rounded)
        self.setMovable(True)

    def add_tab(self, widget, title):
        self.addTab(widget, title)
        widget.setObjectName(title)


class MainWindow(QMainWindow):
    def __init__(self):
        QMainWindow.__init__(self)
        self.setMouseTracking(True)
        self.axisRects = []
        self.graphs = []
        if HAS_QTCONSOLE:
            self._setup_kernel()
        self._setup_ui()
        self.setGeometry(400, 250, 542, 390)
        self.setWindowTitle("SciQLopPlots Test")
        self.add_to_kernel_namespace('main_window', self)

    def add_tab(self, widget, title):
        self.tabs.add_tab(widget, title)
        self.add_to_kernel_namespace(fix_name(title), widget)

    def _setup_kernel(self):
        self.kernel_manager = QtInProcessKernelManager()
        self.kernel_manager.start_kernel(show_banner=False)
        self.kernel = self.kernel_manager.kernel
        self.kernel.gui = 'qt'
        self.kernel_client = self.kernel_manager.client()
        self.kernel_client.start_channels()

    def add_to_kernel_namespace(self, name, obj):
        if HAS_QTCONSOLE and hasattr(self, 'kernel'):
            self.kernel.shell.push({name: obj})

    def _setup_ui(self):
        self.tabs = Tabs(self)
        self.setCentralWidget(self.tabs)

        if HAS_QTCONSOLE:
            self.ipython_widget = RichJupyterWidget(self)
            dock = QDockWidget("IPython Console", self)
            dock.setWidget(self.ipython_widget)
            self.addDockWidget(Qt.BottomDockWidgetArea, dock)
            self.ipython_widget.kernel_manager = self.kernel_manager
            self.ipython_widget.kernel_client = self.kernel_client

        self.properties_panel = PropertiesPanel(self)
        dock = QDockWidget("Properties panel", self)
        dock.setWidget(self.properties_panel)
        self.addDockWidget(Qt.RightDockWidgetArea, dock)
```

- [ ] **Step 2: Create `tests/manual-tests/demo_data.py`**

Shared data generators used across multiple demos:

```python
"""Shared data generators for demo examples."""
import numpy as np
from datetime import datetime


def sine_multicomponent(n: int = 300_000, time_axis: bool = False):
    """3-component sine wave data (X, Y, Z) at different frequencies."""
    if time_axis:
        start = datetime.now().timestamp()
        x = np.arange(n, dtype=np.float64) + start
    else:
        x = np.arange(n, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


def butterfly_curve(n: int = 5000):
    """Parametric butterfly curve."""
    t = np.linspace(0, 12 * np.pi, n)
    x = np.sin(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    y = np.cos(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    return x, y


def colormap_signal(nx: int = 300_000, ny: int = 64, time_axis: bool = False):
    """Colormap with vertical modulation, noise, and horizontal sweep."""
    x = np.arange(nx, dtype=np.float64)
    if time_axis:
        x += datetime.now().timestamp()
    y = np.logspace(1, 4, ny)

    v_mod = np.tile(np.cos(np.arange(ny) * 6.28 / (ny * 4)), nx).reshape(nx, ny)
    noise = np.random.rand(nx * ny).reshape(nx, ny)
    h_mod = np.cos(np.arange(nx * ny, dtype=np.float64) * 6.28 / (nx / 3.1)).reshape(nx, ny)
    sig = np.cos(np.arange(nx * ny, dtype=np.float64) * 6.28 / (nx * 10)).reshape(nx, ny)
    z = (v_mod * sig * h_mod + noise)
    z = (z * sig) / (z + 1)
    return x, y, z


def uniform_colormap(nx: int = 200, ny: int = 200):
    """Simple 2D cosine pattern for uniform colormap demos."""
    x = np.arange(nx) * 8.0 / nx - 4.0
    y = np.arange(ny) * 8.0 / ny - 4.0
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.cos(np.sqrt((xx + 2) ** 2 + yy ** 2))
    return x, y, z


def two_frequency_signal(lower: float, upper: float, freq1: float = 5.0, freq2: float = 13.0):
    """Sum of two sine waves — good for FFT demos."""
    n = max(int((upper - lower) * 100), 64)
    x = np.linspace(lower, upper, n)
    y = np.sin(2 * np.pi * freq1 * x) + 0.5 * np.sin(2 * np.pi * freq2 * x)
    return x, y


def callable_data_producer(generator=sine_multicomponent):
    """Wraps a generator into a callable(start, stop) for lazy-loading demos."""
    def producer(start, stop):
        n = max(int(stop - start), 100)
        return generator(n, time_axis=True)
    return producer
```

- [ ] **Step 3: Commit**

```bash
git add tests/manual-tests/common.py tests/manual-tests/demo_data.py
git commit -m "feat: add qtconsole fallback and shared demo data generators"
```

---

### Task 6: Create basics/ demos (modernized)

**Files:**
- Create: `tests/manual-tests/basics/__init__.py`
- Create: `tests/manual-tests/basics/line_graph.py`
- Create: `tests/manual-tests/basics/colormap.py`
- Create: `tests/manual-tests/basics/vertical_spans.py`
- Create: `tests/manual-tests/basics/parametric_curve.py`

- [ ] **Step 1: Create `tests/manual-tests/basics/__init__.py`** (empty)

- [ ] **Step 2: Create `tests/manual-tests/basics/line_graph.py`**

Modernized version of `SciQLopGraph/main.py` — uses high-level API instead of `addSciQLopGraph()`:

```python
"""Basic line graph with 3 components and lazy data loading.

Showcases:
- panel.plot() with callable data producer
- Multi-component line graph (X, Y, Z)
- Automatic resampling on zoom/pan
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt

from SciQLopPlots import SciQLopMultiPlotPanel, PlotType


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Line Graph Demo")

panel.plot(
    make_data,
    labels=["X", "Y", "Z"],
    colors=[QColorConstants.Red, QColorConstants.Blue, QColorConstants.Green],
    plot_type=PlotType.TimeSeries,
)

panel.show()
panel.resize(800, 500)
sys.exit(app.exec())
```

- [ ] **Step 3: Create `tests/manual-tests/basics/colormap.py`**

Simplified from `SciQLopColorMap/main.py`:

```python
"""Colormap examples: uniform, 2D-Y non-uniform, non-uniform X-axis.

Showcases:
- panel.plot(x, y, z) for colormaps
- Log-scale Y axis
- Non-uniform axis spacing
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import Qt

from SciQLopPlots import SciQLopMultiPlotPanel


def uniform_colormap(nx=200, ny=200):
    x = np.arange(nx) * 8.0 / nx - 4.0
    y = np.arange(ny) * 8.0 / ny - 4.0
    xx, yy = np.meshgrid(x, y, indexing="ij")
    z = np.cos(np.sqrt((xx + 2) ** 2 + yy ** 2))
    return x, y, z


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=True)
panel.setWindowTitle("Colormap Demo")

# Uniform grid
panel.plot(*uniform_colormap(200, 200))

# Higher resolution
panel.plot(*uniform_colormap(500, 300))

panel.show()
panel.resize(800, 600)
sys.exit(app.exec())
```

- [ ] **Step 4: Create `tests/manual-tests/basics/vertical_spans.py`**

```python
"""Interactive vertical spans with tooltips.

Showcases:
- SciQLopVerticalSpan creation and interaction
- Read-only vs editable spans
- Span tooltips and colors
- Reactive pipeline: span.on.range >> tooltip_updater >> span.on.tooltip
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColor, QColorConstants
from PySide6.QtCore import Qt

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopVerticalSpan, SciQLopPlotRange, PlotType,
)


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.sin(x / 100) * np.cos(x / 10)
    return x, y


def range_to_tooltip(event):
    r = event.value
    return f"[{r.start():.1f}, {r.stop():.1f}] (size: {r.size():.1f})"


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Vertical Spans Demo")

plot, graph = panel.plot(
    make_data,
    labels=["signal"],
    colors=[QColorConstants.DarkBlue],
    plot_type=PlotType.TimeSeries,
)

colors = [
    QColor(200, 100, 100, 80),
    QColor(100, 200, 100, 80),
    QColor(100, 100, 200, 80),
]

r = plot.x_axis().range()
for i, color in enumerate(colors):
    offset = r.size() * (0.2 + i * 0.25)
    span = SciQLopVerticalSpan(
        plot,
        SciQLopPlotRange(r.start() + offset, r.start() + offset + r.size() * 0.1),
        color,
        read_only=(i == 0),
        visible=True,
        tool_tip=f"Span {i+1}" + (" (read-only)" if i == 0 else " (drag me!)"),
    )
    if i > 0:
        span.on.range >> range_to_tooltip >> span.on.tooltip

panel.show()
panel.resize(800, 500)
sys.exit(app.exec())
```

- [ ] **Step 5: Create `tests/manual-tests/basics/parametric_curve.py`**

```python
"""Parametric butterfly curve.

Showcases:
- GraphType.ParametricCurve for non-time-series data
- Decorative items: PixmapItem, EllipseItem
"""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants, QPen, QBrush, QPixmap
from PySide6.QtCore import Qt, QRectF

from SciQLopPlots import (
    SciQLopPlot, GraphType, SciQLopPixmapItem, SciQLopEllipseItem, Coordinates,
)


def butterfly_curve(n=5000):
    t = np.linspace(0, 12 * np.pi, n)
    x = np.sin(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    y = np.cos(t) * (np.exp(np.cos(t)) - 2 * np.cos(4 * t) - np.sin(t / 12) ** 5)
    return x, y


app = QApplication(sys.argv)

plot = SciQLopPlot()
plot.setWindowTitle("Parametric Curve Demo")

x, y = butterfly_curve()
plot.x_axis().set_range(min(x), max(x))
plot.y_axis().set_range(min(y), max(y))

plot.plot(
    x, y,
    labels=["butterfly"],
    colors=[QColorConstants.DarkMagenta],
    graph_type=GraphType.ParametricCurve,
)

# Decorative items at center
SciQLopEllipseItem(
    plot, QRectF(-0.5, -0.5, 1, 1),
    QPen(Qt.red), QBrush(QColorConstants.Transparent),
    True, Coordinates.Data,
)

plot.show()
plot.resize(600, 600)
sys.exit(app.exec())
```

- [ ] **Step 6: Commit**

```bash
git add tests/manual-tests/basics/
git commit -m "feat: add modernized basics demos (line, colormap, spans, curve)"
```

---

### Task 7: Create advanced/ and stress/ demos

**Files:**
- Create: `tests/manual-tests/advanced/__init__.py`
- Create: `tests/manual-tests/advanced/stacked_plots.py`
- Move+simplify: `tests/manual-tests/advanced/pipeline.py` (from `Pipeline/main.py`)
- Move+simplify: `tests/manual-tests/advanced/drag_and_drop.py` (from `DragNDrop/main.py`)
- Move: `tests/manual-tests/advanced/products.py` (from `Products/main.py`)
- Move: `tests/manual-tests/advanced/projections.py` (from `Projections/main.py`)
- Create: `tests/manual-tests/stress/__init__.py`
- Move+simplify: `tests/manual-tests/stress/stress_model.py` (from `StressModel/main.py`)
- Move: `tests/manual-tests/stress/profiling.py` (from `ForProfiling/main.py`)
- Create: `tests/manual-tests/real_data/__init__.py`
- Move: `tests/manual-tests/real_data/micro_sciqlop.py` (from `MicroSciQLop/main.py`)

- [ ] **Step 1: Create `__init__.py` files for all subdirectories** (empty)

- [ ] **Step 2: Create `tests/manual-tests/advanced/stacked_plots.py`**

New demo extracted from `All/main.py`'s StackedPlots, using modern API:

```python
"""Stacked time-series plots with colormap and multi-plot vertical span.

Showcases:
- SciQLopMultiPlotPanel with multiple stacked plots
- Time axis synchronization
- MultiPlotsVerticalSpan across all plots
- Colormap on the last plot
"""
import sys
import numpy as np
from datetime import datetime
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants, QColor
from PySide6.QtCore import Qt

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopTimeSeriesPlot,
    MultiPlotsVerticalSpan, SciQLopPlotRange, PlotType,
)


def make_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([
        np.cos(x / 6) * np.cos(x) * 100,
        np.cos(x / 60) * np.cos(x / 6) * 1300,
        np.cos(x / 600) * np.cos(x / 60) * 17000,
    ])
    return x, y


app = QApplication(sys.argv)

panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
panel.setWindowTitle("Stacked Plots Demo")

for i in range(4):
    panel.plot(
        make_data,
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

panel.show()
panel.resize(800, 700)
sys.exit(app.exec())
```

- [ ] **Step 3: Move and simplify remaining demos**

For each of these, the move consists of:
1. Copy the file to its new location
2. Remove the `sys.path.append` / `from common import MainWindow` boilerplate
3. Make it standalone (no MainWindow dependency — use direct panel/plot)
4. Add a docstring explaining what it showcases
5. Add optional-dependency guards where needed

Files to move:
- `Pipeline/main.py` → `advanced/pipeline.py` (already clean, just copy)
- `DragNDrop/main.py` → `advanced/drag_and_drop.py` (remove MainWindow dependency)
- `Products/main.py` → `advanced/products.py` (copy + adjust imports)
- `Projections/main.py` → `advanced/projections.py` (copy, keep speasy guard)
- `StressModel/main.py` → `stress/stress_model.py` (make standalone, remove MainWindow)
- `ForProfiling/main.py` → `stress/profiling.py` (copy, keep optional dep guards)
- `MicroSciQLop/main.py` → `real_data/micro_sciqlop.py` (copy, keep speasy guard)

- [ ] **Step 4: Commit**

```bash
git add tests/manual-tests/advanced/ tests/manual-tests/stress/ tests/manual-tests/real_data/
git commit -m "feat: restructure demos into basics/advanced/stress/real_data categories"
```

---

### Task 8: Create gallery launcher and update meson.build

**Files:**
- Create: `tests/manual-tests/gallery.py`
- Modify: `tests/manual-tests/meson.build`

- [ ] **Step 1: Create `tests/manual-tests/gallery.py`**

A simple launcher that lists all demos with descriptions:

```python
"""Demo gallery — lists all available demos and launches the selected one."""
import subprocess
import sys
import os

DEMOS = [
    ("Line Graph", "basics/line_graph.py", "Basic 3-component line plot with lazy data loading"),
    ("Colormap", "basics/colormap.py", "Uniform and non-uniform colormap examples"),
    ("Vertical Spans", "basics/vertical_spans.py", "Interactive spans with reactive tooltips"),
    ("Parametric Curve", "basics/parametric_curve.py", "Butterfly curve with decorative items"),
    ("Pipeline", "advanced/pipeline.py", "Reactive pipeline: span→tooltip, data→FFT"),
    ("Stacked Plots", "advanced/stacked_plots.py", "Multi-plot panel with time sync and multi-span"),
    ("Drag & Drop", "advanced/drag_and_drop.py", "Custom drag-drop callback for plot creation"),
    ("Products", "advanced/products.py", "Hierarchical product tree browser"),
    ("Projections", "advanced/projections.py", "3D parametric curves (requires speasy)"),
    ("Stress Test", "stress/stress_model.py", "Random create/destroy panels for stability"),
    ("Profiling", "stress/profiling.py", "Real-time data + audio spectrogram"),
    ("MMS Data", "real_data/micro_sciqlop.py", "MMS spacecraft data (requires speasy)"),
]


def main():
    base = os.path.dirname(os.path.abspath(__file__))

    print("SciQLopPlots Demo Gallery")
    print("=" * 40)
    for i, (name, _, desc) in enumerate(DEMOS, 1):
        print(f"  {i:2d}. {name:20s} — {desc}")
    print(f"  {'q':>3s}. Quit")
    print()

    while True:
        choice = input("Select demo number (or 'q' to quit): ").strip()
        if choice.lower() == "q":
            break
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(DEMOS):
                name, path, _ = DEMOS[idx]
                full_path = os.path.join(base, path)
                print(f"\nLaunching: {name}")
                subprocess.run([sys.executable, full_path])
            else:
                print(f"Invalid choice. Enter 1-{len(DEMOS)}.")
        except ValueError:
            print("Enter a number or 'q'.")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Update `tests/manual-tests/meson.build`**

Replace the old examples list with the new paths:

```meson
examples = [
    {'name': 'LineGraph', 'source': meson.current_source_dir() + '/basics/line_graph.py'},
    {'name': 'Colormap', 'source': meson.current_source_dir() + '/basics/colormap.py'},
    {'name': 'VerticalSpans', 'source': meson.current_source_dir() + '/basics/vertical_spans.py'},
    {'name': 'ParametricCurve', 'source': meson.current_source_dir() + '/basics/parametric_curve.py'},
    {'name': 'Pipeline', 'source': meson.current_source_dir() + '/advanced/pipeline.py'},
    {'name': 'StackedPlots', 'source': meson.current_source_dir() + '/advanced/stacked_plots.py'},
    {'name': 'DragNDrop', 'source': meson.current_source_dir() + '/advanced/drag_and_drop.py'},
    {'name': 'Products', 'source': meson.current_source_dir() + '/advanced/products.py'},
    {'name': 'StressModel', 'source': meson.current_source_dir() + '/stress/stress_model.py'},
]

foreach example : examples
    test('examples_' + example['name'],
        python3,
        args: [example['source']],
        env: ['PYTHONPATH=' + meson.project_build_root()],
        suite: 'manual',
    )
endforeach

sources = []
foreach example : examples
    sources += example['source']
endforeach

if target_machine.system() != 'windows'
    library('SimplePlot_fake', [],
            extra_files: sources)
endif
```

Note: `Projections` and `MicroSciQLop` are excluded from meson test because they require speasy. `Profiling` is excluded because it requires sounddevice.

- [ ] **Step 3: Remove old demo directories**

Delete the now-migrated old directories:
```
tests/manual-tests/SciQLopGraph/
tests/manual-tests/SciQLopColorMap/
tests/manual-tests/SciQLopVerticalSpan/
tests/manual-tests/StackedGraphs/
tests/manual-tests/DrawingAPI/
tests/manual-tests/All/
tests/manual-tests/GigaPoints/
tests/manual-tests/ForProfiling/
tests/manual-tests/StressModel/
tests/manual-tests/Pipeline/
tests/manual-tests/DragNDrop/
tests/manual-tests/MicroSciQLop/
tests/manual-tests/Products/
tests/manual-tests/Projections/
```

- [ ] **Step 4: Verify meson build still works**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && meson setup build --wipe && meson compile -C build`
Expected: Build succeeds

- [ ] **Step 5: Commit**

```bash
git add tests/manual-tests/gallery.py tests/manual-tests/meson.build
git rm -r tests/manual-tests/SciQLopGraph tests/manual-tests/SciQLopColorMap tests/manual-tests/SciQLopVerticalSpan tests/manual-tests/StackedGraphs tests/manual-tests/DrawingAPI tests/manual-tests/All tests/manual-tests/GigaPoints tests/manual-tests/ForProfiling tests/manual-tests/StressModel tests/manual-tests/Pipeline tests/manual-tests/DragNDrop tests/manual-tests/MicroSciQLop tests/manual-tests/Products tests/manual-tests/Projections
git commit -m "feat: add demo gallery launcher, update meson.build, remove old demos"
```

---

## Chunk 3: Automated Stress Test

### Task 9: Convert StressModel into a real integration test

**Files:**
- Create: `tests/integration/test_stress.py`

- [ ] **Step 1: Create `tests/integration/test_stress.py`**

Turn the manual StressModel demo into an automated test with assertions:

```python
"""Automated stress tests for plot panel lifecycle.

Converts the manual StressModel demo into real pytest tests that verify
no crashes during rapid create/destroy cycles.
"""
import gc
import random

import numpy as np
import pytest
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QColorConstants
from PySide6.QtCore import Qt


def process_events():
    QApplication.processEvents()


def force_gc():
    gc.collect()
    process_events()
    gc.collect()


@pytest.fixture
def panel(qtbot):
    from SciQLopPlots import SciQLopMultiPlotPanel
    p = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    qtbot.addWidget(p)
    return p


def _add_plot_with_graph(panel):
    plot, graph = panel.plot(
        np.linspace(0, 10, 200, dtype=np.float64),
        np.sin(np.linspace(0, 10, 200)).astype(np.float64),
        labels=["test"],
        colors=[QColorConstants.Red],
    )
    return plot


def test_rapid_create_destroy_50_cycles(panel):
    """50 rapid create/destroy cycles without crash."""
    for _ in range(50):
        plot = _add_plot_with_graph(panel)
        process_events()
        panel.remove_plot(plot)
        process_events()
    force_gc()
    assert panel.size() == 0


def test_random_create_remove_100_ops(panel):
    """100 random create/remove operations weighted toward creation."""
    plots = []
    for _ in range(100):
        if not plots or random.random() < 0.7:
            plot = _add_plot_with_graph(panel)
            plots.append(plot)
        else:
            idx = random.randint(0, len(plots) - 1)
            panel.remove_plot(plots.pop(idx))
        process_events()

    # Clean up remaining
    for p in plots:
        panel.remove_plot(p)
    force_gc()
    assert panel.size() == 0


def test_create_many_then_clear(panel):
    """Create 20 plots, then clear all at once."""
    for _ in range(20):
        _add_plot_with_graph(panel)
        process_events()
    assert panel.size() == 20

    panel.clear()
    process_events()
    force_gc()
    assert panel.size() == 0
```

- [ ] **Step 2: Run the stress tests**

Run: `cd /var/home/jeandet/Documents/prog/SciQLopPlots && python -m pytest tests/integration/test_stress.py -v --timeout=120`
Expected: All 3 tests PASS

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_stress.py
git commit -m "feat: add automated stress tests for plot panel lifecycle"
```

---

## Dependency Notes

- **hypothesis** must be added as a test dependency (for fuzzing). Add to the test requirements or `pyproject.toml` dev dependencies.
- **pytest-timeout** is recommended for the stress and fuzzing tests.
- The fuzzing framework (`tests/fuzzing/actions.py`, `story.py`) is generic and reusable — consider extracting to a shared package if both SciQLop and SciQLopPlots use it.

## Execution Order

Tasks 1-4 (fuzzing framework) are sequential and should be done first.
Tasks 5-8 (demo cleanup) are mostly independent of Tasks 1-4 and can be parallelized.
Task 9 (stress test) is independent and can be done in parallel with anything.

Recommended parallel execution:
- **Agent A:** Tasks 1 → 2 → 3 → 4 (fuzzing framework)
- **Agent B:** Tasks 5 → 6 → 7 → 8 (demo cleanup)
- **Agent C:** Task 9 (stress test)
