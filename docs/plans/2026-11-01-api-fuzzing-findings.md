# API fuzzing: findings

First full sweep: 2026-11-01 UTC, branch `feat/api-fuzzing` at `f9a63df`. 11,561 cases in 10 class chunks.

| Status | Cases |
|---|---|
| pass | 10,516 |
| swallowed | 426 |
| wrong_class | 403 |
| no_error | 169 |
| crash | 33 |
| bad_message | 14 |

Many of these come from the harness itself. The harness rules were tightened and the sweep was run
again (see "Harness corrections").

Each finding group below is a root cause, not a single method. Status is `open` or `fixed <sha>`.

## Crashes

### F1. `None` for an object argument dereferences a null pointer

- **Status:** fixed 681433cb
- **Affects** about 25 methods:
  - `add_panel`, `insert_panel`, `remove_panel`;
  - `add_widget`, `addWidget`, `remove_widget`, `removeWidget`;
  - `register_behavior`;
  - `XAxisSynchronizer.plotAdded`, `XAxisSynchronizer.resynchronize`;
  - `ProductsModel.add_node`, `ProductsModel.decode_mime_data`, `ProductsModel.node_data_changed`;
  - `ProductsModelNode.add_child`, `ProductsModelNode.child_row`;
  - `DataProviderWorker.set_data_provider`;
  - `DelegateRegistry.create_delegate`;
  - `PropertyDelegateBase.fit_combo_to_content`.
- **Shared fix point:** the bindings. `None` for a pointer the C++ never accepts as null must raise
  `TypeError` before the call.
- **Reproducer:** `SciQLopMultiPlotPanel().add_panel(None)` → segfault.

### F2. A 0-d array as data segfaults

- **Status:** fixed f4a0f6a6 (root cause: shiboken container converter, 29 sites)
- **Affects:** `SciQLopNDProjectionPlot.add_reference_curve`, `reference_curve`;
  `SciQLopCurve.set_color_values`, `set_time_values`; `RemoteDataPipeline.set_data`;
  `DataProviderWorker.set_data`.
- **Shared fix point:** the `SciQLopPyBuffer` conversion (`src/PythonInterface.cpp`).
- **Reproducer:** `curve.set_time_values(np.array(1.0))` → segfault.

### F3. `move_plot` with a bad index

- **Status:** fixed 60fae97a
- **Crash:** `panel.move_plot(None, 0)` and `panel.move_plot(-1, 0)` segfault.
- **Empty message:** `insert_plot` and `move_plot` with an infinite index raise `OverflowError` with
  an empty message.
- **Fix point:** index validation in the panel's move and insert.

### F4. Pen and font sizes are not validated

- **Status:** fixed 32cce9ff
- **Crash:** `set_line_width(nan)` aborts on a Qt assertion (`!std::isnan`).
- **Swallowed:** inf, -1 and huge widths print `QPen::setWidthF: ... out of range`
  (`set_line_width`, `set_contour_width` and others, 40 cases). A font size ≤ 0 prints
  `QFont::setPointSizeF: ...` (`set_font_size`).
- **Fix point:** one shared check for finite, non-negative widths and positive font sizes, used by every
  setter.

## Swallowed errors

### F5. Numbers out of an integer's range

- **Status:** fixed 60fae97a
- **Problem:** passing a huge or non-finite number for an `int` parameter prints shiboken's
  `RuntimeWarning: libshiboken: Overflow` (204 cases). The call then goes on with a truncated value.
- **To check:** which of these calls also raise afterwards. The first sweep counted those as swallowed;
  see "Harness corrections".

### F6. `save` with an empty or missing file name

- **Status:** fixed bd5c2fba
- **Problem:** it prints `QFSFileEngine::open: No file name specified` or
  `QPainter::begin(): Returned false` and returns `False`.
- **Expected:** `ValueError` for an empty name, `OSError` for a write that fails.

### F7. shiboken copy-conversion errors are printed, not raised

- **Status:** fixed b5293543 for this module's string lists; a list of numbers given as QFont families still prints from PySide's own QtGui converter (J3)
- **Problem:** `_pythonToCppCopy: Cannot copy-convert ... to C++` (26 cases), for example `set_labels`
  given numpy floats or `set_label_font([1.0])`.

### F8. A Python traceback printed from inside a call

- **Status:** fixed by 58252984 + 60fae97a: the call raises a named OverflowError; libshiboken still prints its own warning line
- **Problem:** `TypeError: only integer scalar arrays can be converted to a scalar index` (10 cases),
  seen in `plottable()` probes.

### F9. Calls that need a particular axis or plot only warn and do nothing

- **Status:** fixed 47f24dd4 for the axis calls; \"cannot draw a colour axis\" kept as a documented degrade (data still drawn, warned once, also reached by provider batches)
- **Examples:**
  - "A time axis can't be logarithmic";
  - "set_date_format: not a time axis";
  - "Text tick labels are not supported on a time axis";
  - "cannot draw a colour axis";
  - `couple_range_with(None)` / `decouple_range_from(None)`, which print a `QObject::connect` nullptr
    message.
- **Expected:** the synchronous ones raise `ValueError`.

### F10. A complex dtype is silently cast to real

- **Status:** fixed 6cf056c2 (helpers) and 58252984 (lists of doubles)
- **Problem:** `band` and `set_color_values` print numpy's `ComplexWarning`, then drop the imaginary
  part.
- **Expected:** `TypeError`.

## Wrong exception class

### F11. The buffer converter calls a non-array a bad value

- **Status:** fixed 07bb7d67, curve set_data(None) 88d0329e
- **Problem:** `object()` or a dict where array data is expected raises `ValueError` ("called with wrong
  argument values"), 74 cases. It should be `TypeError`.
- **Related:** `None` as array data is accepted without any error (66 cases: `set_data`,
  `set_color_data`, `set_values`, ...). It should be `TypeError`.
- **Fix point:** the `SciQLopPyBuffer` conversion.

### F12. Argument errors thrown as `std::runtime_error`

- **Status:** fixed ecbb4ae3
- **Examples:**
  - "Keys (x) must be float64", for int8 keys (`SciQLopMultiGraphBase.cpp`, `SciQLopColorMap.cpp`):
    should be `TypeError`;
  - the ColorMap size checks and "Histogram2D.set_data: x and y must have the same length": should be
    `ValueError`;
  - "Invalid number of labels" / "Invalid number of colors": should be `ValueError`.
- **Fix point:** the throw sites, which throw `std::invalid_argument` or `sqp::type_error`.

### F13. Other wrong classes

- **Status:** fixed 6cf056c2
- **Examples:**
  - `bars(None, h)` says "edges must be 1-D" (`ValueError`): it should be `TypeError`.
  - `set_normalization(object())` raises `ValueError`: it should be `TypeError`.
  - `set_snap_to(object())` raises `ValueError`: it should be `TypeError`.

## Other

### F14. Invalid colour names are accepted

- **Status:** fixed 4a200ce7 (J3)
- **Problem:** `set_color("not a number")` and similar return normally with an invalid `QColor`.

### F15. `panel.plots()` returns wrappers that the panel's own methods refuse

- **Status:** fixed 4cfb8344 (user's choice: Python gets real plots; history in the commit)
- **Problem:** `index_of`, `plot_stretch` and `set_plot_stretch`, and `XAxisSynchronizer.plotAdded`
  and `resynchronize`, raise `TypeError` for the `SciQLopPlotInterfacePtr` items that `plots()`
  returns.

### F16. `move_panel` is never implemented

- **Status:** fixed 0ec2afb6
- **Problem:** `SciQLopMultiPlotPanel.move_panel` raises `NotImplementedError` ("not supported by
  SciQLopMultiPlotPanel").

### F17. Two async tests fail under machine load

- **Status:** open (observed 2026-11-01, load average about 4–5; both pass on a quiet machine, and the
  first passed 6/6 on the same build afterwards)
- `test_drawn_aggregates::test_a_slot_deleting_the_projection_mid_colouring_does_not_crash`: the slot
  on `data_changed` had not run after one event pump. "No crash" held every time.
- `test_fit_keeps_caller_view::test_a_first_fit_survives_a_linking_change_before_it_runs[2-0]`: pane 0
  was not fitted. This could be a real lost-fit race. Look into it under stress (story fuzzer, deep run).
- `test_panel_span::test_a_rectangular_part_edit_reaches_both_axes`: found one part instead of two, at load
  average 23; it passes 5/5 alone.

### F18. `findChildren` on a temporary plot wrapper invalidates what it found

- **Status:** open, recorded 2026-11-01
- **Problem:** PySide makes the found children's wrappers children of the wrapper `findChildren` was
  called on. When that is a temporary (`panel.plot_at(0).findChildren(...)`), dropping it invalidates
  them while the C++ objects live on: "already deleted".
- **Why it doesn't show today:** the `add_plot` wrapper in `__init__.py` (a Python override of a C++
  virtual, added in June to unwrap handles) keeps every plot's wrapper alive with its panel. That makes
  this safe by accident, and the suite depends on it. Removing that wrapper broke 58 tests.
- **Next:** decide whether to keep it deliberately (and document why) or keep plot wrappers alive
  some other way.

## Judgment calls (for the user)

- **J1. Errors inside user callbacks that run later.** These are data providers and model curves.
  Today a traceback is printed and that batch of data is dropped. Examples: `DataProviderInterface` with
  no `get_data`, a model curve callable with the wrong signature, a provider returning the wrong number
  of arrays.
  - **User's answer (2026-11-01): print the traceback once and also emit `fetch_failed` with the
    message.** The plot stays usable, and the sweep treats this as correct.
  - **Checked 2026-11-01:** callback graphs and model curves already did this (sys.excepthook + fetch_failed).
    A bare `DataProviderInterface` didn't, and now does (e5976604).
- **J2. Python-only methods of a deleted `WidgetPlot`.** Methods such as `color_palette` and
  `on_time_range` keep answering from Python state after the C++ side is gone.
  - **User's answer (2026-11-01): raise `RuntimeError`,** like every other deleted object. Fixed 70fa972c.

- **J3. Conversions PySide owns.** An invalid colour name (`set_color("not a colour")`) becomes an invalid
  QColor with no error. A list of numbers given as QFont family names prints one PySide error per item.
  Both happen in PySide's QtGui converters, shared by every module. A fix here would mean replacing those
  converters for this module (as was done for string lists) and telling an invalid colour from `QColor()`
  meaning "automatic". This is about 40 sweep cases (F14 and part of F7).
  - **User's answer (2026-11-01): fix both here.** Fixed 4a200ce7: the generator checks each colour and font
    argument of this module before PySide converts it.

- **J4. One colour map per plot, or not?** A second `colormap()` was refused, but `histogram2d` (same colour
  scale) wasn't counted, so they stacked. Found by the story fuzzer.
  - **User's answer (2026-11-01): one colour map of any kind.** Fixed cea90658.

## Found by the story fuzzer and the off-thread sweep (Task 8)

- Strided x in band/error_bars refused by the C++ conversion: fixed d17d940d.
- An unknown keyword on a method raised AttributeError: TypeError since 1b5dfec5.
- Off the GUI thread: plot.set_name segfaulted; factories built cross-thread children (bars crashed);
  set_data and set_busy ran on the caller's thread. Fixed dfefc737.

## Found by the opencode scripts (round 1)

opencode wrote 47 adversarial scripts. Most of the failures were mistakes in the scripts: wrong method
names, expecting `deleteLater` to run inside `processEvents()`, or counting one `fetch_failed` per provider
when each new range fetches again. The real bugs:

- Putting a panel inside itself, or inside a panel it holds, hung in layout. ValueError since dfaf473a.
- `panel.plot()` from a worker thread built the plot widget on that thread. Plots and panels raise
  RuntimeError off the GUI thread since dfaf473a.
- A provider returning something other than a list or tuple of arrays was dropped without a word. It
  emits `fetch_failed` with the reason since 57105ddc.
- A band or error_bars callback returning the wrong number of arrays gave an internal "missing positional
  argument" message. It says what it expects since 57105ddc.

## Harness corrections after the first sweep

These are Claude's calls.

- **Integer parameters.** NaN, inf or a float given for an `int` parameter may raise `TypeError`. A float
  is the wrong type there.
- **`bool` parameters** accept any object by truthiness, which is PySide's convention. They get no
  wrong-type values.
- **Dicts.** A dict argument is its own kind. A dict is not a bad value for a sequence parameter,
  because it iterates.
- **Printed noise.** stderr noise fails a case as "swallowed" only when the call returned normally. A
  call that raised the right class is graded on its exception, and the noise is kept in the detail.
