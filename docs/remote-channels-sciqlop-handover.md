# Remote Data Channels — SciQLop Integration & IPC Handover

**Audience:** whoever builds the SciQLop-side integration for out-of-process plugins.
**Status:** SciQLopPlots side shipped (PR #89, on `main`). The IPC layer described in
§4 is **not written** — it is the work this handover hands over.

---

## 1. Why this exists

PySide6 has no realistic No-GIL timeline. A slow, pure-Python plugin driven by the
classic `plot(callback)` path runs its callback on a SciQLopPlots **worker thread**,
where it holds the GIL for the whole compute and starves the rest of the app. The
remote channel removes that: the plugin runs in **another process** (own interpreter,
GIL, core) and only *finished buffers* cross back into SciQLopPlots. No plugin code
ever runs on a SciQLopPlots thread.

SciQLopPlots is **transport-agnostic**: it emits a range request *outward* and accepts
buffers *inward*. **Process spawning, the wire protocol, and shared memory are entirely
SciQLop's job** — that is §4.

---

## 2. What SciQLopPlots gives you (the channel API, already on `main`)

### Factories — create a remote-backed graph
```python
g = plot.add_remote_line_graph(labels=["Bx", "By", "Bz"])   # N=2 (multi or single component)
g = plot.add_remote_curve(["c"])                            # N=2
g = plot.add_remote_waterfall(["w"])                        # N=2
g = plot.add_remote_color_map("spectrogram")               # N=3   <- the heavy/typical case
g = plot.add_remote_histogram2d("hist", x_bins=100, y_bins=100)  # N=2
```
(`N` = number of buffers `set_data` takes; see §3.)

### The channel object
```python
ch = g.remote_channel()          # -> RemoteDataPipeline (a QObject)
```
Signals/slots on `ch`:

| Member | Direction | Meaning |
|---|---|---|
| `data_requested(SciQLopPlotRange)` | **out** (signal) | "I need data for `[r.start(), r.stop()]`." Connect this to your IPC send. |
| `set_data(x, y)` / `set_data(x, y, z)` / `set_data(values)` | **in** (slot) | Hand finished buffers back. Numpy arrays accepted directly. |
| `new_data_2d` / `new_data_3d` / `new_data_nd` | out (signal) | Fired after `set_data`; delivers the buffers as **numpy ndarrays** to Python slots. Mostly for tests/observability — you usually only call `set_data`. |
| `pipeline_idle` | out (signal) | The internal worker queue drained. Not needed for normal use. |

`g.busy()` is wired automatically: **True** from when a request goes out until the
matching `set_data` arrives. You don't manage it.

---

## 3. The contract (what to honour)

1. **Trigger.** Panning/zooming the plot's x-axis makes the graph emit
   `ch.data_requested(range)`. The graph also emits one **initial** request at
   creation (seeded from the current x range).

2. **Reply.** Compute, then call `ch.set_data(...)` with the right arity:
   - **N=2** (line, curve, waterfall, histogram2d): `ch.set_data(x, y)`.
     `x` is 1-D; `y` is 1-D (single component) or 2-D `(n, ncomp)` (multi).
   - **N=3** (colormap): `ch.set_data(x, y, z)`. `x` 1-D (nx), `y` 1-D (ny),
     `z` 2-D `(ny, nx)`.
   - dtype: float32 or float64 both work (SciQLopPlots dispatches on dtype; Speasy is
     float32). Time/x is fine as float64 epoch seconds.

3. **Zero-copy.** `set_data` views any buffer-protocol object — so a numpy array backed
   by `multiprocessing.shared_memory` is read into C++ **without a copy**. That's the
   whole point of the shm path.

4. **⚠ Lifetime (the one true footgun).** SciQLopPlots keeps a reference to the buffer
   you pass until the channel's data is **superseded** by the next `set_data`. You must
   **not** free/unlink the shared-memory segment while it is still live — that is a
   use-after-free and will crash. Rotate a small pool of segments and free one only once
   a newer `set_data` has replaced it. (This is exactly the bug the zero-copy test guards
   against — see `tests/integration/test_remote_data_provider.py::test_zero_copy_shared_memory_buffer`.)

5. **Backpressure / stale replies.** SciQLopPlots already rate-limits outgoing requests
   (~20 ms coalescing). On rapid pans, several requests may still be in flight; **you own
   matching replies to requests** and dropping responses for superseded ranges. SciQLopPlots
   delivers whatever `set_data` it's handed. (A monotonic request id is a possible future
   addition on the C++ side; not there today.)

6. **Threading.** `data_requested` is delivered on whichever thread you connect it from
   (use the main thread / a queued connection). Do the IPC *send* without blocking the UI;
   when the reply arrives, call `set_data` **on the main thread**. `set_data` itself hands
   off to SciQLopPlots' resampler thread internally — you don't manage that.

---

## 4. The IPC layer to build (SciQLop side — not yet written)

### Process / channel model (decoupled, by design)
- **Process** = the plugin's lifecycle unit: spawned once, imports once, holds warm
  caches/connections shared by all its products. Its own interpreter + GIL.
- **Channel** = one per product/graph. Many channels multiplexed over one process.

> Object model: `RemotePlugin (process) → Channel[] → each bound to one graph`.
> Several pure-Python products in one process still share *that* process's GIL — never
> SciQLop's. So GIL isolation is guaranteed; cross-product parallelism is the author's
> lever (more processes = more parallelism).

### Data flow
```
SciQLop main proc                                worker proc (slow plugin)
─────────────────                                ─────────────────────────
ch.data_requested(r) ──[control: (start,stop,req_id)]──▶ compute
                                                          write result into shm segment
np.ndarray(shape,dtype,    ◀──[control: (shm_name,shape,dtype,req_id)]──┘
   buffer=seg.buf)
ch.set_data(x, y[, z])     # zero-copy view into the shm segment
```
The control channel carries only small metadata (range out; shm **handles** back) — the
bulk data travels through shared memory. That separation is what makes it zero-copy.

### Pieces to implement
- **Worker process**: spawn (`multiprocessing`/`subprocess`), an entry point that imports
  the plugin once and serves a `get_data(start, stop) -> (x, y[, z])` loop, writing each
  result into a shared-memory segment and replying with its handle.
- **Control transport**: a `QLocalSocket`/pipe/`multiprocessing.Connection` carrying
  `(req_id, start, stop)` out and `(req_id, shm_name, shape, dtype)` back. Keep it small
  and non-blocking on the UI side.
- **Shared-memory pool**: a rotating set of segments per channel. Reuse/free a segment
  only after a newer `set_data` supersedes it (see contract §4.4). Size to the largest
  expected result; grow on demand.
- **Reply handling**: on a reply, wrap the segment as a numpy view and `ch.set_data(...)`
  on the main thread; drop replies whose `req_id` is older than the latest request for
  that channel.
- **Lifecycle**: start/restart/kill the process; on channel/graph teardown, close the
  process and unlink its segments **after** the graph (and its SciQLopPlots worker) are
  gone.

### Validate without real IPC first
Everything is exercisable in-process by connecting `data_requested` to a local responder —
that's exactly how the shipped tests work. Stand up the integration that way, then swap
the local responder for the real worker process.
```python
g  = plot.add_remote_color_map("spec")
ch = g.remote_channel()
ch.data_requested.connect(lambda r: ch.set_data(*compute(r.start(), r.stop())))  # later: -> IPC
```

---

## 5. Known limitations / TODO on the SciQLopPlots side
- `add_remote_color_map` does **not** yet expose `y_log_scale` / `z_log_scale` (hardcoded
  off in the factory). Add overloads if the remote spectrogram needs log axes.
- Single-line graphs are folded into `add_remote_line_graph` (the multi-line remote handles
  one component); there is no separate `add_remote_single_line_graph`.
- No C++-side request id yet (stale-reply dropping is the consumer's job).

---

## 6. Pointers
- Design spec: `docs/superpowers/specs/2026-06-20-remote-data-provider-design.md`
- Implementation plan: `docs/superpowers/plans/2026-06-20-remote-data-provider.md`
- Tests (in-process responder + zero-copy shm + busy lifecycle):
  `tests/integration/test_remote_data_provider.py`
- Core C++: `RemoteDataProvider` / `RemoteDataPipeline` in
  `include/SciQLopPlots/DataProducer/DataProducer.hpp`; `SciQLopRemoteGraph` mixin in
  `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp`.
- Merged as `SciQLop/SciQLopPlots` PR #89 (squash `29c6e9bd9`).
