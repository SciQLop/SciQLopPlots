# One Base for Everything Drawn Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every drawn family (plottables, components, items, spans, panel-wide objects, plot groups, the legend) answers `visible` / `selected` / `name` / `color` through one QObject base, `SciQLopDrawnObject`, with one signal per change, and the five bugs the spec lists are fixed.

**Architecture:** The base owns the public accessors and the four signals. Its setters hop to the owner thread, compare, call a protected `_x` hook, read back and emit once; NeoQCP-driven changes go through `_notify_x`, which drops repeats. Families only override the hooks they support; the defaults raise `sqp::unsupported_call` (`NotImplementedError` in Python). The base lands alone first, then each family moves under it in its own task with its own reproducers and its rows in one parametrised contract test.

**Tech Stack:** C++20, Qt 6.11.2, NeoQCP, Shiboken6 6.11.2 (bindings.xml), Python 3.11+, pytest + pytest-qt.

**Spec:** `docs/plans/2026-10-22-drawn-object-base-design.md` (revision 5).

## Global Constraints

- Repo /home/jeandet/Documents/prog/SciQLopPlots, branch `feat/fills-histogram`; nothing is pushed. Commit with explicit paths; the untracked `qt.ini` is not ours. Messages end with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`. Never push.
- Build: `cd /home/jeandet/Documents/prog/SciQLopPlots && export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH" PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" LLVM_INSTALL_DIR=/usr && .venv/bin/meson compile -C build-venv -j 4`. Called BUILD below.
- After any bindings.xml or bound-header change (every task here): `cd /home/jeandet/Documents/prog/SciQLopPlots && find build-venv/SciQLopPlots/bindings -name '*_wrapper.cpp' -delete -o -name '*_wrapper.h' -delete && .venv/bin/meson setup --reconfigure build-venv`, then BUILD. Changing the base class of bound classes renumbers shiboken converters: a stale wrapper segfaults. Called CLEAN below.
- After every bindings change, grep the build output for shiboken warnings: `.venv/bin/meson compile -C build-venv -j 4 2>&1 | grep -i "signature\|property\|getter"` must print nothing about our classes. Shiboken only warns on a signature it can't find, and the entry then silently stops applying.
- Tests from /tmp: `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv:/home/jeandet/Documents/prog/SciQLopPlots /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest <files> -q -p no:cacheprovider`, with `<files>` as absolute paths. Called TEST below; `tests/...` paths below are relative to the repo.
- Full suite (FULL below): TEST `tests/unit tests/fuzzing tests/resampler tests/tracing` in one run; then `tests/integration` in two halves: `cd /home/jeandet/Documents/prog/SciQLopPlots && command ls tests/integration/test_*.py | sort | sed "s|^|$PWD/|" > /tmp/sqp-tests.txt && split -n l/2 -d /tmp/sqp-tests.txt /tmp/sqp-half-`, then TEST with `timeout 590` before the python and `$(command cat /tmp/sqp-half-00)`, then the same with `-01`. Never `tests/` (manual-tests hang on `app.exec`). Read the real pass/fail counts and exit codes.
- One build or test command at a time, foreground, waited out; timeout 600000 on the shell tool; never `run_in_background`, never `&`; `-j 4`. Nothing left running. `cat` is aliased to bat: use `command cat`.
- The base API, used verbatim by every task: public `visible()`, `set_visible(bool)`, `selected()`, `set_selected(bool)`, `name()`, `set_name(const QString&)`, `color()`, `set_color(const QColor&)`; signals `visible_changed(bool)`, `selection_changed(bool)`, `name_changed(const QString&)`, `color_changed(const QColor&)`; protected virtual hooks `_visible()`, `_set_visible(bool)`, `_selected()`, `_set_selected(bool)`, `_named(const QString&)`, `_color()`, `_set_color(const QColor&)`, `_always_apply()`; protected `_notify_visible/_notify_selected/_notify_color` and `_seed_visible/_seed_selected/_seed_color`.
- Hook overrides are protected, never `noexcept`, and marked `override`. The public accessors are not virtual: a family never redeclares them, nor the four signals (a same-named signal in a derived class is a different signal).
- Fields that would clash with a hook name are renamed: `_selected` → `m_selected`, `_visible` → `m_visible`, `_color` → `m_color`.
- Unsupported means `throw_unsupported("<what>", *this)` (include/SciQLopPlots/Debug.hpp): `NotImplementedError` "<what> is not supported by <Class>" in Python.
- No Python property is added. The plottables' existing `name` properties switch to `set="set_name"`; the plot's (`SciQLopPlotInterface`) and the panel's (`SciQLopMultiPlotPanel`) stay `setObjectName`. The panel span's and panel line's `visible` / `selected` / `color` properties stay.
- Style: small functions, data over code, comments only for "why".

## Review Focus

1. **A Python subclass overriding `set_visible` or the `_set_visible` hook** (the setter is no longer virtual): a call from a worker thread runs the Python override once, the C++ setter once on the GUI thread, and the hook override is honoured there. Test in Task 1.
2. **A plot deleted while a queued set is pending**: for a graph, Qt drops the call (the graph was its context); for a Python-owned item, the call runs with a null NeoQCP item and does nothing, with no signal. Tests in Task 2 and Task 5.
3. **An item whose NeoQCP item is gone** (its plot was deleted) when `set_selected` / `set_visible` / `set_color` is called: no crash, getters read `False` / invalid, no signal. Test in Task 5.
4. **A slot deleting the object while it applies a set** (a member's `visible_changed` slot calls `plot.remove_group(group)` during `group.set_visible`): no use after free; the base checks a `QPointer` before reading back. Test in Task 8.
5. **Legend aliases on SciQLop's real call pattern** (`legend().is_visible()` / `legend().set_visible(bool(v))` from `SciQLop/user_api/plot/_plots.py:501`, `visibility_changed`, and the inspector's legend checkbox, whose C++ connection moves to `visible_changed`). Test in Task 9.

---

### Task 1: The base class, alone, and the contract harness

**Files:**
- Create: `include/SciQLopPlots/SciQLopDrawnObject.hpp`, `src/SciQLopDrawnObject.cpp`
- Modify: `src/meson.build` (moc header list after `'../include/SciQLopPlots/SciQLopPlotRange.hpp',`; sources after `'Tracing.cpp',`)
- Modify: `SciQLopPlots/bindings/bindings.h` (include the header before `SciQLopGraphInterface.hpp`)
- Modify: `SciQLopPlots/bindings/bindings.xml` (template after `returning_raises_python_errors` ~line 108; object-type before `SciQLopGraphComponentInterface` ~line 307)
- Modify: `tests/integration/conftest.py` (two helpers)
- Test: `tests/integration/test_drawn_object.py`, `tests/integration/test_drawn_contract.py` (create)

**Interfaces:**
- Produces: the base API of Global Constraints; `conftest.record(signal) -> list`, `conftest.off_thread(call)`; in test_drawn_contract.py: `Row(id, classes, builders, cells, properties=frozenset())`, `ROWS`, `ABSTRACT`, `NOT_BUILDABLE`, `OK`, `RAISE`, the `hosts` fixture (`hosts.plot`, `.ts_plot`, `.nd`, `.panel`, `.qtbot`, `.keep`), constants `X`, `Y`, `Z`, `RED`, `BLUE`. Later tasks append rows and names to `ABSTRACT`.

- [ ] **Step 1: Helpers in conftest.py** (add `import threading` at the top, then):

```python
def record(signal):
    """Every value `signal` emits, in order."""
    seen = []
    signal.connect(seen.append)
    return seen


def off_thread(call):
    """Runs `call` on a plain Python thread and waits; the call itself must not block."""
    thread = threading.Thread(target=call)
    thread.start()
    thread.join(timeout=10)
    assert not thread.is_alive(), "the off-thread call blocked"
```

- [ ] **Step 2: Failing base tests** — `tests/integration/test_drawn_object.py`:

```python
"""SciQLopDrawnObject's rules, on Python subclasses (docs/plans/2026-10-22-drawn-object-base-design.md)."""
import pytest
from PySide6.QtGui import QColor

from SciQLopPlots import SciQLopDrawnObject
from conftest import off_thread, record


class Lamp(SciQLopDrawnObject):
    """Supports visible only, like a family overriding one pair of hooks."""

    def __init__(self):
        super().__init__()
        self.on = True
        self.applied = []

    def _visible(self):
        return self.on

    def _set_visible(self, visible):
        self.applied.append(visible)
        self.on = visible


class Dimmer(Lamp):
    """Overrides the public setter too, the way a user subclass might."""

    def __init__(self):
        super().__init__()
        self.calls = []

    def set_visible(self, visible):
        self.calls.append(visible)
        super().set_visible(visible)


class Pair(SciQLopDrawnObject):
    """An aggregate: visible while any part is, and a set reaches every part."""

    def __init__(self):
        super().__init__()
        self.parts = [Lamp(), Lamp()]
        for part in self.parts:
            part.visible_changed.connect(lambda _: self._notify_visible(self._visible()))
        self._seed_visible(self._visible())

    def _always_apply(self):
        return True

    def _visible(self):
        return any(p.visible() for p in self.parts)

    def _set_visible(self, visible):
        for part in self.parts:
            part.set_visible(visible)


def test_a_change_applies_once_and_signals_once(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(False)
    assert (lamp.visible(), lamp.applied, seen) == (False, [False], [False])


def test_the_same_value_neither_applies_nor_signals(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(True)
    assert (lamp.applied, seen) == ([], [])


def test_a_change_behind_the_setter_does_not_swallow_the_next_signal(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp.set_visible(False)
    lamp.on = True  # changed without the setter: no signal, the last emitted value is stale
    lamp.set_visible(False)
    assert seen == [False, False]


def test_notify_drops_repeats_and_seed_records_silently(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    lamp._seed_visible(True)
    lamp._notify_visible(True)
    lamp._notify_visible(False)
    lamp._notify_visible(False)
    assert seen == [False]


def test_a_set_from_a_worker_thread_waits_for_the_gui_thread(qtbot):
    lamp = Lamp()
    seen = record(lamp.visible_changed)
    off_thread(lambda: lamp.set_visible(False))
    assert (lamp.applied, seen) == ([], [])
    qtbot.waitUntil(lambda: seen == [False], timeout=2000)
    assert lamp.applied == [False]


def test_a_python_override_of_set_visible_runs_once_from_a_worker_thread(qtbot):
    dimmer = Dimmer()
    seen = record(dimmer.visible_changed)
    off_thread(lambda: dimmer.set_visible(False))
    qtbot.waitUntil(lambda: seen == [False], timeout=2000)
    assert (dimmer.calls, dimmer.applied) == ([False], [False])


@pytest.mark.parametrize("call", [
    lambda o: o.selected(), lambda o: o.set_selected(True),
    lambda o: o.color(), lambda o: o.set_color(QColor("red")),
], ids=["selected", "set_selected", "color", "set_color"])
def test_an_unsupported_attribute_raises_naming_the_class(qtbot, call):
    with pytest.raises(NotImplementedError, match="SciQLopDrawnObject"):
        call(Lamp())


def test_an_unsupported_set_from_a_worker_thread_only_warns(qtbot, qtlog):
    lamp = Lamp()
    off_thread(lambda: lamp.set_selected(True))
    qtbot.wait(50)  # the queued call runs here: a throw escaping it would std::terminate
    assert any("set_selected is not supported" in r.message for r in qtlog.records)
    assert lamp.visible()


def test_set_name_is_the_object_name_and_signals_once(qtbot):
    lamp = Lamp()
    seen = record(lamp.name_changed)
    lamp.set_name("desk")
    lamp.set_name("desk")
    assert (lamp.name(), lamp.objectName(), seen) == ("desk", "desk", ["desk"])


def test_an_aggregate_shows_a_hidden_part_without_signalling(qtbot):
    pair = Pair()
    pair.parts[0].set_visible(False)
    seen = record(pair.visible_changed)
    pair.set_visible(True)
    assert (pair.parts[0].visible(), seen) == (True, [])


def test_an_aggregate_signals_once_when_its_last_part_hides(qtbot):
    pair = Pair()
    seen = record(pair.visible_changed)
    pair.set_visible(False)
    assert seen == [False]
```

(`Lamp` objects are built from Python: their C++ type is shiboken's `SciQLopDrawnObjectWrapper`, which the error message names `SciQLopDrawnObject`.)

- [ ] **Step 3: Failing contract harness** — `tests/integration/test_drawn_contract.py`:

```python
"""Every drawn object answers visible / selected / name / color the same way.

The contract of docs/plans/2026-10-22-drawn-object-base-design.md, one row per family: a supported
attribute round-trips, signals exactly once, stays silent on the same value, and a set from a
worker thread waits for the GUI thread; an unsupported one raises NotImplementedError naming the
class. Every bound SciQLopDrawnObject subclass is in exactly one row, or listed as abstract.
"""
from collections import Counter
from dataclasses import dataclass

import numpy as np
import pytest
from PySide6.QtGui import QColor

import SciQLopPlots
from SciQLopPlots import (SciQLopDrawnObject, SciQLopMultiPlotPanel, SciQLopNDProjectionPlot,
                          SciQLopPlot, SciQLopPlotRange, SciQLopTimeSeriesPlot)
from conftest import off_thread, record

OK, RAISE = "ok", "raise"
X = np.linspace(0.0, 10.0, 50)
Y = np.linspace(0.0, 5.0, 20)
Z = np.random.default_rng(0).random((len(Y), len(X)))
RED, BLUE = QColor(200, 10, 10), QColor(10, 10, 200)
SIGNALS = {"visible": "visible_changed", "selected": "selection_changed",
           "name": "name_changed", "color": "color_changed"}


@dataclass(frozen=True)
class Row:
    """A family: the classes the spec lists, a builder for each concrete class they cover."""
    id: str
    classes: tuple
    builders: dict
    cells: dict
    properties: frozenset = frozenset()  # attributes read and written as Python properties


ROWS: list = []

# Bound bases never built on their own.
ABSTRACT = {"SciQLopDrawnObject"}
# Covered by a row, but no Python factory makes one.
NOT_BUILDABLE: dict = {}


def _ranged(make):
    def made():
        plot = make()
        if plot.x_axis() is not None:
            plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        if plot.time_axis() is not None:
            plot.time_axis().set_range(SciQLopPlotRange(0.0, 10.0))
        return plot
    return made


def _panel():
    panel = SciQLopMultiPlotPanel()
    panel.plot(X, np.sin(X))
    return panel


class Hosts:
    """One of each plot kind, made on first use, each with a range so callbacks fetch."""

    def __init__(self, qtbot):
        self.qtbot = qtbot
        self.keep = []  # Python callables wired to C++ signals must outlive the builder
        self._made = {}

    def _get(self, key, make):
        if key not in self._made:
            widget = make()
            self.qtbot.addWidget(widget)
            self._made[key] = widget
        return self._made[key]

    @property
    def plot(self):
        return self._get("plot", _ranged(SciQLopPlot))

    @property
    def ts_plot(self):
        return self._get("ts_plot", _ranged(SciQLopTimeSeriesPlot))

    @property
    def nd(self):
        return self._get("nd", _ranged(lambda: SciQLopNDProjectionPlot(3)))

    @property
    def panel(self):
        return self._get("panel", _panel)


@pytest.fixture
def hosts(qtbot):
    return Hosts(qtbot)


# --- rows: each task inserts its families above this line --------------------------------------
# (parametrize reads ROWS when the test functions below are defined, so rows must come first)


def _bound_subclasses(cls):
    for sub in cls.__subclasses__():
        if sub.__module__.startswith("SciQLopPlots"):
            yield sub
            yield from _bound_subclasses(sub)


def _closure(row):
    return set().union(*({name} | {s.__name__ for s in _bound_subclasses(getattr(SciQLopPlots, name))}
                         for name in row.classes))


def test_every_drawn_class_is_in_exactly_one_row_or_abstract():
    drawn = {c.__name__ for c in _bound_subclasses(SciQLopDrawnObject)}
    reached = Counter(name for row in ROWS for name in _closure(row))
    problems = [f"{n}: in no row" for n in sorted(drawn - set(reached) - ABSTRACT)]
    problems += [f"{n}: in {k} rows" for n, k in reached.items() if k > 1]
    problems += [f"{n}: in a row but not drawn" for n in sorted(set(reached) - drawn)]
    problems += [f"{row.id}: builders {sorted(row.builders)} != {sorted(_closure(row) - set(NOT_BUILDABLE))}"
                 for row in ROWS if set(row.builders) != _closure(row) - set(NOT_BUILDABLE)]
    assert not problems, "\n".join(problems)


def _row(name):
    return next(row for row in ROWS if name in row.builders)


def _get(obj, row, attr):
    value = getattr(obj, attr)
    return value if attr in row.properties else value()


def _set(obj, row, attr, value):
    if attr in row.properties:
        setattr(obj, attr, value)
    else:
        getattr(obj, "set_" + attr)(value)


def _changed(attr, current):
    if attr == "name":
        return current + "-renamed"
    if attr == "color":
        return BLUE if current == RED else RED
    return not current


def _any(attr):
    return {"visible": True, "selected": True, "name": "x", "color": RED}[attr]


def _cases(cell):
    return [pytest.param(name, attr, id=f"{name}-{attr}")
            for row in ROWS for name in row.builders
            for attr, kind in row.cells.items() if kind == cell]


@pytest.mark.parametrize("name, attr", _cases(OK))
def test_a_supported_attribute_round_trips_and_signals_once(hosts, name, attr):
    row = _row(name)
    obj = row.builders[name](hosts)
    value = _changed(attr, _get(obj, row, attr))
    seen = record(getattr(obj, SIGNALS[attr]))
    _set(obj, row, attr, value)
    assert _get(obj, row, attr) == value
    _set(obj, row, attr, value)
    assert seen == [value]


@pytest.mark.parametrize("name, attr", _cases(OK))
def test_a_set_from_a_worker_thread_waits_for_the_gui_thread(hosts, qtbot, name, attr):
    row = _row(name)
    obj = row.builders[name](hosts)
    before = _get(obj, row, attr)
    value = _changed(attr, before)
    seen = record(getattr(obj, SIGNALS[attr]))
    off_thread(lambda: _set(obj, row, attr, value))
    assert (_get(obj, row, attr), seen) == (before, [])
    qtbot.waitUntil(lambda: seen == [value], timeout=2000)
    assert _get(obj, row, attr) == value


@pytest.mark.parametrize("name, attr", _cases(RAISE))
def test_an_unsupported_attribute_raises_naming_the_class(hosts, name, attr):
    obj = _row(name).builders[name](hosts)
    with pytest.raises(NotImplementedError, match=type(obj).__name__):
        getattr(obj, attr)()
    with pytest.raises(NotImplementedError, match=type(obj).__name__):
        getattr(obj, "set_" + attr)(_any(attr))
```

- [ ] **Step 4: Run** TEST `tests/integration/test_drawn_object.py tests/integration/test_drawn_contract.py` — expected FAIL at collection: `ImportError: cannot import name 'SciQLopDrawnObject' from 'SciQLopPlots'`.

- [ ] **Step 5: The header** — `include/SciQLopPlots/SciQLopDrawnObject.hpp` (license block copied from `ThreadGuard.hpp`, Copyright 2026):

```cpp
#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <optional>

/*!
 * \brief What everything drawn shares: visible, selected, name and colour.
 *
 * The public setters own the signals (docs/plans/2026-10-22-drawn-object-base-design.md): off
 * the owner thread they re-post themselves; on it they compare, apply through a _x hook, read
 * back and emit once. A family overrides the hooks it supports; the defaults throw
 * sqp::unsupported_call, NotImplementedError in Python.
 */
class SciQLopDrawnObject : public QObject
{
    Q_OBJECT

    std::optional<bool> m_last_visible;
    std::optional<bool> m_last_selected;
    std::optional<QColor> m_last_color;

public:
    explicit SciQLopDrawnObject(QObject* parent = nullptr);
    ~SciQLopDrawnObject() override = default;

    bool visible() const;
    void set_visible(bool visible);
    bool selected() const;
    void set_selected(bool selected);
    QString name() const;
    void set_name(const QString& name);
    QColor color() const;
    void set_color(const QColor& color);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void visible_changed(bool visible);
    Q_SIGNAL void selection_changed(bool selected);
    Q_SIGNAL void name_changed(const QString& name);
    Q_SIGNAL void color_changed(const QColor& color);

protected:
    // Not noexcept: the defaults throw, and so may an override (a throw out of noexcept terminates).
    virtual bool _visible() const;
    virtual void _set_visible(bool visible);
    virtual bool _selected() const;
    virtual void _set_selected(bool selected);
    virtual void _named(const QString& name) { Q_UNUSED(name); }
    virtual QColor _color() const;
    virtual void _set_color(const QColor& color);

    //! Aggregates (a graph over its components, a group over its members) apply even when they
    //! already read the value: showing a visible group still shows its one hidden member.
    virtual bool _always_apply() const { return false; }

    //! For changes NeoQCP makes itself (a click selects an item): emit unless it is a repeat.
    void _notify_visible(bool visible);
    void _notify_selected(bool selected);
    void _notify_color(const QColor& color);

    //! Record the current value without emitting.
    void _seed_visible(bool visible) { m_last_visible = visible; }
    void _seed_selected(bool selected) { m_last_selected = selected; }
    void _seed_color(const QColor& color) { m_last_color = color; }
};
```

- [ ] **Step 6: The implementation** — `src/SciQLopDrawnObject.cpp` (same license block):

```cpp
#include "SciQLopPlots/SciQLopDrawnObject.hpp"
#include "SciQLopPlots/Debug.hpp"
#include "SciQLopPlots/ThreadGuard.hpp"

#include <QDebug>
#include <QPointer>
#include <utility>

namespace
{
//! A queued set has no caller left to raise to, and a throw out of a queued slot terminates the
//! process: anything thrown there only warns.
// simplify: a worker-thread caller never learns its set was unsupported; upgrade path = a
// completion signal or a future returned by the setter, if a caller ever needs it.
template <typename Call>
void run_queued(const Call& call)
{
    try
    {
        call();
    }
    catch (const std::exception& e)
    {
        qWarning() << "SciQLopPlots:" << e.what();
    }
}

//! Off the owner thread: re-run \a call there and return true, the setter then returns.
template <typename Call>
bool hopped(SciQLopDrawnObject* self, Call call)
{
    return SciQLopPlots::posted_to_owner_thread(self, [call] { run_queued(call); });
}

template <typename T>
using Getter = T (SciQLopDrawnObject::*)() const;
template <typename Arg>
using Setter = void (SciQLopDrawnObject::*)(Arg);

template <typename T, typename Arg>
void apply_change(SciQLopDrawnObject* self, bool always_apply, const T& value, Getter<T> get,
                  Setter<Arg> set, Setter<Arg> seed, Setter<Arg> notify)
{
    const T before = (self->*get)();
    if (before == value && !always_apply)
        return;
    // Seeded with the live value, before _set_x and never after: a change that bypassed the
    // setters must not make this one's signal look like a repeat.
    (self->*seed)(before);
    // A slot run while applying may delete self (a member's slot removing its group).
    QPointer<SciQLopDrawnObject> alive = self;
    (self->*set)(value);
    if (!alive)
        return;
    if (const T after = (self->*get)(); after != before)
        (self->*notify)(after);
}
}

SciQLopDrawnObject::SciQLopDrawnObject(QObject* parent) : QObject(parent) { }

bool SciQLopDrawnObject::visible() const
{
    return _visible();
}

void SciQLopDrawnObject::set_visible(bool visible)
{
    if (hopped(this, [this, visible] { set_visible(visible); }))
        return;
    apply_change(this, _always_apply(), visible, &SciQLopDrawnObject::_visible,
                 &SciQLopDrawnObject::_set_visible, &SciQLopDrawnObject::_seed_visible,
                 &SciQLopDrawnObject::_notify_visible);
}

bool SciQLopDrawnObject::selected() const
{
    return _selected();
}

void SciQLopDrawnObject::set_selected(bool selected)
{
    if (hopped(this, [this, selected] { set_selected(selected); }))
        return;
    apply_change(this, _always_apply(), selected, &SciQLopDrawnObject::_selected,
                 &SciQLopDrawnObject::_set_selected, &SciQLopDrawnObject::_seed_selected,
                 &SciQLopDrawnObject::_notify_selected);
}

QString SciQLopDrawnObject::name() const
{
    return objectName();
}

void SciQLopDrawnObject::set_name(const QString& name)
{
    if (hopped(this, [this, name] { set_name(name); }))
        return;
    if (objectName() == name)
        return;
    setObjectName(name);
    _named(name);
    Q_EMIT name_changed(name);
}

QColor SciQLopDrawnObject::color() const
{
    return _color();
}

void SciQLopDrawnObject::set_color(const QColor& color)
{
    if (hopped(this, [this, color] { set_color(color); }))
        return;
    apply_change(this, _always_apply(), color, &SciQLopDrawnObject::_color,
                 &SciQLopDrawnObject::_set_color, &SciQLopDrawnObject::_seed_color,
                 &SciQLopDrawnObject::_notify_color);
}

void SciQLopDrawnObject::_notify_visible(bool visible)
{
    if (std::exchange(m_last_visible, visible) != visible)
        Q_EMIT visible_changed(visible);
}

void SciQLopDrawnObject::_notify_selected(bool selected)
{
    if (std::exchange(m_last_selected, selected) != selected)
        Q_EMIT selection_changed(selected);
}

void SciQLopDrawnObject::_notify_color(const QColor& color)
{
    if (std::exchange(m_last_color, color) != color)
        Q_EMIT color_changed(color);
}

bool SciQLopDrawnObject::_visible() const
{
    throw_unsupported("visible", *this);
}

void SciQLopDrawnObject::_set_visible(bool)
{
    throw_unsupported("set_visible", *this);
}

bool SciQLopDrawnObject::_selected() const
{
    throw_unsupported("selected", *this);
}

void SciQLopDrawnObject::_set_selected(bool)
{
    throw_unsupported("set_selected", *this);
}

QColor SciQLopDrawnObject::_color() const
{
    throw_unsupported("color", *this);
}

void SciQLopDrawnObject::_set_color(const QColor&)
{
    throw_unsupported("set_color", *this);
}
```

- [ ] **Step 7: Meson and bindings.h** — `src/meson.build`: add `'../include/SciQLopPlots/SciQLopDrawnObject.hpp',` to `sciqlopplots_moc_headers` after `SciQLopPlotRange.hpp`, and `'SciQLopDrawnObject.cpp',` to `sciqlopplots_sources` after `'Tracing.cpp',` (the wasm build reuses both lists). `SciQLopPlots/bindings/bindings.h`: `#include <SciQLopPlots/SciQLopDrawnObject.hpp>` before the `SciQLopGraphInterface.hpp` include.

- [ ] **Step 8: bindings.xml** — after the `returning_raises_python_errors` template:

```xml
    <template name="void_raises_python_errors">
        try {
            %CPPSELF.%FUNCTION_NAME(%ARGUMENT_NAMES);
        } catch (const sqp::unsupported_call&amp; e) {
            PyErr_SetString(PyExc_NotImplementedError, e.what());
            return nullptr;
        } catch (const std::exception&amp; e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
            return nullptr;
        }
    </template>
```

  and before `<object-type name="SciQLopGraphComponentInterface"`:

```xml
    <!-- The accessors of everything drawn: an unsupported one is a NotImplementedError. -->
    <object-type name="SciQLopDrawnObject" parent-management="yes">
        <modify-function signature="visible() const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="bool"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="selected() const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="bool"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="color() const">
            <inject-code class="target" position="beginning">
                <insert-template name="returning_raises_python_errors"><replace from="$TYPE" to="QColor"/></insert-template>
            </inject-code>
        </modify-function>
        <modify-function signature="set_visible(bool)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_python_errors"/>
            </inject-code>
        </modify-function>
        <modify-function signature="set_selected(bool)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_python_errors"/>
            </inject-code>
        </modify-function>
        <modify-function signature="set_color(const QColor&amp;)">
            <inject-code class="target" position="beginning">
                <insert-template name="void_raises_python_errors"/>
            </inject-code>
        </modify-function>
    </object-type>
```

  The protected hooks, `_notify_*` and `_seed_*` stay bound on purpose: they are the extension points a Python subclass uses (Step 2's `Lamp` and `Pair`).

- [ ] **Step 9: Build** — CLEAN, BUILD, the shiboken-warning grep (none for `SciQLopDrawnObject`).
- [ ] **Step 10: Run** TEST `tests/integration/test_drawn_object.py tests/integration/test_drawn_contract.py` — PASS (the contract's parametrised tests are skipped with an empty parameter set: no family uses the base yet, so no full suite here).
- [ ] **Step 11: Commit**

```bash
git add include/SciQLopPlots/SciQLopDrawnObject.hpp src/SciQLopDrawnObject.cpp src/meson.build \
    SciQLopPlots/bindings/bindings.h SciQLopPlots/bindings/bindings.xml tests/integration/conftest.py \
    tests/integration/test_drawn_object.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): SciQLopDrawnObject, one base for visible/selected/name/color (API review 5.3)

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Plottables under the base (graph selection bug, timeline signal, name through set_name)

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp` (base class; drop :69 and :259 `Q_PROPERTY(selected)` (nothing reads it, here or in SciQLop; a Qt property reader would also get an exception from families that raise, so the base declares none), :85 `set_visible`, :87-91 `set_name`, :101-107 `visible`/`name`, :159-165 `set_selected`/`selected`, signals :225, :226, :232; add hooks)
- Modify: `src/SciQLopGraphInterface.cpp:34-41` (the `SciQLopPlottableInterface` constructor: new base, drop the `objectNameChanged → name_changed` connection at :40)
- Modify: `include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp` (drop the dead free functions `_set_selected<T>` and `set_selected(QCPAbstractPlottable*, bool)`, :28-50: a member `_set_selected` would hide them; hooks), `src/QCPAbstractPlottableWrapper.cpp:73-169`
- Modify: `include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp:74-81`, `SciQLopLineGraph.hpp:70` + `src/SciQLopLineGraph.cpp:67-71`, `SciQLopCurve.hpp:92` + `src/SciQLopCurve.cpp:330-334`
- Modify: `include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp:70-80` + `src/SciQLopNDProjectionCurves.cpp:92-96,146-158,329-343`
- Modify: `include/SciQLopPlots/Plotables/SciQLopHistogram.hpp:42,92,102-105` + `src/SciQLopHistogram.cpp:330-377`
- Modify: `include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp:36,78-86,119-135` + `src/SciQLopColorMapBase.cpp:88-103`
- Modify: `include/SciQLopPlots/Plotables/SciQLopTimeline.hpp:70-74` + `src/SciQLopTimeline.cpp:75-81,213-217,268-278`
- Modify: `SciQLopPlots/bindings/bindings.xml` (every plottable `name` property: `set="set_name"`)
- Test: `tests/integration/test_drawn_plottables.py` (create), `tests/integration/test_drawn_contract.py` (rows)

**Interfaces:**
- Consumes: Task 1's base API and harness.
- Produces: `SciQLopPlottableInterface : SciQLopDrawnObject`; `SQPQCPAbstractPlottableWrapper::_always_apply() == true` (the graph's aggregate signals from its parts start in Task 3; until then only the setter path signals) with `_visible` / `_selected` = any component; `SciQLopGraphInterface::_color` / `_set_color` raising "...; use colors()" / "...; use set_colors()"; contract rows `graph`, `colour map`, `histogram`, `timeline`.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_plottables.py`:

```python
"""Plottables under SciQLopDrawnObject: the bugs the move fixes."""
import numpy as np
import pytest
from PySide6.QtCore import QCoreApplication, QEvent

from SciQLopPlots import SciQLopPlot, SciQLopPlotRange
from conftest import off_thread, process_events, record

X = np.linspace(0.0, 10.0, 50)
TWO = np.column_stack([np.sin(X), np.cos(X)])


def test_selecting_a_graph_selects_its_components(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    graph.set_selected(True)
    assert [c.selected() for c in graph.components()] == [True, True]
    assert graph.selected()


def test_hiding_a_timeline_signals_once(ts_plot):
    timeline = ts_plot.add_timeline()
    seen = record(timeline.visible_changed)
    timeline.set_visible(False)
    timeline.set_visible(False)
    assert (timeline.visible(), seen) == (False, [False])


def test_the_name_property_goes_through_set_name(qtbot, plot):
    """m_user_named is set: a callback graph's late components then take the name."""
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    graph = plot.plot(lambda start, stop: [X, np.sin(X)])
    graph.name = "speed"
    qtbot.waitUntil(lambda: len(graph.components()) == 1, timeout=3000)
    assert graph.components()[0].name() == "speed"


def test_renaming_a_plottable_signals_once(plot):  # guard
    graph = plot.plot(X, np.sin(X), labels=["a"])
    seen = record(graph.name_changed)
    graph.name = "trace"
    graph.set_name("trace")
    assert (graph.name, seen) == ("trace", ["trace"])


def test_a_graph_colour_points_to_colors(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    with pytest.raises(NotImplementedError, match="use colors"):
        graph.color()


def test_a_queued_set_on_a_graph_of_a_deleted_plot_is_dropped(qtbot):  # Review Focus 2
    plot = SciQLopPlot()
    graph = plot.plot(X, np.sin(X), labels=["a"])
    off_thread(lambda: graph.set_visible(False))
    plot.deleteLater()
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    process_events()  # the graph was the queued call's context: Qt dropped the call with it
```

  In `tests/integration/test_drawn_contract.py`, insert above the rows marker (and add `GraphType` to the `SciQLopPlots` import):

```python
LINES = np.column_stack([np.sin(X), np.cos(X)])


def _with_components(hosts, graph):
    hosts.qtbot.waitUntil(lambda: len(graph.components()) > 0, timeout=3000)
    return graph


def _shown(hosts, plottable):
    hosts.qtbot.waitUntil(plottable.visible, timeout=3000)
    return plottable


def _fed(hosts, graph, *batch):
    """A remote graph answering every request with `batch`; a new x range asks for one."""
    channel = graph.remote_channel()
    answer = lambda _range: channel.set_data(*batch)
    channel.data_requested.connect(answer)
    hosts.keep.append(answer)
    hosts.plot.x_axis().set_range(SciQLopPlotRange(1.0, 9.0))
    return _with_components(hosts, graph)


def _timeline(hosts):
    timeline = hosts.ts_plot.add_timeline()
    timeline.set_intervals([0.0, 6.0], [5.0, 9.0], lane=["A", "A"])
    return timeline


PLOTTABLE_CELLS = {"visible": OK, "selected": OK, "name": OK, "color": RAISE}

ROWS += [
    Row("graph", ("SciQLopLineGraph", "SciQLopCurve", "SciQLopWaterfallGraph", "SciQLopNDProjectionCurves"), {
        "SciQLopLineGraph": lambda h: h.plot.plot(X, LINES, labels=["a", "b"]),
        "SciQLopLineGraphFunction": lambda h: _with_components(h, h.plot.plot(lambda a, b: [X, LINES])),
        "SciQLopLineGraphRemote": lambda h: _fed(h, h.plot.add_remote_line_graph(["a", "b"]), X, LINES),
        "SciQLopCurve": lambda h: h.plot.plot(X, np.sin(X), graph_type=GraphType.ParametricCurve),
        "SciQLopCurveFunction": lambda h: _with_components(
            h, h.plot.plot(lambda a, b: [X, np.sin(X)], graph_type=GraphType.ParametricCurve)),
        "SciQLopCurveRemote": lambda h: _fed(h, h.plot.add_remote_curve(["c"]), X, np.sin(X)),
        "SciQLopWaterfallGraph": lambda h: h.plot.plot(X, LINES, graph_type=GraphType.Waterfall),
        "SciQLopWaterfallGraphFunction": lambda h: _with_components(
            h, h.plot.plot(lambda a, b: [X, LINES], graph_type=GraphType.Waterfall)),
        "SciQLopWaterfallGraphRemote": lambda h: _fed(h, h.plot.add_remote_waterfall(["a", "b"]), X, LINES),
        "SciQLopNDProjectionCurves": lambda h: _with_components(h, h.nd.plot([X, X, np.sin(X), np.cos(X)])),
        "SciQLopNDProjectionCurvesFunction": lambda h: _with_components(
            h, h.nd.plot(lambda a, b: [X, X, np.sin(X), np.cos(X)], graph_type=GraphType.ParametricCurve)),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("colour map", ("SciQLopColorMap", "SciQLopHistogram2D"), {
        "SciQLopColorMap": lambda h: h.plot.colormap(X, Y, Z),
        "SciQLopColorMapFunction": lambda h: h.plot.plot(lambda a, b: [X, Y, Z], graph_type=GraphType.ColorMap),
        "SciQLopColorMapRemote": lambda h: h.plot.add_remote_color_map("cm"),
        "SciQLopHistogram2D": lambda h: h.plot.histogram2d(X, np.sin(X)),
        "SciQLopHistogram2DFunction": lambda h: h.plot.plot(lambda a, b: [X, np.sin(X)],
                                                            graph_type=GraphType.Histogram2D),
        "SciQLopHistogram2DRemote": lambda h: h.plot.add_remote_histogram2d("h"),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("histogram", ("SciQLopHistogram",), {
        "SciQLopHistogram": lambda h: h.plot.histogram(np.sin(X)),
        "SciQLopHistogramFunction": lambda h: _shown(h, h.plot.histogram(lambda a, b: [np.sin(X)])),
    }, PLOTTABLE_CELLS, frozenset({"name"})),
    Row("timeline", ("SciQLopTimeline",), {"SciQLopTimeline": _timeline},
        PLOTTABLE_CELLS, frozenset({"name"})),
]
ABSTRACT |= {"SciQLopPlottableInterface", "SciQLopGraphInterface", "SQPQCPAbstractPlottableWrapper",
             "SciQLopMultiGraphBase", "SciQLopColorMapInterface", "SciQLopColorMapBase"}
NOT_BUILDABLE["SciQLopHistogramRemote"] = "no Python factory makes one"
```

  If a builder's factory call is spelled differently in the bindings (a keyword, a return shape), fix the builder, never the contract.

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_plottables.py tests/integration/test_drawn_contract.py` — expected FAIL: components not selected (`[False, False]`), timeline `seen == []`, component name `''` not `'speed'`, `AttributeError: ... 'color'` on the graph, and the coverage test reporting every plottable class as `in a row but not drawn`. The two guards pass.

- [ ] **Step 3: SciQLopGraphInterface.hpp** — `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`; `class SciQLopPlottableInterface : public SciQLopDrawnObject`; delete the lines listed in Files. In the existing `protected:` block at the end of `SciQLopPlottableInterface` (after `bool _got_first_data`), add:

```cpp
    //! A name given through set_name() is the caller's: it then labels late components.
    void _named(const QString& name) override
    {
        Q_UNUSED(name);
        m_user_named = true;
    }
```

  At the end of `SciQLopGraphInterface` (after the `#endif` of its `#ifndef BINDINGS_H` block):

```cpp
protected:
    // A graph has one colour per component.
    QColor _color() const override
    {
        throw sqp::unsupported_call(unsupported_message("color", typeid(*this).name())
                                    + "; use colors()");
    }

    void _set_color(const QColor&) override
    {
        throw sqp::unsupported_call(unsupported_message("set_color", typeid(*this).name())
                                    + "; use set_colors()");
    }
```

  `src/SciQLopGraphInterface.cpp`: the constructor becomes `: SciQLopDrawnObject(parent)` and loses its body's `connect(this, &QObject::objectNameChanged, ...)` (the base's `set_name` emits `name_changed` once; the connection would make it twice).

- [ ] **Step 4: The graph aggregate** — in `QCPAbstractPlottableWrapper.hpp` delete the free `_set_selected<T>` template and `set_selected(QCPAbstractPlottable*, bool)` (callers: none), drop the public `set_visible`, `visible`, `set_selected`, `selected` declarations, and add before the `#ifdef BINDINGS_H` signals block:

```cpp
protected:
    bool _always_apply() const override { return true; }
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;
```

  In `src/QCPAbstractPlottableWrapper.cpp` replace `set_visible`, `visible`, `set_selected` (the commented-out loop) and `selected` with (add `#include <algorithm>`):

```cpp
// The components are copied: a slot of one may delete another.
void SQPQCPAbstractPlottableWrapper::_set_visible(bool visible)
{
    const auto components = m_components;
    for (const auto& component : components)
        if (component)
            component->set_visible(visible);
}

bool SQPQCPAbstractPlottableWrapper::_visible() const
{
    return std::ranges::any_of(m_components, [](const auto& c) { return c && c->visible(); });
}

void SQPQCPAbstractPlottableWrapper::_set_selected(bool selected)
{
    const auto components = m_components;
    for (const auto& component : components)
        if (component)
            component->set_selected(selected);
}

bool SQPQCPAbstractPlottableWrapper::_selected() const
{
    return std::ranges::any_of(m_components, [](const auto& c) { return c && c->selected(); });
}
```

- [ ] **Step 5: Line graphs, curves, multigraph name** — `SciQLopLineGraph.hpp`: replace `void set_visible(bool visible) noexcept override;` with a `protected:` `void _set_visible(bool visible) override;`; `src/SciQLopLineGraph.cpp`:

```cpp
void SciQLopLineGraph::_set_visible(bool visible)
{
    SciQLopMultiGraphBase::_set_visible(visible);
    notify_color_scale();
}
```

  Same in `SciQLopCurve.hpp` / `src/SciQLopCurve.cpp` with `SQPQCPAbstractPlottableWrapper::_set_visible(visible);`. `SciQLopMultiGraphBase.hpp`: replace `set_name` (keep its comment) with

```cpp
protected:
    void _named(const QString& name) override
    {
        SQPQCPAbstractPlottableWrapper::_named(name);
        if (_multiGraph)
            _multiGraph->setName(name);
    }

public:
```

- [ ] **Step 6: Projection curves** — `SciQLopNDProjectionCurves.hpp`: drop `set_name`, `set_selected`, `selected`, `set_visible`, `visible`; add (keep the "also renames every pane's legend entry" comment on `_named`):

```cpp
protected:
    bool _always_apply() const override { return true; }
    void _named(const QString& name) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;
    bool _visible() const override;
    void _set_visible(bool visible) override;
```

  `src/SciQLopNDProjectionCurves.cpp`:

```cpp
void SciQLopNDProjectionCurves::_named(const QString& name)
{
    SciQLopGraphInterface::_named(name);
    for (auto* component : components())
        component->set_name(name);
}

void SciQLopNDProjectionCurves::_set_selected(bool selected)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_selected(selected);
}

bool SciQLopNDProjectionCurves::_selected() const
{
    return std::ranges::any_of(m_curves, [](const SciQLopCurve* c) { return c->selected(); });
}

void SciQLopNDProjectionCurves::_set_visible(bool visible)
{
    for (auto* curve : std::as_const(m_curves))
        curve->set_visible(visible);
    _update_color_scale();
}

bool SciQLopNDProjectionCurves::_visible() const
{
    return std::ranges::any_of(m_curves, [](const SciQLopCurve* c) { return c->visible(); });
}
```

  (`selected` was the first pane's curve only; any pane is the aggregate rule.)

- [ ] **Step 7: Histograms and colour maps** — first rename the field that would clash with the hook: `sed -i 's/\b_selected\b/m_selected/g' include/SciQLopPlots/Plotables/SciQLopHistogram.hpp src/SciQLopHistogram.cpp include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp src/SciQLopColorMapBase.cpp` (the only `_selected` there today is the field and its uses), then check `grep -n "m_selected\|_selected" src/SciQLopColorMap.cpp src/SciQLopHistogram2D.cpp` shows only the `set_selected` connects.

  `SciQLopHistogram.hpp`: drop `set_name`, `set_visible`, `visible`, `set_selected`, `selected`; add

```cpp
protected:
    void _named(const QString& name) override;
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;
```

  `src/SciQLopHistogram.cpp`:

```cpp
void SciQLopHistogram::_named(const QString& name)
{
    SciQLopGraphInterface::_named(name);
    if (_hist)
        _hist->setName(name);
}

void SciQLopHistogram::_set_visible(bool visible)
{
    if (!_hist)
        return;
    _hist->setVisible(visible);
    Q_EMIT replot();
}

bool SciQLopHistogram::_visible() const
{
    return _hist && _hist->visible();
}

void SciQLopHistogram::_set_selected(bool selected)
{
    m_selected = selected;
    if (auto* legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
        legend_item->setSelected(selected);
}
```

  `SciQLopColorMapBase.hpp`: drop `set_name`, `set_selected`, `selected`, `set_visible`, `visible`; add

```cpp
protected:
    void _named(const QString& name) override
    {
        SciQLopColorMapInterface::_named(name);
        if (auto* p = plottable(); p)
            p->setName(name);
    }

    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;

    bool _visible() const override
    {
        auto* p = plottable();
        return p && p->visible();
    }

    void _set_visible(bool visible) override
    {
        if (auto* p = plottable(); p)
        {
            p->setVisible(visible);
            if (auto* plot = _plot())
                plot->replot(QCustomPlot::rpQueuedReplot);
        }
    }

public:
```

  `src/SciQLopColorMapBase.cpp`:

```cpp
void SciQLopColorMapBase::_set_selected(bool selected)
{
    m_selected = selected;
    if (auto* legend_item = _legend_item(); legend_item && legend_item->selected() != selected)
        legend_item->setSelected(selected);
}
```

  The legend-item connects (`&SciQLopColorMap::set_selected`, `&SciQLopHistogram2D::set_selected`, `&SciQLopHistogram::set_selected`) now name the base's setter and compile unchanged.

- [ ] **Step 8: Timeline** — `SciQLopTimeline.hpp`: drop `set_visible`, `visible`, `selected`, `set_selected`; add at the end of the class

```cpp
protected:
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override;
    void _set_selected(bool selected) override;
```

  `src/SciQLopTimeline.cpp`:

```cpp
void SciQLopTimeline::_set_visible(bool visible)
{
    if (_intervals)
        _intervals->setVisible(visible);
    emit replot();
}

bool SciQLopTimeline::_visible() const { return _intervals && _intervals->visible(); }

bool SciQLopTimeline::_selected() const { return _intervals && _intervals->selected(); }

void SciQLopTimeline::_set_selected(bool selected)
{
    if (!_intervals)
        return;
    QVector<int> rows;
    if (selected)
        for (int row = 0; row < count(); ++row)
            rows.append(row);
    _intervals->setSelectedRows(rows);
    emit replot();
}
```

  and in the constructor's `QCPAbstractPlottable::selectionChanged` lambda replace `emit selection_changed(selected());` with `_notify_selected(selected());` (the selected set changes more often than the flag).

- [ ] **Step 9: The name property** — in bindings.xml every plottable's `name` property goes through `set_name`, except the plot's and the panel's:

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots && .venv/bin/python - <<'EOF'
import pathlib, re
path = pathlib.Path("SciQLopPlots/bindings/bindings.xml")
owner, out = None, []
for line in path.read_text().splitlines(keepends=True):
    if m := re.search(r'<object-type name="([^"]+)"', line):
        owner = m.group(1)
    if owner not in ("SciQLopPlotInterface", "SciQLopMultiPlotPanel"):
        line = line.replace('get="objectName" set="setObjectName"', 'get="objectName" set="set_name"')
    out.append(line)
path.write_text("".join(out))
EOF
grep -n 'set="setObjectName"' SciQLopPlots/bindings/bindings.xml
```

  Expected: exactly two lines, in `SciQLopPlotInterface` (~705) and `SciQLopMultiPlotPanel` (~1637).

- [ ] **Step 10: Leftover qualified calls** — `grep -rn "::set_visible(\|::set_name(\|::set_selected(\|::visible()\|::selected()\|visible() const noexcept\|selected() const noexcept" src/SciQLop{GraphInterface,Curve,LineGraph,MultiGraphBase,NDProjectionCurves,Histogram,ColorMapBase,Timeline}.cpp src/QCPAbstractPlottableWrapper.cpp include/SciQLopPlots/Plotables/` prints nothing except the graph-component lines (`SciQLopGraphComponent*`, `SciQLopGraphComponentInterface.hpp`), which Task 4 moves.
- [ ] **Step 11: Build** — this is the first task that changes the base of bound classes: CLEAN is mandatory here (every `*_wrapper.cpp` removed and a reconfigure; a stale wrapper segfaults on import), then BUILD, then the shiboken-warning grep.
- [ ] **Step 12: Run** TEST `tests/integration/test_drawn_plottables.py tests/integration/test_drawn_contract.py tests/integration/test_plot_name_kwarg.py tests/integration/test_visibility.py tests/integration/test_timeline.py` — PASS. Then FULL.
- [ ] **Step 13: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp src/SciQLopGraphInterface.cpp \
    include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp src/QCPAbstractPlottableWrapper.cpp \
    include/SciQLopPlots/Plotables/SciQLopMultiGraphBase.hpp include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp \
    src/SciQLopLineGraph.cpp include/SciQLopPlots/Plotables/SciQLopCurve.hpp src/SciQLopCurve.cpp \
    include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp src/SciQLopNDProjectionCurves.cpp \
    include/SciQLopPlots/Plotables/SciQLopHistogram.hpp src/SciQLopHistogram.cpp \
    include/SciQLopPlots/Plotables/SciQLopColorMapBase.hpp src/SciQLopColorMapBase.cpp \
    include/SciQLopPlots/Plotables/SciQLopTimeline.hpp src/SciQLopTimeline.cpp \
    SciQLopPlots/bindings/bindings.xml tests/integration/test_drawn_plottables.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): plottables under SciQLopDrawnObject

graph.set_selected() selects the components (the loop was commented out), the timeline
signals visible_changed, and the Python name property goes through set_name().

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Aggregates follow their parts (graphs, projection curves)

**Files:**
- Modify: `include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp` (`clear_plottables`, `_seed_parts` declaration), `src/QCPAbstractPlottableWrapper.cpp` (`_register_component`, `_forget_destroyed_components`)
- Modify: `include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp`, `src/SciQLopNDProjectionCurves.cpp` (constructor, ~:66-89)
- Test: `tests/integration/test_drawn_aggregates.py` (create)

**Interfaces:**
- Consumes: Task 2's aggregate hooks.
- Produces: rule 2 on graphs and projection curves: seeded when built and when a part comes or goes, `_notify_*` on each part's signal. A part's change that flips the aggregate signals once on the aggregate; one that doesn't flip it signals nothing there.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_aggregates.py`:

```python
"""Rule 2: an aggregate reads its parts and signals only when that reading flips."""
import numpy as np

from conftest import process_events, record

X = np.linspace(0.0, 10.0, 50)
TWO = np.column_stack([np.sin(X), np.cos(X)])


def _two(plot):
    graph = plot.plot(X, TWO, labels=["a", "b"])
    return graph, graph.components()


def test_hiding_components_one_by_one_hides_the_graph_once(plot):
    graph, (a, b) = _two(plot)
    seen = record(graph.visible_changed)
    a.set_visible(False)
    assert seen == []  # b still shows
    b.set_visible(False)
    assert seen == [False]


def test_selecting_components_one_by_one_selects_the_graph_once(plot):
    graph, (a, b) = _two(plot)
    seen = record(graph.selection_changed)
    a.set_selected(True)  # what a click on its legend row does
    b.set_selected(True)
    assert seen == [True]


def test_selecting_a_partly_selected_graph_selects_both_silently(plot):  # guard after Task 2
    graph, (a, b) = _two(plot)
    a.set_selected(True)
    seen = record(graph.selection_changed)
    graph.set_selected(True)
    assert (a.selected(), b.selected(), seen) == (True, True, [])


def test_a_removed_component_reseeds_the_graph(plot):
    graph, (a, _) = _two(plot)
    a.set_visible(False)
    graph.set_data(X, np.sin(X)[:, None])  # one column: the second component goes
    process_events()
    assert len(graph.components()) == 1 and not graph.visible()
    seen = record(graph.visible_changed)
    graph.components()[0].set_visible(True)
    assert seen == [True]


def test_hiding_every_pane_curve_hides_the_projection_once(qtbot):
    from SciQLopPlots import SciQLopNDProjectionPlot
    nd = SciQLopNDProjectionPlot(3)
    qtbot.addWidget(nd)
    projection = nd.plot([X, X, np.sin(X), np.cos(X)])
    qtbot.waitUntil(lambda: len(projection.components()) == 3, timeout=3000)
    seen = record(projection.visible_changed)
    for component in projection.components():
        component.set_visible(False)
    assert seen == [False]
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_aggregates.py` — expected FAIL: `seen == []` where `[False]` / `[True]` is expected (nothing follows the parts yet); the guard passes.
- [ ] **Step 3: Graphs** — `QCPAbstractPlottableWrapper.hpp`: declare `void _seed_parts();` next to `_register_component`, and make `clear_plottables` (the destructor's call included) stop listening before it deletes:

```cpp
    inline void clear_plottables()
    {
        for (auto component : std::exchange(m_components, {}))
            if (component)
            {
                // A dying component signals; this graph may be half destroyed already. This drops
                // every component-to-graph connection (replot included): only teardown and a
                // shrinking set_data reach here, and both rebuild or discard the components.
                disconnect(component, nullptr, this, nullptr);
                delete component.data();
            }
        _seed_parts();
    }
```

  `src/QCPAbstractPlottableWrapper.cpp` — in `_register_component`, after the existing `connect(component, &QObject::destroyed, ...)`:

```cpp
    connect(component, &SciQLopGraphComponentInterface::visible_changed, this,
            [this] { _notify_visible(_visible()); });
    connect(component, &SciQLopGraphComponentInterface::selection_changed, this,
            [this] { _notify_selected(_selected()); });
    _seed_parts();
```

  in `_forget_destroyed_components`, reseed whatever was removed (a `takeLast()` + `delete` path removed it before this slot ran):

```cpp
void SQPQCPAbstractPlottableWrapper::_forget_destroyed_components()
{
    // destroyed() fires after Qt has nulled the QPointer to the dying component.
    const bool forgot = m_components.removeIf([](const auto& c) { return c.isNull(); }) > 0;
    _seed_parts();
    if (forgot)
        Q_EMIT this->component_list_changed();
}

void SQPQCPAbstractPlottableWrapper::_seed_parts()
{
    _seed_visible(_visible());
    _seed_selected(_selected());
}
```

- [ ] **Step 4: Projection curves** — declare `void _seed_parts();` in the private part of `SciQLopNDProjectionCurves`. In the constructor's `if (curve)` block add the two follows and reseed on a pane's curve going; after the loop, seed:

```cpp
            connect(curve, &SciQLopDrawnObject::visible_changed, this,
                    [this] { _notify_visible(_visible()); });
            connect(curve, &SciQLopDrawnObject::selection_changed, this,
                    [this] { _notify_selected(_selected()); });
            // A pane can go first (the whole plot torn down): forget its curve then.
            connect(curve, &QObject::destroyed, this,
                    [this, curve]
                    {
                        m_curves.removeOne(curve);
                        _seed_parts();
                    });
        }
    }
    _seed_parts();
}

void SciQLopNDProjectionCurves::_seed_parts()
{
    _seed_visible(_visible());
    _seed_selected(_selected());
}
```

  (replacing the existing `destroyed` connection.)
- [ ] **Step 5: Build** — CLEAN, BUILD.
- [ ] **Step 6: Run** TEST `tests/integration/test_drawn_aggregates.py tests/integration/test_drawn_contract.py tests/integration/test_inspector_delete_selected.py tests/integration/test_remove_plottable.py` — PASS. Then FULL.
- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/Plotables/QCPAbstractPlottableWrapper.hpp src/QCPAbstractPlottableWrapper.cpp \
    include/SciQLopPlots/Plotables/SciQLopNDProjectionCurves.hpp src/SciQLopNDProjectionCurves.cpp \
    tests/integration/test_drawn_aggregates.py
git commit -m "feat(drawn): graphs and projections signal when their parts flip them

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Components under the base

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp` (base class; drop :44 `Q_PROPERTY` (see Task 2), :57 `set_color`, :69 `set_visible`, :71 `set_name`, :73-77 `set_selected`, :85-89 `color`, :121-125 `visible`, :141 `name`, :143-147 `selected`, signals :168, :169, :176)
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp` (drop the inline `set_color`, `visible`, `name`, `selected` and the `set_visible`, `set_name`, `set_selected`, `color` declarations; add hooks), `src/SciQLopGraphComponent.cpp` (`set_visible` :137, `set_name` :156, `set_selected` :179, `color` :251)
- Test: `tests/integration/test_drawn_components.py` (create), `tests/integration/test_drawn_contract.py` (row)

**Interfaces:**
- Consumes: the base API.
- Produces: `SciQLopGraphComponentInterface : SciQLopDrawnObject` (also the Python face of the unbound `SciQLopGraphComponent`); components keep their NeoQCP signals wired to the public `set_selected` (the constructors' connects, unchanged) and cache `m_selected`.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_components.py`:

```python
"""Components under SciQLopDrawnObject."""
import numpy as np
from PySide6.QtGui import QColor

from conftest import record

X = np.linspace(0.0, 10.0, 50)


def test_renaming_a_component_signals_once_and_renames_its_legend_row(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.name_changed)
    component.set_name("b")
    component.set_name("b")
    assert (seen, plot.legend().row_labels()) == (["b"], ["b"])


def test_a_component_colour_change_signals_once(plot):
    component = plot.plot(X, np.sin(X), labels=["a"]).components()[0]
    seen = record(component.color_changed)
    component.set_color(QColor("red"))
    component.set_color(QColor("red"))
    assert (component.color(), seen) == (QColor("red"), [QColor("red")])


def test_set_colors_signals_each_component(plot):
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    seen = [record(c.color_changed) for c in graph.components()]
    graph.set_colors([QColor("red"), QColor("blue")])
    assert seen == [[QColor("red")], [QColor("blue")]]
```

  Insert above the contract's rows marker:

```python
ROWS.append(Row("component", ("SciQLopGraphComponentInterface",), {
    "SciQLopGraphComponentInterface": lambda h: h.plot.plot(X, np.sin(X), labels=["a"]).components()[0],
}, {"visible": OK, "selected": OK, "name": OK, "color": OK}))
```

  (`SciQLopGraphComponent` itself is not bound: Python sees every component as `SciQLopGraphComponentInterface`, so that class is the row, not an abstract base.)

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_components.py tests/integration/test_drawn_contract.py -k "component or every_drawn"` — expected FAIL: `AttributeError: ... has no attribute 'color_changed'`; the coverage test: `SciQLopGraphComponentInterface: in a row but not drawn`; the worker-thread rows apply on the worker thread (no hop today).
- [ ] **Step 3: The interface** — `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`; `class SciQLopGraphComponentInterface : public SciQLopDrawnObject`; the constructor's initializer becomes `: SciQLopDrawnObject(parent)`; delete the lines listed in Files.
- [ ] **Step 4: The component** — in `SciQLopGraphComponent.hpp` delete the declarations listed in Files, add to the private part `void _select_in_plottable(bool selected); void _select_in_legend(bool selected);`, and add

```cpp
protected:
    bool _visible() const override;
    void _set_visible(bool visible) override;
    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;
    void _named(const QString& name) override;
    QColor _color() const override;
    void _set_color(const QColor& color) override;
```

  `src/SciQLopGraphComponent.cpp` — replace `set_visible`, `set_name`, `set_selected`, `color` (the old `color()` body becomes `_color()` unchanged) with:

```cpp
void SciQLopGraphComponent::_set_visible(bool visible)
{
    if (!m_plottable)
        return;
    if (m_componentIndex < 0)
        m_plottable->setVisible(visible);
    else if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
        mg->setComponentVisible(m_componentIndex, visible);
    emit replot();
}

bool SciQLopGraphComponent::_visible() const
{
    if (!m_plottable)
        return false;
    if (m_componentIndex < 0)
        return m_plottable->visible();
    auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data());
    return mg && mg->component(m_componentIndex).visible;
}

void SciQLopGraphComponent::_named(const QString& name)
{
    if (!m_plottable)
        return;
    if (m_componentIndex < 0)
        m_plottable->setName(name);
    else if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
        mg->component(m_componentIndex).name = name;
}

// Moved, not retyped: this is today's inline set_color body (SciQLopGraphComponent.hpp:124-138),
// deleted from the header in the same step, so one copy exists.
void SciQLopGraphComponent::_set_color(const QColor& color)
{
    if (!m_plottable)
        return;
    auto line = pen();
    line.setColor(color);
    set_pen(line);
    auto marker = marker_pen();
    if (marker.style() == Qt::NoPen)
        marker = QPen(color);
    else
        marker.setColor(color);
    set_marker_pen(marker);
}

void SciQLopGraphComponent::_set_selected(bool selected)
{
    if (!m_plottable)
        return;
    // Stored first: NeoQCP's selectionChanged comes back to set_selected, which then sees no change.
    m_selected = selected;
    _select_in_plottable(selected);
    _select_in_legend(selected);
    emit replot();
}

void SciQLopGraphComponent::_select_in_plottable(bool selected)
{
    if (m_componentIndex >= 0)
    {
        if (auto* mg = qobject_cast<QCPMultiGraph*>(m_plottable.data()))
            mg->setComponentSelection(m_componentIndex,
                                      selected ? QCPDataSelection(QCPDataRange(0, mg->dataCount()))
                                               : QCPDataSelection());
        return;
    }
    std::visit(visitor { [selected](auto* any)
                         {
                             any->setSelection(selected
                                                   ? QCPDataSelection(any->data()->dataRange())
                                                   : QCPDataSelection());
                         },
                         [](MultiGraphRef) {}, [](std::monostate) {} },
               to_variant());
}

void SciQLopGraphComponent::_select_in_legend(bool selected)
{
    if (m_componentIndex < 0)
    {
        if (auto* item = _legend_item(); item && item->selected() != selected)
            item->setSelected(selected);
        return;
    }
    auto* group = _group_legend_item();
    if (!group)
        return;
    if (selected && group->selectedComponent() != m_componentIndex)
    {
        group->setExpanded(true);
        group->setSelectedComponent(m_componentIndex);
        group->setSelected(true);
    }
    else if (!selected && group->selected() && group->selectedComponent() == m_componentIndex)
        group->setSelected(false);
}
```

  The old "emit only when the underlying name changed" check goes: the base's compare replaces it. The constructors' `set_selected` connects, `_on_legend_double_clicked`'s `set_visible(!visible())` and the destructor's `set_selected(false)` stay as they are.
- [ ] **Step 5: Build** — CLEAN, BUILD, warning grep.
- [ ] **Step 6: Run** TEST `tests/integration/test_drawn_components.py tests/integration/test_drawn_contract.py tests/integration/test_visibility.py tests/integration/test_fills_errorbars.py tests/integration/test_inspector_delete_selected.py tests/integration/test_plot_groups.py` — PASS. Then FULL.
- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphComponentInterface.hpp \
    include/SciQLopPlots/Plotables/SciQLopGraphComponent.hpp src/SciQLopGraphComponent.cpp \
    tests/integration/test_drawn_components.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): graph components under SciQLopDrawnObject

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Items and spans under the base (tooltip and movable stubs, item selection)

Items and spans move together: every span derives `SciQLopItemInterface`, so moving that interface moves them. The span selection signal waits for Task 6.

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopPlotItem.hpp` (`SciQLopItemInterface` :121-157 base class, drop `set_visible`/`visible`, add `_follow_selection`; `SciQLopMovableItemInterface` gains `movable()`; `SciQLopLineItemInterface` drops `set_color`/`color`)
- Modify: `include/SciQLopPlots/Items/SciQLopTextItem.hpp` (`impl::TextItem` gains the tooltip mixin; `SciQLopTextItem`)
- Modify: `include/SciQLopPlots/Items/SciQLopPixmapItem.hpp`, `include/SciQLopPlots/Items/SciQLopShapesItems.hpp` (ellipse :203-347, curved line :349-470)
- Modify: `include/SciQLopPlots/Items/SciQLopStraightLines.hpp:148-299` (`SciQLopStraightLine : SciQLopDrawnObject`)
- Modify: `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp`, `SciQLopHorizontalSpan.hpp`, `SciQLopRectangularSpan.hpp` (visible, color, selected → hooks; their `selectionChanged` signal stays until Task 6)
- Test: `tests/integration/test_drawn_items.py` (create), `tests/integration/test_drawn_contract.py` (rows)

**Interfaces:**
- Consumes: the base API.
- Produces: `SciQLopItemInterface : SciQLopDrawnObject` with protected `void _follow_selection(QCPAbstractItem* item)`; `SciQLopMovableItemInterface::movable() const noexcept` (virtual); text, curved line, straight lines, ellipse and pixmap gain `selected` / `name` and follow NeoQCP's `selectionChanged`.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_items.py`:

```python
"""Items under SciQLopDrawnObject: the stubs that did nothing, and items whose plot is gone."""
import pytest
from PySide6.QtCore import QCoreApplication, QEvent, QPointF, QRectF
from PySide6.QtGui import QColor, QPixmap

from SciQLopPlots import SciQLopEllipseItem, SciQLopPixmapItem, SciQLopPlot, SciQLopTextItem
from conftest import off_thread, process_events, record

MAKERS = {
    "text": lambda p: SciQLopTextItem(p, "t", QPointF(10, 10)),
    "ellipse": lambda p: SciQLopEllipseItem(p, QRectF(10, 10, 20, 20)),
    "pixmap": lambda p: SciQLopPixmapItem(p, QPixmap(4, 4), QRectF(1, 1, 2, 2)),
}


@pytest.mark.parametrize("kind", ["text", "pixmap"])
def test_a_tooltip_round_trips(plot, kind):
    item = MAKERS[kind](plot)
    item.set_tool_tip("hello")
    assert item.tool_tip() == "hello"


@pytest.mark.parametrize("kind", ["text", "ellipse", "pixmap"])
def test_movable_round_trips(plot, kind):
    item = MAKERS[kind](plot)
    item.set_movable(True)
    assert item.movable()
    item.set_movable(False)
    assert not item.movable()


def test_selecting_a_text_item_signals_once_though_neoqcp_signals_too(plot):
    text = MAKERS["text"](plot)
    seen = record(text.selection_changed)
    text.set_selected(True)
    assert (text.selected(), seen) == (True, [True])


def _flush_deferred_delete():
    QCoreApplication.sendPostedEvents(None, QEvent.Type.DeferredDelete)
    process_events()


def test_an_item_whose_plot_is_gone_ignores_every_set(qtbot):  # Review Focus 3
    plot = SciQLopPlot()
    text = SciQLopTextItem(plot, "t", QPointF(10, 10))  # no parent: it outlives the plot
    seen = [record(text.selection_changed), record(text.visible_changed), record(text.color_changed)]
    plot.deleteLater()
    _flush_deferred_delete()
    text.set_selected(True)
    text.set_visible(True)
    text.set_color(QColor("red"))
    assert (text.selected(), text.visible(), text.color().isValid(), seen) == (False, False, False, [[], [], []])


def test_a_queued_set_running_after_the_plot_is_gone_is_harmless(qtbot):  # Review Focus 2
    plot = SciQLopPlot()
    text = SciQLopTextItem(plot, "t", QPointF(10, 10))
    seen = record(text.visible_changed)
    off_thread(lambda: text.set_visible(False))
    plot.deleteLater()
    _flush_deferred_delete()  # the plot goes first, then the queued set runs on a null item
    assert (text.visible(), seen) == (False, [])
```

  Insert above the contract's rows marker (imports: `from PySide6.QtCore import QPointF, QRectF`, `from PySide6.QtGui import QPixmap`, and `SciQLopCurvedLineItem, SciQLopEllipseItem, SciQLopHorizontalLine, SciQLopHorizontalSpan, SciQLopPixmapItem, SciQLopRectangularSpan, SciQLopStraightLine, SciQLopTextItem, SciQLopVerticalLine, SciQLopVerticalSpan` from `SciQLopPlots`):

```python
ALL = {"visible": OK, "selected": OK, "name": OK, "color": OK}
R = SciQLopPlotRange

ROWS += [
    Row("text and lines", ("SciQLopTextItem", "SciQLopCurvedLineItem", "SciQLopStraightLine",
                           "SciQLopVerticalLine", "SciQLopHorizontalLine"), {
        "SciQLopTextItem": lambda h: SciQLopTextItem(h.plot, "t", QPointF(10, 10)),
        "SciQLopCurvedLineItem": lambda h: SciQLopCurvedLineItem(h.plot, QPointF(0, 0), QPointF(5, 5)),
        "SciQLopStraightLine": lambda h: SciQLopStraightLine(h.plot, 1.0),
        "SciQLopVerticalLine": lambda h: SciQLopVerticalLine(h.plot, 1.0),
        "SciQLopHorizontalLine": lambda h: SciQLopHorizontalLine(h.plot, 0.5),
    }, ALL),
    Row("ellipse", ("SciQLopEllipseItem",), {
        "SciQLopEllipseItem": lambda h: SciQLopEllipseItem(h.plot, QRectF(10, 10, 20, 20)),
    }, ALL),
    Row("pixmap", ("SciQLopPixmapItem",), {
        "SciQLopPixmapItem": lambda h: SciQLopPixmapItem(h.plot, QPixmap(4, 4), QRectF(1, 1, 2, 2)),
    }, {"visible": OK, "selected": OK, "name": OK, "color": RAISE}),
    Row("span", ("SciQLopVerticalSpan", "SciQLopHorizontalSpan", "SciQLopRectangularSpan"), {
        "SciQLopVerticalSpan": lambda h: SciQLopVerticalSpan(h.plot, R(1.0, 2.0)),
        "SciQLopHorizontalSpan": lambda h: SciQLopHorizontalSpan(h.plot, R(1.0, 2.0)),
        "SciQLopRectangularSpan": lambda h: SciQLopRectangularSpan(h.plot, R(1.0, 2.0), R(1.0, 2.0)),
    }, ALL),
]
ABSTRACT |= {"SciQLopItemInterface", "SciQLopMovableItemInterface", "SciQLopLineItemInterface",
             "SciQLopBoundingRectItemInterface", "SciQLopPolygonItemInterface", "SciQLopRangeItemInterface"}
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_items.py tests/integration/test_drawn_contract.py -k "Item or Line or Span or every_drawn"` — expected FAIL: `NotImplementedError: ... set_tool_tip is not supported by SciQLopTextItem` (and pixmap), `AttributeError: ... 'movable'`, `AttributeError: ... 'selection_changed'` on the text item, the coverage test reporting the new rows' classes as `in a row but not drawn`.
- [ ] **Step 3: The item interfaces** — `SciQLopPlotItem.hpp`: `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`; `class SciQLopItemInterface : public SciQLopDrawnObject` with constructor `: SciQLopDrawnObject { parent }`; delete its `set_visible` and `visible`; add to its `protected:` part

```cpp
    //! NeoQCP selects an item on click: follow it, so selection_changed fires then too.
    void _follow_selection(QCPAbstractItem* item)
    {
        if (item)
            connect(item, &QCPAbstractItem::selectionChanged, this,
                    &SciQLopItemInterface::_notify_selected, Qt::UniqueConnection);
    }
```

  `SciQLopMovableItemInterface`, after `set_movable`:

```cpp
    [[nodiscard]] inline virtual bool movable() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return false;
    }
```

  `SciQLopLineItemInterface`: delete `set_color` and `color`.
- [ ] **Step 4: Text item** — `impl::TextItem` becomes `class TextItem : public SciQLopPlotItem<QCPItemText>, public SciQlopItemWithToolTip` (the mixin the ellipse and pixmap use, read by `SciQLopPlot`'s hover code at src/SciQLopPlot.cpp:583). In `SciQLopTextItem`: once `m_item` is set, after the constructor's `if`/`else` (on every construction path), call `_follow_selection(m_item);`; delete `set_visible`, `visible`, `set_color`, `color`; add

```cpp
    [[nodiscard]] inline QString tool_tip() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->tooltip(); });
    }

    inline void set_tool_tip(const QString& tool_tip) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_tool_tip(tool_tip));
        qptr_apply(m_item, [tool_tip](auto&& item) { item->setToolTip(tool_tip); });
    }

    inline void set_movable(bool movable) noexcept override
    {
        SCIQLOP_ON_OWNER_THREAD(set_movable(movable));
        qptr_apply(m_item, [movable](auto&& item) { item->setMovable(movable); });
    }

    [[nodiscard]] inline bool movable() const noexcept override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->movable(); }, false);
    }

protected:
    void _set_visible(bool visible) override
    {
        qptr_apply(m_item, [visible](auto&& item) { item->setVisible(visible); item->replot(); });
    }

    bool _visible() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->visible(); }, false);
    }

    void _set_selected(bool selected) override
    {
        qptr_apply(m_item, [selected](auto&& item) { item->setSelected(selected); item->replot(); });
    }

    bool _selected() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->selected(); }, false);
    }

    void _set_color(const QColor& color) override
    {
        qptr_apply(m_item, [color](auto&& item) { item->setColor(color); item->replot(); });
    }

    QColor _color() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->color(); });
    }

public:
```

  (`set_font_color` keeps calling `setColor` without a signal; the base's seeding keeps the next `set_color` signal right.)
- [ ] **Step 5: Pixmap** — once `m_item` is set, after the constructor's `if`/`else` (every path), `_follow_selection(m_item);`; delete `set_visible`, `visible`; add the same `tool_tip`, `set_tool_tip`, `movable` overrides as the text item (it keeps its existing `set_movable`) and the `_set_visible`, `_visible`, `_set_selected`, `_selected` hooks of Step 4 (no colour hooks: `color` raises).
- [ ] **Step 6: Ellipse and curved line** — ellipse: `_follow_selection(m_item);` at the end of each of the three constructors (it connects with `Qt::UniqueConnection`; no code re-creates an item's NeoQCP item today, so this is defensive only); delete `set_visible`, `visible`, `set_color`, `color`; add the `set_movable` / `movable` overrides of Step 4, the visible and selected hooks of Step 4, and

```cpp
    void _set_color(const QColor& color) override
    {
        auto outline = pen();
        outline.setColor(color);
        set_pen(outline);
    }

    QColor _color() const override { return pen().color(); }
```

  Curved line: `_follow_selection(m_item);` once `m_item` is set, after the constructor's `if`/`else` (every path); delete `set_visible`, `visible`, `set_color`, `color`; add the visible and selected hooks of Step 4 and

```cpp
    void _set_color(const QColor& color) override
    {
        qptr_apply(m_item, [color](auto&& item) { item->setColor(color); item->replot(); });
    }

    QColor _color() const override
    {
        return qptr_apply_or(m_item, [](auto&& item) { return item->color(); });
    }
```

- [ ] **Step 7: Straight lines** — `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`; `class SciQLopStraightLine : public SciQLopDrawnObject`; in the constructor, after the `moved` connect:

```cpp
        connect(m_line, &QCPAbstractItem::selectionChanged, this,
                &SciQLopStraightLine::_notify_selected, Qt::UniqueConnection);
```

  delete `set_color`, `color`, `set_visible`, `visible`; add

```cpp
protected:
    void _set_visible(bool visible) override
    {
        if (m_line.isNull())
            return;
        m_line->setVisible(visible);
        m_line->replot();
    }

    bool _visible() const override { return !m_line.isNull() && m_line->visible(); }

    void _set_selected(bool selected) override
    {
        if (m_line.isNull())
            return;
        m_line->setSelected(selected);
        m_line->replot();
    }

    bool _selected() const override { return !m_line.isNull() && m_line->selected(); }

    void _set_color(const QColor& color) override
    {
        if (!m_line.isNull())
            m_line->set_color(color);
    }

    QColor _color() const override { return m_line.isNull() ? QColor() : m_line->color(); }

public:
```

- [ ] **Step 8: Spans** — in each of `SciQLopVerticalSpan`, `SciQLopHorizontalSpan`, `SciQLopRectangularSpan` delete `set_visible`, `visible`, `set_color`, `color`, `set_selected`, `selected` and add to the existing `protected:` part:

```cpp
    void _set_visible(bool visible) override { _base.set_visible(visible); }
    bool _visible() const override { return _base.visible(); }
    void _set_color(const QColor& color) override { _base.set_color(color); }
    QColor _color() const override { return _base.color(); }
    void _set_selected(bool selected) override { _base.set_selected(selected); }
    bool _selected() const override { return _base.selected(); }
```

  Their constructors' `set_color` / `set_visible` calls now reach the base's setters and compile unchanged; their `selectionChanged` signal and its connect stay until Task 6 (Python's `span.selection_changed` is still the alias of it, so span selection signals once).
- [ ] **Step 9: Build** — CLEAN, BUILD, warning grep.
- [ ] **Step 10: Run** TEST `tests/integration/test_drawn_items.py tests/integration/test_drawn_contract.py tests/integration/test_item_ownership.py tests/integration/test_thread_guards.py tests/integration/test_items_api.py tests/integration/test_items_style_api.py tests/integration/test_item_signals_and_pixel.py tests/integration/test_api_hygiene.py` — PASS. Then FULL.
- [ ] **Step 11: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopPlotItem.hpp include/SciQLopPlots/Items/SciQLopTextItem.hpp \
    include/SciQLopPlots/Items/SciQLopPixmapItem.hpp include/SciQLopPlots/Items/SciQLopShapesItems.hpp \
    include/SciQLopPlots/Items/SciQLopStraightLines.hpp include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp \
    include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp \
    tests/integration/test_drawn_items.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): items and spans under SciQLopDrawnObject

Items gain selected and name; text and pixmap tooltips and text and ellipse set_movable
work instead of doing nothing.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Span selection: `selection_changed` native, `selectionChanged` the alias

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp:218`, `SciQLopHorizontalSpan.hpp:179`, `SciQLopRectangularSpan.hpp:173` (drop `Q_SIGNAL void selectionChanged(bool);`)
- Modify: `src/SciQLopVerticalSpan.cpp:159-160`, `src/SciQLopHorizontalSpan.cpp:90-91`, `src/SciQLopRectangularSpan.cpp:104-105`
- Modify: `src/MultiPlotsVSpan.cpp:61` (the C++ user of the camelCase signal)
- Modify: `SciQLopPlots/__init__.py:602-621` (alias inverted)
- Test: `tests/integration/test_drawn_spans.py` (create)

**Interfaces:**
- Consumes: Task 5's `_follow_selection`.
- Produces: `SciQLopVerticalSpan.selectionChanged is SciQLopVerticalSpan.selection_changed` (the base's signal), same for H/R spans; `_DEPRECATED_ALIASES` in `__init__.py` (alias name → native name), which Task 9 extends.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_spans.py`:

```python
"""Spans signal selection through the base; the camelCase name is an alias."""
import pytest
from PySide6.QtCore import QPoint, Qt
from PySide6.QtTest import QTest
from PySide6.QtWidgets import QWidget

from SciQLopPlots import (SciQLopHorizontalSpan, SciQLopPlotRange, SciQLopRectangularSpan,
                          SciQLopVerticalSpan)
from conftest import process_events, record

SPANS = [SciQLopVerticalSpan, SciQLopHorizontalSpan, SciQLopRectangularSpan]


@pytest.mark.parametrize("cls", SPANS)
def test_a_span_declares_no_selection_signal_of_its_own(cls):
    meta = cls.staticMetaObject
    assert meta.indexOfSignal("selectionChanged(bool)") == -1
    assert meta.indexOfSignal("selection_changed(bool)") != -1  # the base's


def test_the_camel_case_alias_still_connects(plot):
    span = SciQLopVerticalSpan(plot, SciQLopPlotRange(0.2, 0.4))
    seen = record(span.selectionChanged)
    span.set_selected(True)
    assert seen == [True]


def test_clicking_a_span_selects_it_and_signals_once(qtbot, plot):
    plot.x_axis().set_range(SciQLopPlotRange(0.0, 10.0))
    span = SciQLopVerticalSpan(plot, SciQLopPlotRange(2.0, 8.0))
    seen = record(span.selection_changed)
    plot.resize(600, 400)
    plot.show()
    qtbot.waitExposed(plot)
    plot.replot(True)
    process_events()
    # Clicked on the QCustomPlot canvas itself, like tests/integration/test_timeline_edit.py:15:
    # the plot's child at a point may be an axis or legend widget instead.
    canvas = next(w for w in plot.findChildren(QWidget) if w.inherits("QCustomPlot"))
    centre = QPoint(canvas.width() // 2, canvas.height() // 2)
    QTest.mouseClick(canvas, Qt.LeftButton, Qt.NoModifier, centre)
    process_events()
    assert (span.selected(), seen) == (True, [True])
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_spans.py` — expected FAIL: `indexOfSignal("selectionChanged(bool)")` is not -1 (the spans still declare their own); the other two are guards.
- [ ] **Step 3: Implement** — delete the three `selectionChanged` declarations; in each span constructor replace `connect(_impl.data(), &QCPAbstractItem::selectionChanged, this, &SciQLop<X>Span::selectionChanged);` with `_follow_selection(_impl.data());`; in `src/MultiPlotsVSpan.cpp` replace `&SciQLopVerticalSpan::selectionChanged` with `&SciQLopVerticalSpan::selection_changed`. In `__init__.py` remove the three span entries from `_SNAKE_CASE_ALIASES` and add after its loop:

```python
# Old names kept as aliases of the native ones (docs/plans/2026-10-22-drawn-object-base-design.md),
# deprecated. The same attribute under a second name, never a wrapper (see above); a warning on
# connect would need a proxy object.
_DEPRECATED_ALIASES = {
    SciQLopPlotsBindings.SciQLopVerticalSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopHorizontalSpan: {"selectionChanged": "selection_changed"},
    SciQLopPlotsBindings.SciQLopRectangularSpan: {"selectionChanged": "selection_changed"},
}
for _cls, _aliases in _DEPRECATED_ALIASES.items():
    for _alias, _native in _aliases.items():
        setattr(_cls, _alias, getattr(_cls, _native))
```

  `wasm/sciqlopplots_js.cpp:852` uses the panel span's `selection_changed` and needs nothing.
- [ ] **Step 4: Build** — CLEAN, BUILD, warning grep; delete any span rename entry the grep reports as not found.
- [ ] **Step 5: Run** TEST `tests/integration/test_drawn_spans.py tests/integration/test_api_hygiene.py tests/integration/test_drawn_contract.py` — PASS. Then FULL.
- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopVerticalSpan.hpp include/SciQLopPlots/Items/SciQLopHorizontalSpan.hpp \
    include/SciQLopPlots/Items/SciQLopRectangularSpan.hpp src/SciQLopVerticalSpan.cpp src/SciQLopHorizontalSpan.cpp \
    src/SciQLopRectangularSpan.cpp src/MultiPlotsVSpan.cpp SciQLopPlots/__init__.py tests/integration/test_drawn_spans.py
git commit -m "feat(drawn): spans signal selection_changed; selectionChanged is the deprecated alias

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Panel-wide objects under the base

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp:28` + `src/SciQLopMultiPlotObject.cpp:40-41` (base class)
- Modify: `include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp:32-169` + `src/MultiPlotsVSpan.cpp:52-105`
- Modify: `include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp:32-81` + `src/MultiPlotsVLine.cpp:29-89`
- Modify: `SciQLopPlots/bindings/bindings.xml:1660-1664` (drop the `color() const`, `visible() const`, `selected() const` renames on `MultiPlotsVerticalSpan`) and `:1690-1694` (drop `color() const`, `visible() const` on `MultiPlotsVerticalLine`); the properties stay
- Test: `tests/integration/test_drawn_panel_objects.py` (create), `tests/integration/test_drawn_contract.py` (rows)

**Interfaces:**
- Consumes: Task 6's native span `selection_changed`.
- Produces: `SciQLopMultiPlotObject : SciQLopDrawnObject`; panel span supports all four (own state `m_visible`, `m_selected`, `m_color`), panel line visible, name and color (`m_visible`, `m_color`), the span collection name only.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_panel_objects.py`:

```python
"""Panel-wide objects under SciQLopDrawnObject."""
import numpy as np
from PySide6.QtGui import QColor

from SciQLopPlots import (MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection,
                          SciQLopPlotRange)
from conftest import record

X = np.linspace(0.0, 10.0, 50)


def test_the_catalogue_overlay_pattern_signals_once(panel):
    """SciQLop's overlay connects selection_changed and sets `span.selected = True`."""
    panel.plot(X, np.sin(X))
    collection = MultiPlotsVSpanCollection(panel)
    span = collection.create_span(SciQLopPlotRange(1.0, 2.0), id="event")
    seen = []
    span.selection_changed.connect(seen.append)
    span.selected = True
    span.selected = True
    assert (span.selected, seen) == (True, [True])


def test_panel_span_state_stays_attributes(panel):
    panel.plot(X, np.sin(X))
    span = MultiPlotsVerticalSpan(panel, SciQLopPlotRange(1.0, 2.0))
    seen = record(span.visible_changed)
    span.visible = False
    span.color = QColor("red")
    assert (span.visible, span.color, seen) == (False, QColor("red"), [False])


def test_a_panel_line_colour_signals_once(panel):
    panel.plot(X, np.sin(X))
    line = MultiPlotsVerticalLine(panel, 2.0)
    seen = record(line.color_changed)
    line.color = QColor("red")
    line.color = QColor("red")
    assert seen == [QColor("red")]
```

  Insert above the contract's rows marker (import `MultiPlotsVerticalLine, MultiPlotsVerticalSpan, MultiPlotsVSpanCollection`):

```python
ROWS += [
    Row("panel span", ("MultiPlotsVerticalSpan",), {
        "MultiPlotsVerticalSpan": lambda h: MultiPlotsVerticalSpan(h.panel, R(1.0, 2.0)),
    }, ALL, frozenset({"visible", "selected", "color"})),
    Row("panel line", ("MultiPlotsVerticalLine",), {
        "MultiPlotsVerticalLine": lambda h: MultiPlotsVerticalLine(h.panel, 2.0),
    }, {"visible": OK, "selected": RAISE, "name": OK, "color": OK}, frozenset({"visible", "color"})),
    Row("span collection", ("MultiPlotsVSpanCollection",), {
        "MultiPlotsVSpanCollection": lambda h: MultiPlotsVSpanCollection(h.panel),
    }, {"visible": RAISE, "selected": RAISE, "name": OK, "color": RAISE}),
]
ABSTRACT.add("SciQLopMultiPlotObject")
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_panel_objects.py tests/integration/test_drawn_contract.py -k "panel or collection or every_drawn"` — expected FAIL: `AttributeError: ... 'visible_changed'` / `'color_changed'`; the overlay pattern is a guard.
- [ ] **Step 3: The base of panel objects** — `SciQLopMultiPlotObject.hpp`: `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`, `class SciQLopMultiPlotObject : public SciQLopDrawnObject`; `src/SciQLopMultiPlotObject.cpp`: `: SciQLopDrawnObject { dynamic_cast<QObject*>(parent) }`.
- [ ] **Step 4: Panel span** — rename the fields that clash with hooks: `sed -i 's/\b_selected\b/m_selected/g; s/\b_visible\b/m_visible/g; s/\b_color\b/m_color/g' include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp src/MultiPlotsVSpan.cpp`. Then delete `set_selected`, `selected`, `set_color`, `color`, `set_visible`, `visible` and the `selection_changed` signal from the header; add

```cpp
protected:
    bool _selected() const override { return m_selected; }
    void _set_selected(bool selected) override;
    QColor _color() const override { return m_color; }
    void _set_color(const QColor& color) override;
    bool _visible() const override { return m_visible; }
    void _set_visible(bool visible) override;

public:
```

  `src/MultiPlotsVSpan.cpp` — replace `MultiPlotsVerticalSpan::set_selected` with:

```cpp
void MultiPlotsVerticalSpan::_set_selected(bool selected)
{
    // Stored first: each plot's span echoes the selection back through set_selected.
    m_selected = selected;
    for (auto span : _spans)
        if (span)
            span->set_selected(selected);
    replotAll();
}

void MultiPlotsVerticalSpan::_set_color(const QColor& color)
{
    m_color = color;
    for (auto span : _spans)
        if (span)
            span->set_color(color);
}

void MultiPlotsVerticalSpan::_set_visible(bool visible)
{
    m_visible = visible;
    for (auto span : _spans)
        if (span)
            span->set_visible(visible);
}
```

  `addObject` keeps `connect(new_span, &SciQLopVerticalSpan::selection_changed, this, &MultiPlotsVerticalSpan::set_selected)` (now the base's setter); `show()` / `hide()` keep calling `set_visible`.
- [ ] **Step 5: Panel line** — `sed -i 's/\b_visible\b/m_visible/g; s/\b_color\b/m_color/g' include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp src/MultiPlotsVLine.cpp`; delete `set_color`, `color`, `set_visible`, `visible` from the header; add

```cpp
protected:
    QColor _color() const override { return m_color; }
    void _set_color(const QColor& color) override;
    bool _visible() const override { return m_visible; }
    void _set_visible(bool visible) override;

public:
```

  `src/MultiPlotsVLine.cpp`:

```cpp
void MultiPlotsVerticalLine::_set_color(const QColor& color)
{
    m_color = color;
    for_each_line([&color](SciQLopVerticalLine* line) { line->set_color(color); });
}

void MultiPlotsVerticalLine::_set_visible(bool visible)
{
    m_visible = visible;
    for_each_line([visible](SciQLopVerticalLine* line) { line->set_visible(visible); });
}
```

- [ ] **Step 6: bindings.xml** — delete the five dead `rename` entries listed in Files; keep every `<property>`. Build (CLEAN, BUILD) and grep the warnings for `MultiPlotsVertical`. If shiboken reports it cannot find the `visible` / `selected` / `color` getters for those properties (they are now inherited), delete those five `<property>` elements and add to `__init__.py`, next to `_DEPRECATED_ALIASES`:

```python
for _cls, _names in ((SciQLopPlotsBindings.MultiPlotsVerticalSpan, ("visible", "selected", "color")),
                     (SciQLopPlotsBindings.MultiPlotsVerticalLine, ("visible", "color"))):
    for _name in _names:
        # Only when the XML route bound nothing: never wrap a property in a property.
        assert not isinstance(_cls.__dict__.get(_name), property), (_cls, _name)
        setattr(_cls, _name, property(getattr(SciQLopPlotsBindings.SciQLopDrawnObject, _name),
                                      getattr(SciQLopPlotsBindings.SciQLopDrawnObject, "set_" + _name)))
```

  (The plottables' `name` property already reads an inherited getter, `objectName`, so the XML route is expected to work.)
- [ ] **Step 7: Run** TEST `tests/integration/test_drawn_panel_objects.py tests/integration/test_drawn_contract.py tests/integration/test_multiplots_vertical_line.py tests/integration/test_thread_guards.py tests/integration/test_api_hygiene.py tests/integration/test_panel_api.py` — PASS. Then FULL.
- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotObject.hpp src/SciQLopMultiPlotObject.cpp \
    include/SciQLopPlots/MultiPlots/MultiPlotsVSpan.hpp src/MultiPlotsVSpan.cpp \
    include/SciQLopPlots/MultiPlots/MultiPlotsVLine.hpp src/MultiPlotsVLine.cpp \
    SciQLopPlots/bindings/bindings.xml tests/integration/test_drawn_panel_objects.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): panel spans, panel lines and span collections under SciQLopDrawnObject

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

  (Add `SciQLopPlots/__init__.py` to the paths if Step 6's fallback was needed.)

---

### Task 8: Plot groups under the base (aggregate rule, name on objectName)

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotGroup.hpp:40-73`, `src/SciQLopPlotGroup.cpp:55-197`
- Test: `tests/integration/test_drawn_group.py` (create), `tests/integration/test_drawn_contract.py` (row)

**Interfaces:**
- Consumes: the base API; members' `visible_changed`.
- Produces: `SciQLopPlotGroup : SciQLopDrawnObject`, `_always_apply() == true`, visible = any member, `visible_changed` when that flips; `m_name` gone, the name is `objectName`, set through `set_name` from the constructor.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_group.py`:

```python
"""Plot groups: visible is an aggregate of the members (rule 2)."""
import numpy as np
import pytest

from conftest import process_events, record

X = np.linspace(0.0, 10.0, 50)


@pytest.fixture
def pair(plot):
    a = plot.plot(X, np.sin(X), labels=["a"])
    b = plot.plot(X, np.cos(X), labels=["b"])
    return plot.group("ab", [a, b]), a, b


def test_hiding_the_last_visible_member_hides_the_group_once(pair):
    group, a, b = pair
    seen = record(group.visible_changed)
    a.set_visible(False)
    assert seen == []  # b still shows
    b.set_visible(False)
    assert (group.visible(), seen) == (False, [False])


def test_showing_a_group_with_one_hidden_member_shows_it_silently(pair):
    group, a, _ = pair
    a.set_visible(False)
    seen = record(group.visible_changed)
    group.set_visible(True)
    assert (a.visible(), seen) == (True, [])


def test_the_group_name_is_its_object_name(pair):
    group, _, _ = pair
    seen = record(group.name_changed)
    group.set_name("model")
    assert (group.objectName(), group.name(), seen) == ("model", "model", ["model"])


def test_a_slot_removing_the_group_while_it_hides_is_safe(plot, pair):  # Review Focus 4
    group, a, _ = pair
    a.visible_changed.connect(lambda _: plot.remove_group(group))
    group.set_visible(False)  # the group is deleted while it applies; nothing reads it after
    process_events()
    assert plot.groups() == []
```

  Insert above the contract's rows marker:

```python
def _group(hosts):
    plot = hosts.plot
    return plot.group("g", [plot.plot(X, np.sin(X), labels=["a"]), plot.plot(X, np.cos(X), labels=["b"])])


ROWS.append(Row("group", ("SciQLopPlotGroup",), {"SciQLopPlotGroup": _group},
                {"visible": OK, "selected": RAISE, "name": OK, "color": RAISE}))
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_group.py tests/integration/test_drawn_contract.py -k "group or every_drawn"` — expected FAIL: `AttributeError: ... 'visible_changed'` (three tests), `objectName() == ''`, and the coverage test reporting `SciQLopPlotGroup: in a row but not drawn`.
- [ ] **Step 3: The header** — `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`; `class SciQLopPlotGroup : public SciQLopDrawnObject`; delete `QString m_name;`, `name()`, `set_name`, `visible`, `set_visible`; add `void _seed_parts();` to the private part and

```cpp
protected:
    bool _always_apply() const override { return true; }
    //! True while any member is visible.
    bool _visible() const override;
    void _set_visible(bool visible) override;
    void _named(const QString& name) override;
```

- [ ] **Step 4: The implementation** — `src/SciQLopPlotGroup.cpp`:

```cpp
SciQLopPlotGroup::SciQLopPlotGroup(const QString& name, SciQLopPlotInterface* plot)
        : SciQLopDrawnObject(plot), m_plot(plot)
{
    set_name(name);
}

void SciQLopPlotGroup::_named(const QString& name)
{
    for (const auto& group : m_canvas_groups)
        if (group)
        {
            group->setName(name);
            group->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        }
}

bool SciQLopPlotGroup::_visible() const
{
    const auto live = members();
    return std::ranges::any_of(live, [](SciQLopPlottableInterface* m) { return m->visible(); });
}

void SciQLopPlotGroup::_set_visible(bool visible)
{
    // A member's visible_changed slot may remove another member: iterate guarded pointers.
    const auto snapshot = m_members;
    for (const auto& member : snapshot)
        if (member)
            member->set_visible(visible);
}

void SciQLopPlotGroup::_seed_parts()
{
    _seed_visible(_visible());
}
```

  Also: `canvas_group` creates its NeoQCP group with `canvas->addPlottableGroup(objectName())`; in `add`, after `m_members.append(member);` and its `destroyed` connect:

```cpp
    connect(member, &SciQLopDrawnObject::visible_changed, this,
            [this] { _notify_visible(_visible()); });
    _seed_parts();
```

  in `forget`, after `m_members.removeAll(member);` call `_seed_parts();`; `forget_destroyed_members` becomes

```cpp
void SciQLopPlotGroup::forget_destroyed_members()
{
    // destroyed() fires after Qt has nulled the QPointer to the dying member.
    const bool forgot = m_members.removeIf([](const auto& m) { return m.isNull(); }) > 0;
    _seed_parts();
    if (forgot)
        Q_EMIT members_changed();
}
```

  The constructor runs on the owner thread (`SciQLopPlotInterface::group` checks it), so its `set_name` applies at once and the canvas groups made later start named.
- [ ] **Step 5: Build** — CLEAN, BUILD, warning grep.
- [ ] **Step 6: Run** TEST `tests/integration/test_drawn_group.py tests/integration/test_drawn_contract.py tests/integration/test_plot_groups.py` — PASS (`test_plot_groups.py` unchanged). Then FULL.
- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotGroup.hpp src/SciQLopPlotGroup.cpp \
    tests/integration/test_drawn_group.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): plot groups under SciQLopDrawnObject, visible as an aggregate

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: The legend under the base (aliases `is_visible`, `visibility_changed`)

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotLegendInterface.hpp:32-123` (base class; drop `is_visible`, `set_visible`, `color`, `set_color`, signals `visibility_changed`, `color_changed`)
- Modify: `include/SciQLopPlots/SciQLopPlotLegend.hpp:39-56`, `src/SciQLopPlotLegend.cpp:33-46,139-150`
- Modify: `src/SciQLopPlotDelegate.cpp:59-63` (`is_visible()` → `visible()`, `visibility_changed` → `visible_changed`)
- Modify: `SciQLopPlots/__init__.py` (`_DEPRECATED_ALIASES`)
- Test: `tests/integration/test_drawn_legend.py` (create), `tests/integration/test_drawn_contract.py` (row)

**Interfaces:**
- Consumes: Task 6's `_DEPRECATED_ALIASES`.
- Produces: `SciQLopPlotLegendInterface : SciQLopDrawnObject`; Python `legend.is_visible` is `legend.visible`, `legend.visibility_changed` is `legend.visible_changed`.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_legend.py`:

```python
"""The legend under SciQLopDrawnObject; its old names are aliases."""
from PySide6.QtWidgets import QCheckBox, QGroupBox

from SciQLopPlots import DelegateRegistry
from conftest import process_events, record


def test_the_old_names_are_aliases(plot):
    legend = plot.legend()
    assert type(legend).staticMetaObject.indexOfSignal("visibility_changed(bool)") == -1
    legend.set_visible(False)
    assert legend.is_visible() is legend.visible() is False


def test_sciqlop_legend_visible_property_pattern(plot):  # Review Focus 5
    """SciQLop's legend_visible: `legend().is_visible()` and `legend().set_visible(bool(v))`."""
    seen = record(plot.legend().visibility_changed)
    plot.legend().set_visible(bool(0))
    assert plot.legend().is_visible() is False
    plot.legend().set_visible(bool(1))
    assert (plot.legend().is_visible(), seen) == (True, [False, True])


def test_the_inspector_legend_checkbox_follows(qtbot, plot):  # Review Focus 5
    delegate = DelegateRegistry.instance().create_delegate(plot, None)
    qtbot.addWidget(delegate)
    box = next(b for b in delegate.findChildren(QGroupBox) if b.title() == "Legend")
    check = box.findChildren(QCheckBox)[0]
    plot.legend().set_visible(False)
    process_events()
    assert not check.isChecked()


def test_the_legend_colour_signals_once(plot):
    from PySide6.QtGui import QColor
    seen = record(plot.legend().color_changed)
    plot.legend().set_color(QColor("red"))
    plot.legend().set_color(QColor("red"))
    assert seen == [QColor("red")]
```

  Insert above the contract's rows marker:

```python
ROWS.append(Row("legend", ("SciQLopPlotLegend",), {"SciQLopPlotLegend": lambda h: h.plot.legend()},
                {"visible": OK, "selected": RAISE, "name": OK, "color": OK}))
ABSTRACT.add("SciQLopPlotLegendInterface")
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_legend.py tests/integration/test_drawn_contract.py -k "legend or every_drawn"` — expected FAIL: `indexOfSignal("visibility_changed(bool)")` is not -1, the coverage test reporting `SciQLopPlotLegend: in a row but not drawn`; the SciQLop pattern, checkbox and colour tests are guards (today's names).
- [ ] **Step 3: Implement** — interface: `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`, `class SciQLopPlotLegendInterface : public SciQLopDrawnObject`, constructor `: SciQLopDrawnObject(parent)`; delete the lines in Files. `SciQLopPlotLegend.hpp`: delete `is_visible`, `set_visible`, `color`, `set_color`; add

```cpp
protected:
    bool _visible() const override;
    void _set_visible(bool visible) override;
    QColor _color() const override;
    void _set_color(const QColor& color) override;
```

  `src/SciQLopPlotLegend.cpp`:

```cpp
bool SciQLopPlotLegend::_visible() const
{
    return m_legend->visible();
}

void SciQLopPlotLegend::_set_visible(bool visible)
{
    m_legend->setVisible(visible);
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}

QColor SciQLopPlotLegend::_color() const
{
    return m_legend->textColor();
}

void SciQLopPlotLegend::_set_color(const QColor& color)
{
    m_legend->setTextColor(color);
    m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
}
```

  `src/SciQLopPlotDelegate.cpp`: `new LegendDelegate(legend->visible(), legendBox)` and `connect(legend, &SciQLopPlotLegendInterface::visible_changed, legend_delegate, &LegendDelegate::set_visible);` (the `color_changed` connect at :74 now names the base's signal and compiles unchanged). `__init__.py`: add to `_DEPRECATED_ALIASES`

```python
    SciQLopPlotsBindings.SciQLopPlotLegendInterface: {"is_visible": "visible",
                                                      "visibility_changed": "visible_changed"},
```

  (on the interface, so every bound legend class inherits them).
- [ ] **Step 4: Build** — CLEAN, BUILD, warning grep. Check no other caller: `grep -rn "is_visible\|visibility_changed" src include wasm/sciqlopplots_js.cpp | grep -iv "LegendDelegate\|colorscale"` is empty.
- [ ] **Step 5: Run** TEST `tests/integration/test_drawn_legend.py tests/integration/test_drawn_contract.py tests/integration/test_api_features.py tests/integration/test_projection_graph_api_gaps.py tests/integration/test_plot_groups.py` — PASS. Then FULL.
- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotLegendInterface.hpp include/SciQLopPlots/SciQLopPlotLegend.hpp \
    src/SciQLopPlotLegend.cpp src/SciQLopPlotDelegate.cpp SciQLopPlots/__init__.py \
    tests/integration/test_drawn_legend.py tests/integration/test_drawn_contract.py
git commit -m "feat(drawn): the legend under SciQLopDrawnObject; is_visible and visibility_changed are aliases

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: The inspector selects through the base

**Files:**
- Modify: `src/Registrations.cpp` (the `set_selected` of the graph :306-310, component :318-321, colour map :328-331, 2-D histogram :337-340, timeline :346-349 registrations; text item :355 `nullptr`)
- Test: `tests/integration/test_drawn_inspector.py` (create)

**Interfaces:**
- Consumes: every drawn family under the base.
- Produces: one `select_drawn(QObject*, bool)` in Registrations.cpp. Panel, plot, projection pane and axis registrations keep their own.

- [ ] **Step 1: Failing tests** — `tests/integration/test_drawn_inspector.py`:

```python
"""The inspector selects every drawn object the same way."""
import numpy as np
from PySide6.QtCore import QModelIndex, QPointF

from SciQLopPlots import PlotsModel, SciQLopTextItem

X = np.linspace(0.0, 10.0, 50)


def _row_of(model, parent, obj):
    for r in range(model.rowCount(parent)):
        index = model.index(r, 0, parent)
        if PlotsModel.object(index) is obj:
            return index
    raise AssertionError(f"{obj} not in the inspector model")


def test_the_inspector_selects_a_text_item(plot):
    text = SciQLopTextItem(plot, "note", QPointF(10, 10))
    model = PlotsModel.instance()
    model.addTopLevelNode(text)
    model.set_selected([_row_of(model, QModelIndex(), text)], True)
    assert text.selected()


def test_the_inspector_selects_a_graph_and_its_components(plot):  # guard after Task 2
    graph = plot.plot(X, np.column_stack([np.sin(X), np.cos(X)]), labels=["a", "b"])
    model = PlotsModel.instance()
    model.addTopLevelNode(graph)
    model.set_selected([_row_of(model, QModelIndex(), graph)], True)
    assert [c.selected() for c in graph.components()] == [True, True]
```

- [ ] **Step 2: Run** TEST `tests/integration/test_drawn_inspector.py` — expected FAIL on the text item (its registration's `set_selected` is `nullptr`, so the model skips it).
- [ ] **Step 3: Implement** — in the anonymous namespace of `src/Registrations.cpp` (add `#include "SciQLopPlots/SciQLopDrawnObject.hpp"`):

```cpp
void select_drawn(QObject* obj, bool selected)
{
    if (auto* drawn = qobject_cast<SciQLopDrawnObject*>(obj))
        drawn->set_selected(selected);
}
```

  and set `.set_selected = select_drawn,` in the six registrations listed in Files (the graph's `g->selected() != s` guard goes: the base compares).
- [ ] **Step 4: Build** — BUILD (no bound header changed).
- [ ] **Step 5: Run** TEST `tests/integration/test_drawn_inspector.py tests/integration/test_inspector_model.py tests/integration/test_inspector_delete_selected.py tests/integration/test_inspector_drag_drop.py tests/integration/test_inspector_extensions.py tests/integration/test_inspector_insert_order.py tests/integration/test_timeline_inspector.py tests/integration/test_projection_inspector.py` — PASS (inspector-only change: no full suite).
- [ ] **Step 6: Commit**

```bash
git add src/Registrations.cpp tests/integration/test_drawn_inspector.py
git commit -m "refactor(inspector): select every drawn object through SciQLopDrawnObject

The text item becomes selectable from the inspector.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 11: Docs, portability check, SciQLop and plugins gate

**Files:**
- Modify: `docs/user-guide.md` (a subsection at the end of "Colours, names and themes", ~line 676; a bullet in "Overlays: spans, lines and text")
- Modify: `docs/api-review-2026-10-20.md` (Progress, phase 4 paragraph)

There is no changelog file in the repo; the API-review progress note carries the change list.

- [ ] **Step 1: User guide** — add under "Colours, names and themes" (the guide test runs every snippet):

````markdown
### Visible, selected, name and colour

Everything drawn — graphs and their components, colour maps, histograms, timelines, items, spans,
groups, the legend — answers the same four questions, with the same signals:

```python
import numpy as np
from PySide6.QtCore import QPointF
from SciQLopPlots import SciQLopPlot, SciQLopTextItem

plot = SciQLopPlot()
x = np.linspace(0, 10, 100)
graph = plot.plot(x, np.sin(x), labels=["sin"])
note = SciQLopTextItem(plot, "peak", QPointF(20, 20))

note.visible_changed.connect(lambda visible: print("note visible:", visible))
note.set_visible(False)                         # visible / set_visible / visible_changed
note.set_selected(True)                         # selected / set_selected / selection_changed
note.set_name("marker")                         # name / set_name / name_changed
note.set_color(graph.component(0).color())      # color / set_color / color_changed
graph.name = "sine"                             # a plottable's name is a property
try:
    graph.color()                               # a graph has one colour per line
except NotImplementedError as error:
    print(error)                                # "... use colors()"
```

- A setter signals once per change and stays silent when the value is the same. Called from
  another thread, it runs on the GUI thread a moment later.
- What a kind can't do raises `NotImplementedError` naming the class: a group's colour, a
  pixmap's colour, a panel line's selection.
- Deprecated names still work: `span.selectionChanged` (use `selection_changed`),
  `legend.is_visible()` and `legend.visibility_changed` (use `visible()` and `visible_changed`).
- C++ subclasses: override the protected hooks (`_visible`, `_set_visible`, ...), not the
  accessors, and without `noexcept`.
````

  In "Overlays", after the `MultiPlotsVerticalSpan` bullet: `- Text, line, ellipse and pixmap items have `selected()` / `set_selected()` and `name()` / `set_name()` too; text and pixmap tooltips and `set_movable` work on text, ellipse and pixmap.`
- [ ] **Step 2: API review progress** — append to the phase 4 paragraph: `Item 5.3 part A done: SciQLopDrawnObject under plottables, components, items, spans, panel spans and lines, groups and the legend. Fixed on the way: graph.set_selected() did nothing, the timeline never signalled visible_changed, the name property bypassed set_name(), text/pixmap tooltips and text/ellipse set_movable were stubs. Breaks: C++ overrides of these accessors move to the _x hooks and drop noexcept; color/selected raise NotImplementedError where the kind has none; setObjectName() no longer emits name_changed (set_name does); the Q_PROPERTY(selected) declarations on graphs and components are gone. Deprecated aliases: span selectionChanged, legend is_visible/visibility_changed. Part B (factories, plot.remove, ownership) is next.`
- [ ] **Step 3: Portability check** (GCC accepted code that broke clang before): run clang's syntax check on the sources this branch changed:

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots/build-venv && ../.venv/bin/python - <<'EOF'
import json, pathlib, shlex, subprocess
changed = {"SciQLopDrawnObject.cpp", "QCPAbstractPlottableWrapper.cpp", "SciQLopGraphComponent.cpp",
           "SciQLopGraphInterface.cpp", "SciQLopNDProjectionCurves.cpp", "SciQLopHistogram.cpp",
           "SciQLopColorMapBase.cpp", "SciQLopTimeline.cpp", "SciQLopPlotGroup.cpp", "SciQLopPlotLegend.cpp",
           "MultiPlotsVSpan.cpp", "MultiPlotsVLine.cpp", "Registrations.cpp", "SciQLopVerticalSpan.cpp"}
failed = []
for entry in json.load(open("compile_commands.json")):
    path = pathlib.Path(entry["file"])
    if path.name in changed and path.parent.name == "src":
        args = shlex.split(entry["command"])
        result = subprocess.run(["clang++", "-fsyntax-only", *args[1:]], cwd=entry["directory"],
                                capture_output=True, text=True)
        if result.returncode != 0:
            failed.append((path.name, result.stderr[-2000:]))
for name, err in failed:
    print(name, err)
print("clang: OK" if not failed else f"clang: {len(failed)} failed")
EOF
```

  Expected: `clang: OK` (warnings about GCC-only flags are fine; errors in our files are not — fix them).
- [ ] **Step 4: Full SciQLopPlots suite** — FULL.
- [ ] **Step 5: SciQLop and the plugins** against `build-venv`: from /home/jeandet/Documents/prog/SciQLop, `xvfb-run -a -s "-screen 0 2560x1440x24" env PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-venv timeout 590 .venv/bin/python -m pytest <files> -q -p no:cacheprovider -o addopts="" --no-xvfb`, in chunks under 590 s (xvfb-run outside `timeout`, so a timeout kill never orphans Xvfb). Plugins sciqlop_msa, sciqlop_radio, sciqlop_sismo with `PYTHONPATH=<build-venv>:<plugin dir>`. Known pre-existing failure: test_onboarding_integration.py drag step. Check especially `tests/test_catalog_overlay.py` (the overlay's `selection_changed` + `selected = True`) and the legend-visible property tests. End with `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` empty. A SciQLop failure caused by `setObjectName()` no longer emitting `name_changed` on a plottable is ours: report it, do not edit SciQLop.
- [ ] **Step 6: Reviews** — an opus review and an opencode review of `git diff main...HEAD` restricted to this plan's commits. Fix every real finding with its own reproducer first; flag false positives.
- [ ] **Step 7: Commit docs**

```bash
git add docs/user-guide.md docs/api-review-2026-10-20.md
git commit -m "docs(drawn): visible/selected/name/color on everything drawn

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```
