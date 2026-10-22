# One plot() Dispatch Table Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One Python table drives `plot()` for every host, the plot and panel factories take the same keywords, and every silent dispatch failure becomes a result or a clear error.

**Architecture:** C++ first, signatures before injections: align the factory signatures (panel `marker`, native `name`, waterfall bindings), then add a dedicated `sqp::unsupported_call` exception mapped to `NotImplementedError` by one bindings template on every factory entry, then turn the panel's `(nullptr, nullptr)` paths into `ValueError`. Then the Python table (`SciQLopPlots/_dispatch.py`) replaces the if-chain in `plot()`, and a per-overload signature parity test keeps table and C++ in step.

**Tech Stack:** C++20, Qt 6.11.2, Shiboken6 typesystem (bindings.xml), Python 3.11+, pytest + pytest-qt.

**Spec:** `docs/plans/2026-10-22-dispatch-table-design.md` (revision 3). Revision 2 of this plan, after an opencode review.

## Global Constraints

- Repo /home/jeandet/Documents/prog/SciQLopPlots, branch `feat/fills-histogram`; nothing is pushed. Commit with explicit paths; the untracked `qt.ini` is not ours. Messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Never push.
- Build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`.
- After any bindings.xml or bound-header change: `rm build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.cpp build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.h && .venv/bin/meson setup --reconfigure build-venv`, then build. Stale wrappers segfault. Python files under `SciQLopPlots/` are plain copies in `build-venv/SciQLopPlots/`; a build re-copies them.
- When a header signature changes, the matching `<modify-function signature="...">` string in bindings.xml changes with it. Shiboken only warns on a signature it can't find, and the renames and injections then silently stop applying: after every bindings change, grep the build log for "signature" warnings (must be none) and check the generated wrapper of one touched entry.
- Tests from /tmp: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`. Full suite: `tests/unit tests/fuzzing tests/resampler tests/tracing` in one run, then `tests/integration` in two halves (sorted file list), each under `timeout 590`. Never `tests/`.
- One build or test command at a time, foreground, waited out; timeout 600000 on the shell tool; never `run_in_background`, never `&`; `-j 4`. Nothing left running. `cat` is aliased to bat: use `command cat`.
- Shiboken raises **AttributeError** ("unsupported keyword") for a keyword a factory lacks; the table's own keyword errors are **TypeError**. A graph's name is read with `graph.objectName()` or `graph.components()[0].name()` (`name` is a method).
- Pair returns are spelled `std::pair<SciQLopPlotInterface*,X*>` in bindings.xml (the only spelling the converters know; see the `histogram`/`bars` entries ~1268-1283).
- New parameters go after `metaData`, `name` last. `set_name` keeps working.
- Unsupported (kind, host) raises `NotImplementedError` "<kind> is not supported by <class>".
- Style: small functions, data over code, comments only for "why".

## Review Focus

1. **SciQLop's projection calls** — `impl.plot(list, plot_type=Projections, graph_type=ParametricCurve, name=...)` (SciQLop/user_api/plot/_panel.py:390-392) and `nd.plot([t, X, Y, Z], graph_type=ParametricCurve)` / `proj.plot(callable, name=, labels=, graph_type=ParametricCurve)` (scene_plot.py:68, 82, 212): keep working; `name` reaches the graph through `set_name`. Tests in Task 4.
2. **`plot()` with `labels=None, colors=None, name=None`** (SciQLop forwards Nones): dropped, never passed on. Test in Task 4.
3. **`panel.plot(x, y, graph_type=ParametricCurve, plot_type=TimeSeries)`**: a time-series subplot (the dropped-`plot_type` bug). Test in Task 4.
4. **`plot(x, y, ["a"], ["#f00"], graph_type=ParametricCurve)`**: still a curve labelled "a". Test in Task 4.
5. **A flat list of values** `plot([1.0, 2.0, 3.0], graph_type=Histogram)`: a histogram, not a projection error. Test in Task 4.

---

### Task 1: Factories take the same keywords on plots and panels

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (~317-407): `line`, `scatter`, `parametric_curve` (all three overloads), `waterfall` gain `QString name = QString()` last.
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp` (~276-430) and the overrides in `SciQLopMultiPlotPanel.hpp`: panel `line`, `parametric_curve`, `waterfall` gain `::GraphMarkerShape marker` after `metaData`; `line`, `scatter`, `parametric_curve`, `waterfall` gain `QString name` last.
- `marker` needs no plumbing: the panel's `plot_impl(x, y, ...)` already carries it and `__plot` forwards it (SciQLopMultiPlotPanel.hpp ~92-105, ~184-190); pass the new parameter through. `name` needs one helper applying `set_name` when the name is non-empty and the graph non-null, with two forms: graph pointer (plots) and the pair's second element (panels).
- Modify: `SciQLopPlots/bindings/bindings.xml`: update every touched entry's signature string; add `waterfall` entries (buffer + callable) on the plot interface and the collection interface. Spell the renames per entry from that entry's real parameters: plot buffer `(x, y, labels, colors, marker, metadata, name)`; plot callable adds `sync_with`; panel buffer `(x, y, labels, colors, plot_type, index, metadata, marker, name)` (no `sync_with`); panel callable adds `sync_with`. A rename for a parameter the entry doesn't have is a warning and is dropped.
- Test: `tests/integration/test_dispatch_parity_kwargs.py` (create)

**Interfaces:**
- Produces: `plot.line/scatter/parametric_curve/waterfall(..., name=)`, `panel.line/parametric_curve/waterfall(..., marker=)`, `waterfall(..., metadata=)`; the new signature strings Task 2 injects into.

- [ ] **Step 1: Failing tests**

```python
"""The same factory takes the same keywords on a plot and on a panel."""
import numpy as np
import pytest

from SciQLopPlots import GraphMarkerShape

X = np.linspace(0, 1, 10)
Z = np.random.default_rng(0).random((10, 4))


def _graph(result):
    return result[1] if isinstance(result, tuple) else result


def _y(factory):
    return Z if factory == "waterfall" else X


@pytest.mark.parametrize("factory", ["line", "parametric_curve", "waterfall"])
def test_panel_factories_take_marker(qtbot, panel, factory):
    assert _graph(getattr(panel, factory)(X, _y(factory), marker=GraphMarkerShape.Circle)) is not None


@pytest.mark.parametrize("host", ["plot", "panel"])
@pytest.mark.parametrize("factory", ["line", "scatter", "parametric_curve", "waterfall"])
def test_factories_take_name(qtbot, request, host, factory):
    target = request.getfixturevalue(host)
    assert _graph(getattr(target, factory)(X, _y(factory), name="n")).objectName() == "n"


@pytest.mark.parametrize("host", ["plot", "panel"])
def test_waterfall_takes_metadata_not_metaData(qtbot, request, host):
    target = request.getfixturevalue(host)
    assert _graph(target.waterfall(X, Z, metadata={"k": 1})) is not None
    with pytest.raises(AttributeError, match="metaData"):
        target.waterfall(X, Z, metaData={"k": 1})


def test_positional_prefix_unchanged(qtbot, plot):
    assert _graph(plot.line(X, X, ["a"])).components()[0].name() == "a"
```

(`test_positional_prefix_unchanged` is a regression guard, green before the change.)

- [ ] **Step 2: Run** — expected FAIL: `AttributeError: ... unsupported keyword 'marker' / 'name' / 'metadata'`.
- [ ] **Step 3: Implement** per Files. Stale-wrapper cleanup, build, grep for "signature" warnings.
- [ ] **Step 4: Run** — PASS; full suite (test_api_hygiene's `metaData` checks on `line` still hold).
- [ ] **Step 5: Commit** — `git commit -m "feat(factories): marker on panel factories, native name, waterfall bindings (metaData -> metadata)"`.

---

### Task 2: Unsupported calls raise NotImplementedError at the binding boundary

**Files:**
- Modify: `include/SciQLopPlots/Debug.hpp` (it is already included by SciQLopPlotInterface.hpp and so by bindings.h): `namespace sqp { struct unsupported_call : std::runtime_error { using std::runtime_error::runtime_error; }; }` — derived from `runtime_error`, so existing C++ `catch (const std::runtime_error&)` sites keep working.
- Modify: `src/UnsupportedCall.cpp`: extract `std::string unsupported_message(const char* function, const char* concrete_class)` from `report_unsupported_call` (same demangled "<what> is not supported by <class>" text; the demangling `readable()` helper stays in the .cpp) and use it in both. Declare `unsupported_message` in `Debug.hpp` next to `report_unsupported_call`, so the inline stubs in SciQLopPlotInterface.hpp link.
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp:59-116`: the seven `plot_impl` stubs `throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()))`.
- Modify: `SciQLopPlots/bindings/bindings.xml`: a template next to `returning_raises_valueerror` (~line 80), injected into every factory entry that can reach a stub, using the signature strings from Task 1.
- Test: `tests/integration/test_dispatch_errors.py` (create)

**Interfaces:**
- Consumes: Task 1's signature strings.
- Produces: template `returning_raises_python_errors` (`$TYPE` per entry): `sqp::unsupported_call` → `NotImplementedError`, `std::invalid_argument` → `ValueError`, other `std::exception` → `RuntimeError`.

- [ ] **Step 1: Failing tests**

```python
"""Dispatch errors (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot

X = np.linspace(0, 1, 10)


@pytest.mark.parametrize("call", [
    lambda nd: nd.line(X, X),
    lambda nd: nd.colormap(X, X, np.zeros((10, 10))),
    lambda nd: nd.histogram2d(X, X),
])
def test_projection_plot_factories_it_cannot_serve_raise(qtbot, call):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    with pytest.raises(NotImplementedError, match="SciQLopNDProjectionPlot"):
        call(nd)


def test_a_plain_plot_given_a_list_raises(qtbot, plot):
    with pytest.raises(NotImplementedError, match="SciQLopPlot"):
        plot.parametric_curve([X, X, X])
```

- [ ] **Step 2: Run** — expected FAIL: `RuntimeError: Not implemented`.
- [ ] **Step 3: Implement** the exception, the shared message, the stubs, and the template:

```xml
    <!-- Insert with <replace from="$TYPE" to="the return type"/>. -->
    <template name="returning_raises_python_errors">
        try {
            $TYPE result = %CPPSELF.%FUNCTION_NAME(%ARGUMENT_NAMES);
            %PYARG_0 = %CONVERTTOPYTHON[$TYPE](result);
        } catch (const sqp::unsupported_call&amp; e) {
            PyErr_SetString(PyExc_NotImplementedError, e.what());
            return nullptr;
        } catch (const std::invalid_argument&amp; e) {
            PyErr_SetString(PyExc_ValueError, e.what());
            return nullptr;
        } catch (const std::exception&amp; e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return nullptr;
        }
    </template>
```

  Inject it with `<inject-code class="target" position="beginning"><insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="..."/></insert-template></inject-code>` into: plot interface `line` ×2, `scatter` ×2, `parametric_curve` ×3, `colormap` ×2, `histogram2d` ×2, `waterfall` ×2 (`SciQLopGraphInterface*` or `SciQLopColorMapInterface*`); collection interface `line` ×2, `scatter` ×2, `parametric_curve` ×2, `projection` ×2, `colormap` ×2, `histogram2d` ×2, `waterfall` ×2 (`std::pair<SciQLopPlotInterface*,SciQLopGraphInterface*>` or `std::pair<SciQLopPlotInterface*,SciQLopColorMapInterface*>`). Entries that already carry `returning_raises_valueerror` (histogram ×2 and bars on SciQLopPlot and the panel) switch to the new template. Do not touch `add_reference_curve` (it keeps its own template). Existing `<modify-argument>` children stay.
- [ ] **Step 4: Run** — PASS; stale-wrapper cleanup, full suite, "signature" grep empty.
- [ ] **Step 5: Commit** — `git commit -m "feat(bindings): unsupported factory calls raise NotImplementedError"`.

---

### Task 3: A panel never returns (None, None)

**Files:**
- Modify: `src/SciQLopMultiPlotPanel.cpp:403-457` (the `plot_impl(x, y, ...)` and `(x, y, z, ...)` `PlotType` switches) and `:503-546` (`_plot_histogram2d` callers' switches); `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp:155-180` (`_plot<T>`: an unhandled `GraphType` must throw **before** the plot is added to the panel).
- Test: `tests/integration/test_dispatch_errors.py`

**Interfaces:**
- Consumes: Task 2's template on the collection entries (`std::invalid_argument` → `ValueError`).

- [ ] **Step 1: Failing tests**

```python
from SciQLopPlots import PlotType

Z2 = np.zeros((10, 10))


@pytest.mark.parametrize("call", [
    lambda p: p.line(X, X, plot_type=PlotType.Projections),
    lambda p: p.scatter(X, X, plot_type=PlotType.Projections),
    lambda p: p.parametric_curve(X, X, plot_type=PlotType.Projections),
    lambda p: p.waterfall(X, np.zeros((10, 4)), plot_type=PlotType.Projections),
    lambda p: p.colormap(X, X, Z2, plot_type=PlotType.Projections),
    lambda p: p.histogram2d(X, X, plot_type=PlotType.Projections),
])
def test_arrays_on_a_projections_panel_raise(qtbot, panel, call):
    with pytest.raises(ValueError, match="list of arrays or a callback"):
        call(panel)
    assert panel.plot_count() == 0
```

- [ ] **Step 2: Run** — expected FAIL: `(None, None)` returned, no exception.
- [ ] **Step 3: Implement** — `case ::PlotType::Projections:` → `throw std::invalid_argument("projections take a list of arrays or a callback, not x and y arrays")`; the `default:` → `throw std::invalid_argument("unknown plot type")`; `_plot<T>` checks the `GraphType` first and throws `std::invalid_argument("unsupported graph type")` before creating or adding the plot. The `_plot<T>` guard is defensive (no Python path reaches it with an unhandled `GraphType` today): say so in the commit message rather than claiming a user-visible fix.
- [ ] **Step 4: Run** — PASS; `pytest tests/integration -k projection`; full suite.
- [ ] **Step 5: Commit** — `git commit -m "fix(panel): arrays with plot_type=Projections raise instead of returning (None, None)"`.

---

### Task 4: The dispatch table drives plot()

**Files:**
- Create: `SciQLopPlots/_dispatch.py`; add it to the package file list in `SciQLopPlots/meson.build` (~line 57, next to the other .py files).
- Modify: `SciQLopPlots/__init__.py:125-232`: replace `plot_func`/`plot`/`_plot`, `_KWARGS_OF`, `_reject_foreign_kwargs` and the inline `_TWO_ARGUMENT_HISTOGRAM` use by the table; keep `_apply_waterfall_kwargs`, `_projection_buffers`, `_graph_of`, the histogram wrapper. Import order: `_dispatch` imports its classes from `.SciQLopPlotsBindings` (no cycle); `__init__` does `from . import _dispatch` after the bindings block.
- Test: `tests/integration/test_dispatch_table.py` (create)

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: `Shape(name, nargs, options, post=frozenset())`, `Kind(graph_type, shapes, factory, native, panel_keywords, returns, consumed=frozenset())`, `KINDS: dict[str, Kind]`, `COMMON = frozenset({"metadata"})`, `resolve(host_cls, args, graph_type, kwargs) -> tuple[str, Kind, Shape]`, `host_class(cls)`, and the new `plot()`.

The table (factories are method **names**, resolved with `getattr` at call time, so the Python `histogram` wrapper installed later in `__init__` is the one called):

| kind | graph_type | shapes: options (post) | factory per host | native | panel_keywords | returns |
|---|---|---|---|---|---|---|
| line | Line | arrays(2): labels colors marker; callback: labels colors marker sync_with | plot, ts, panel → `line` | name | plot_type index | SciQLopGraphInterface |
| scatter | Scatter | same as line | plot, ts, panel → `scatter` | name | plot_type index | SciQLopGraphInterface |
| parametric_curve | ParametricCurve | arrays(2): labels colors marker; arrays(4): marker (positional x, y, labels, colors); callback: labels colors marker sync_with | plot, ts, panel → `parametric_curve` | name | plot_type index | SciQLopGraphInterface |
| projection | ParametricCurve | list: labels colors; callback: labels colors sync_with | panel → `projection`; nd → `parametric_curve` | (none: name via `set_name`) | index | SciQLopGraphInterface |
| colormap | ColorMap | arrays(3): y_log_scale z_log_scale; callback: y_log_scale z_log_scale sync_with | plot, ts, panel → `colormap` | name | plot_type index | SciQLopColorMapInterface |
| waterfall | Waterfall | arrays(2): labels colors marker (offsets normalize gain); callback: labels colors marker sync_with (offsets normalize gain) | plot, ts, panel → `waterfall` | name | plot_type index | SciQLopGraphInterface |
| histogram2d | Histogram2D | arrays(2): x_bins y_bins x_bins_log y_bins_log; callback: same + sync_with | plot, ts, panel → `histogram2d` | name | plot_type index | SciQLopColorMapInterface |
| histogram | Histogram | arrays(1): bins range log_bins normalization style; callback: same + sync_with | plot, panel → `histogram` | name | index | SciQLopHistogram |

`consumed` (keywords the table reads and never forwards): `{plot_type}` on the projection row only — it picks the kind and the container, and `projection()` has no `plot_type`.

(`COMMON` is only `metadata`: colour maps and histograms take no `labels`/`colors`. Hosts: `SciQLopPlot`, `SciQLopTimeSeriesPlot`, `SciQLopNDProjectionPlot`, `SciQLopMultiPlotPanel`; `host_class` checks `isinstance` in the order time series, projection plot, panel, plot.)

- [ ] **Step 1: Failing tests** (behaviour tests first; the ones marked *guard* are green before the change and stay as regression guards)

```python
"""plot() is driven by one table (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (GraphType, PlotType, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopTimeSeriesPlot)

X = np.linspace(0, 1, 10)


def test_a_list_makes_a_projection_without_graph_type(qtbot, panel):
    p, _ = panel.plot([X, X, X], plot_type=PlotType.Projections)
    assert isinstance(p, SciQLopNDProjectionPlot)


def test_a_list_of_arrays_with_another_graph_type_raises(qtbot, panel):
    with pytest.raises(TypeError, match="projection"):
        panel.plot([X, X, X], graph_type=GraphType.Waterfall)


def test_a_flat_list_of_values_is_not_a_projection(qtbot, plot):
    h = plot.plot([1.0, 2.0, 3.0], graph_type=GraphType.Histogram)
    assert h is not None


def test_plot_type_on_a_plot_is_a_type_error(qtbot, plot):
    with pytest.raises(TypeError, match="plot_type"):
        plot.plot(X, X, plot_type=PlotType.TimeSeries)


def test_plot_type_is_not_dropped_on_the_curve_path(qtbot, panel):
    p, _ = panel.plot(X, X, graph_type=GraphType.ParametricCurve, plot_type=PlotType.TimeSeries)
    assert isinstance(p, SciQLopTimeSeriesPlot)


def test_unsupported_kind_names_the_host_from_the_table(qtbot, ts_plot):
    with pytest.raises(NotImplementedError, match="histogram is not supported by SciQLopTimeSeriesPlot"):
        ts_plot.plot(X, graph_type=GraphType.Histogram)


def test_name_on_a_projections_panel_call(qtbot, panel):
    _, g = panel.plot([X, X, X], plot_type=PlotType.Projections,
                      graph_type=GraphType.ParametricCurve, name="scene")
    assert g.objectName() == "scene"


def test_histogram_keyword_not_valid_for_plot_type_on_a_panel(qtbot, panel):
    with pytest.raises(TypeError, match="plot_type"):
        panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, plot_type=PlotType.BasicXY)


# guards
def test_two_arrays_as_histogram_points_to_bars(qtbot, plot):
    with pytest.raises(TypeError, match="plot.bars"):
        plot.plot(X, X, graph_type=GraphType.Histogram)


def test_foreign_keyword_names_its_owner(qtbot, plot):
    with pytest.raises(TypeError, match="Histogram2D"):
        plot.plot(X, X, x_bins=10)


def test_nones_are_dropped(qtbot, plot):
    plot.plot(X, X, labels=None, colors=None, name=None)


def test_four_array_curve_still_works(qtbot, plot):
    g = plot.plot(X, X, ["a"], ["#f00"], graph_type=GraphType.ParametricCurve)
    assert g.components()[0].name() == "a"


def test_histogram_post_options_on_a_panel(qtbot, panel):
    _, h = panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, bins=5, range=(0.0, 10.0))
    assert h.bins() == 5


def test_projection_plot_scene_calls(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.plot([X, X, X, X], graph_type=GraphType.ParametricCurve)
    nd.plot(lambda a, b: [X, X, X, X], name="model", labels=["m"] * 3,
            graph_type=GraphType.ParametricCurve)


@pytest.fixture
def nd(qtbot):
    p = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(p)
    return p


HOSTS = {"plot": SciQLopPlot, "ts_plot": SciQLopTimeSeriesPlot,
         "panel": SciQLopMultiPlotPanel, "nd": SciQLopNDProjectionPlot}


def _data(kind_name, shape):
    """Arguments the kind's factory accepts for this shape."""
    if shape.name == "list":
        return ([X, X, X, X],)
    if shape.name == "callback":
        batch = {"colormap": (X, X, np.zeros((10, 10))), "projection": [X, X, X, X],
                 "histogram": [X], "waterfall": (X, np.zeros((10, 4)))}.get(kind_name, (X, X))
        return (lambda a, b, batch=batch: batch,)
    return {1: (X,), 2: (X, np.zeros((10, 4)) if kind_name == "waterfall" else X),
            3: (X, X, np.zeros((10, 10))), 4: (X, X, ["a"], ["#f00"])}[shape.nargs]


@pytest.mark.parametrize("host", list(HOSTS))
def test_dispatch_walk(qtbot, request, host):
    from SciQLopPlots._dispatch import KINDS, host_class
    target = request.getfixturevalue(host)
    for name, kind in KINDS.items():
        for shape in kind.shapes:
            args = _data(name, shape)
            kw = {"plot_type": PlotType.Projections} if (host == "panel" and name == "projection") else {}
            if host_class(HOSTS[host]) not in kind.factory:
                with pytest.raises(NotImplementedError, match=HOSTS[host].__name__):
                    target.plot(*args, graph_type=kind.graph_type, **kw)
                continue
            result = target.plot(*args, graph_type=kind.graph_type, **kw)
            made = result[1] if host == "panel" else result
            assert isinstance(made, kind.returns), (host, name, shape)
```

- [ ] **Step 2: Run** — expected FAIL for the non-guard tests (`ModuleNotFoundError: SciQLopPlots._dispatch`, then behaviour).
- [ ] **Step 3: Implement** `_dispatch.py`:

```python
from dataclasses import dataclass
from typing import Mapping

import numpy as np

from .SciQLopPlotsBindings import (GraphType, PlotType, SciQLopColorMapInterface,
                                   SciQLopGraphInterface, SciQLopHistogram, SciQLopMultiPlotPanel,
                                   SciQLopNDProjectionPlot, SciQLopPlot, SciQLopTimeSeriesPlot)

COMMON = frozenset({"metadata"})
HOSTS = (SciQLopTimeSeriesPlot, SciQLopNDProjectionPlot, SciQLopMultiPlotPanel, SciQLopPlot)


@dataclass(frozen=True)
class Shape:
    name: str
    nargs: int | None
    options: frozenset
    post: frozenset = frozenset()


@dataclass(frozen=True)
class Kind:
    graph_type: object
    shapes: tuple
    factory: Mapping
    native: frozenset
    panel_keywords: frozenset
    returns: type
    consumed: frozenset = frozenset()


KINDS = {...}  # the rows of the table above, written out in full


def host_class(cls):
    return next(h for h in HOSTS if issubclass(cls, h))


def _is_list_of_arrays(value):
    return isinstance(value, (list, tuple)) and len(value) > 0 and all(np.ndim(v) >= 1 for v in value)


def shape_of(args):
    if len(args) == 1 and callable(args[0]):
        return "callback", None
    if len(args) == 1 and _is_list_of_arrays(args[0]):
        return "list", None
    return "arrays", len(args)


def _kind_name(shape, nargs, graph_type, kwargs):
    if shape == "list":
        if graph_type not in (None, GraphType.ParametricCurve):
            raise TypeError(f"a list of arrays makes a projection, not a {graph_type.name}")
        return "projection"
    if shape == "arrays" and nargs == 3:
        _warn_if_three_arrays_with(graph_type)          # today's DeprecationWarning, until 1.0
        return "colormap"
    if shape == "callback" and kwargs.get("plot_type") == PlotType.Projections:
        return "projection"
    if graph_type is not None:
        return _kind_by_graph_type(graph_type, shape, nargs)   # 2 arrays + Histogram: the plot.bars message
    if shape == "arrays" and nargs == 4:
        return "parametric_curve"
    if shape in ("arrays", "callback") and nargs in (None, 2):
        return "line"
    raise TypeError(_accepted_forms(graph_type))


def _check_host(name, kind, host_cls):
    if host_class(host_cls) not in kind.factory:
        raise NotImplementedError(f"{name} is not supported by {host_cls.__name__}")


def _check_keywords(name, kind, shape, host_cls, kwargs):
    allowed = COMMON | kind.native | kind.consumed | shape.options | shape.post | {"name"}
    if host_class(host_cls) is SciQLopMultiPlotPanel:
        allowed |= kind.panel_keywords
    stray = sorted(set(kwargs) - allowed)
    if stray:
        raise TypeError(_foreign_keyword_message(stray, name))


def resolve(host_cls, args, graph_type, kwargs):
    shape, nargs = shape_of(args)
    name = _kind_name(shape, nargs, graph_type, kwargs)
    kind = KINDS[name]
    row = _shape_row(name, kind, shape, nargs)           # TypeError if the kind lacks this shape
    _check_host(name, kind, host_cls)
    _check_keywords(name, kind, row, host_cls, kwargs)
    return name, kind, row
```

  `_kind_by_graph_type`, `_accepted_forms`, `_shape_row` and `_warn_if_three_arrays_with` are small helpers; `_foreign_keyword_message` names the kind whose options hold the keyword, in today's `_reject_foreign_kwargs` wording ("x_bins kwarg(s) only apply to GraphType.Histogram2D, got graph_type=..."), else says "<stray> kwarg(s) not accepted by <kind>; accepted: …". Both forms start with the stray names, so `plot_type` on a plot, or on a panel row without it, gives a `TypeError` whose text contains `plot_type`.

  `plot()` in `__init__.py`:

```python
def plot(self, *args, graph_type=None, **kwargs):
    kwargs = {k: v for k, v in kwargs.items() if v is not None}
    name, kind, shape = _dispatch.resolve(type(self), args, graph_type, kwargs)
    post = {k: kwargs.pop(k) for k in shape.post if k in kwargs}
    label = kwargs.pop("name", None) if "name" not in kind.native else None
    for consumed in kind.consumed:
        kwargs.pop(consumed, None)
    factory = getattr(self, kind.factory[_dispatch.host_class(type(self))])
    args = (_projection_buffers(args),) if name == "projection" else args
    result = factory(*args, **kwargs)
    _apply_post(name, self, result, post)
    if label is not None:
        _graph_of(result).set_name(label)
    return result
```

  `_projection_buffers(args)` already accepts the list itself, `(t, d0, d1, …)` and `(t, values2d)`: pass `args` straight to it, never convert twice. `_apply_post` runs `_apply_waterfall_kwargs` plus the existing rescale of the new plot for waterfall (the `rescale_axes` call from today's 2-array branch), and nothing for the others. The 3-array `DeprecationWarning` is raised in `_kind_name`. `plot_type` for `projection` is in the row's `consumed` set: validation accepts it, `_kind_name` reads it, and it is popped before the call. `plot_func` stays as a thin alias, `plot_func(self, callback, graph_type=None, **kw) -> self.plot(callback, graph_type=graph_type, **kw)`, because it is public on the four patched classes. `plot()`'s signature becomes `plot(*args, graph_type=None, **kwargs)`; Task 6 documents it.
- [ ] **Step 4: Run** — PASS; then the existing dispatch test files (test_api_footguns, test_p1_bugs, test_waterfall, test_histogram2d, test_histogram2d_log_bins, test_histogram1d, test_plot_name_kwarg, test_scatter_kwargs, the projection tests, test_api_hygiene, test_fills_errorbars) unchanged; then the full suite.
- [ ] **Step 5: Commit** — `git commit -m "feat(plot): one dispatch table drives plot() on every host"`.

---

### Task 5: Signature parity between the table and the factories

**Files:**
- Test: `tests/integration/test_dispatch_parity.py` (create)

**Interfaces:**
- Consumes: `_dispatch.KINDS`, `COMMON`, `HOSTS`, `host_class`.

- [ ] **Step 1: Write the test** — for every kind, shape and host in the row's `factory`: enumerate the overloads with `shibokensupport.signature.get_signature(method)` (returns a list for overloaded methods), the same helper `tests/integration/test_scatter_kwargs.py:12,25-27` uses, with its `@pytest.mark.filterwarnings("ignore:pyside_type_init:RuntimeWarning")` guard (`:39-40`). Pick the overload for the shape (callback = first parameter a callable; list = first parameter a list/sequence; arrays = `nargs` leading buffer parameters, the 4-array curve counting labels and colors as positional). Assert its keyword names (minus the positional data parameters) equal `COMMON | kind.native | shape.options` plus, on the panel, `kind.panel_keywords`. `shape.post` is excluded: those keywords never reach the factory. For the histogram row the factory is the Python wrapper, which hides `metadata`/`index` in `**kwargs`: compare against the wrapped C++ method (kept in `_cpp_histogram` in `__init__.py`) and add `{range, log_bins, normalization, style}` as the documented Python-only extras. Report every mismatch in one assertion message.
- [ ] **Step 2: Mutation check** — remove `marker` from the panel `line` row's options in the table: the test FAILS naming it; restore: PASS. Then remove `name` from `waterfall.native`: FAIL; restore.
- [ ] **Step 3: Commit** — `git commit -m "test(plot): the dispatch table and the factory signatures stay in step"`.

---

### Task 6: Docs, SciQLop check

- [ ] **Step 1:** `docs/user-guide.md`: show `plot(*args, graph_type=None, **kwargs)` where the guide documents `plot()`'s signature, and in the plot-types intro, one paragraph on how `plot()` picks the kind (shape + `graph_type`; a list of arrays makes a projection; three arrays make a colour map). Troubleshooting: `NotImplementedError: <kind> is not supported by <class>`, and `waterfall(metaData=)` → `metadata=`. Snippets must run (the guide test executes them).
- [ ] **Step 2:** `docs/api-review-2026-10-20.md` Progress: item 5.4 done, noting the waterfall `metaData` break.
- [ ] **Step 3:** Full SciQLopPlots suite. Then SciQLop's suite and the plugins against `build-venv`: from /home/jeandet/Documents/prog/SciQLop, `xvfb-run -a -s "-screen 0 2560x1440x24" env PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv timeout 590 .venv/bin/python -m pytest <files> -q -p no:cacheprovider -o addopts="" --no-xvfb`, chunks under 590 s; plugins sciqlop_msa, sciqlop_radio, sciqlop_sismo with `PYTHONPATH=<build-venv>:<plugin dir>` (cdf_workbench can't run: old pycdfpp). Known pre-existing failure: test_onboarding_integration.py drag-step test. End with `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` empty.
- [ ] **Step 4:** Commit docs.
