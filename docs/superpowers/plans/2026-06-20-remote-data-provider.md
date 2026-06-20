# RemoteDataProvider Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a transport-agnostic `RemoteDataProvider` that inverts the `plot(callback)` data direction — emit a range request *outward* and receive final buffers *inward* — so slow, pure-Python plugins can run out-of-process without holding SciQLop's GIL.

**Architecture:** A new `DataProviderInterface` subclass reuses the existing worker thread + rate-limit/pending machinery. Its range `get_data` **emits a signal** instead of running a callable and returns no synchronous data; its buffer `get_data` overrides **pass the incoming buffers straight through** as final output. A `RemoteDataPipeline` (the bound "channel" object) re-exposes `data_requested` out and `set_data(...)` in. A `SciQLopRemoteGraph` mixin wires it to a graph, marking busy on request and clearing it on data arrival. The first vertical slice targets the line graph; other graph types replicate the same pattern.

**Tech Stack:** C++20, Qt6 (QObject/signals/threads), Shiboken6 bindings, Meson `build-venv`, pytest integration tests.

---

## Background the implementer must know

**Build & test (from `CLAUDE.md`):** never run a bare `meson setup build`. Use the in-tree `.venv` + `build-venv`:

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
# run tests from /tmp so the source pkg doesn't shadow the build:
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```

**Shiboken quirks (from project memory):**
- After editing any Python-exposed header, **`touch SciQLopPlots/bindings/bindings.xml`** — the meson custom_target does not track headers.
- Each `<add-function>` needs its **own try/catch** in its `<inject-code>`, or a thrown `std::exception` escapes to `std::terminate`.
- A stale shiboken wrapper after structural changes: `rm` the offending `build-venv/SciQLopPlots/bindings/.../*_wrapper.cpp` and `meson setup --reconfigure build-venv`.

**Key existing code to mirror (read these first):**
- `include/SciQLopPlots/DataProducer/DataProducer.hpp` — `DataProviderInterface`, `DataProviderWorker`, `SimplePyCallablePWrapper`, `SimplePyCallablePipeline`.
- `src/DataProducer.cpp` — `_range_based_update` (calls `get_data(start,stop)` then `_notify_new_data`), `_data_based_update(_2D_data)` (calls `get_data(x,y)` — this is why the remote buffer override must pass through), `SimplePyCallablePipeline` ctor wiring.
- `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp` — `SciQLopFunctionGraph` mixin (note: **not** a QObject; `call(range)` sets `as_graph->set_busy(true)`).
- `src/SciQLopGraphInterface.cpp` — `SciQLopFunctionGraph` ctor: the `drop_bad_batch` guard + switch on N wiring `new_data_* → graph.set_data`, `pipeline_idle → set_busy(false)`, `range_changed → pipeline.call(range)`.
- `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp` + `src/SciQLopLineGraph.cpp` — `SciQLopLineGraphFunction` (multiple-inherits `SciQLopLineGraph` + `SciQLopFunctionGraph`).

**The two semantic differences from the callable path (the whole point):**
1. Range request: `get_data(lower, upper)` **emits `data_requested(range)` and returns `{}`** (no synchronous data; the answer arrives later via `set_data`).
2. Buffer delivery: `get_data(x,y)` / `(x,y,z)` / `(nd)` **return their inputs unchanged** (the incoming buffers *are* the final output), so `_data_based_update → _notify_new_data` emits `new_data_*`.
3. Busy spans request→response: set busy on `call(range)`, clear on `new_data_*` (do **not** wire `pipeline_idle → set_busy(false)` for remote — `pipeline_idle` fires right after the request is emitted).

---

## File structure

- **Modify** `include/SciQLopPlots/DataProducer/DataProducer.hpp` — add `RemoteDataProvider` (provider subclass) and `RemoteDataPipeline` (channel wrapper). Both belong here next to `SimplePyCallablePipeline`; they share the worker/rate-limit machinery declared in this file.
- **Modify** `src/DataProducer.cpp` — `RemoteDataProvider` method bodies + `RemoteDataPipeline` ctor wiring.
- **Modify** `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp` — add `SciQLopRemoteGraph` mixin (sibling of `SciQLopFunctionGraph`) + a small shared templated free function `connect_pipeline_data_to_graph` (extracted from the existing ctor; DRY because the `drop_bad_batch` switch is identical and tricky).
- **Modify** `src/SciQLopGraphInterface.cpp` — define `connect_pipeline_data_to_graph`, refactor `SciQLopFunctionGraph` ctor to use it, define `SciQLopRemoteGraph` ctor.
- **Modify** `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp` + `src/SciQLopLineGraph.cpp` — add `SciQLopLineGraphRemote` (mirror of `SciQLopLineGraphFunction`).
- **Modify** `SciQLopPlots/bindings/bindings.xml` — expose `RemoteDataPipeline` (signal `data_requested`, slots `set_data`), `SciQLopRemoteGraph` accessor, `SciQLopLineGraphRemote`.
- **Create** `tests/integration/test_remote_data_provider.py` — the integration tests (in-process, no real IPC).

---

## Task 1: Core primitive — `RemoteDataProvider` + `RemoteDataPipeline`

**Files:**
- Modify: `include/SciQLopPlots/DataProducer/DataProducer.hpp`
- Modify: `src/DataProducer.cpp`

- [ ] **Step 1: Declare `RemoteDataProvider` in the header**

In `DataProducer.hpp`, after the `SimplePyCallablePWrapper` class, add:

```cpp
// A provider that does not compute: it emits a request for a range and waits.
// Range get_data emits data_requested and returns nothing (the answer arrives
// later via set_data). Buffer get_data overrides pass their inputs straight
// through, so the existing _data_based_update push path turns an incoming
// set_data(x,y) into new_data_2d(x,y) with no transformation.
class RemoteDataProvider : public DataProviderInterface
{
    Q_OBJECT
public:
    RemoteDataProvider(QObject* parent = nullptr) : DataProviderInterface(parent) { }
    virtual ~RemoteDataProvider() = default;

    inline QList<SciQLopPyBuffer> get_data(double lower, double upper) override
    {
        Q_EMIT data_requested(SciQLopPlotRange { lower, upper });
        return {}; // no synchronous data; response arrives via set_data
    }

    inline QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y) override
    {
        return { x, y };
    }

    inline QList<SciQLopPyBuffer> get_data(SciQLopPyBuffer x, SciQLopPyBuffer y,
                                           SciQLopPyBuffer z) override
    {
        return { x, y, z };
    }

    inline QList<SciQLopPyBuffer> get_data(QList<SciQLopPyBuffer> values) override
    {
        return values;
    }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void data_requested(const SciQLopPlotRange& range);
};
```

- [ ] **Step 2: Declare `RemoteDataPipeline` in the header**

After `SimplePyCallablePipeline`, add the channel wrapper:

```cpp
// The bound "channel" object. One per remote product. data_requested goes out
// (SciQLop forwards it to the worker process); set_data(...) brings the final
// buffers back in.
class RemoteDataPipeline : public QObject
{
    Q_OBJECT
    RemoteDataProvider* m_provider;
    DataProviderWorker* m_worker;

public:
    RemoteDataPipeline(QObject* parent = nullptr);
    virtual ~RemoteDataPipeline() = default;

    inline Q_SLOT void call(const SciQLopPlotRange& range) { m_worker->set_range(range); }

    inline Q_SLOT void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y)
    {
        m_worker->set_data(x, y);
    }
    inline Q_SLOT void set_data(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
    {
        m_worker->set_data(x, y, z);
    }
    inline Q_SLOT void set_data(QList<SciQLopPyBuffer> values) { m_worker->set_data(values); }

    inline void invalidate_cache() { m_provider->invalidate_cache(); }

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void data_requested(const SciQLopPlotRange& range);
    Q_SIGNAL void new_data_3d(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z);
    Q_SIGNAL void new_data_2d(SciQLopPyBuffer x, SciQLopPyBuffer y);
    Q_SIGNAL void new_data_nd(QList<SciQLopPyBuffer> values);
    Q_SIGNAL void pipeline_idle();
};
```

- [ ] **Step 3: Define `RemoteDataPipeline` ctor in `src/DataProducer.cpp`**

At the end of the file, mirroring `SimplePyCallablePipeline`'s ctor but also forwarding `data_requested`:

```cpp
RemoteDataPipeline::RemoteDataPipeline(QObject* parent) : QObject(parent)
{
    m_provider = new RemoteDataProvider(this);
    m_worker = new DataProviderWorker(this);
    m_worker->set_data_provider(m_provider);
    connect(m_provider, &RemoteDataProvider::data_requested, this,
            &RemoteDataPipeline::data_requested);
    connect(m_provider, &RemoteDataProvider::new_data_2d, this,
            &RemoteDataPipeline::new_data_2d);
    connect(m_provider, &RemoteDataProvider::new_data_3d, this,
            &RemoteDataPipeline::new_data_3d);
    connect(m_provider, &RemoteDataProvider::new_data_nd, this,
            &RemoteDataPipeline::new_data_nd);
    connect(m_provider, &RemoteDataProvider::pipeline_idle, this,
            &RemoteDataPipeline::pipeline_idle);
}
```

Note: `set_data_provider` reparents the provider to nullptr then moves it to the worker thread (see existing impl), so `new RemoteDataProvider(this)` parentage is intentionally dropped — identical to how `SimplePyCallablePWrapper` is handled.

- [ ] **Step 4: Build**

Run:
```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots && VENV=$(pwd)/.venv && \
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH" && \
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" && \
$VENV/bin/meson compile -C build-venv
```
Expected: compiles clean (no Python surface yet — pure C++).

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/DataProducer/DataProducer.hpp src/DataProducer.cpp
git commit -m "feat(remote): add RemoteDataProvider + RemoteDataPipeline core primitive

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Shared `connect_pipeline_data_to_graph` helper + refactor `SciQLopFunctionGraph`

This extracts the identical `drop_bad_batch`+switch wiring so the remote adapter reuses it. Templated on the pipeline type because `SimplePyCallablePipeline` and `RemoteDataPipeline` expose the same `new_data_*` signals.

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp`
- Modify: `src/SciQLopGraphInterface.cpp`

- [ ] **Step 1: Declare the helper in the header**

In `SciQLopGraphInterface.hpp`, above `class SciQLopFunctionGraph`, add a templated free function declaration + definition (header-defined because it is templated):

```cpp
// Wire a pipeline's new_data_* signals to graph->set_data, guarding against
// exceptions escaping a queued connection (which would std::terminate). N picks
// the arity: 2 -> new_data_2d, 3 -> new_data_3d, else new_data_nd.
template <typename Pipeline>
inline QList<QMetaObject::Connection>
connect_pipeline_data_to_graph(Pipeline* pipeline, SciQLopPlottableInterface* graph, int N)
{
    const auto drop_bad_batch = [](SciQLopPlottableInterface* g, auto&&... buffers)
    {
        try
        {
            g->set_data(std::forward<decltype(buffers)>(buffers)...);
        }
        catch (const std::exception& e)
        {
            qWarning() << "SciQLopPlots: dropping data batch from provider for"
                       << g->objectName() << ":" << e.what();
        }
    };
    QList<QMetaObject::Connection> conns;
    switch (N)
    {
        case 2:
            conns << QObject::connect(pipeline, &Pipeline::new_data_2d, graph,
                [g = graph, drop_bad_batch](SciQLopPyBuffer x, SciQLopPyBuffer y)
                { drop_bad_batch(g, std::move(x), std::move(y)); });
            break;
        case 3:
            conns << QObject::connect(pipeline, &Pipeline::new_data_3d, graph,
                [g = graph, drop_bad_batch](SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
                { drop_bad_batch(g, std::move(x), std::move(y), std::move(z)); });
            break;
        default:
            conns << QObject::connect(pipeline, &Pipeline::new_data_nd, graph,
                [g = graph, drop_bad_batch](const QList<SciQLopPyBuffer>& values)
                { drop_bad_batch(g, values); });
            break;
    }
    return conns;
}
```

Ensure `<QDebug>` (for `qWarning`) and `<QMetaObject>` are available via existing includes; add `#include <QDebug>` to the header if not already transitively included.

- [ ] **Step 2: Refactor `SciQLopFunctionGraph` ctor to use the helper**

In `src/SciQLopGraphInterface.cpp`, replace the inline `drop_bad_batch` lambda + `switch (N)` block (lines ~109-139) with:

```cpp
    for (const auto& c : connect_pipeline_data_to_graph(m_pipeline, this->as_graph, N))
        Q_UNUSED(c);
```

Leave the `pipeline_idle → set_busy(false)` and `range_changed → pipeline.call` connections (lines ~140-143) unchanged.

- [ ] **Step 3: Build**

Run the `meson compile -C build-venv` command from Task 1 Step 4.
Expected: compiles clean; the callable path is behavior-identical (pure refactor).

- [ ] **Step 4: Run the existing callable-path tests to prove no regression**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $(cd /var/home/jeandet/Documents/prog/SciQLopPlots && pwd)/.venv/bin/python \
  -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q -k "graph or plot"
```
Expected: PASS (same counts as before this task).

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp src/SciQLopGraphInterface.cpp
git commit -m "refactor(graph): extract connect_pipeline_data_to_graph helper

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: `SciQLopRemoteGraph` mixin

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp`
- Modify: `src/SciQLopGraphInterface.cpp`

- [ ] **Step 1: Declare `SciQLopRemoteGraph` in the header**

After `class SciQLopFunctionGraph { ... };`, add the sibling mixin. It holds the channel and exposes it so SciQLop can connect `data_requested` and call `set_data`:

```cpp
class SciQLopRemoteGraph
{
protected:
    RemoteDataPipeline* m_pipeline;
    SciQLopPlottableInterface* as_graph = nullptr;
    QList<QMetaObject::Connection> m_connections;

public:
    SciQLopRemoteGraph() { }
    SciQLopRemoteGraph(SciQLopPlottableInterface* as_graph, int N = 2);
    virtual ~SciQLopRemoteGraph() = default;

    // The bound channel endpoint: SciQLop connects channel.data_requested to its
    // IPC send and calls channel.set_data(...) with shared-memory-backed numpy.
    inline RemoteDataPipeline* remote_channel() const noexcept { return m_pipeline; }

    inline void invalidate_pipeline_cache() noexcept { m_pipeline->invalidate_cache(); }
};
```

- [ ] **Step 2: Define the ctor in `src/SciQLopGraphInterface.cpp`**

Busy spans request→response: set busy when a range request is initiated, clear it when data arrives. Do **not** connect `pipeline_idle → set_busy(false)`.

```cpp
SciQLopRemoteGraph::SciQLopRemoteGraph(SciQLopPlottableInterface* as_graph, int N)
        : m_pipeline { new RemoteDataPipeline(as_graph) }, as_graph { as_graph }
{
    m_connections = connect_pipeline_data_to_graph(m_pipeline, this->as_graph, N);

    // Busy: on request out, clear on data in (NOT on pipeline_idle, which fires
    // as soon as the request is emitted, before the response arrives).
    m_connections << QObject::connect(this->as_graph,
        &SciQLopGraphInterface::range_changed, m_pipeline,
        [pipeline = m_pipeline, g = this->as_graph](const SciQLopPlotRange& range)
        { g->set_busy(true); pipeline->call(range); });

    const auto clear_busy = [g = this->as_graph]() { g->set_busy(false); };
    m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_2d, this->as_graph, clear_busy);
    m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_3d, this->as_graph, clear_busy);
    m_connections << QObject::connect(m_pipeline, &RemoteDataPipeline::new_data_nd, this->as_graph, clear_busy);
}
```

Note: `range_changed` carries `SciQLopPlotRange`. Confirm the signal signature in `SciQLopGraphInterface.hpp` (it is `Q_SIGNAL void range_changed(const SciQLopPlotRange&)`); if the project routes range via the axis instead (as `SciQLopFunctionGraph::observe` does for axes), connect through the same axis `range_changed` the function graph uses. Match whichever the line graph actually emits — verify by reading how `SciQLopLineGraphFunction` receives ranges before finalizing this connection.

- [ ] **Step 3: Build**

Run `meson compile -C build-venv`. Expected: clean compile.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopGraphInterface.hpp src/SciQLopGraphInterface.cpp
git commit -m "feat(remote): add SciQLopRemoteGraph mixin (busy spans request->response)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: `SciQLopLineGraphRemote` concrete graph

**Files:**
- Modify: `include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp`
- Modify: `src/SciQLopLineGraph.cpp`

- [ ] **Step 1: Declare `SciQLopLineGraphRemote` in the header**

Mirror `SciQLopLineGraphFunction` (which multiple-inherits `SciQLopLineGraph` + `SciQLopFunctionGraph`):

```cpp
class SciQLopLineGraphRemote : public SciQLopLineGraph, public SciQLopRemoteGraph
{
    Q_OBJECT
public:
    explicit SciQLopLineGraphRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                    SciQLopPlotAxis* value_axis,
                                    const QStringList& labels = QStringList());
    ~SciQLopLineGraphRemote() override = default;
};
```

(Match the exact ctor parameter list of `SciQLopLineGraphFunction` in the same header, minus the `GetDataPyCallable&&` argument.)

- [ ] **Step 2: Define the ctor in `src/SciQLopLineGraph.cpp`**

Mirror `SciQLopLineGraphFunction` (line ~32), passing `N = 2`:

```cpp
SciQLopLineGraphRemote::SciQLopLineGraphRemote(QCustomPlot* parent, SciQLopPlotAxis* key_axis,
                                               SciQLopPlotAxis* value_axis,
                                               const QStringList& labels)
        : SciQLopLineGraph(parent, key_axis, value_axis, labels)
        , SciQLopRemoteGraph(this, 2)
{
}
```

(Copy the exact base-ctor argument list from `SciQLopLineGraphFunction`'s definition — read it first; it may pass more than shown here.)

- [ ] **Step 3: Build**

Run `meson compile -C build-venv`. Expected: clean compile.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/Plotables/SciQLopLineGraph.hpp src/SciQLopLineGraph.cpp
git commit -m "feat(remote): add SciQLopLineGraphRemote concrete graph

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: Bindings — expose the channel + remote line graph to Python

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Add typesystem entries**

Find the `<object-type name="SimplePyCallablePipeline"/>` entry (or the section that registers the DataProducer types) and add alongside it:

```xml
<object-type name="RemoteDataPipeline">
    <!-- data_requested(range) out; set_data(...) in; new_data_* / pipeline_idle as on SimplePyCallablePipeline -->
</object-type>
<object-type name="RemoteDataProvider"/>
```

Find the `<object-type name="SciQLopLineGraphFunction"/>` entry and add:

```xml
<object-type name="SciQLopRemoteGraph"/>
<object-type name="SciQLopLineGraphRemote"/>
```

If any `<add-function>` / `<inject-code>` is needed for `set_data` overload disambiguation, give **each** its own `try { ... } catch (const std::exception& e) { ... }` block (see `shiboken_addfunction_exception_pattern` memory).

- [ ] **Step 2: Force binding regen and build**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots && touch SciQLopPlots/bindings/bindings.xml && \
VENV=$(pwd)/.venv && \
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH" && \
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH" && \
$VENV/bin/meson compile -C build-venv
```
Expected: regenerates wrappers and compiles. If a stale `*_wrapper.cpp` errors, `rm` it and `meson setup --reconfigure build-venv`, then recompile.

- [ ] **Step 3: Smoke-check the symbols import**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $(cd /var/home/jeandet/Documents/prog/SciQLopPlots && pwd)/.venv/bin/python -c \
  "import SciQLopPlots as s; print(s.RemoteDataPipeline, s.SciQLopLineGraphRemote)"
```
Expected: prints both type objects, no `AttributeError`.

- [ ] **Step 4: Commit**

```bash
git add SciQLopPlots/bindings/bindings.xml
git commit -m "feat(remote): expose RemoteDataPipeline + SciQLopLineGraphRemote bindings

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: Integration tests — in-process, no real IPC

**Files:**
- Create: `tests/integration/test_remote_data_provider.py`

This is the behavioral specification. A pure-Python responder stands in for the remote process: it receives `data_requested`, computes, and calls `set_data`. (Read an existing test in `tests/integration/` first to copy the app/qtbot fixture conventions.)

- [ ] **Step 1: Write the failing tests**

```python
import numpy as np
import pytest
from multiprocessing import shared_memory
import SciQLopPlots as s


def _make_remote_line_graph(plot):
    # Adjust to the project's actual add/plot factory for SciQLopLineGraphRemote.
    # The graph exposes remote_channel() -> RemoteDataPipeline.
    return plot.add_remote_line_graph(["B"])  # see Task 7 for the factory


def test_request_emitted_on_range_change(qtbot, plot_with_time_axis):
    plot = plot_with_time_axis
    g = _make_remote_line_graph(plot)
    ch = g.remote_channel()

    requests = []
    ch.data_requested.connect(lambda r: requests.append((r.start(), r.stop())))

    with qtbot.waitSignal(ch.data_requested, timeout=2000):
        plot.x_axis().set_range(s.SciQLopPlotRange(10.0, 20.0))

    assert requests and requests[-1] == (10.0, 20.0)


def test_set_data_delivers_to_graph(qtbot, plot_with_time_axis):
    plot = plot_with_time_axis
    g = _make_remote_line_graph(plot)
    ch = g.remote_channel()

    x = np.linspace(10.0, 20.0, 100, dtype=np.float64)
    y = np.sin(x).astype(np.float64)

    def respond(_range):
        ch.set_data(x, y)

    ch.data_requested.connect(respond)
    with qtbot.waitSignal(ch.new_data_2d, timeout=2000):
        plot.x_axis().set_range(s.SciQLopPlotRange(10.0, 20.0))

    # graph received the data (assert via the graph's data accessor used elsewhere
    # in the suite, e.g. g.data() or a plotted-points count).


def test_busy_clears_only_after_data_arrives(qtbot, plot_with_time_axis):
    plot = plot_with_time_axis
    g = _make_remote_line_graph(plot)
    ch = g.remote_channel()

    # Defer the response so we can observe busy==True in between.
    pending = {}
    ch.data_requested.connect(lambda r: pending.setdefault("r", r))

    plot.x_axis().set_range(s.SciQLopPlotRange(10.0, 20.0))
    qtbot.waitUntil(lambda: "r" in pending, timeout=2000)
    assert g.busy() is True  # request out, no data yet

    x = np.linspace(10.0, 20.0, 10, dtype=np.float64)
    ch.set_data(x, x)
    qtbot.waitUntil(lambda: g.busy() is False, timeout=2000)


def test_zero_copy_shared_memory_buffer(qtbot, plot_with_time_axis):
    plot = plot_with_time_axis
    g = _make_remote_line_graph(plot)
    ch = g.remote_channel()

    shm = shared_memory.SharedMemory(create=True, size=100 * 8)
    try:
        x = np.ndarray((100,), dtype=np.float64, buffer=shm.buf)
        x[:] = np.linspace(10.0, 20.0, 100)
        ch.data_requested.connect(lambda r: ch.set_data(x, x))
        with qtbot.waitSignal(ch.new_data_2d, timeout=2000):
            plot.x_axis().set_range(s.SciQLopPlotRange(10.0, 20.0))
    finally:
        shm.close()
        shm.unlink()
```

Adjust `plot_with_time_axis`, `busy()`, and the data-accessor calls to the suite's real fixtures/API (read `tests/integration/conftest.py` and a colormap/line-graph test first). Replace `add_remote_line_graph` with whatever Task 7 exposes.

- [ ] **Step 2: Run to verify they fail**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $(cd /var/home/jeandet/Documents/prog/SciQLopPlots && pwd)/.venv/bin/python \
  -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_remote_data_provider.py -v
```
Expected: FAIL — `add_remote_line_graph` does not exist yet (drives Task 7).

- [ ] **Step 3: (after Task 7) Run to verify they pass**

Same command. Expected: 4 passed.

- [ ] **Step 4: Commit**

```bash
git add tests/integration/test_remote_data_provider.py
git commit -m "test(remote): in-process integration tests incl. zero-copy shm path

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: `plot`/factory integration so SciQLop can create a remote line graph

The construction sugar is SciQLop's concern, but SciQLopPlots must expose a creation entry point analogous to the callable path (`SciQLopPlot::plot_impl(GetDataPyCallable, ...)`). Add a no-callable factory that builds a `SciQLopLineGraphRemote`.

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp`
- Modify: `src/SciQLopPlot.cpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Declare a factory on `SciQLopPlot`**

Mirror the existing `add_*`/`plot_impl` callable methods (`src/SciQLopPlot.cpp:875` `plot_impl(GetDataPyCallable, QStringList labels, ...)`). Add a sibling that takes labels (no callable) and returns the remote graph:

```cpp
SciQLopLineGraphRemote* add_remote_line_graph(const QStringList& labels = QStringList());
```

(Place it next to the line-graph creation methods; match their axis-resolution boilerplate.)

- [ ] **Step 2: Define it in `src/SciQLopPlot.cpp`**

Follow the body of the callable line-graph branch in `plot_impl` (around line 889 `add_plottable<SciQLopLineGraphFunction>(...)`), substituting `SciQLopLineGraphRemote` and dropping the callable argument:

```cpp
SciQLopLineGraphRemote* SciQLopPlot::add_remote_line_graph(const QStringList& labels)
{
    return m_impl->add_plottable<SciQLopLineGraphRemote>(
        /* same axis args as the SciQLopLineGraphFunction branch */ labels);
}
```

(Read the exact `add_plottable<SciQLopLineGraphFunction>` call and copy its argument list verbatim, minus the callable.)

- [ ] **Step 3: Bind the factory**

Add `add_remote_line_graph` to the `SciQLopPlot` `<object-type>` in `bindings.xml` (it is a plain method returning a bound type — likely no `<add-function>` needed, just ensure the return type is registered from Task 5). `touch bindings.xml`.

- [ ] **Step 4: Build**

Run the regen+compile command from Task 5 Step 2. Expected: clean.

- [ ] **Step 5: Run the Task 6 tests**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $(cd /var/home/jeandet/Documents/prog/SciQLopPlots && pwd)/.venv/bin/python \
  -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_remote_data_provider.py -v
```
Expected: **4 passed**. If `busy()`/data-accessor names were guessed wrong, fix the test to the real API and re-run.

- [ ] **Step 6: Run the full integration suite (no regressions)**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $(cd /var/home/jeandet/Documents/prog/SciQLopPlots && pwd)/.venv/bin/python \
  -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```
Expected: all pass (same counts as before + the 4 new).

- [ ] **Step 7: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp SciQLopPlots/bindings/bindings.xml
git commit -m "feat(remote): add_remote_line_graph factory + binding

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: Replicate to the other graph types (follow-up scope)

The line-graph slice proves the pattern end-to-end. The remaining graph types each get a `*Remote` twin mirroring their `*Function` class, reusing `SciQLopRemoteGraph` with the correct `N`:

| Function class | Remote twin | N |
|---|---|---|
| `SciQLopSingleLineGraphFunction` | `SciQLopSingleLineGraphRemote` | 2 |
| `SciQLopCurveFunction` | `SciQLopCurveRemote` | 2 |
| `SciQLopColorMapFunction` | `SciQLopColorMapRemote` | 3 |
| `SciQLopHistogram2DFunction` | `SciQLopHistogram2DRemote` | 3 |
| `SciQLopWaterfallGraphFunction` | `SciQLopWaterfallGraphRemote` | 3 (verify) |

- [ ] For each: add the concrete class (header+cpp) mirroring Task 4, add an `add_remote_*` factory mirroring Task 7, bind it (Task 5 pattern), and add a `set_data`-delivers test mirroring Task 6.
- [ ] Verify the correct `N` (output arity) per type by checking the `N` its `*Function` ctor passes to `SciQLopFunctionGraph`.
- [ ] Run the full suite after each.

This task may be split into its own plan if it grows large; it is intentionally listed at lower fidelity because every entry is a mechanical repeat of Tasks 4–7.

---

## Self-review notes

- **Spec coverage:** §Architecture/§Components → Tasks 1,3,4; §Data flow + zero-copy → Tasks 1,6; §Busy semantics → Task 3 + `test_busy_clears_only_after_data_arrives`; §Bindings → Task 5; §Testing (all 5 listed cases) → Task 6 (the optional real-subprocess smoke test is deferred — noted, not required for v1); §Out-of-scope (request token, external attacher) correctly left unimplemented.
- **Open verification flagged in-task:** the exact `range_changed` carrier (graph vs axis) — Task 3 Step 2 tells the implementer to confirm against how `SciQLopLineGraphFunction` actually receives ranges before finalizing. The test fixtures/accessors (`plot_with_time_axis`, `busy()`, data accessor) are marked "adjust to the suite's real API."
- **Type consistency:** `remote_channel()` returns `RemoteDataPipeline*` consistently (Tasks 3, 6); `set_data`/`data_requested`/`new_data_*` names match across provider, pipeline, and tests.
