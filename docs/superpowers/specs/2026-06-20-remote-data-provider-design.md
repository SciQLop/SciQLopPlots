# RemoteDataProvider — out-of-process data channels

**Date:** 2026-06-20
**Status:** Implemented (PR #89) — line, curve, waterfall, colormap, histogram2d
**Branch:** `feat/remote-data-provider`

## Motivation

PySide6 has no realistic No-GIL timeline. Plugins that are *inherently slow and
Python-only* are a problem today: even though `plot(callback)` runs the callable
on a worker `QThread`, that thread **holds the GIL for the whole compute**,
starving every other piece of Python in the app — other plots' callbacks, the
embedded console, other plugins. There is also no way to get true CPU
parallelism across several heavy plugins.

Goals, in priority order:

1. **GIL contention (primary)** — a slow plugin must not starve the rest of
   SciQLop's Python.
2. **True CPU parallelism (primary)** — N heavy plugins should run on N cores.
3. **Crash/fault isolation (bonus)** — a segfaulting plugin should not take down
   SciQLop.

The cure is to run the slow Python work in **separate OS processes** (each with
its own interpreter, GIL, and core) and stream the result back **zero-copy** via
shared memory. The user-facing shape is `plot(callback)` → `plot(remote=...)`.

## Scope & boundary

This spec covers **only the SciQLopPlots side**: the thin C++ seam that lets data
arriving *from outside the process* feed the plotables/resamplers, with no Python
callable ever running on a worker thread.

Out of scope — **owned by the SciQLop application, not this repo:**

- process spawning and lifecycle (start / restart / kill)
- the channel/broker and discovery
- shared-memory allocation
- the control/wire protocol
- the user-facing `plot(remote=...)` keyword and the plugin manifest

### Process / channel model (context, implemented in SciQLop)

Process and channel are **decoupled**:

- **Process** = the plugin's lifecycle unit. Spawned once, imports once, holds
  warm caches/connections shared by all its products. Owns its own interpreter
  and GIL, fully separate from SciQLop's.
- **Channel** = the per-product streaming endpoint. One per plotable. Many
  channels are multiplexed over one process.

Object model: `RemotePlugin (process) → exposes → Channel[] → each binds to one
graph`.

Deliberate tradeoff: several pure-Python products served by **one** plugin
process still contend on **that process's** GIL — never on SciQLop's. So goal #1
is always satisfied; goal #3 is a lever for the plugin author (spread heavy
products across more processes for parallelism; group cheap ones to save
interpreters).

## Key insight: zero-copy is already free

`SciQLopPyBuffer(PyObject*)` accepts **any** buffer-protocol object. A numpy
array backed by `multiprocessing.shared_memory.SharedMemory` is exactly that.
SciQLop wraps the shared segment as a numpy view and hands it to the provider;
viewing it via the buffer protocol copies nothing into C++. **No buffer-layer
changes are required.**

## Existing machinery this reuses

`DataProviderInterface` already has:

- a `DataProviderWorker` on its own `QThread`,
- a 100 ms rate-limit timer + **decoupled** pending slots for range-based vs
  data-based updates (a race-safe design where neither path clobbers the other),
- a **push** path: `set_data(x,y[,z]|nd)` → `new_data_2d/3d/nd`.

`SimplePyCallablePipeline` wraps that and is wired to a graph by
`SciQLopFunctionGraph` (`src/SciQLopGraphInterface.cpp`):

- `graph.range_changed → pipeline.call(range)`
- `pipeline.new_data_* → graph.set_data` (guarded by `drop_bad_batch`)
- `pipeline.pipeline_idle → graph.set_busy(false)`

The remote provider slots into exactly these seams.

## Architecture

The new provider mirrors `SimplePyCallablePipeline` but **inverts the data
direction**: instead of *pulling* by running a Python callable on the worker
thread, it *emits a request signal outward* and *receives buffers inward*. It
never runs plugin code and never holds the GIL for compute — it only marshals a
request out and buffers in. It is **transport-agnostic**: it knows only "request
a range" / "here are buffers"; how SciQLop fulfills that (subprocess+shm, a
thread, or a test stub) is invisible.

### Components (this repo)

- **`RemoteDataProvider : DataProviderInterface`** — reuses the existing worker
  thread, rate-limit timer, and decoupled pending slots. The one override: its
  range-update path **emits `Q_SIGNAL data_requested(SciQLopPlotRange)`** instead
  of calling a callable. The incoming `set_data(...)` push path is the
  *already-existing* one — buffers flow straight into `new_data_*`.
- **`RemoteDataPipeline`** — sibling of `SimplePyCallablePipeline`: owns the
  provider+worker, re-exposes `data_requested` outward and `set_data(...)`
  inward, plus the existing `new_data_*` / `pipeline_idle`.
- **`SciQLopRemoteGraph`** (or a small extension of `SciQLopFunctionGraph`) — the
  adapter: wires `range_changed → data_requested` and `new_data_* →
  graph.set_data` (reusing the existing `drop_bad_batch` guard), and exposes
  `data_requested` so SciQLop can connect its IPC send.

## Data flow

```
pan/zoom → graph.range_changed → [rate-limit/coalesce in worker]
         → data_requested(range) ──out──► SciQLop IPC
                                              │ (subprocess fills shm)
graph.set_data(x,y) ◄── new_data_2d ◄── [push path]
         ◄── pipeline.set_data(x,y) ◄──in── SciQLop wraps shm-numpy view
```

## Threading & busy semantics (the one real behavioral change)

With the callable model, "busy" spans the blocking `get_data` call and
`pipeline_idle` fires when the worker drains. With a **remote** provider, request
and response are decoupled in time, so the existing idle detection would clear
"busy" the instant the request is emitted — before data arrives.

Therefore the `SciQLopRemoteGraph` adapter drives busy directly: it **sets busy
on the graph's `range_changed`** (as the request goes out) and **clears it on
`new_data_*`** (as the response arrives) — it does *not* wire `pipeline_idle`,
which fires the instant the request is emitted. Because the QCP components that
back the base `busy()` only exist after the first `set_data`, busy is held in a
mixin flag (`m_busy`, read by `busy()`); each concrete remote graph's `set_busy`
override updates that flag, emits `busy_changed`, and also forwards to the base
so the component's own busy flag stays consistent once components exist. This is
the only behavioral change beyond signal rewiring.

### Backpressure / stale responses

SciQLop owns dropping responses for superseded ranges — it knows the latest
requested range. C++ stays dumb and delivers whatever `set_data` it is handed.
A monotonic request token (so C++ could drop stale responses itself) is a
possible later addition; **not in v1.**

## Bindings

`data_requested` (signal) and `set_data(...)` (slots) on `RemoteDataPipeline`
must be exposed in `bindings.xml`, since SciQLop drives them from Python.
Per the project's shiboken convention, each `<add-function>` needs its own
try/catch in its `<inject-code>`, and `bindings.xml` must be touched after
editing any Python-exposed interface header (the meson custom_target does not
track headers).

## Testing

Fully testable **in-process, no real IPC**:

1. Connect `data_requested` to a Python responder that calls `set_data` with
   numpy arrays; assert the graph updates (2D, 3D, ND).
2. Drive rapid `range_changed` and assert requests coalesce at the rate limit.
3. Assert busy is set on request and cleared on the matching `set_data`
   (the §"busy semantics" change).
4. Zero-copy proof: back a numpy array with `multiprocessing.shared_memory`,
   feed it through `set_data`, assert the data lands without a copy.
5. Optional smoke test: spawn a real child process that fills a shared segment
   and streams one range back.

Tests follow the project convention — integration tests under `tests/`, run from
`/tmp` against `build-venv`.

## Out of scope / future

- Monotonic request token for C++-side stale-response dropping.
- External already-running process attaching to a channel (remote hosts /
  long-lived daemon). The `data_requested`/`set_data` contract is designed so an
  external attacher *could* be fulfilled by SciQLop later without changing this
  primitive.
