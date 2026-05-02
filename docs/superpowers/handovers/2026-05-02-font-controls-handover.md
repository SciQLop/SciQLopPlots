# Handover — Inspector Font Controls

**Date**: 2026-05-02
**Predecessor work**: PR #52 (inspector properties expansion) + `e2a6a3c` (Marker-group disable when shape is NoMarker)
**Tracking issue**: SciQLop/SciQLopPlots#53
**Spec**: `docs/superpowers/specs/2026-05-02-text-font-controls-design.md`

## Where things stand

- `main` (both `upstream` and `origin`) is at `e2a6a3c`. PR #52 is merged. The Marker-group-disable cherry-pick is on top.
- The font-controls spec lives on branch `font-controls-spec` on `origin` (one commit `dd489a4` on top of main, no PR yet, doc-only). When you check out the branch, you get the spec file alongside the main tree.
- Memory updated: see `recent_work.md`, `backlog_font_controls.md` in the project memory dir.

## What this work does

Add font (family / point size / bold / italic) + color controls in the inspector for the four text-bearing UI elements that already have inspector controls but no font customization:

| Element | Object | What's missing today |
|---|---|---|
| Axis label | `SciQLopPlotAxis` | `set_label_font` / `set_label_color` |
| Axis tick labels | `SciQLopPlotAxis` | `set_tick_label_font` / `set_tick_label_color` |
| Legend entries | `SciQLopPlotLegend` | `set_font` / `set_color` (current wrapper has only visibility) |
| Text overlay items | `SciQLopTextItem` | font family setter (size + color exist) |

Out of scope: ColorMap contour-label font — `QCPColorMap2` has no font API for them, would need an upstream NeoQCP change.

## How to start a fresh session

1. **Pull the spec branch** (or merge it locally) so the spec is visible:
   ```bash
   git fetch origin font-controls-spec
   git checkout font-controls-spec
   ```
   Or stay on `main` and read the spec via `git show origin/font-controls-spec:docs/superpowers/specs/2026-05-02-text-font-controls-design.md`.

2. **Read the spec end-to-end.** It defines the reusable `FontDelegate` widget shape, the C++ wrapper additions per object, the delegate updates, and the test plan.

3. **Decide if any clarifying questions are needed** before writing the plan. The spec deliberately leaves a few decisions unresolved that you should flag with the user:
   - Does `FontDelegate` show all four font controls inline, or hide bold/italic behind a "More…" disclosure? Spec assumes inline for compactness; if the panel feels crowded, revisit.
   - For axis label & tick-label fonts, do they share a single `FontDelegate` (one font for both) or two separate widgets? Spec says two separate. Worth confirming with the user given the panel will grow noticeably.
   - Plot title is still missing in C++; the spec excludes it. If the user has appetite to add a plot title in this round, scope expands considerably.

4. **Invoke `superpowers:writing-plans`** against the spec to produce a task-by-task implementation plan in `docs/superpowers/plans/`. The plan will look similar in structure to PR #52's: Phase 1 plotable surface (signals + getters), Phase 2 reusable widget, Phase 3 delegate updates, Phase 4 tests, Phase 5 bindings, Phase 6 integration.

5. **Execute via `superpowers:subagent-driven-development`** — same flow as PR #52. Branch off `main`, dispatch one implementer per task with two-stage review (spec + code), commit per step.

## Project conventions to remember

These cost time when forgotten — load them into context early:

- **Build**: `meson setup build --buildtype=debugoptimized` (only once); then `PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build`. Never use plain debug (`-O0` makes CPU code unusably slow).
- **Tests**: run via `meson test -C build inspector_properties` from the project root with the SciQLop venv on PYTHONPATH=build. Never set `QT_QPA_PLATFORM=offscreen`.
- **Shiboken regen quirk**: any change to a Python-exposed interface header (anything in `include/SciQLopPlots/Plotables/`, `include/SciQLopPlots/SciQLopPlot.hpp`, etc.) requires `touch SciQLopPlots/bindings/bindings.xml && meson compile -C build` to re-run the generator. The custom_target only depends on `bindings.xml`. Then `cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/`.
- **Q_OBJECT registration**: any new class with `Q_OBJECT` must be added to BOTH `moc_headers` AND `sources` in `SciQLopPlots/meson.build`. Forgetting `moc_headers` produces a clean build but an undefined `vtable for <Class>` symbol — surface only fails when the class is instantiated. Detect with `nm -C build/.../SciQLopPlotsBindings.so | grep "vtable for <Class>"` (`d` = good, `U` = bad).
- **Inspector pattern**: forward path = widget signal → setter; reverse path = `*_changed` signal → widget under `QSignalBlocker`. Idempotent setters (early-return when state matches) prevent emit loops.
- **Delegates expose at most `PropertyDelegateBase`** to Python. Don't write Python tests that assert `type(delegate).__name__ == 'SciQLopXxxDelegate'`. Identify delegates by widget composition (group titles, child counts) instead.
- **Panel-level Waterfall** now works (`panel.plot(graph_type=Waterfall)` returns `(plot, wf)` tuple, `_apply_waterfall_kwargs` unwraps).
- **Push targets**: `origin` IS the personal fork (`jeandet/SciQLopPlots`). `upstream` is `SciQLop/SciQLopPlots`. The user has direct push to upstream when they ask for it; don't push there unsolicited.

## Where to find the spec's load-bearing decisions

When you read the spec, pay attention to these sections:

- **"Reusable widget: `FontDelegate`"** — the most visible design choice. Two-signal emission (`fontChanged` + `colorChanged`) instead of one combined `(QFont, QColor)` struct.
- **"Why one widget that emits two signals"** — explains the choice so you don't merge them when planning.
- **"Risks"** section — `QFontComboBox` is heavy on construction; spec proposes one per axis. May need to share or lazy-construct.

## Quick verification once you start

After reading the spec but before planning, sanity-check that the underlying APIs still exist:

```bash
grep -n "labelFont\|tickLabelFont\|labelColor\|tickLabelColor" subprojects/NeoQCP/src/axis/axis.h
grep -n "QFont font\|setFont\|setTextColor" subprojects/NeoQCP/src/layoutelements/layoutelement-legend.h
grep -n "set_font_size\|set_color\|set_font_family" include/SciQLopPlots/Items/SciQLopTextItem.hpp
```

If any of those grep results changed since 2026-05-02, the spec needs a small update before planning.

## Estimated scope

Comparable to roughly two-thirds of PR #52's task volume. ~12-15 implementation tasks once decomposed:
- 1 reusable widget (`FontDelegate`)
- 1 new delegate (`SciQLopTextItemDelegate`)
- 4 plotable wrapper extensions (axis × 4 setters, legend × 2 setters, text item × 1 setter)
- 3 delegate updates (axis, plot, text item)
- ~8-10 test classes
- Bindings + meson registration
