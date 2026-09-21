# Session handover: where the 0.37.0 work stands and what to do next

For a fresh session picking this up. Read this first, then the two other notes:
- `docs/handover-2026-09-21-sciqlop-141.md`: the API SciQLop can use, behaviour changes, open items, build hazards.
- `docs/colour-by-scalar-curves-vs-line-graphs.md`: why line graphs are deferred (NeoQCP).

## State (2026-09-21, about 11:40)

| | |
|---|---|
| Branch | `main`, **30 commits ahead of `origin/main`, nothing pushed** |
| HEAD | third review round fixes (gradient kept across a colormap, inspector drops the colour axes, inspector picks recorded) on top of `2cd1f3a` |
| Tag | `v0.37.0` on HEAD, **local only** (it was moved several times; safe because never pushed) |
| Version | 0.37.0 (`meson.build`, `SciQLopPlots/__init__.py`) |
| Tests | `tests/integration`: **1133 passed, exit 0** |
| Working tree | clean |
| NeoQCP | untouched, pin unchanged |
| SciQLop repo | never modified (user rule: do not touch it; reading is fine) |

Issues #138, #141, #142 on SciQLop have up-to-date comments from me (plan, shipped API, follow-up with the gaps and the curve scale). They do not mention the last two review rounds (ownership and visibility details); fold that into the comment made when pushing.

## What is done (one line each, details in the API handover)
- #138: `ProductsModel::add_node` queues onto the model thread. Real crash not confirmed fixed (needs macOS).
- #141: colour a projection curve by a scalar; one shared scale per projection plot; the same for curves on ordinary plots (`ColorScaleController`); NaN gaps; log; gradient; pinning; line width; components; visibility; shared legend; panel plot stretch; projection graphs fetch on creation.
- #142: `time_marker_changed` signal; arrow-on-pane spike (works, pixel head).
- Deferred on purpose: quiver plottable; colour-by-scalar for `SciQLopLineGraph` (needs NeoQCP change, push and pin bump: user consent needed).

## Also done after the colour work
- `ProductsModel::remove_node(path)` (design challenged by opencode, tests first, 12 tests): path read like `node()`, queued onto the model thread, subtree removed, no pruning of empty parents, root and missing paths ignored, `ProductsView` suggestions refresh on removal too. The `ProductsModel`'s own completer list stays append-only (nothing reads it). **Not reviewed by opencode yet.**
- Backlog: `docs/backlog-2026-09-21.md` B1, a way for SciQLop to identify product kinds (virtual products, vectors, spectrograms) without SciQLopPlots implementing each.

## What to do next

0. **Round 6 and 7 are done** (opencode reviewed `2cd1f3a`, challenged the fix plan, reviewed the fixes): gradient requests are now recorded by `ColorScaleController` also during colormap ownership and re-applied on reclaim; `PlotsModel::remove_rows_unchecked` detaches the z/y2 axis nodes; inspector gradient picks go through `plot.set_z_gradient`. Known and left: a colour scale left visible when the controller is disabled and a colormap goes (was never hidden before the controller either).
1. **Decide with the user: one more opencode review round, or push.** (Round 8 reviewed the inspector-pick fix: no defect. The "Custom" combo entry, the last commit, is unreviewed.) Every round so far found real bugs (once a segfault introduced by my own fix). The last commit `2cd1f3a` touches the colormap-ownership logic again and has been reviewed by nobody. My recommendation was one more round. Run it with the workflow below, brief scope: `git diff 7f32124..HEAD`.
2. **Push only on the user's explicit request** (global rule). Push `main` and the tag `v0.37.0` to the fork/origin the user names; check `git remote -v` first (a personal-username remote next to an org remote is the tell; never push a feature branch to the upstream org remote). Before opening any PR: `meson wrap update`, rebuild, rerun the full suite (user rule for meson projects).
3. After pushing: comment on SciQLop #141/#142/#138 with the release link and the ownership/visibility notes. Nothing more until something changes.
4. Optional, low priority, all listed in the API handover "Still open": nested-panel `set_plot_stretch`, legend/`H` visibility bypass, time marker following zoom (unchecked), quiver.

## How to build and test (this machine)
```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export LLVM_INSTALL_DIR=/usr        # else shiboken fails on stddef.h
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=<repo>/build-venv $VENV/bin/python -m pytest <repo>/tests/integration -q -p no:cacheprovider
```
Rules: one build or test invocation at a time on `build-venv`, in the foreground. After changing a bound class's header, delete its `*_wrapper.cpp` under `build-venv/SciQLopPlots/bindings/SciQLopPlotsBindings/` and `meson setup --reconfigure build-venv`, or new methods silently do not exist.

## How to get an opencode review (worked five times)
Always by pane id, never by name; confirm the pane id before prompting.
```bash
NEW=$(herdr pane split --current --direction right --cwd "$PWD" --no-focus | grep -o '"pane_id":"[^"]*"' | head -1 | cut -d'"' -f4)
herdr agent start <name> --kind opencode --pane "$NEW" -- --auto     # the first start sometimes needs a second try
herdr agent list | grep -o '"name":"<name>","pane_id":"[^"]*"'       # verify
herdr agent prompt "$NEW" "Read <brief.md> and follow its instructions exactly." --wait --timeout 1500000
herdr agent wait "$NEW" --until idle --until done --timeout 1500000  # `blocked` right after the prompt is a transient misread
herdr pane close "$NEW"                                              # only panes you created
```
- Put the request in a brief file (scratchpad), ask for the answer in a file, reply only `DONE`. Say READ-ONLY, no builds, no files in the repo.
- **The reviewer has twice left a stray empty file in the repo root** (`z_axis(`, then ` s…`). After every review run `git status --short`, check the file is empty (`python3 -c "import os; print(os.path.getsize(...))"`, the shell mangles odd names) and delete it.
- Verify every finding against the code before acting. About a third were false positives or already-handled; the rest were real. Write a failing test first, and **run each new test against the build without the fix** at least once: two of my "reproducers" passed without the fix.

## Pitfalls specific to this session (do not repeat)
- `pkill -f` / `pgrep -f` with a pattern that appears in my own command line kills my own shell (exit 144). Use PIDs.
- Heredocs inside long Bash commands got mangled twice; write files with the Write tool.
- `ls` is an alias that prints the project root when the argument does not exist; use `find` or Python.
- `QRhiWidget.grab()` is black on the offscreen platform; render panes with `save_png`.
- Rendering tests: count saturated pixels or crop, never total ink (axes, legend and the colour bar add pixels). Crop the colour bar out when measuring a curve.
- A bare `Q_SIGNAL` in a bound header works incrementally but turns into a plain method after a clean shiboken regeneration; declare signals after `#ifdef BINDINGS_H` / `signals:` (see `SciQLopPlotInterface.hpp` ~517) and restore `public:`.
- Do not register `SciQLopPlotColorScaleAxis` in `bindings.xml` (breaks `SciQLopColorMapBase`'s wrapper).
- Anything that reacts to `graph_list_changed` and walks a plot's children must stay quiet during the plot's destruction (`ColorScaleController::m_dying`, `quiesce()`); it crashed once at test exit.
- The shell's working directory drifts when a command `cd`s; use absolute paths.

## Where things live
- `include/SciQLopPlots/ColorScaleController.hpp`, `src/ColorScaleController.cpp`: the shared scale logic (ownership vs colormaps, auto-range, pinning, gradient, show/hide, teardown guard).
- `src/SciQLopNDProjectionPlot.cpp`: `_setup_color_scale` (panes' own controllers switched off, one for the plot on the last pane).
- `src/SciQLopPlot.cpp`: the pane/plot wrapper API (`z_auto_range`, `set_z_gradient`, `set_curve_color_scale_enabled`, `show/hide_color_scale`).
- `src/SciQLopCurve.cpp`, `src/SciQLopTimeColoredCurve.cpp`: per-curve colour data, scale attachment, NaN gaps, `_notify_plot`.
- `src/SciQLopNDProjectionCurves.cpp`: the projection graph facade (components, visibility, colour forwarding).
- Tests: `test_curve_color_scale.py`, `test_projection_color_scale.py`, `test_projection_color_data.py`, `test_projection_graph_api_gaps.py`, `test_projection_fetch_after_range.py`, `test_panel_plot_stretch.py`, `test_projection_time_marker_signal.py`, `test_projection_arrow_on_pane.py`, `test_products_model_threading.py`, `test_show_color_scale.py`.
- Memory (auto-loaded): `project-colour-axis-plan-sciqlop-141.md`, `reference-colour-by-scalar-curves-vs-line-graphs.md`.
