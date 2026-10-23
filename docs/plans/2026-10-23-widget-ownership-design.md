# Widgets follow PySide's ownership rules

Status: design, 2026-10-23, revision 2 after an opencode review. Follows API review item 1.1 (ownership) for widgets. Items (text, shapes, spans, lines) are part B (item factories, `plot.remove`), which applies the same rule to what a plot draws.

## The rule

The same as PySide: **ownership follows the Qt parent.** A widget built with a parent, or handed to a container, lives as long as that parent, whatever happens to the Python reference. A widget without a parent — never parented, or unparented again — belongs to its Python object.

## What is wrong today

Every SciQLopPlots widget built with a parent belongs to Python, unlike every PySide widget:

```python
host = QWidget()
QLabel(host)                      # survives: host owns it
SciQLopPlot(host)                 # deleted at the next garbage collection
SciQLopTimeSeriesPlot(host)       # same
SciQLopNDProjectionPlot(3, host)  # same
SciQLopMultiPlotPanel(host)       # same
```

So `panel.addWidget(SciQLopNDProjectionPlot(3, panel))` adds a plot that vanishes a moment later.

The build passes shiboken's `--enable-parent-ctor-heuristic` (SciQLopPlots/bindings/helper_scripts/shiboken-gen.py:34), and our constructors meet its documented conditions, yet no generated constructor sets a parent. The cause is an undocumented condition: shiboken skips the constructor heuristic for a class whose typesystem entry has `parent-management="yes"`, and almost every bound class has it. An opencode experiment proved it with two generations of a one-class module that differ only in that attribute: without it, `Sbk_Probe_Init` calls `Shiboken::Object::setParent(pyArgs[0], self)`; with it, there is none.

`parent-management="yes"` can't simply be dropped: it is also what makes returned objects (`create_plot()`, `plot()`, `legend()`, `group()`) owned by C++, which the container and item-ownership tests pin. So the constructors get explicit markup, which is also what PySide does where its heuristic can't apply (`QGraphicsItem`, `QDialog` in `typesystem_widgets_common.xml`). The heuristic flag stays: it is harmless and covers any future class without the attribute.

The containers are already right: `add_plot`, `insert_plot`, `addWidget`, `add_panel`, `insert_panel` give the widget to the panel (`<parent index="this" action="add"/>` at bindings.xml:406, :427, :811, :838, :1669-1709, :1799).

## Changes

1. **Constructors.** Every bound constructor that takes a `QWidget* parent` / `QObject* parent` gives the new object to that parent when one is passed: `<modify-argument index="this"><parent index="N" action="add"/></modify-argument>` on each, with the exact bound signature (defaults included; `SciQLopNDProjectionPlot(projection_count, parent)` and the panel's `(parent, synchronize_x, ...)` have the parent elsewhere than last). A test walks every bound class whose constructor has a `parent` argument (read from the bound signature, not assumed last), so a new class can't miss it.
2. **Removal that keeps the widget.** Our non-destroying removals already unparent the widget in C++ (`SciQLopPlotContainer::remove_plot(plot, false)` and `removeWidget(widget, false)` call `setParent(nullptr)`, src/SciQLopPlotContainer.cpp:107-129), so, as with PySide's `setParent(None)`, Python owns it again: `<parent index="1" action="remove"/>` on every bound removal taking a `destroy` flag. Removals that destroy (`remove_panel`, the panel's `removeWidget`, `remove_plot` with `destroy=True`) keep destroying. A walk test covers every bound removal with a `destroy` argument.
3. **`setParent`.** Our widgets inherit `QWidget.setParent`; PySide marks it (`typesystem_widgets_common.xml:2033-2046`) so `w.setParent(None)` gives the widget back to Python and `w.setParent(other)` gives it to `other`. Check that this markup applies to our subclasses (it lives on `QWidget`'s entry); if it doesn't, add the same markup on our classes.
4. **Nothing else changes for Python callers** who keep their references: same classes, same signatures.

## Who is affected

Code that relied on dropping the last reference to delete a parented widget now keeps the widget until its parent dies. It calls `deleteLater()` or removes the widget instead. SciQLop pins SciQLopPlots and is updated with this change; no SciQLop pattern is impossible to migrate:

- lifetime controlled by Python: build without a parent, or `setParent(None)` (Python owns, as today);
- delete now: `deleteLater()` or the container's removal;
- Python subclasses of our widgets: the parent keeps the Python object, and its Python-side state, alive with the C++ widget — today that state can be collected while the widget is in use.

The cost is memory, not correctness: a forgotten parented widget lives until its parent dies, as in Qt.

SciQLop today (read-only survey): production code builds our widgets without a parent everywhere except `SciQLop/user_api/plot/_panel.py:201` (`_SciQLopMultiPlotPanel(self._impl, ...)` then `add_panel`, which already transfers ownership). `TimeSyncPanel` (`time_sync_panel.py:1312`) is a production Python subclass of `SciQLopMultiPlotPanel`: once parented, its Python object lives as long as the C++ panel. Its close / `remove_panel` paths release through `destroyed` handlers, not garbage collection, so that is safe; the implementation checks it.

## Tests

- For each bound widget class, built with a parent: after the Python reference is dropped and garbage collected, `widget.parent() is host` and `host.findChild(...)` finds it; then deleting the host invalidates the wrapper (`shiboken6.isValid` is False) — that last step proves the parent owns it, not a leak. Built without a parent: dropping the reference invalidates the wrapper and `host.findChild(...)` finds nothing.
- `setParent(None)` on a parented widget, then dropping the reference: the wrapper is invalid.
- A class walk: every bound constructor with a `parent` argument transfers ownership (catches a new class).
- Every non-destroying removal: afterwards the widget has no parent and Python owns it (dropping the reference invalidates it).
- Existing ownership tests (`tests/integration/test_item_ownership.py` for items) are unchanged here.
- Gate: SciQLopPlots suite; SciQLop's full suite and the plugins against the build, with SciQLop updated where it relied on dropping references (survey first: SciQLop code building SciQLopPlots widgets with a parent and not keeping them, or dropping them to delete them).
