# Handover: SciQLopPlots 0.37.0 for SciQLop #138 / #141 / #142

Written 2026-09-21. Everything below is committed on `main` locally. **Nothing is pushed, nothing is released.** The local tag `v0.37.0` marks the release commit.

## State

| | |
|---|---|
| Branch | `main`, ahead of `origin/main` (unpushed) |
| Version | `0.37.0` (meson.build, `SciQLopPlots/__init__.py`) |
| Tests | `tests/integration`: 1106 passed, exit 0 (run recipe below) |
| Reviews | 4 opencode review rounds over the range; findings fixed or answered in the commit messages |
| NeoQCP | unchanged, pin untouched |

Commits since 0.36.2, oldest first: `8216de4`/`6808f7c`/`4e1840a` (#138), `e72bcaf`/`41790d6` (ND colour API), `b05c284` (auto-enable, decoupling), `b0e960c`/`fff75ae` (shared scale), `da3a62a` (marker signal, arrow spike), `df4ff61` (hide scale, signal declaration), `dc4df6e` (related gaps).

## What SciQLop can use now

### Colour a projection curve by a scalar (#141)
```python
proj = SciQLopNDProjectionPlot(3)
g = proj.parametric_curve([t, x, y, z], labels=["xy", "yz", "zx"])
g.set_color_data(density, ColorGradient.Jet)   # ValueError on wrong length, TypeError on non-numeric
proj.z_axis().set_log(True)                    # non-positive values are not drawn
proj.z_axis().set_label("n [cm^-3]")
proj.z_axis().set_range(SciQLopPlotRange(1, 100))   # pins the range
proj.set_z_auto_range(True)                    # follow the data again
proj.set_z_gradient(ColorGradient.Thermal)     # preset for the shared scale
proj.set_z_gradient_colors(QColor("blue"), QColor("red"))   # two-stop ramp
```
- **One scale per projection plot.** It sits next to the last pane, shows only while a graph carries a scalar, and hides again when the last scalar is cleared (`set_color_data(empty)`) or the last coloured graph is removed. Every pane and graph reads range, log and gradient from it.
- The range follows the coloured data of all graphs until it is set through `z_axis()`.
- A `3n` payload via `set_data([x, y, c] * 3)` switches colouring on by itself and keeps the plot's gradient. `set_color_data(values, gradient)` always applies its gradient to the shared scale (default `Jet`), replacing an earlier `set_z_gradient`.
- NaN in the scalar is a gap. Points beyond the end of a shorter scalar are not drawn.
- Also new on the ND graph: `set_color_gradient(ColorGradient)`, `colors()`, `set_line_width(w)` / `line_width()`, `set_visible(bool)` / `visible()` (+ `visible_changed`), `components()` / `component(i)` / `component(name)` (one per pane).
- Also new: `SciQLopNDProjectionPlot.set_shared_legend(True)` (legend on the first pane only), `SciQLopMultiPlotPanel.set_plot_stretch(plot, factor)` / `plot_stretch(plot)` (relative heights, honoured by `organize_plots()`).

### Colour a curve on an ordinary plot
```python
curve = plot.plot(t, y, graph_type=GraphType.ParametricCurve, labels=["B"])   # x = time works too
curve.set_color_data(values, ColorGradient.Jet)
plot.z_axis().set_log(True); plot.z_axis().set_label("n [cm^-3]")
plot.set_z_gradient(ColorGradient.Thermal); plot.set_z_auto_range(True)
plot.set_curve_color_scale_enabled(False)   # old behaviour: own range and gradient, no scale
```
The plot's own colour scale (the one colormaps use) now serves its coloured curves, with the same rules as a projection plot: shown while a curve carries a scalar, range follows all coloured curves until set through `z_axis()`, hidden when none does. A colormap on the plot keeps the scale to itself. **Behaviour change:** `set_color_data` on a curve now draws a colour bar. Why curves and not line graphs: `docs/colour-by-scalar-curves-vs-line-graphs.md`.

### Follow the time marker (#142)
`proj.time_marker_changed(t: float)` fires when the marker moves, `nan` when cleared, silent when unchanged. Nothing in this library moves the marker on cursor movement: that producer, and any throttling, are SciQLop's.

### Arrows on a pane (#142)
A `SciQLopCurvedLineItem(pane, start, stop, NoneTermination, Arrow, Coordinates.Data)` renders on `proj.subplot(i)` with a pixel-sized head. SciQLop does the 3-D to pane projection and hands 2-D data coordinates down. See `tests/integration/test_projection_arrow_on_pane.py`.

### Behaviour changes to expect
1. A `3n` payload switches colouring on without `set_time_color_enabled(True)`.
2. Time values no longer overwrite an explicit scalar, so re-applying the scalar after a data refresh is only needed when the length changes.
3. `proj.z_axis()` is no longer `None` (it was an abstract-method warning). It is invisible until something is coloured.
4. A projection function graph fetches for the plot's current time range as soon as it is created.
5. `organize_plots()` now weights by plot stretch (all 1 by default, so unchanged unless someone sets one).

### #138
`ProductsModel::add_node` moves a node called from another thread to the model's thread and re-queues (non-blocking). A node that cannot be moved (already parented) is refused with a warning. **The real crash is not confirmed fixed**: it reads freed memory, so the tests only check which thread mutates the model. Confirm on macOS: re-run a cell that re-registers a virtual product with a Products search query active. The proper fix is still SciQLop's (`EasyProvider.__init__`, `layers/_provider.py` should register on the GUI thread).

## Still open

| Item | Where | Notes |
|---|---|---|
| Colour-by-scalar for `SciQLopLineGraph` (#141 item 7) | SciQLopPlots + **NeoQCP** | Deferred, see "Decision: line graphs deferred". |
| Quiver plottable (#142) | SciQLopPlots | Deferred by choice. Depends on the shared scale, which now exists. |
| `remove_graph` on `ProjectionPlot`, `color_by=`, `ColorBy`, `SpeasyVariable` returns, `Graph` colour properties, inspector/templates, `describe_panel` | SciQLop | `SciQLopPlotInterface.remove_plottable(graph)` exists here and deletes the graph. |
| Layer `Depends`, `MarkerTime` producer, typed annotation renderer, arrow scale key | SciQLop | See #142. |
| Time marker follows zoom? | SciQLopPlots | `set_time_marker` stores pixel coordinates once; not checked whether they follow later zoom/pan. |
| `SciQLopPlotColorScaleAxis` is not exposed to Python | SciQLopPlots | Registering it breaks `SciQLopColorMapBase`'s wrapper (abstract). Gradient goes through `proj.set_z_gradient` instead. |

## Decision: line graphs deferred
Full analysis and how to pick it up: `docs/colour-by-scalar-curves-vs-line-graphs.md`. Summary:
`SciQLopLineGraph` is a NeoQCP `QCPMultiGraph`. Per-point scatter colours exist only on `QCPGraph2` (`setScatterColorValues`, about 120 lines across CPU and GPU paths). `QCPMultiGraph` has none, and coloured *line segments* exist in no NeoQCP plottable. So colouring a time-series line by a scalar needs a NeoQCP change (markers only, or markers and line segments), which means a push to NeoQCP's origin and a pin bump in `subprojects/NeoQCP.wrap` before this repo can build for anyone else. Deferred by the maintainer on 2026-09-21; curves on ordinary plots get a real Z scale instead (see the note).

## Build and test recipe (this machine)
```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export LLVM_INSTALL_DIR=/usr        # else shiboken fails on stddef.h
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=<repo>/build-venv $VENV/bin/python -m pytest <repo>/tests/integration -q
```
One build or test invocation at a time against `build-venv`.

## Hazards found on the way
- **Stale shiboken wrapper.** After changing a bound class's header, a new method may be missing (or a virtual override ignored). Fix: delete `build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/<class>_wrapper.cpp`, then `meson setup --reconfigure build-venv`.
- **Signals.** A new signal on a bound class must sit between `#ifdef BINDINGS_H` / `signals:` / `#endif` and a following `#ifdef BINDINGS_H` / `public:` / `#endif` (see `SciQLopPlotInterface.hpp` around line 517). A bare `Q_SIGNAL` among public members works in an incremental build but is exposed as a plain method after a clean regeneration.
- **Do not register `SciQLopPlotColorScaleAxis`** in `bindings.xml` (see above).
- **Rendering tests.** Count saturated pixels or crop, never total ink (axes, legend and the colour bar add pixels). `QRhiWidget.grab()` is black offscreen; use `save_png`. Run a new test against the build without the fix before trusting it: two "reproducers" here passed without it.
- **`ls` is an alias** on this machine that prints the project root when its argument does not exist.

## Not verified
- Nothing was run against the SciQLop repo or the Magnetopause notebook.
- Rendering thresholds (hue counts, pixel ratios) were tuned on the offscreen platform only.
- The #138 crash itself (above).
