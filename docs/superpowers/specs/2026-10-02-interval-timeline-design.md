# Interval timeline — design

Issue: [SciQLop/SciQLopPlots#123](https://github.com/SciQLop/SciQLopPlots/issues/123)
Date: 2026-10-02

## Goal

One plottable for time intervals on named lanes. It replaces today's workarounds
(step lines, vertical spans, rectangles drawn one by one).

First user: the BepiColombo / Mio planning plugin. It must show a plan, let the user
move, resize, re-lane, create and delete windows, and write the result back to its plan
file. Other uses: instrument modes, event catalogs, plasma regions, data availability.

Success criteria:

- One call with columnar data draws everything: `set_intervals(start, stop, lane=...)`.
- 10⁵–10⁶ intervals stay fluid when panning and zooming.
- Shares the panel's time axis.
- Edits are only reported through signals. The application owns the data, accepts or
  refuses, then calls `set_intervals` again.
- Compact: a lane is a fixed number of pixels, not a share of the plot.

## Scope

In scope:

- Drawing, categories with colours and legend, instant events.
- Several timelines in one plot, stacking by lane name.
- Two placements: a dedicated compact timeline plot, and a strip on top of any
  time-series plot.
- Ids, hover, selection (click, ctrl-click, rubber band).
- Editing: move, resize, change lane, create, delete; snapping; keyboard nudge.

Out of scope (later spec):

- Collapsible lane groups (`lane=("MPPE_MSA", "LM")`).
- Callback data source `f(start, stop) -> intervals`.
- SciQLop / speasy catalog mapping.
- Curves drawn inside a single lane, value-coloured bars, dependency arrows.
- Per-category legend entries: the plottable gets one plain legend icon for now.

## Architecture

Two layers, following the `QCPMultiGraph` / `SciQLopLineGraph` split.

- **NeoQCP** — `QCPIntervals` (plottable) and `QCPLaneLayout` (shared lane geometry).
  Owns data columns, GPU drawing, hit testing, rect selection, the drag state machine.
  Works only with integer lanes, categories and ids.
- **SciQLopPlots** — `SciQLopTimeline` (plotable wrapper). Maps lane and category strings
  to indices, owns the panel-wide category palette, converts times, exposes the Python
  API and signals with the user's ids.

No new plot class. `panel.add_timeline()` creates a `SciQLopTimeSeriesPlot` configured
for the lanes placement, so `TimeAxisSynchronizer` syncs it with no change.

Rejected alternatives:

- Editing in `SciQLopPlot` mouse handlers: duplicates NeoQCP event dispatch and fights
  the rubber band.
- One span item per interval: one `QCPAbstractItem` each, does not scale past a few
  thousand.

### Two invariants

1. **Data is never edited in place.** During a gesture `QCPIntervals` draws a preview of
   the dragged bars only. On release it emits the edit. The data changes only when the
   application calls `setData` / `set_intervals`.
2. **Who gets the mouse press** is decided by the hit test (see Interaction). When the
   plottable does not claim the press, it falls through to the rubber band or pan.

## Data model

### `QCPIntervals` (NeoQCP)

Columns, all the same length *n*:

| column | type | notes |
|---|---|---|
| start | double | seconds since epoch |
| stop | double | `stop >= start`; `stop == start` is an instant event |
| lane | int | index into the shared `QCPLaneLayout` |
| category | int | index into the colour table |
| id | int64 | opaque, reported back in signals |
| label | QString (optional) | drawn inside the bar when it fits |

- `setData(...)` replaces all columns at once. There is no per-item mutation API.
- Internally rows are grouped per lane and sorted by `start`. Each lane keeps its
  maximum duration, so the visible rows are found by binary search on
  `[range.lower - maxDuration, range.upper]`.

### `QCPLaneLayout` (NeoQCP)

Shared by every `QCPIntervals` in one axis rect.

- Ordered lane names, visibility per lane, `laneHeight` (px, default 14), placement
  (`Strip` or `Lanes`).
- One pure function: `laneBand(laneIndex, axisRect) -> (top, bottom)` in pixels.
  Drawing, hit testing and the lane tick labels all go through it.
- `Lanes` placement: the bands start at the top of the axis rect and the y axis is set
  to the matching range, with lane names as tick labels (the #120 tick-label API).
- `Strip` placement: the bands start at the top of the axis rect, drawn over the data.
  The plot's own y axis is untouched; lane names are drawn as small text at the left of
  the strip.

### `SciQLopTimeline` (SciQLopPlots)

- Lane strings → indices in the plot's lane layout; unknown names are appended in
  first-seen order. Two timelines using the same name share that lane.
- Category strings → indices; colours come from a process-wide palette, so a category
  has the same colour in every plot, in every panel. `set_category_colors` overrides.
- `ids` default to `0..n-1`. Signals always report these ids, never row indices.
- Times accept epoch floats or `datetime64`.
- Validation raises `ValueError`: columns of different lengths, `stop < start`, NaN in
  `start` or `stop`.

## Drawing

### Vertex build (CPU, one pass)

1. For each visible lane, binary-search the visible rows.
2. Map each bar to pixels; clamp its width to at least 1 px.
3. Merge consecutive bars on the same lane with the same category when they overlap in
   pixels. Output size is then bounded by roughly lanes × plot width.
4. Emit two triangles per bar (6 floats per vertex: position + premultiplied colour)
   into one buffer and send it with `QCPPlottableRhiLayer::addPlottable`. Instant events
   become a small diamond.

### Cache

- Vertices are rebuilt when data, axis range, plot size, layout or colours change. The
  merge bounds a rebuild to the visible rows. GPU-offset reuse on pan is the upgrade
  path.
- The "needs rebuild" decision is one predicate, used by the draw code and covered by
  its own unit test. (The 2026-10-02 step-line selection bug came from two places
  deciding this separately.)

### Overlays (QPainter, visible items only)

- Bar labels, only when the text fits inside the bar.
- Selection outline (selected bars keep their category colour).
- Drag preview.
- Lane names in `Strip` placement.

### Colour and export

- Strip placement draws bars at ~70% opacity so the data below stays readable.
- PDF/SVG export draws the same rectangles with QPainter, as other GPU plottables do.

## Interaction

### Hit test (pixels, through the lane layout)

1. Lane from the mouse y via `laneBand`.
2. Nearest bar on that lane near the mouse x (binary search).
3. Part:
   - `LeftEdge` / `RightEdge`: within 4 px of an edge.
   - `Body`: inside the bar.
   - `Empty`: on a lane, no bar.

   Bars narrower than ~10 px have only a `Body`.
4. The cursor follows the part (resize over edges, move over the body), through
   `SciQLopPlot::_update_mouse_cursor` and its existing 60 Hz throttle.

### Press routing

| press on | `editable == False` | `editable == True` |
|---|---|---|
| Body | select | select, drag to move; vertical drag changes lane if `change_lane` |
| Edge | select | resize |
| Empty lane | Shift+drag for rubber band, else pan | create if `create`, else Shift+drag for rubber band, else pan |

- Ctrl-click toggles a bar in the selection.
- Rubber band is Shift+drag on empty lane space, handled by `QCPIntervals` itself.
  QCustomPlot's selection-rect mode is not enabled: it steals every press.
- `QCPIntervals` implements `QCPPlottableInterface1D`, so rect selection reaches it via
  `selectTestRect` (as `QCPMultiGraph` does).

### Gesture lifecycle

1. **Press** — record the selected ids with their original start, stop and lane.
2. **Move** — compute the delta, snap it, update the preview only.
   - `snap_to="edges"`: snap the moving edge to the nearest other interval edge within
     8 px.
   - `snap_to=<seconds>`: snap to that step.
   - `snap_to=None`: no snapping.
3. **Release** — emit one signal for the whole gesture. Emit nothing if nothing moved.
4. **Escape** during a gesture cancels it and drops the preview.

### Keyboard

- Arrow keys nudge the selection by the snap step, or by one pixel's worth of time when
  there is no step. Each nudge emits `intervals_changed`.
- Delete emits `delete_requested(ids)` when `delete` is in `edit_modes`.

## Python API

```python
plot, tl = panel.add_timeline(lane_height=14)  # new compact plot, Lanes placement
tl2 = plot.add_timeline()                      # joins that plot's lanes (stacks)
strip = ts_plot.add_timeline(lane_height=12)   # on a regular time-series plot: Strip

tl.set_intervals(start, stop, lane=lanes,
                 category=None, label=None, ids=None)
tl.lanes = ["MSA", "MPPE", "MGF"]              # sets order and visibility, shared per plot
tl.rename_lane("MSA", "MSA_HI")                # renames a lane in place
tl.set_category_colors({"LM": "#f59e0b"})

tl.editable = True                             # default False
tl.edit_modes = {"move", "resize", "change_lane", "create", "delete"}
tl.snap_to = "edges"                           # or seconds, or None

tl.intervals_changed.connect(cb)               # [(id, start, stop, lane), ...]
tl.interval_created.connect(cb)                # (start, stop, lane)
tl.delete_requested.connect(cb)                # [ids]
tl.selected_intervals_changed.connect(cb)      # [ids]
tl.hovered.connect(cb)                         # id, or -1 when leaving
```

- Return values follow the existing convention: plot-level factories return the
  plottable, panel-level ones return `(plot, plottable)`.
- `plot.add_timeline()` takes the placement of the plot's lane layout: `Lanes` on a plot
  made by `panel.add_timeline()`, `Strip` on any other time-series plot.
- A timeline plot is a time-series plot: `plot.plot(x, y)` puts a line graph on its
  right y axis, across the full height.
- A dedicated timeline plot asks the panel for a fixed height:
  `visible lanes × lane_height + margins`.
- Threading follows the v0.42.2 guards: mutators queue to the GUI thread, factories
  raise off it.

## Testing

TDD throughout; every bug found gets a reproducer first.

NeoQCP (C++, `tests/auto`):

- `laneBand` for both placements.
- Visible-row search, sub-pixel merge; 10⁶ intervals under a time budget.
- Vertex cache predicate.
- Hit-test parts, including narrow bars.
- Gestures with `QTest` mouse events: move, resize, change lane, create, rubber band,
  ctrl-click, Escape cancel, no signal on a click without movement.
- Snapping (edges, step, none).
- GPU frame pixel test, run on Wayland (`QT_QPA_PLATFORM=wayland`); skipped offscreen.

SciQLopPlots (Python, `tests/integration`):

- Lane and category mapping, shared lanes between two timelines, id round trip.
- Validation errors.
- Signal payloads from simulated gestures.
- Strip placement in a normal time-series plot.
- Time sync inside a panel; compact fixed height.
- Thread guards.
- A fuzzer action creating and editing timelines.

## Delivery

One branch per repo, `feat/interval-timeline`, with three checkpoints, each leaving
both the NeoQCP and SciQLopPlots suites green:

1. **Draw** — `QCPIntervals`, `QCPLaneLayout`, both placements, categories, labels,
   Python `set_intervals` / `lanes` / colours.
2. **Identity** — ids, hover, selection, rubber band, `selected_intervals_changed`,
   `hovered`.
3. **Edit** — gestures, snapping, keyboard, edit signals.

Then one release. NeoQCP is pushed before the SciQLopPlots wrap pin is bumped.
