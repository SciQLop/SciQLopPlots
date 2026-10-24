# Item factories, one `remove`, one ownership model: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Items come from plot and panel factories and belong to what draws them. One `remove` deletes anything a plot or panel draws. The old item constructors keep working, with a deprecation warning.

**Architecture:**
- Every item constructor gets its plot as its QObject parent, through one helper that also rejects a null plot.
- The factories are virtuals on `SciQLopPlotInterface` that raise NotImplementedError by default. `SciQLopPlot` implements them with `new Item(this, ...)`, and the bindings hand the result to C++ (`define-ownership`).
- `remove` is four overloads on `SciQLopPlotInterface`. They share one ownership check: walk the parent chain to the owning plot, and reject parts a panel object marked as its own.
- Deprecation lives in the Python `__init__` wrappers (constructors) and in binding inject-code (`remove_plottable`, `remove_group`).

**Tech Stack:** C++20, Qt 6.11.2, NeoQCP, Shiboken6 6.11.2 (bindings.xml), pytest + pytest-qt.

**Spec:** docs/plans/2026-10-24-item-factories-design.md (rev 3). Read it before your task. Where this plan and the spec disagree, the spec wins.

## Global Constraints

- Build: one build at a time, in the foreground, `-j 4`, shell timeout 600000 ms. Never background a build or a test run. Never start a second one while one runs.
- Build commands: run them from the repo root, with Qt on PATH and `LLVM_INSTALL_DIR=/usr`:
  ```bash
  export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH"
  export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
  export LLVM_INSTALL_DIR=/usr
  .venv/bin/meson compile -C build-venv -j 4
  ```
  After a header-only change that a binding uses, a wrapper may not regenerate. If so, `rm` that `build-venv/SciQLopPlots/bindings/**/<class>_wrapper.cpp` and compile again.
- Tests: run from /tmp, as their own command, never chained after the build. Use `timeout 540`:
  ```bash
  cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots timeout 540 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <paths> -q -p no:cacheprovider
  ```
- Full suite: `tests/integration tests/unit tests/fuzzing tests/resampler tests/tracing`. Never plain `tests/`, because manual-tests hang. Read the real pass/fail count and exit code. If it takes more than 540 s, split it into two halves.
- TDD: write the test first and watch it fail for the expected reason, then implement.
- Commits:
  - Use explicit paths, never `git add -A`.
  - `qt.ini` is not ours: never stage it.
  - End every message with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
  - Never push.
- `cat` and `ls` are aliased in this shell. Use `command cat`, `command ls` and `test -f`.
- Never touch /home/jeandet/Documents/prog/SciQLop: no reads, no greps, no runs.
- Errors from C++ to Python:
  - `std::invalid_argument` → ValueError
  - `sqp::unsupported_call` → NotImplementedError
  - other `std::exception` → RuntimeError (off-thread calls)
  - a wrong argument type → TypeError (from shiboken overload resolution)
- Deprecation message format, used verbatim by the tests and the warning filter: `<Old>(...) is deprecated: use <new>(...)`. For example `SciQLopTextItem(plot, ...) is deprecated: use plot.text(...)` and `remove_plottable(...) is deprecated: use plot.remove(...)`.
- Do not dispatch subagents.

## Review Focus

1. `panel.remove(x)` called with the `SciQLopPlotInterfacePtr` or `MultiPlotsVerticalSpanPtr` handles that `panel.plots()` and `create_span` return. It must work like the raw object. Task 6 adds `remove` to the panel's handle-dereferencing method list and tests it.
2. An item removed while the plot is hidden or never shown. Removal must not crash, and the next paint must not show it. Task 2's test removes items from an unshown plot.
3. A factory-built item on a projection pane (`nd.subplot(0).text(...)`). It must belong to the pane and die with the projection plot. Task 3 tests it.
4. `plot.remove(x)` twice on the same item. The second call gets an invalid wrapper and must raise `RuntimeError`, never crash. Task 5 tests it.
5. A panel-wide span whose plot leaves and rejoins the panel. Its rebuilt per-plot part must still be marked and refused by `plot.remove`. Task 6 tests it.

---

## File map

| File | Change | Tasks |
|---|---|---|
| `include/SciQLopPlots/helpers.hpp` | `qcp_item_remove` queues a replot | 2 |
| `include/SciQLopPlots/Items/SciQLopPlotItem.hpp` | `item_plot(plot, what)` helper | 1 |
| `include/SciQLopPlots/Items/SciQLopTextItem.hpp`, `SciQLopPixmapItem.hpp`, `SciQLopShapesItems.hpp`, `SciQLopStraightLines.hpp`, `SciQLopHorizontalSpan.hpp`/`.cpp`, `SciQLopRectangularSpan.hpp`/`.cpp`; `src/SciQLopVerticalSpan.cpp` | base initialiser goes through `item_plot` | 1 |
| `src/MultiPlotsVLine.cpp` | drop the now redundant `setParent`; mark parts | 1, 5 |
| `src/MultiPlotsVSpan.cpp` | mark parts | 5 |
| `include/SciQLopPlots/SciQLopPlotInterface.hpp`, `src/SciQLopPlotInterface.cpp` | item factory virtuals, `remove` overloads, `owning_plot`, panel-part mark | 3, 5, 7 |
| `include/SciQLopPlots/SciQLopPlot.hpp`, `src/SciQLopPlot.cpp` | factory overrides; error message names `remove` | 3, 7 |
| `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp` | `vertical_span`, `vertical_line`, `remove` | 6 |
| `SciQLopPlots/bindings/bindings.xml` | ownership + error mapping on the factories and `remove`; deprecation inject-code | 3, 5, 6, 7 |
| `SciQLopPlots/__init__.py` | handle-deref for `panel.remove`; constructor deprecation | 6, 11 |
| `wasm/sciqlopplots_js.cpp` | `remove_plottable` → `remove` | 7 |
| `pyproject.toml` | warning filter that turns our deprecations into errors | 11 |
| tests (new) | `tests/integration/test_item_factories.py`, `test_plot_remove.py` | 1-7 |
| tests, docs, examples | migrate to the factories | 8-10 |

---

### Task 1: Every item's parent is its plot; a null plot is a ValueError

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopPlotItem.hpp` (add the helper near the top, after the includes)
- Modify: the item constructors listed in the file map
- Modify: `src/MultiPlotsVLine.cpp:35` (delete `line->setParent(scp);` and the comment above it, because the constructor does it now)
- Test: `tests/integration/test_item_factories.py` (create)

**Interfaces:**
- Produces: `template <typename Plot> Plot* item_plot(Plot* plot, const char* what)` in namespace `SciQLopPlots`. It throws `std::invalid_argument(std::string(what) + ": plot cannot be None")` for null, then runs `require_owner_thread(plot, what)`, then returns `plot`.
- Produces: every item class's QObject `parent()` is the `SciQLopPlot*` it was built on.

- [ ] **Step 1: Write the failing tests**

```python
"""Items belong to their plot: QObject parent, null plot, death with the plot (part B)."""
import pytest
import shiboken6
from PySide6.QtCore import QPointF, QRectF
from PySide6.QtGui import QColor, QPixmap

from SciQLopPlots import (
    SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopHorizontalLine, SciQLopHorizontalSpan,
    SciQLopPixmapItem, SciQLopPlot, SciQLopPlotRange as R, SciQLopRectangularSpan,
    SciQLopStraightLine, SciQLopTextItem, SciQLopVerticalLine, SciQLopVerticalSpan,
)
from conftest import collect

CONSTRUCTORS = {
    "text": lambda p: SciQLopTextItem(p, "t", QPointF(10, 10)),
    "pixmap": lambda p: SciQLopPixmapItem(p, QPixmap(4, 4), QRectF(0, 0, 4, 4)),
    "ellipse": lambda p: SciQLopEllipseItem(p, QRectF(0, 0, 4, 4)),
    "curved_line": lambda p: SciQLopCurvedLineItem(p, QPointF(0, 0), QPointF(5, 5)),
    "straight_line": lambda p: SciQLopStraightLine(p, 1.0),
    "vertical_line": lambda p: SciQLopVerticalLine(p, 1.0),
    "horizontal_line": lambda p: SciQLopHorizontalLine(p, 1.0),
    "vertical_span": lambda p: SciQLopVerticalSpan(p, R(1.0, 2.0)),
    "horizontal_span": lambda p: SciQLopHorizontalSpan(p, R(1.0, 2.0)),
    "rectangular_span": lambda p: SciQLopRectangularSpan(p, R(1.0, 2.0), R(1.0, 2.0)),
}


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_parents_the_item_to_its_plot(plot, kind):
    item = CONSTRUCTORS[kind](plot)
    assert item.parent() is plot


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_constructor_rejects_a_null_plot(kind):
    with pytest.raises(ValueError, match="plot cannot be None"):
        CONSTRUCTORS[kind](None)


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_held_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = CONSTRUCTORS[kind](plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)
```

- [ ] **Step 2: Run them and watch them fail**

Run: `pytest tests/integration/test_item_factories.py -q`.
Expected failures:
- the parent test fails for text, straight_line, vertical_line and horizontal_line;
- the null test fails for every class (wrong exception type, or a crash for the lines and spans);
- the dies-with-plot test fails for the text and the lines.

A null dereference may crash the run. If it does, add `-k "not null"` to see the other failures, then go on.

- [ ] **Step 3: Add the helper**

In `SciQLopPlotItem.hpp`, inside `namespace SciQLopPlots`, or by opening one next to the includes:

```cpp
namespace SciQLopPlots
{
//! The plot an item is built on, for the item's first base-class initializer: the item's
//! QObject parent, checked before anything is built on it.
template <typename Plot>
Plot* item_plot(Plot* plot, const char* what)
{
    if (plot == nullptr)
        throw std::invalid_argument(std::string(what) + ": plot cannot be None");
    return on_owner_thread(plot, what);
}
}
```

It needs `#include "SciQLopPlots/ThreadGuard.hpp"` and `<stdexcept>`, unless they are already included.

- [ ] **Step 4: Route every constructor through it**

Each constructor's first base initialiser becomes `SciQLopPlots::item_plot(plot, "<ClassName>")`. Then delete that constructor's own `require_owner_thread` call and its null check:
- `SciQLopTextItem`: `: SciQLopMovableItemInterface { SciQLopPlots::item_plot(plot, "SciQLopTextItem") }`. Body: `m_item = new impl::TextItem(plot->qcp_plot(), ...); _follow_selection(m_item);`.
- `SciQLopPixmapItem`, the three `SciQLopEllipseItem` constructors, `SciQLopCurvedLineItem`: replace `{ plot }` / `(plot)` in the initialiser. Remove the `if (plot == nullptr) throw ...; else` and keep the `m_item = ...` statement.
- `SciQLopStraightLine`: add `: SciQLopDrawnObject { SciQLopPlots::item_plot(plot, "SciQLopStraightLine") }`. The vertical and horizontal lines delegate to it, so they need nothing.
- `SciQLopVerticalSpan` (src/SciQLopVerticalSpan.cpp:166): replace `SciQLopPlots::on_owner_thread(plot, "SciQLopVerticalSpan")` with `SciQLopPlots::item_plot(plot, "SciQLopVerticalSpan")`.
- `SciQLopHorizontalSpan` and `SciQLopRectangularSpan`: the same replacement in their .cpp constructors.

Check that every base class constructor forwards its `QObject* parent` to `SciQLopDrawnObject`. Look at `SciQLopItemInterface` and its subclasses in `SciQLopPlotItem.hpp`, which take `QObject* parent = nullptr`.

- [ ] **Step 5: Drop the redundant `setParent` in `MultiPlotsVerticalLine::addObject`**

- [ ] **Step 6: Build, then run the new tests and the item suites**

Build (Global Constraints). Then run: `pytest tests/integration/test_item_factories.py tests/integration/test_item_ownership.py tests/integration/test_items_api.py tests/integration/test_memory.py tests/integration/test_panel_api.py tests/integration/test_multiplots_vertical_line.py -q`.
Expected: all pass. A test that asserted the old RuntimeError for a null Text plot, or that a Text or line wrapper outlives its plot, now contradicts the spec ("Who is affected"). Update it to the new behaviour, and name it in the report.

- [ ] **Step 7: Full suite, then commit**

```bash
git add include/SciQLopPlots/Items/ src/SciQLopVerticalSpan.cpp src/SciQLopHorizontalSpan.cpp src/SciQLopRectangularSpan.cpp src/MultiPlotsVLine.cpp tests/integration/test_item_factories.py <any updated test>
git commit -m "fix(items): every item is its plot's child and refuses a null plot with ValueError"
```

---

### Task 2: A detached item disappears at once

**Files:**
- Modify: `include/SciQLopPlots/helpers.hpp:53-61` (`qcp_item_remove`)
- Test: `tests/integration/test_item_factories.py` (append)

**Interfaces:**
- Produces: `qcp_item_remove(ptr)` does `plot->removeItem(item)`, then `plot->replot(QCustomPlot::rpQueuedReplot)`.

- [ ] **Step 1: Write the failing test**

`afterReplot` is the QCustomPlot child's signal. It is reached through the meta-object because it is not bound. The string-based `QObject.connect(obj, SIGNAL("afterReplot()"), slot)` works in PySide6 6.11.2: it was checked against build-venv, and it fired on `plot.replot()`. `obj.afterReplot` does not exist, because the child comes back as a plain `QWidget`.

```python
from PySide6.QtCore import QObject, SIGNAL
from PySide6.QtWidgets import QApplication, QWidget


def _replots(plot):
    qcp = next(w for w in plot.findChildren(QWidget)
               if w.metaObject().indexOfSignal("afterReplot()") >= 0)
    hits = []
    QObject.connect(qcp, SIGNAL("afterReplot()"), lambda: hits.append(True))
    return hits


@pytest.mark.parametrize("kind", CONSTRUCTORS)
def test_deleting_an_item_repaints_the_plot(plot, kind):
    item = CONSTRUCTORS[kind](plot)
    for _ in range(3):
        QApplication.processEvents()
    hits = _replots(plot)
    shiboken6.delete(item)
    for _ in range(3):
        QApplication.processEvents()
    assert hits, "the plot was not repainted after the item was detached"
```

The `plot` fixture is never shown. That covers Review Focus 2: removal from an unshown plot must not crash.

- [ ] **Step 2: Run it and watch it fail**

Expected: it FAILS for text, pixmap, ellipse, curved_line and the three lines, and passes for the spans, which replot through `SciQLopSpanBase`. If the non-span cases pass too, something else is replotting. Stop and report it: the test must be able to fail.

- [ ] **Step 3: Implement**

```cpp
inline auto qcp_item_remove(auto&& ptr) -> void
{
    qptr_apply(ptr,
               [](auto&& item)
               {
                   if (auto* plot = item->parentPlot())
                   {
                       (void)plot->removeItem(item);
                       plot->replot(QCustomPlot::rpQueuedReplot);
                   }
               });
}
```

Update the comment above it in place: one clause saying a queued replot takes the item off the screen.

The replot must stay inside the QPointer guard. When a plot dies, its QCustomPlot (`m_impl`, the plot's first child, src/SciQLopPlot.cpp:860) deletes its QCP items before Qt deletes the item wrappers. So a wrapper's destructor finds a null QPointer and must not touch the plot. `SciQLopSpanBase::~SciQLopSpanBase` (src/SciQLopSpanBase.cpp:26) does the same through its own check: leave it as it is.

- [ ] **Step 4: Build, run the new test, then the full suite**

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/helpers.hpp tests/integration/test_item_factories.py
git commit -m "fix(items): a detached item repaints its plot instead of lingering until the next replot"
```

---

### Task 3: Item factories on plots

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp`: add the virtuals near `group(...)` (~line 484). Forward-declare the ten item classes and `class QPixmap;`. `Coordinates` and `LineTermination` come from `SciQLopPlots/enums.hpp`, already included.
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp`: add the overrides in the public section of `class SciQLopPlot` (the outer QFrame wrapper, not `_impl::SciQLopPlot`).
- Modify: `src/SciQLopPlot.cpp`: the definitions, plus includes of the item headers.
- Modify: `SciQLopPlots/bindings/bindings.xml`: the `SciQLopPlotInterface` object-type (~line 482).
- Test: `tests/integration/test_item_factories.py` (append)

**Interfaces:**
- Consumes: Task 1 (`item_plot`; the constructors parent to the plot).
- Produces, on `SciQLopPlotInterface` (each default throws `sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()))`, like `plottable(int)`), overridden on `SciQLopPlot`:
  ```cpp
  virtual SciQLopTextItem* text(const QString& text, const QPointF& position, bool movable = false, Coordinates coordinates = Coordinates::Pixels);
  virtual SciQLopPixmapItem* pixmap(const QPixmap& pixmap, const QRectF& rect, bool movable = false, Coordinates coordinates = Coordinates::Pixels);
  virtual SciQLopEllipseItem* ellipse(const QRectF& rect, bool movable = false, Coordinates coordinates = Coordinates::Pixels, const QString& tool_tip = "");
  virtual SciQLopEllipseItem* ellipse(const QRectF& rect, const QPen& pen, const QBrush& brush, bool movable = false, Coordinates coordinates = Coordinates::Pixels, const QString& tool_tip = "");
  virtual SciQLopEllipseItem* ellipse(const QRectF& rect, const QColor& line_color, qreal line_width, const QColor& fill_color, bool movable = false, Coordinates coordinates = Coordinates::Pixels, const QString& tool_tip = "");
  virtual SciQLopCurvedLineItem* curved_line(const QPointF& start, const QPointF& stop, LineTermination start_terminator = LineTermination::NoneTermination, LineTermination stop_terminator = LineTermination::Arrow, Coordinates coordinates = Coordinates::Pixels, const QString& tool_tip = "");
  virtual SciQLopVerticalLine* vertical_line(double position, bool movable = false, Coordinates coordinates = Coordinates::Data);
  virtual SciQLopHorizontalLine* horizontal_line(double position, bool movable = false, Coordinates coordinates = Coordinates::Data);
  virtual SciQLopVerticalSpan* vertical_span(SciQLopPlotRange range, QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "", Coordinates coordinates = Coordinates::Data);
  virtual SciQLopHorizontalSpan* horizontal_span(SciQLopPlotRange range, QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "");
  virtual SciQLopRectangularSpan* rectangular_span(SciQLopPlotRange key_range, SciQLopPlotRange value_range, QColor color = QColor::fromRgb(100, 100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "");
  ```
  In Python, the factories return C++-owned objects. `SciQLopPlot`'s implementation is `return new SciQLopTextItem(this, text, position, movable, coordinates);` and so on: the constructor sets the parent and checks the thread.

- [ ] **Step 1: Write the failing tests**

```python
import gc
from PySide6.QtGui import QBrush, QPen
from PySide6.QtWidgets import QApplication
from SciQLopPlots import (Coordinates, LineTermination, SciQLopNDProjectionPlot,
                          SciQLopDrawnObject, SciQLopItemInterface)
from conftest import off_thread

FACTORIES = {
    "text": (lambda p: p.text("t", QPointF(10, 10)), SciQLopTextItem),
    "pixmap": (lambda p: p.pixmap(QPixmap(4, 4), QRectF(0, 0, 4, 4)), SciQLopPixmapItem),
    "ellipse": (lambda p: p.ellipse(QRectF(0, 0, 4, 4)), SciQLopEllipseItem),
    "ellipse_pen": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QPen(QColor("red")), QBrush(QColor("blue"))),
                    SciQLopEllipseItem),
    "ellipse_colors": (lambda p: p.ellipse(QRectF(0, 0, 4, 4), QColor("red"), 2.0, QColor("blue")),
                       SciQLopEllipseItem),
    "curved_line": (lambda p: p.curved_line(QPointF(0, 0), QPointF(5, 5)), SciQLopCurvedLineItem),
    "vertical_line": (lambda p: p.vertical_line(1.0), SciQLopVerticalLine),
    "horizontal_line": (lambda p: p.horizontal_line(1.0), SciQLopHorizontalLine),
    "vertical_span": (lambda p: p.vertical_span(R(1.0, 2.0)), SciQLopVerticalSpan),
    "horizontal_span": (lambda p: p.horizontal_span(R(1.0, 2.0)), SciQLopHorizontalSpan),
    "rectangular_span": (lambda p: p.rectangular_span(R(1.0, 2.0), R(1.0, 2.0)), SciQLopRectangularSpan),
}


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_returns_an_item_of_its_plot(plot, kind):
    make, cls = FACTORIES[kind]
    item = make(plot)
    assert type(item) is cls
    assert item.parent() is plot


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_survives_dropping_the_reference(plot, kind):
    make, cls = FACTORIES[kind]
    make(plot)
    gc.collect(); QApplication.processEvents(); gc.collect()
    assert len(plot.findChildren(cls)) == 1


@pytest.mark.parametrize("kind", FACTORIES)
def test_factory_item_dies_with_its_plot(qtbot, kind):
    plot = SciQLopPlot()
    item = FACTORIES[kind][0](plot)
    shiboken6.delete(plot)
    collect()
    assert not shiboken6.isValid(item)


def test_factory_off_thread_raises(plot):
    out = {}
    def call():
        try:
            plot.text("t", QPointF(1, 1))
        except RuntimeError as e:
            out["error"] = e
    off_thread(call)
    assert "thread" in str(out.get("error", ""))


def test_factory_keeps_constructor_argument_names(plot):
    span = plot.vertical_span(R(1.0, 2.0), color=QColor("red"), read_only=True,
                              visible=False, tool_tip="tip", coordinates=Coordinates.Data)
    assert span.tool_tip() == "tip" if callable(span.tool_tip) else span.tool_tip == "tip"
    line = plot.curved_line(QPointF(0, 0), QPointF(1, 1), start_terminator=LineTermination.Arrow,
                            stop_terminator=LineTermination.NoneTermination)
    assert line is not None


def test_projection_pane_has_the_factories(qtbot):
    nd = SciQLopNDProjectionPlot()
    qtbot.addWidget(nd)
    pane = nd.subplot(0)
    item = pane.text("t", QPointF(1, 1))
    assert item.parent() is pane


def test_projection_plot_itself_has_none(qtbot):
    nd = SciQLopNDProjectionPlot()
    qtbot.addWidget(nd)
    with pytest.raises(NotImplementedError):
        nd.text("t", QPointF(1, 1))


def _all_subclasses(cls):
    for sub in cls.__subclasses__():
        yield sub
        yield from _all_subclasses(sub)


# Every drawn item class is reachable from a factory, or listed here with the reason.
NO_PLOT_FACTORY = {
    "SciQLopStraightLine": "deprecated-only: vertical_line / horizontal_line cover it",
    "MultiPlotsVerticalSpan": "panel-only (panel.vertical_span)",
    "MultiPlotsVerticalLine": "panel-only (panel.vertical_line)",
}


def test_every_item_class_has_a_factory():
    covered = {cls.__name__ for _, cls in FACTORIES.values()}
    items = {c.__name__ for c in _all_subclasses(SciQLopItemInterface)} | \
            {c.__name__ for c in _all_subclasses(SciQLopStraightLine)} | {"SciQLopStraightLine"}
    abstract = {n for n in items if n.endswith("Interface")}
    missing = items - covered - abstract - set(NO_PLOT_FACTORY)
    assert not missing, f"item classes with no plot factory: {sorted(missing)}"
```

Adjust the `tool_tip` assertion to how the span exposes it in Python: check `dir(span)` once and keep a single plain assertion. If `SciQLopItemInterface` is not importable from `SciQLopPlots`, take it from `SciQLopPlotsBindings`. Projection pane tests: check the existing projection tests (`grep -rn "subplot(" tests/integration`) for the right fixture or set-up. A pane may need dimension pairs before `subplot(0)` exists.

- [ ] **Step 2: Run them and watch them fail** with `AttributeError: 'SciQLopPlot' object has no attribute 'text'`, and so on.

- [ ] **Step 3: Declare the virtuals on the interface**

Each default is the same one-liner:

```cpp
inline virtual SciQLopTextItem* text(const QString& text, const QPointF& position,
                                     bool movable = false,
                                     Coordinates coordinates = Coordinates::Pixels)
{
    throw sqp::unsupported_call(unsupported_message(Q_FUNC_INFO, typeid(*this).name()));
}
```

Put them in one block with a single doc comment above it: `//! Item factories: the item belongs to this plot (QObject child, C++-owned in Python) until remove() or the plot's death.`

- [ ] **Step 4: Override on `SciQLopPlot` and define them in src/SciQLopPlot.cpp**

```cpp
SciQLopTextItem* SciQLopPlot::text(const QString& text, const QPointF& position, bool movable,
                                   Coordinates coordinates)
{
    return new SciQLopTextItem(this, text, position, movable, coordinates);
}
```

The other ten follow the same pattern, each forwarding every argument in order to the constructor of the same shape. (Section 1's table in the spec maps each factory to its constructor.)

- [ ] **Step 5: Bindings**

Inside `<object-type name="SciQLopPlotInterface" ...>`, give each factory signature a `modify-function` with C++ ownership on the return and the error mapping. For example:

```xml
<modify-function signature="text(const QString&amp;,const QPointF&amp;,bool,Coordinates)">
    <modify-argument index="return"><define-ownership class="target" owner="c++"/></modify-argument>
    <inject-code class="target" position="beginning">
        <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="SciQLopTextItem*"/></insert-template>
    </inject-code>
</modify-function>
```

Do this for all eleven signatures, copying each signature exactly as declared. Keep the C++ argument names. They are already the Python keyword names, and Step 1's keyword test checks it.

The overrides on `SciQLopPlot` must also get C++ ownership. Shiboken generates a separate wrapper method for an override that is redeclared in a subclass. Check the generated `build-venv/SciQLopPlots/bindings/**/sciqlopplot_wrapper.cpp`:
- if `text` is wrapped there, repeat the markup under `<object-type name="SciQLopPlot">`;
- either way, `test_factory_item_survives_dropping_the_reference` is the proof. Without the markup the item dies at `gc.collect()`.

- [ ] **Step 6: Build, run the new tests, then the full suite, then the signature test**

`tests/integration/test_binding_signatures.py` must still show zero shiboken warnings.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_item_factories.py
git commit -m "feat(items): plot.text, pixmap, ellipse, curved_line, the lines and the spans: items the plot owns"
```

---

### Task 4: Mark the parts a panel object builds

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (two free functions next to the class, in namespace `SciQLopPlots`)
- Modify: `src/MultiPlotsVSpan.cpp:48-60` (`addObject`), `src/MultiPlotsVLine.cpp:29-40` (`addObject`)
- Test: `tests/integration/test_plot_remove.py` (create)

**Interfaces:**
- Produces:
  ```cpp
  namespace SciQLopPlots {
  //! Marks \a part as built by a panel-wide object: removing it alone would leave that object out of step.
  inline void mark_panel_part(QObject* part) { part->setProperty("_sciqlop_panel_part", true); }
  inline bool is_panel_part(const QObject* part) { return part->property("_sciqlop_panel_part").toBool(); }
  }
  ```
- Consumed by Task 5's `remove` checks.

- [ ] **Step 1: Write the failing test**

```python
"""plot.remove / panel.remove (part B)."""
import pytest
import shiboken6
from PySide6.QtCore import QPointF
from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, SciQLopPlotRange as R,
                          SciQLopVerticalLine, SciQLopVerticalSpan)
from conftest import process_events


def _panel_with_plot(panel):
    plot = panel.create_plot()
    process_events()
    return plot


def test_panel_span_marks_its_per_plot_parts(panel):
    plot = _panel_with_plot(panel)
    span = MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    line = MultiPlotsVerticalLine(panel, 1.5)
    process_events()
    parts = plot.findChildren(SciQLopVerticalSpan) + plot.findChildren(SciQLopVerticalLine)
    assert parts and all(p.property("_sciqlop_panel_part") for p in parts)
```

If `create_plot` returns a Ptr handle, use `.data()`. The existing tests in `test_panel_api.py:259` reach the parts this way.

- [ ] **Step 2: Run it and watch it fail.** The property is unset, so the assertion fails.

- [ ] **Step 3: Implement.** In each `addObject`, call `SciQLopPlots::mark_panel_part(span)` (or `line`) right after the `new`. A rebuilt part goes through `addObject` again, so it is marked again.

- [ ] **Step 4: Build, run, full suite, commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp src/MultiPlotsVSpan.cpp src/MultiPlotsVLine.cpp tests/integration/test_plot_remove.py
git commit -m "feat(panel): panel-wide spans and lines mark the per-plot parts they build"
```

---

### Task 5: `plot.remove(x)`

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp` (declarations next to `remove_group`), `src/SciQLopPlotInterface.cpp` (definitions)
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopPlotInterface` object-type)
- Test: `tests/integration/test_plot_remove.py` (append)

**Interfaces:**
- Consumes: Task 4's `is_panel_part`; Task 3's factories (in the tests).
- Produces:
  ```cpp
  void remove(SciQLopPlottableInterface* plottable);
  void remove(SciQLopPlotGroup* group);
  void remove(SciQLopItemInterface* item);
  void remove(SciQLopStraightLine* line);
  ```
  Each one runs `check_removable(x)` before anything is queued, then `SCIQLOP_ON_OWNER_THREAD_WITH(x, remove(x));`, then `delete x;`.
- Produces (file-local in src/SciQLopPlotInterface.cpp):
  - `SciQLopPlotInterface* owning_plot(const QObject*)`: the nearest `SciQLopPlotInterface` ancestor, walking `parent()`. A plottable's parent is the plot's inner QCustomPlot, whose parent is the plot. An item's or group's parent is the plot itself.
  - A private member `void check_removable(const QObject* x) const`, declared in the header. It throws `std::invalid_argument`:
    - null: `"remove: got None"`;
    - `is_panel_part(x)`: `"remove: this <class> is part of a panel-wide object; remove that object from the panel (panel.remove)"`;
    - `owning_plot(x) != this`: `"remove: this <class> belongs to another plot ('<owner objectName>')"`, or `"... does not belong to any plot"` when there is no owner.

    `<class>` is `x->metaObject()->className()`.

`remove_group` keeps its own body until Task 7.

- [ ] **Step 1: Write the failing tests**

```python
from PySide6.QtCore import QRectF
from SciQLopPlots import SciQLopPlot, SciQLopTextItem
from conftest import collect, off_thread, on_destroyed


ITEM_MAKERS = {
    "text": lambda p: p.text("t", QPointF(10, 10)),
    "ellipse": lambda p: p.ellipse(QRectF(0, 0, 4, 4)),
    "vertical_line": lambda p: p.vertical_line(1.0),
    "vertical_span": lambda p: p.vertical_span(R(1.0, 2.0)),
    "rectangular_span": lambda p: p.rectangular_span(R(1.0, 2.0), R(1.0, 2.0)),
    "ctor_text": lambda p: SciQLopTextItem(p, "t", QPointF(1, 1)),
    "ctor_line": lambda p: SciQLopVerticalLine(p, 1.0),
}


@pytest.mark.parametrize("kind", ITEM_MAKERS)
def test_remove_deletes_an_item_now(plot, kind):
    item = ITEM_MAKERS[kind](plot)
    gone = on_destroyed(item)
    plot.remove(item)
    assert gone == [True]
    assert not shiboken6.isValid(item)
    collect()  # a constructor-built (Python-owned) item must not be freed twice


def test_remove_deletes_a_plottable(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    n = len(plot.plottables())
    plot.remove(graph)
    assert len(plot.plottables()) == n - 1
    assert not shiboken6.isValid(graph)


def test_remove_deletes_a_group_and_a_group_drops_a_removed_member(plot, sample_data):
    x, y = sample_data
    a, b = plot.line(x, y), plot.line(x, y)
    group = plot.group("g", [a, b])
    plot.remove(a)
    assert len(group.members()) == 1
    plot.remove(group)
    assert not shiboken6.isValid(group)


def test_remove_rejects_another_plots_item(qtbot, plot):
    other = SciQLopPlot()
    other.setObjectName("other")
    qtbot.addWidget(other)
    item = other.text("t", QPointF(1, 1))
    with pytest.raises(ValueError, match="belongs to another plot.*other"):
        plot.remove(item)
    assert shiboken6.isValid(item)


def test_remove_rejects_another_plots_plottable(qtbot, plot, sample_data):
    other = SciQLopPlot()
    qtbot.addWidget(other)
    x, y = sample_data
    graph = other.line(x, y)
    with pytest.raises(ValueError, match="another plot"):
        plot.remove(graph)
    assert shiboken6.isValid(graph)


def test_remove_rejects_none(plot):
    with pytest.raises(ValueError, match="None"):
        plot.remove(None)


def test_remove_rejects_a_panel_part(panel):
    plot = _panel_with_plot(panel)
    MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    process_events()
    part = plot.findChildren(SciQLopVerticalSpan)[0]
    with pytest.raises(ValueError, match="panel"):
        plot.remove(part)


def test_remove_rejects_the_legend_and_a_component(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    with pytest.raises(TypeError):
        plot.remove(plot.legend())
    with pytest.raises(TypeError):
        plot.remove(graph.components()[0])


def test_remove_twice_raises_not_crashes(plot):
    item = plot.text("t", QPointF(1, 1))
    plot.remove(item)
    with pytest.raises(RuntimeError):
        plot.remove(item)


def test_remove_from_the_items_own_signal(plot):
    line = plot.vertical_line(1.0, True)
    line.position_changed.connect(lambda *_: plot.remove(line))
    line.set_position(2.0)
    process_events()
    assert not shiboken6.isValid(line)


def test_remove_a_span_from_its_own_signal(plot):
    span = plot.vertical_span(R(1.0, 2.0))
    span.range_changed.connect(lambda *_: plot.remove(span))
    span.set_range(R(3.0, 4.0))
    process_events()
    assert not shiboken6.isValid(span)


def test_remove_off_thread_lands_on_the_gui_thread(plot):
    item = plot.text("t", QPointF(1, 1))
    gone = on_destroyed(item)
    off_thread(lambda: plot.remove(item))
    assert gone == []
    process_events()
    assert gone == [True]


def test_remove_off_thread_is_dropped_if_the_item_dies_first(plot):
    item = plot.text("t", QPointF(1, 1))
    off_thread(lambda: plot.remove(item))
    shiboken6.delete(item)
    process_events()  # the queued call finds a null QPointer and does nothing
```

Use the real accessor names: the group's members list (check `SciQLopPlotGroup.hpp`), `plot.legend()`, `graph.components()`. `remove_twice` covers Review Focus 4.

The `ctor_text` and `ctor_line` cases are what prove the spec's "no ownership markup on the argument" decision. A Python-owned wrapper deleted from C++ must be invalid afterwards, with no double free when Python lets go. Keep the `collect()` after the assertion. Task 1's `test_held_item_dies_with_its_plot` proves the same for the plot's death.

The two own-signal tests pass because their emitters are hardened: `impl::StraightLine::set_position` (src/SciQLopStraightLines.cpp:73-81) emits, then replots only if a QPointer to itself is still alive. The span's `set_range` path is the same kind. If either test crashes, the emitter is missing that guard. Fix it there (emit last, or a QPointer check after the emit), with the test as the reproducer. The invalid wrapper raises shiboken's own RuntimeError before C++ runs.

- [ ] **Step 2: Run them and watch them fail** with `AttributeError: ... has no attribute 'remove'`.

- [ ] **Step 3: Implement**

```cpp
namespace
{
SciQLopPlotInterface* owning_plot(const QObject* object)
{
    for (auto* p = object->parent(); p; p = p->parent())
        if (auto* plot = qobject_cast<SciQLopPlotInterface*>(p))
            return plot;
    return nullptr;
}
}

void SciQLopPlotInterface::check_removable(const QObject* x) const
{
    if (x == nullptr)
        throw std::invalid_argument("remove: got None");
    const std::string what = x->metaObject()->className();
    if (SciQLopPlots::is_panel_part(x))
        throw std::invalid_argument("remove: this " + what
                                    + " is part of a panel-wide object; remove that object from "
                                      "the panel (panel.remove)");
    if (auto* owner = owning_plot(x); owner != this)
        throw std::invalid_argument(
            owner ? "remove: this " + what + " belongs to another plot ('"
                    + owner->objectName().toStdString() + "')"
                  : "remove: this " + what + " does not belong to any plot");
}

// Checked before queuing: off-thread, a throw from the queued call would escape the event loop.
void SciQLopPlotInterface::remove(SciQLopPlottableInterface* plottable)
{
    check_removable(plottable);
    SCIQLOP_ON_OWNER_THREAD_WITH(plottable, remove(plottable));
    delete plottable;
}
```

The group, item and line overloads have the same three lines. One comment above the first one covers all four.

Check the cast: `owning_plot` uses `qobject_cast<SciQLopPlotInterface*>`, and for a projection pane the nearest plot ancestor is the pane. Run Review Focus 3: `pane.remove(pane.text(...))` must pass. Add it as a test.

- [ ] **Step 4: Bindings**

For each of the four `remove` signatures, a `modify-function` with `<inject-code class="target" position="beginning"><insert-template name="void_raises_valueerror"/></inject-code>`. Add no ownership markup on the argument (spec section 4).

- [ ] **Step 5: Build, run the new tests, the full suite, the signature test**

- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp src/SciQLopPlotInterface.cpp SciQLopPlots/bindings/bindings.xml tests/integration/test_plot_remove.py
git commit -m "feat(plot): plot.remove(x) for plottables, groups and items, refusing foreign objects and panel parts"
```

---

### Task 6: `panel.vertical_span`, `panel.vertical_line`, `panel.remove`

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`, `src/SciQLopMultiPlotPanel.cpp`
- Modify: `SciQLopPlots/bindings/bindings.xml` (`SciQLopMultiPlotPanel` object-type)
- Modify: `SciQLopPlots/__init__.py:430` (add `"remove"` to the handle-dereferencing method tuple)
- Test: `tests/integration/test_plot_remove.py` (append)

**Interfaces:**
- Produces on `SciQLopMultiPlotPanel`:
  ```cpp
  MultiPlotsVerticalSpan* vertical_span(SciQLopPlotRange range, QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "", const QString& id = "");
  MultiPlotsVerticalLine* vertical_line(double position, QColor color = QColor::fromRgb(100, 100, 100), bool read_only = false, bool visible = true, const QString& tool_tip = "");
  void remove(MultiPlotsVerticalSpan* span);
  void remove(MultiPlotsVerticalLine* line);
  void remove(SciQLopPlotInterface* plot);
  ```
  - The factories: `require_owner_thread(this, ...)`, then `return new MultiPlotsVerticalSpan(this, ...)`. Copy the constructor defaults from `MultiPlotsVSpan.hpp:70` and `MultiPlotsVLine.hpp:61` exactly.
  - `remove(span|line)` throws `std::invalid_argument` for null (`"remove: got None"`) or for `x->parent() != this` (`"remove: this <class> belongs to another panel"`). Then `SCIQLOP_ON_OWNER_THREAD_WITH`, then `delete`.
  - `remove(plot)` throws for null or `!contains(plot)` (`"remove: this plot is not in this panel"`), then `remove_plot(plot)`. Deletion is deferred, as in `remove_plot`.

- [ ] **Step 1: Write the failing tests**

```python
def test_panel_factories_return_panel_owned_objects(panel):
    _panel_with_plot(panel)
    span = panel.vertical_span(R(1.0, 2.0), tool_tip="s", id="a")
    line = panel.vertical_line(1.5)
    import gc; gc.collect(); process_events()
    assert isinstance(span, MultiPlotsVerticalSpan) and span.parent() is panel
    assert isinstance(line, MultiPlotsVerticalLine) and line.parent() is panel
    assert shiboken6.isValid(span) and shiboken6.isValid(line)


def test_panel_remove_deletes_a_span_and_its_parts(panel):
    plot = _panel_with_plot(panel)
    span = panel.vertical_span(R(1.0, 2.0))
    process_events()
    panel.remove(span)
    assert not shiboken6.isValid(span)
    assert plot.findChildren(SciQLopVerticalSpan) == []


def test_panel_remove_deletes_a_line(panel):
    _panel_with_plot(panel)
    line = panel.vertical_line(1.5)
    panel.remove(line)
    assert not shiboken6.isValid(line)


def test_panel_remove_removes_a_plot(panel):
    plot = _panel_with_plot(panel)
    panel.remove(plot)
    collect()
    assert len(panel.plots()) == 0


def test_panel_remove_accepts_handles(panel):
    from SciQLopPlots import MultiPlotsVSpanCollection
    _panel_with_plot(panel)
    collection = MultiPlotsVSpanCollection(panel)
    handle = collection.create_span(R(1.0, 2.0))
    panel.remove(handle)
    panel.remove(panel.plots()[0])
    collect()
    assert len(panel.plots()) == 0


def test_panel_remove_rejects_foreign_and_none(qtbot, panel):
    from SciQLopPlots import SciQLopMultiPlotPanel
    other = SciQLopMultiPlotPanel()
    qtbot.addWidget(other)
    foreign = other.vertical_span(R(1.0, 2.0))
    with pytest.raises(ValueError):
        panel.remove(foreign)
    with pytest.raises(ValueError):
        panel.remove(other.create_plot())
    with pytest.raises(ValueError):
        panel.remove(None)


def test_part_built_for_a_later_plot_is_refused(panel):
    from SciQLopPlots import SciQLopPlot
    _panel_with_plot(panel)
    panel.vertical_span(R(1.0, 2.0))
    late = SciQLopPlot()
    panel.add_plot(late)
    process_events()
    part = late.findChildren(SciQLopVerticalSpan)[0]
    with pytest.raises(ValueError, match="panel"):
        late.remove(part)
```

The last test covers Review Focus 5. A plot that joins after the span exists gets its part from `addObject`, the same path a returning plot takes. The bound API has no way to take a plot out without deleting it, so a join stands in for a return.

The `None` case: `panel.remove(None)` resolves to the first overload, and each overload rejects null.

- [ ] **Step 2: Run them and watch them fail.**

- [ ] **Step 3: Implement** the five methods in src/SciQLopMultiPlotPanel.cpp, next to `remove_plot`.

- [ ] **Step 4: Bindings**
  - `vertical_span` and `vertical_line`: `<define-ownership class="target" owner="c++"/>` on the return, plus `returning_raises_python_errors` with `$TYPE` set to the return type.
  - The three `remove` signatures: `void_raises_valueerror`.
  - If `create_plot` returns a handle, use `.data()` in the foreign-plot test.

- [ ] **Step 5: `__init__.py`.** Add `"remove"` to the tuple at line 430. This wraps the method in a Python function. That is acceptable here: `remove` is not a Qt slot (memory: no Python wrappers on slots). Mind that rule if `remove` were ever made `Q_SLOT`.

- [ ] **Step 6: Build, run, full suite, signature test, commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp SciQLopPlots/bindings/bindings.xml SciQLopPlots/__init__.py tests/integration/test_plot_remove.py
git commit -m "feat(panel): panel.vertical_span, panel.vertical_line and panel.remove"
```

---

### Task 7: `remove_plottable` and `remove_group` become deprecated aliases

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp:464-488`, `src/SciQLopPlotInterface.cpp:73-80`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `src/SciQLopPlot.cpp:54` (the error message says `remove_plottable`; make it `remove`)
- Modify: `wasm/sciqlopplots_js.cpp:474` (`plot->remove_plottable(plottable)` → `plot->remove(plottable)`; it is already the owning plot)
- Test: `tests/integration/test_plot_remove.py` (append)

**Interfaces:**
- `remove_plottable(SciQLopPlottableInterface* p)`: `if (p) remove(p);`. A null stays a no-op, a foreign plottable now raises ValueError.
- `remove_plottable(int)` and `remove_plottable(const QString&)`: unchanged bodies (a silent no-op for unknown).
- `remove_group(SciQLopPlotGroup* g)`: `if (g) remove(g);`. Today a null group is `delete nullptr`, a no-op, so keep it one.
- Python: each of the four signatures warns `DeprecationWarning` with the Global Constraints format: `remove_plottable(...) is deprecated: use plot.remove(...)` and `remove_group(...) is deprecated: use plot.remove(...)`.

- [ ] **Step 1: Write the failing tests**

```python
def test_remove_plottable_warns_and_raises_for_foreign(qtbot, plot, sample_data):
    other = SciQLopPlot()
    qtbot.addWidget(other)
    x, y = sample_data
    graph = other.line(x, y)
    with pytest.warns(DeprecationWarning, match=r"remove_plottable\(\.\.\.\) is deprecated: use plot\.remove"):
        with pytest.raises(ValueError):
            plot.remove_plottable(graph)
    assert shiboken6.isValid(graph)


@pytest.mark.parametrize("arg", [None, 99, "nope"])
def test_remove_plottable_noops_stay(plot, arg):
    with pytest.warns(DeprecationWarning):
        plot.remove_plottable(arg)


def test_remove_group_warns(plot, sample_data):
    x, y = sample_data
    group = plot.group("g", [plot.line(x, y)])
    with pytest.warns(DeprecationWarning, match=r"remove_group\(\.\.\.\) is deprecated"):
        plot.remove_group(group)
    assert not shiboken6.isValid(group)


def test_deprecation_names_the_callers_line(plot, sample_data):
    x, y = sample_data
    graph = plot.line(x, y)
    with pytest.warns(DeprecationWarning) as record:
        plot.remove_plottable(graph)
    assert record[0].filename == __file__
```

- [ ] **Step 2: Run them and watch them fail** (no warning, and the foreign plottable is deleted).

- [ ] **Step 3: Implement the C++ bodies**

```cpp
//! Deprecated: remove(). A plottable of another plot raises, a null one does nothing.
virtual void remove_plottable(SciQLopPlottableInterface* plottable)
{
    if (plottable)
        remove(plottable);
}
```

`remove_group` gets the same shape and loses its own ownership check, because `remove` does it now.

- [ ] **Step 4: Bindings.** For each of the four signatures, an inject-code at the beginning. It warns, then falls through to the call. The pointer overloads use `void_raises_valueerror` after the warning:

```xml
<inject-code class="target" position="beginning">
    if (PyErr_WarnEx(PyExc_DeprecationWarning, "remove_plottable(...) is deprecated: use plot.remove(...)", 1) &lt; 0)
        return nullptr;
    <insert-template name="void_raises_valueerror"/>
</inject-code>
```

`stacklevel` 1 from C names the Python caller's line, because the C function has no frame of its own. `test_deprecation_names_the_callers_line` checks it. The existing `remove_group` modify-function already has an inject-code: replace it, so there is one. With `-W error`, `PyErr_WarnEx` returns -1, and the `return nullptr` makes it raise.

- [ ] **Step 5: Build, run the new tests, then `tests/integration/test_remove_plottable.py` and the full suite.** Existing `remove_plottable` / `remove_group` call sites now warn, but warnings are not errors yet (Task 11), so the suite passes. A test that pinned "remove_plottable of a foreign plottable deletes it" contradicts the spec: update it and name it in the report.

- [ ] **Step 6: Build the wasm change?** No. The wasm build is separate and slow. Make the one-line change, and say in the report that it was not compiled.

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp src/SciQLopPlotInterface.cpp src/SciQLopPlot.cpp SciQLopPlots/bindings/bindings.xml wasm/sciqlopplots_js.cpp tests/integration/test_plot_remove.py <updated tests>
git commit -m "feat(plot): remove_plottable and remove_group are deprecated aliases of remove; a foreign plottable raises"
```

---

### Tasks 8-10: Move every call site to the factories and `remove`

These three tasks have the same rules over different files. Each one is its own dispatch and its own commit.

**Rules (all three):**
- `SciQLopTextItem(plot, a, b, ...)` → `plot.text(a, b, ...)`, and likewise for each class. The spec section 1 table maps each constructor to its factory. `SciQLopStraightLine(plot, pos, movable, coords, Qt.Vertical)` → `plot.vertical_line(pos, movable, coords)`, and `Qt.Horizontal` → `plot.horizontal_line(...)`.
- `MultiPlotsVerticalSpan(panel, ...)` → `panel.vertical_span(...)`, and `MultiPlotsVerticalLine(panel, ...)` → `panel.vertical_line(...)`.
- `plot.remove_plottable(x)` and `plot.remove_group(g)` → `plot.remove(x)`. Index and name forms: `plot.remove(plot.plottable(i))`.
- `del item` / `item = None`, written to delete an item → `plot.remove(item)`. `item.deleteLater()` and `shiboken6.delete(item)` → `plot.remove(item)`, unless the test is about those calls.
- **Keep the constructor or alias where it is the subject of the test.** That covers Python ownership of constructor-built items (test_item_ownership.py), the alias behaviour (test_remove_plottable.py, tests from Task 7), the null-plot and parent tests of Task 1, and the thread-guard tests of constructors. Wrap each kept call in `pytest.warns(DeprecationWarning)` only in Task 11. List every kept call in the report, with file:line.
- Wherever a test held a reference only to keep an item alive, keep it. It is harmless.
- `tests/integration/test_drawn_contract.py` builds one of each class through a builder row: switch the builders to the factories.
- Each task runs the full suite before its commit, and must leave it green.
- Find the call sites with `grep -rnE "SciQLop(Text|Pixmap|Ellipse|CurvedLine)Item\(|SciQLop(Straight|Vertical|Horizontal)Line\(|SciQLop(Vertical|Horizontal|Rectangular)Span\(|MultiPlotsVertical(Span|Line)\(|remove_plottable\(|remove_group\(" <files>`. Ignore class names inside strings or `isinstance`.

### Task 8: Migrate the item tests

**Files:** in tests/integration: test_items_api.py, test_items_style_api.py, test_item_signals_and_pixel.py, test_item_self_delete.py, test_drawn_contract.py, test_drawn_items.py, test_drawn_spans.py, test_drawn_components.py, test_drawn_inspector.py, test_drawn_panel_objects.py, test_drawn_aggregates.py, test_pixel_items_axis_rect_anchor.py, test_far_items_mouse_move.py, test_multiplots_vertical_line.py, test_panel_api.py, test_memory.py.

- [ ] Step 1: Grep the call sites. Rewrite them per the rules.
- [ ] Step 2: Run those files, then the full suite.
- [ ] Step 3: Commit: `test: build items with the plot and panel factories (item suites)`.

### Task 9: Migrate the other tests, the fuzzers and the sweep

**Files:**
- In tests/integration: test_thread_guards.py (keep the constructor guard tests), test_latex_axis_labels.py, test_audit_2026_04_28_reproducers.py, test_fuzzing_findings.py, test_api_footguns.py, test_api_hygiene.py, test_binding_signatures.py, test_pipeline_integration.py, test_signals.py, test_projection_arrow_on_pane.py, test_self_delete_sweep_regressions.py, test_remove_plottable.py (only the calls that are not about the alias), test_curve_color_scale.py, test_colormap_api.py, test_inspector_model.py, test_projection_color_scale.py.
- Every other file with a `remove_plottable(` or `remove_group(` call site.
- tests/fuzzing/item_actions.py, tests/sweeps/self_delete/common.py.

Steps:
- [ ] Step 1: Rewrite the call sites.
- [ ] Step 2: Run the full suite.
- [ ] Step 3: Run the self-delete sweep: `cd tests/sweeps/self_delete`, delete `discovery.jsonl` and `pairs.jsonl` if present, then `driver.py discover`, `driver.py pairs`, `driver.py summary`. Each is one command, waited out. It takes about 15 min, so split it if one command would pass 540 s. The sweep resumes, so rerun the same phase. Expected: 0 crashed, 0 hung pairs.
- [ ] Step 4: Commit: `test: build items with the factories in the remaining suites, fuzzers and the self-delete sweep`.

### Task 10: Migrate docs and examples

**Files:**
- docs/user-guide.md:
  - "Who keeps a plot alive", ~:112-126: add the item rule. What a plot draws belongs to the plot. `plot.remove(x)` or the plot's death deletes it.
  - The drawn-object section, ~:696-725.
  - The groups section, ~:740-760: `remove_group` → `remove`.
  - "Overlays: spans, lines and text", ~:790-840: factories. Replace the "follow the same pattern" bullet with the factory list. Panel objects via `panel.vertical_span` / `panel.vertical_line`. One sentence on `plot.remove` and `panel.remove`.
  - "Reactive pipelines", ~:939-946.
  - The self-delete footgun note, ~:1350: say `plot.remove` from an object's own signal is safe.
  - The deprecations list, ~:1394: add the item constructors, `remove_plottable` and `remove_group`.
- README.md:83-88.
- tests/manual-tests/: gallery.py, basics/vertical_spans.py, basics/parametric_curve.py, advanced/pipeline.py, advanced/stacked_plots.py, stress/stress_model.py, test_inspector_properties.py.

Steps:
- [ ] Step 1: Rewrite. Guide snippets are executed by tests/integration/test_user_guide.py, so keep them runnable.
- [ ] Step 2: Run `tests/integration/test_user_guide.py`, then `python -m py_compile` on each changed manual test. Manual tests are not run: they block on `app.exec`. Then run the full suite.
- [ ] Step 3: Commit: `docs: items from plot and panel factories, one remove`.

---

### Task 11: Deprecate the constructors; make our deprecations errors in the suite

**Files:**
- Modify: `SciQLopPlots/__init__.py:396-425`
- Modify: `pyproject.toml` (`[tool.pytest.ini_options]`)
- Modify: the tests that kept a constructor or alias call (listed in the Task 8 and 9 reports): wrap each call in `pytest.warns(DeprecationWarning)`.
- Test: `tests/integration/test_item_factories.py` (append)

**Interfaces:**
- Consumes: the factory names from Tasks 3 and 6.
- Produces: every item constructor called from Python warns `<Class>(plot, ...) is deprecated: use plot.<factory>(...)`, with `stacklevel=2`. For the panel objects: `MultiPlotsVerticalSpan(panel, ...) is deprecated: use panel.vertical_span(...)`, and the same for the line. `SciQLopStraightLine` → `use plot.vertical_line(...) or plot.horizontal_line(...)`.

- [ ] **Step 1: Write the failing tests**

```python
from SciQLopPlots import MultiPlotsVerticalLine, MultiPlotsVerticalSpan

DEPRECATED = {
    **{k: (v, f"use plot.{k}") for k, v in CONSTRUCTORS.items() if k != "straight_line"},
    "straight_line": (CONSTRUCTORS["straight_line"], "use plot.vertical_line"),
}


@pytest.mark.parametrize("kind", DEPRECATED)
def test_constructor_warns_and_names_the_factory(plot, kind):
    make, hint = DEPRECATED[kind]
    with pytest.warns(DeprecationWarning, match=hint) as record:
        item = make(plot)
    assert record[0].filename == __file__
    assert shiboken6.isValid(item)


def test_panel_constructors_warn(panel):
    with pytest.warns(DeprecationWarning, match=r"use panel\.vertical_span"):
        MultiPlotsVerticalSpan(panel, R(1.0, 2.0))
    with pytest.warns(DeprecationWarning, match=r"use panel\.vertical_line"):
        MultiPlotsVerticalLine(panel, 1.0)


def test_factories_do_not_warn(plot, recwarn):
    plot.text("t", QPointF(1, 1))
    plot.vertical_span(R(1.0, 2.0))
    assert not [w for w in recwarn if issubclass(w.category, DeprecationWarning)]
```

The Task 1 tests call the constructors on purpose. Wrap those three tests' calls in `pytest.warns(DeprecationWarning)`.

- [ ] **Step 2: Run them and watch them fail.**

- [ ] **Step 3: Implement in `__init__.py`.** Data drives it: one table, and the existing wrapper grows a message.

```python
import warnings

_ITEM_FACTORIES = {
    SciQLopPlotsBindings.SciQLopTextItem: "plot.text",
    SciQLopPlotsBindings.SciQLopPixmapItem: "plot.pixmap",
    SciQLopPlotsBindings.SciQLopEllipseItem: "plot.ellipse",
    SciQLopPlotsBindings.SciQLopCurvedLineItem: "plot.curved_line",
    SciQLopPlotsBindings.SciQLopStraightLine: "plot.vertical_line(...) or plot.horizontal_line",
    SciQLopPlotsBindings.SciQLopVerticalLine: "plot.vertical_line",
    SciQLopPlotsBindings.SciQLopHorizontalLine: "plot.horizontal_line",
    SciQLopPlotsBindings.SciQLopVerticalSpan: "plot.vertical_span",
    SciQLopPlotsBindings.SciQLopHorizontalSpan: "plot.horizontal_span",
    SciQLopPlotsBindings.SciQLopRectangularSpan: "plot.rectangular_span",
    SciQLopPlotsBindings.MultiPlotsVerticalSpan: "panel.vertical_span",
    SciQLopPlotsBindings.MultiPlotsVerticalLine: "panel.vertical_line",
}


def _deprecated_constructor(cls, factory):
    init = _accept_ptr_handle(cls.__init__)
    owner = factory.split(".")[0]
    message = f"{cls.__name__}({owner}, ...) is deprecated: use {factory}(...)"

    @functools.wraps(init)
    def wrapper(self, *args, **kwargs):
        if type(self) is cls:
            warnings.warn(message, DeprecationWarning, stacklevel=2)
        init(self, *args, **kwargs)
    return wrapper


for _item_cls, _factory in _ITEM_FACTORIES.items():
    _item_cls.__init__ = _deprecated_constructor(_item_cls, _factory)
```

- This replaces the old `for _item_cls in (...)` loop.
- `type(self) is cls`: `SciQLopVerticalLine.__init__` does not go through `SciQLopStraightLine.__init__` in Python, but a Python subclass of an item might. That subclass gets the warning once, from its own class entry, or not at all if it is not in the table. Keep it simple.
- Update the comment block above it. One added sentence: the wrapper also warns that the constructors are deprecated.
- `stacklevel=2` names the caller: the wrapper is frame 1.
- C++ callers (the panel objects' `addObject`, wasm) do not go through `__init__`, so they do not warn.

- [ ] **Step 4: Make our deprecations errors in the suite.** In `pyproject.toml`, under `[tool.pytest.ini_options]`:

```toml
filterwarnings = [
    "error:.*is deprecated. use (plot|panel)\\.:DeprecationWarning",
]
```

The message regex is matched against the start of the message, so the leading `.*` is required. Only messages of the Global Constraints format become errors. Other deprecations (camelCase aliases) are untouched. `pytest.warns` blocks still capture.

- [ ] **Step 5: Wrap the kept calls.** Take each kept constructor or alias call listed in the Task 8 and 9 reports and wrap it in `with pytest.warns(DeprecationWarning):`. For a fixture or helper that builds many items through constructors (test_item_ownership.py), use `pytestmark = pytest.mark.filterwarnings("ignore:.*is deprecated. use plot:DeprecationWarning")` at module level, with a one-line comment saying the module is about the deprecated constructors.

- [ ] **Step 6: Run the new tests, then the full suite.** Any failure from the error filter is a missed call site: migrate it (Tasks 8-10 rules) and rerun until green. Then run the user guide test.

- [ ] **Step 7: ASan check of the plot's death with every item class.** This is one-off and is not committed as a test.
  - Is `build-asan` configured with `-Db_sanitize=address`? If its build is older than these commits, rebuild it: `meson compile -C build-asan -j 4`, alone, in the foreground, timeout 600000. A rebuild longer than 600 s is normal: rerun the same command, it resumes.
  - Then:
    ```bash
    cd /tmp && LD_PRELOAD=/usr/lib64/libasan.so.8 ASAN_OPTIONS=detect_leaks=0 QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-asan:/home/jeandet/Documents/prog/SciQLopPlots timeout 540 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_item_factories.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_plot_remove.py -q -p no:cacheprovider
    ```
  - Expected: passes with no ASan report. Put the summary line in the report.

- [ ] **Step 8: Commit**

```bash
git add SciQLopPlots/__init__.py pyproject.toml tests/
git commit -m "feat(items): item constructors are deprecated in favour of the plot and panel factories"
```

Stage `tests/` by explicit paths, not the directory. Use the files this task changed (`git status --short tests`).
