# Widgets follow PySide's ownership rules

Status: design, 2026-10-23. Follows API review item 1.1 (ownership) for widgets. Items (text, shapes, spans, lines) are part B (item factories, `plot.remove`), which applies the same rule to what a plot draws.

## The rule

The same as PySide: **a Qt parent owns its child.** A widget built with a parent, or handed to a container, lives as long as that parent, whatever happens to the Python reference. A widget without a parent belongs to its Python object.

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

The build passes shiboken's `--enable-parent-ctor-heuristic` (SciQLopPlots/bindings/helper_scripts/shiboken-gen.py:34), which is how PySide's own widgets get this behaviour. The documented conditions hold for our constructors (a constructor, an argument named `parent`, a pointer type), yet no generated constructor sets a parent (`build-venv/.../sciqlopplot_wrapper.cpp`, `Sbk_SciQLopPlot_Init`, has no `Shiboken::Object::setParent`). Every `setParent` in our wrappers comes from explicit `<parent index="this" action="add"/>` markup on methods.

The containers are already right: `add_plot`, `insert_plot`, `addWidget`, `add_panel`, `insert_panel` (bindings.xml ~1669-1709) give the widget to the panel.

## Changes

1. **Constructors.** Every bound constructor that takes a `QWidget* parent` / `QObject* parent` gives the new object to that parent when one is passed. First find why the heuristic doesn't apply and fix that at its root, so new classes get it for free, like PySide's. If shiboken can't be made to apply it to our module, mark each such constructor's parent argument explicitly in bindings.xml (`<modify-argument index="this"><parent index="N" action="add"/></modify-argument>`), and add a test that walks every bound class whose constructor has a `parent` argument, so a new class can't miss it.
2. **Removal that keeps the widget.** `remove_plot(plot, destroy=False)` and any other removal that hands a live widget back gives ownership back to Python (`<parent index="1" action="remove"/>`), as PySide does when an object leaves its parent. Removals that destroy keep destroying.
3. **Nothing else changes for Python callers** who keep their references: same classes, same signatures.

## Who is affected

Code that relied on dropping the last reference to delete a parented widget now keeps the widget until its parent dies. It calls `deleteLater()` or removes the widget instead. SciQLop pins SciQLopPlots and is updated with this change; no SciQLop pattern is impossible to migrate:

- lifetime controlled by Python: build without a parent (Python owns, as today);
- delete now: `deleteLater()` or the container's removal;
- Python subclasses of our widgets: the parent keeps the Python object, and its Python-side state, alive with the C++ widget — today that state can be collected while the widget is in use.

The cost is memory, not correctness: a forgotten parented widget lives until its parent dies, as in Qt.

## Tests

- For each bound widget class: built with a parent, the Python reference dropped and garbage collected, the widget is still a child of the parent and usable; built without a parent, dropping the reference deletes it.
- A class walk: every bound constructor with a `parent` argument transfers ownership (catches a new class).
- `remove_plot(plot, destroy=False)`: afterwards Python owns the plot (dropping the reference deletes it).
- Existing ownership tests (`tests/integration/test_item_ownership.py` for items) are unchanged here.
- Gate: SciQLopPlots suite; SciQLop's full suite and the plugins against the build, with SciQLop updated where it relied on dropping references (survey first: SciQLop code building SciQLopPlots widgets with a parent and not keeping them, or dropping them to delete them).
