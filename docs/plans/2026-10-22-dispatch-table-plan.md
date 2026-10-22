# One plot() Dispatch Table Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One Python table drives `plot()` for every host, the plot and panel factories take the same keywords, and every silent dispatch failure becomes a result or a clear error.

**Architecture:** C++ first: a dedicated `sqp::unsupported_call` exception mapped to `NotImplementedError` at the binding boundary, the panel's `(nullptr, nullptr)` Projections case turned into `ValueError`, and the factory signatures aligned (panel `marker`, native `name`, waterfall bindings). Then the Python table (`SciQLopPlots/_dispatch.py`) replaces the if-chain in `plot()`, and two tests (a dispatch walk and a per-overload signature parity check) keep the table and the C++ in step.

**Tech Stack:** C++20, Qt 6.11.2, Shiboken6 typesystem (bindings.xml), Python 3.11+, pytest + pytest-qt.

**Spec:** `docs/plans/2026-10-22-dispatch-table-design.md` (revision 3).

## Global Constraints

- Repo /home/jeandet/Documents/prog/SciQLopPlots, branch `feat/fills-histogram` (continue on it; nothing is pushed). Commit with explicit paths; the untracked `qt.ini` is not ours. Commit messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Never push.
- Build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`.
- After any bindings.xml or bound-header change: `rm build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.cpp build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/*_wrapper.h && .venv/bin/meson setup --reconfigure build-venv`, then build. Stale wrappers segfault. Python files under `SciQLopPlots/` are plain copies in `build-venv/SciQLopPlots/`: a build re-copies them.
- Tests from /tmp: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`. Full suite: `tests/unit tests/fuzzing tests/resampler tests/tracing` in one run, then `tests/integration` in two halves (sorted file list), each under `timeout 590`. Never `tests/`.
- One build or test command at a time, foreground, waited out; pass timeout 600000 to the shell tool; never `run_in_background`, never `&`; `-j 4`. Nothing left running at the end. `cat` is aliased to bat: use `command cat`.
- When a signature in a header changes, the matching `<modify-function signature="...">` string in bindings.xml must change with it: shiboken only warns about a signature it can't find, and the renames/injections then silently stop applying. Grep the build output for "signature" warnings after each bindings change.
- New parameters go after `metaData`, `name` last (spec "C++ changes"). `set_name` keeps working.
- Unsupported (kind, host) raises `NotImplementedError` whose message names the host class; tests assert the type and the class name only.
- Python style (user's rules): small functions, data over code, no comment-decorated blocks, comments only for "why".

## Review Focus

1. **A projection plot used by SciQLop's scenes** (`proj.plot(callable, name=, labels=[n]*3, graph_type=ParametricCurve)` and `nd.plot([t, X, Y, Z], graph_type=ParametricCurve)`): must keep working after the table routes projections. Test in Task 4.
2. **`plot()` called with `labels=None, colors=None, name=None`** (SciQLop forwards Nones): Nones are dropped, not passed to factories that don't take them. Test in Task 4.
3. **A panel `plot(x, y, graph_type=ParametricCurve, plot_type=TimeSeries)`** (the dropped-`plot_type` bug): creates a time-series subplot. Test in Task 4.
4. **The 4-array parametric form `plot(x, y, ["a"], ["#f00"], graph_type=ParametricCurve)`**: still a curve with that label and colour. Test in Task 4.
5. **Histogram keywords on the panel path** (`panel.plot(values, graph_type=Histogram, bins=5, range=(0, 1))`): the `post` options still reach the Python wrapper. Test in Task 4.

---

### Task 1: Unsupported calls raise NotImplementedError at the binding boundary

**Files:**
- Modify: `include/SciQLopPlots/Debug.hpp` (or a new `include/SciQLopPlots/UnsupportedCall.hpp` if Debug.hpp is the wrong home; keep it tiny)
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp:59-116` (the seven `plot_impl` stubs)
- Modify: `SciQLopPlots/bindings/bindings.xml` (new template next to `returning_raises_valueerror` ~line 80; inject it on every factory entry of `SciQLopPlotInterface` ~lines 411-590 and of the collection interface ~lines 989-1230)
- Test: `tests/integration/test_dispatch_errors.py` (create)

**Interfaces:**
- Produces: `namespace sqp { struct unsupported_call : std::logic_error { using std::logic_error::logic_error; }; }`; bindings template `returning_raises_python_errors` (`$TYPE` replaced per entry) mapping `sqp::unsupported_call` → `NotImplementedError`, `std::invalid_argument` → `ValueError`, any other `std::exception` → `RuntimeError`.

- [ ] **Step 1: Failing tests**

```python
"""Dispatch errors (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import SciQLopNDProjectionPlot, SciQLopPlot

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
- [ ] **Step 3: Implement**
  - The seven base `plot_impl` stubs throw `sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()))`, where `unsupported_message` builds the same "<what> is not supported by <class>" text `report_unsupported_call` prints (src/UnsupportedCall.cpp:52-69; extract a shared `std::string unsupported_message(const char* function, const char* concrete_class)` there and reuse it, demangling as it does).
  - bindings.xml template:

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

  - Inject it (`<inject-code class="target" position="beginning"><insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopGraphInterface*"/></insert-template></inject-code>`, or `SciQLopColorMapInterface*`, or the panel's `QPair<SciQLopPlotInterface*,SciQLopGraphInterface*>` / `QPair<...,SciQLopColorMapInterface*>`) on every factory entry that can reach a stub: plot interface line ×2, scatter ×2, parametric_curve ×3, colormap ×2, histogram2d ×2, waterfall ×2 (entries added in Task 3 — add the injection there); collection interface line ×2, scatter ×2, parametric_curve ×2, projection ×2, colormap ×2, histogram2d ×2. Existing `<modify-argument>`/ownership children stay. Where an entry already carries `returning_raises_valueerror` (histogram/bars on SciQLopPlot and the panel), switch it to the new template.
  - Add `sqp::unsupported_call`'s header to `SciQLopPlots/bindings/bindings.h` so the generated wrappers see it.
- [ ] **Step 4: Run** — PASS. Then the stale-wrapper cleanup and full suite (Global Constraints); grep the build log for "signature" warnings: none.
- [ ] **Step 5: Commit** — `git commit -m "feat(bindings): unsupported factory calls raise NotImplementedError"` with the touched files.

---

### Task 2: A panel never returns (None, None) for Projections

**Files:**
- Modify: `src/SciQLopMultiPlotPanel.cpp:403-457` (the buffer `plot_impl` overloads' `PlotType` switches)
- Test: `tests/integration/test_dispatch_errors.py`

**Interfaces:**
- Consumes: Task 1's template on the collection interface's `line`/`scatter`/`parametric_curve`/`colormap`/`histogram2d` buffer entries (so `std::invalid_argument` → `ValueError`).

- [ ] **Step 1: Failing tests**

```python
from SciQLopPlots import PlotType, SciQLopMultiPlotPanel


@pytest.mark.parametrize("factory", ["line", "scatter", "parametric_curve"])
def test_two_arrays_on_a_projections_panel_raise(qtbot, panel, factory):
    with pytest.raises(ValueError, match="list of arrays or a callback"):
        getattr(panel, factory)(X, X, plot_type=PlotType.Projections)
    assert panel.size() == 0
```

(`panel` fixture: tests/integration/conftest.py. If `panel.size()` isn't the bound name, use the panel's plot-count accessor that exists — check conftest/other tests.)

- [ ] **Step 2: Run** — expected FAIL: returns `(None, None)`, no exception.
- [ ] **Step 3: Implement** — in each buffer `plot_impl`'s `case ::PlotType::Projections:` (and the `default:` that falls through to `{nullptr, nullptr}`), `throw std::invalid_argument("projections take a list of arrays or a callback, not x and y arrays");`. Unknown `PlotType` values: same throw with "unknown plot type".
- [ ] **Step 4: Run** — PASS; then the projection tests (`pytest tests/integration -k projection`) and the full suite.
- [ ] **Step 5: Commit** — `git commit -m "fix(panel): two arrays with plot_type=Projections raise instead of returning (None, None)"`.

---

### Task 3: Factories take the same keywords on plots and panels

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (~317-407: line, scatter, parametric_curve, waterfall — add `QString name = QString()` last)
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopPlotCollection.hpp` (~276-430: panel line, parametric_curve, waterfall gain `::GraphMarkerShape marker` after `metaData`; line, scatter, parametric_curve, waterfall gain `QString name` last) and `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp` overrides
- Modify: the `plot_impl` plumbing so `marker` and `name` reach the graph (the panel's `__plot` already forwards `marker` to the subplot — SciQLopMultiPlotPanel.hpp ~87; `name` is applied with `graph->set_name(name)` when non-empty, after creation, in one helper used by every factory)
- Modify: `SciQLopPlots/bindings/bindings.xml`: update the signature strings of every touched entry; add `waterfall` entries on the plot interface (buffer + callable) and the collection interface (buffer + callable) with the same renames as `line` (`metaData`→`metadata`, labels, colors, marker, plot_type, index, sync_with, name) and Task 1's injection
- Test: `tests/integration/test_dispatch_parity_kwargs.py` (create)

**Interfaces:**
- Produces: `plot.line/scatter/parametric_curve/waterfall(..., name=)`; `panel.line/parametric_curve/waterfall(..., marker=)`; `waterfall(..., metadata=)`.

- [ ] **Step 1: Failing tests**

```python
"""The same factory takes the same keywords on a plot and on a panel."""
import numpy as np
import pytest

from SciQLopPlots import GraphMarkerShape, PlotType

X = np.linspace(0, 1, 10)
Z = np.random.default_rng(0).random((10, 4))


def _graph(result):
    return result[1] if isinstance(result, tuple) else result


@pytest.mark.parametrize("factory", ["line", "parametric_curve", "waterfall"])
def test_panel_factories_take_marker(qtbot, panel, factory):
    y = Z if factory == "waterfall" else X
    _graph(getattr(panel, factory)(X, y, marker=GraphMarkerShape.Circle))


@pytest.mark.parametrize("host", ["plot", "panel"])
@pytest.mark.parametrize("factory", ["line", "scatter", "parametric_curve", "waterfall"])
def test_factories_take_name(qtbot, request, host, factory):
    target = request.getfixturevalue(host)
    y = Z if factory == "waterfall" else X
    assert _graph(getattr(target, factory)(X, y, name="n")).name == "n"


@pytest.mark.parametrize("host", ["plot", "panel"])
def test_waterfall_takes_metadata_not_metaData(qtbot, request, host):
    target = request.getfixturevalue(host)
    _graph(target.waterfall(X, Z, metadata={"k": 1}))
    with pytest.raises(TypeError):
        target.waterfall(X, Z, metaData={"k": 1})


def test_positional_prefix_unchanged(qtbot, plot):
    assert _graph(plot.line(X, X, ["a"])).components()[0].name() == "a"
```

(Check how a graph's name is read in Python — property `name` or `name()`; tests/integration/test_plot_name_kwarg.py shows it. Adjust the assertions to that, keeping the behaviour.)

- [ ] **Step 2: Run** — expected FAIL (unsupported keyword `marker` / `name` / `metadata`).
- [ ] **Step 3: Implement** per Files; parameter order exactly as in Global Constraints. Stale-wrapper cleanup, build, grep the build log for "signature" warnings (must be none).
- [ ] **Step 4: Run** — PASS; full suite (test_api_hygiene's `metaData` checks for line must still hold; the waterfall rename is the only intended break).
- [ ] **Step 5: Commit** — `git commit -m "feat(factories): marker on panel factories, native name, waterfall bindings (metaData -> metadata)"`.

---

### Task 4: The dispatch table drives plot()

**Files:**
- Create: `SciQLopPlots/_dispatch.py`
- Modify: `SciQLopPlots/__init__.py:81-235` (replace `plot_func`/`plot`/`_plot`, `_KWARGS_OF`, `_reject_foreign_kwargs`, `_TWO_ARGUMENT_HISTOGRAM` by the table; keep `_apply_waterfall_kwargs`, `_projection_buffers`, `_graph_of`, the histogram wrapper); add `_dispatch.py` to the package's file list in `SciQLopPlots/meson.build` (check how `widget_plot.py`/`pipeline.py` are listed)
- Test: `tests/integration/test_dispatch_table.py` (create)

**Interfaces:**
- Consumes: Tasks 1-3 (NotImplementedError/ValueError from C++, aligned keywords).
- Produces: `_dispatch.Shape(name, nargs, options, post=frozenset())`, `_dispatch.Kind(graph_type, shapes, factory, native, returns)`, `_dispatch.KINDS: dict[str, Kind]`, `_dispatch.COMMON = frozenset({"labels", "colors", "metadata"})`, `_dispatch.resolve(host, args, graph_type, kwargs) -> tuple[Kind, Shape]`, and the new `plot()`.

- [ ] **Step 1: Failing tests**

```python
"""plot() is driven by one table (docs/plans/2026-10-22-dispatch-table-design.md)."""
import numpy as np
import pytest

from SciQLopPlots import (GraphType, PlotType, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopTimeSeriesPlot)

X = np.linspace(0, 1, 10)


def test_a_list_makes_a_projection_without_graph_type(qtbot, panel):
    p, g = panel.plot([X, X, X], plot_type=PlotType.Projections)
    assert isinstance(p, SciQLopNDProjectionPlot)


def test_a_list_with_another_graph_type_raises(qtbot, panel):
    with pytest.raises(TypeError, match="list"):
        panel.plot([X, X, X], graph_type=GraphType.Waterfall)


def test_plot_type_on_a_plot_is_a_type_error(qtbot, plot):
    with pytest.raises(TypeError, match="plot_type"):
        plot.plot(X, X, plot_type=PlotType.TimeSeries)


def test_plot_type_is_not_dropped_on_the_curve_path(qtbot, panel):
    p, _ = panel.plot(X, X, graph_type=GraphType.ParametricCurve, plot_type=PlotType.TimeSeries)
    assert isinstance(p, SciQLopTimeSeriesPlot)


def test_unsupported_kind_names_the_host(qtbot, ts_plot):
    with pytest.raises(NotImplementedError, match="SciQLopTimeSeriesPlot"):
        ts_plot.plot(X, graph_type=GraphType.Histogram)


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
    p, h = panel.plot(np.arange(10.0), graph_type=GraphType.Histogram, bins=5, range=(0.0, 10.0))
    assert h.bins() == 5


def test_projection_plot_scene_calls(qtbot):
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    nd.plot([X, X, X, X], graph_type=GraphType.ParametricCurve)
    nd.plot(lambda a, b: [X, X, X, X], name="model", labels=["m"] * 3,
            graph_type=GraphType.ParametricCurve)


HOSTS = {"plot": SciQLopPlot, "ts_plot": SciQLopTimeSeriesPlot, "panel": SciQLopMultiPlotPanel}


def _args_for(shape):
    if shape.name == "list":
        return ([X, X, X],)
    if shape.name == "callback":
        return (lambda a, b: (X, X),)
    return {1: (X,), 2: (X, X), 3: (X, X, np.zeros((10, 10))),
            4: (X, X, ["a"], ["#f00"])}[shape.nargs]


@pytest.mark.parametrize("host", list(HOSTS))
def test_dispatch_walk(qtbot, request, host):
    from SciQLopPlots._dispatch import KINDS
    target = request.getfixturevalue(host)
    for name, kind in KINDS.items():
        for shape in kind.shapes:
            args = _args_for(shape)
            if HOSTS[host] not in kind.factory:
                with pytest.raises(NotImplementedError, match=HOSTS[host].__name__):
                    target.plot(*args, graph_type=kind.graph_type)
                continue
            kw = {"plot_type": PlotType.Projections} if (host == "panel" and name == "projection") else {}
            result = target.plot(*args, graph_type=kind.graph_type, **kw)
            made = result[1] if host == "panel" else result
            assert isinstance(made, kind.returns), (name, shape)
```

(The walk asserts `kind.returns`, which must be the bound Python classes — `SciQLopGraphInterface`, `SciQLopColorMapInterface`, `SciQLopHistogram`. Callback-shaped colour maps need a 3-buffer callback: make `_args_for` return a kind-appropriate callback by reading `kind` — adjust the helper so every row gets data its factory accepts.)

- [ ] **Step 2: Run** — expected FAIL (`ModuleNotFoundError: SciQLopPlots._dispatch`, then the behaviour tests).
- [ ] **Step 3: Implement** `_dispatch.py` — the table exactly as the spec's table and per-shape options; the core:

```python
from dataclasses import dataclass, field
from typing import Mapping

COMMON = frozenset({"labels", "colors", "metadata"})
PANEL_ONLY = frozenset({"plot_type", "index"})


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
    returns: type


def shape_of(args):
    if len(args) == 1 and callable(args[0]):
        return "callback", None
    if len(args) == 1 and isinstance(args[0], (list, tuple)):
        return "list", None
    return "arrays", len(args)


DEFAULT_KIND = {"arrays": {2: "line", 3: "colormap", 1: None, 4: "parametric_curve"},
                "callback": "line", "list": "projection"}


def _kind_name(shape, nargs, graph_type, kwargs):
    if shape == "list":
        if graph_type not in (None, GraphType.ParametricCurve):
            raise TypeError(f"a list of arrays makes a projection, not a {graph_type.name}")
        return "projection"
    if shape == "arrays" and nargs == 3:
        _warn_if_three_arrays_with(graph_type)          # today's DeprecationWarning, kept until 1.0
        return "colormap"
    if shape == "callback" and kwargs.get("plot_type") == PlotType.Projections:
        return "projection"
    if graph_type is not None:
        return _kind_by_graph_type(graph_type, shape, nargs)   # raises the TypeError of rule 1
    default = DEFAULT_KIND[shape] if shape != "arrays" else DEFAULT_KIND["arrays"][nargs]
    if default is None:
        raise TypeError(_accepted_forms(None))
    return default


def _check_host(kind_name, kind, host_cls):
    if host_class(host_cls) not in kind.factory:
        raise NotImplementedError(f"{kind_name} is not supported by {host_cls.__name__}")


def _check_keywords(kind_name, kind, shape, host_cls, kwargs):
    allowed = COMMON | kind.native | shape.options | shape.post | {"name"}
    if host_class(host_cls) is SciQLopMultiPlotPanel:
        allowed |= PANEL_ONLY
    stray = sorted(set(kwargs) - allowed)
    if stray:
        raise TypeError(_foreign_keyword_message(stray, kind_name))  # names the owning kind if any


def resolve(host_cls, args, graph_type, kwargs):
    """The (kind name, Kind, Shape) for this call, or the error the user should see."""
    shape, nargs = shape_of(args)
    name = _kind_name(shape, nargs, graph_type, kwargs)
    kind = KINDS[name]
    shape_row = _shape_row(kind, shape, nargs)                        # TypeError if the kind lacks it
    _check_host(name, kind, host_cls)
    _check_keywords(name, kind, shape_row, host_cls, kwargs)
    return name, kind, shape_row
```

  `_kind_by_graph_type` maps `graph_type` → kind name (`Histogram` with two arrays raises the existing `plot.bars` message); `_accepted_forms(graph_type)` lists the shapes the table accepts for it; `_foreign_keyword_message` finds the kind whose options contain the keyword and words it like today's `_reject_foreign_kwargs` ("x_bins kwarg(s) only apply to GraphType.Histogram2D, got graph_type=..."). `plot_type` on a plot fails `_check_keywords` (it isn't in `allowed` there) with a `TypeError` naming `plot_type`. `plot()` becomes:

```python
def plot(self, *args, graph_type=None, **kwargs):
    kwargs = {k: v for k, v in kwargs.items() if v is not None}
    _, kind, shape = _dispatch.resolve(type(self), args, graph_type, kwargs)
    post = {k: kwargs.pop(k) for k in shape.post if k in kwargs}
    name = kwargs.pop("name", None) if "name" not in kind.native else None
    factory = getattr(self, kind.factory[_dispatch.host_class(type(self))])
    result = factory(*_dispatch.call_args(kind, shape, args), **_dispatch.factory_kwargs(self, kind, shape, kwargs))
    _dispatch.apply_post(kind, result, post)
    if name is not None:
        _graph_of(result).set_name(name)
    return result
```

  `host_class` maps a subclass to its table host (`SciQLopTimeSeriesPlot` before `SciQLopPlot` — use `isinstance` against the hosts in the order: time series, projection plot, panel, plot). `call_args` turns `(t, values2d)` / `(t, d0, d1…)` into the projection list via `_projection_buffers`. `factory_kwargs` drops `plot_type` for `projection` and passes `plot_type`/`index` only to panel factories that take them. `apply_post` runs `_apply_waterfall_kwargs` (+ the existing rescale of the new plot) for waterfall — waterfall's `offsets`/`normalize`/`gain` are its `post` options. The histogram row's factory is the Python `histogram` wrapper (it already consumes `range`/`log_bins`/`normalization`/`style`), so those are the histogram shapes' factory `options` and its `post` is empty; Task 5 compares the histogram row against the wrapper's Python signature, not the C++ one. The 3-array `DeprecationWarning` stays, raised in `_kind_for_shape`.
- [ ] **Step 4: Run** — PASS; then every existing dispatch test file listed in the spec (test_api_footguns, test_p1_bugs, test_waterfall, test_histogram2d, test_histogram2d_log_bins, test_histogram1d, test_plot_name_kwarg, test_scatter_kwargs, the projection tests, test_api_hygiene, test_fills_errorbars) unchanged; then the full suite.
- [ ] **Step 5: Commit** — `git commit -m "feat(plot): one dispatch table drives plot() on every host"`.

---

### Task 5: Signature parity between the table and the C++ factories

**Files:**
- Test: `tests/integration/test_dispatch_parity.py` (create)

**Interfaces:**
- Consumes: `_dispatch.KINDS`, `COMMON`, `PANEL_ONLY`.

- [ ] **Step 1: Write the test** — for every kind, shape and host whose factory is a C++ method (for the histogram row the factory is the Python wrapper: compare with `inspect.signature` of the wrapper instead), parse the overload list shiboken puts in the method's `__doc__` (print one `SciQLopPlot.line.__doc__` first to see the exact format; each overload is one line with `name: type = default` parameters), pick the overload for the shape (callback = the one with a `GetDataPyCallable`/`callable` first parameter; list = `QList`/`Sequence` first parameter; arrays = the rest, matching `nargs` positional buffers), and assert its keyword names equal `COMMON | kind.native | shape.options | (PANEL_ONLY ∩ the panel factory's parameters on panels)` minus the positional data parameters. Report every mismatch in one assertion message.
- [ ] **Step 2: Mutation check** — temporarily remove `marker` from one panel factory's table options (not the C++), run: FAIL naming that factory; restore: PASS.
- [ ] **Step 3: Commit** — `git commit -m "test(plot): the dispatch table and the C++ factory signatures stay in step"`.

---

### Task 6: Docs, SciQLop check, review

- [ ] **Step 1:** `docs/user-guide.md`: one paragraph in the plot-types intro on how `plot()` picks the kind (shape + `graph_type`, lists make projections, three arrays make a colour map), and a Troubleshooting line for `NotImplementedError: ... not supported by <class>` and for `waterfall(metaData=)` → `metadata=`. Snippets must run (the guide test executes them).
- [ ] **Step 2:** `docs/api-review-2026-10-20.md` Progress: item 5.4 done (one row), noting the `metaData` break on waterfall.
- [ ] **Step 3:** Full SciQLopPlots suite; then SciQLop's suite and the four plugins against `build-venv`: from /home/jeandet/Documents/prog/SciQLop, `xvfb-run -a -s "-screen 0 2560x1440x24" env PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv timeout 590 .venv/bin/python -m pytest <files> -q -p no:cacheprovider -o addopts="" --no-xvfb`, chunks under 590 s; plugins (sciqlop_msa, sciqlop_radio, sciqlop_sismo; cdf_workbench can't run) with `PYTHONPATH=<build-venv>:<plugin dir>`. Known pre-existing failure: test_onboarding_integration.py drag-step test. End with `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` empty.
- [ ] **Step 4:** Commit docs.
