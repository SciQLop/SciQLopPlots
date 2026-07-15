# Products search: pluggable "smart search" methods (semantic, extensible)

Date: 2026-07-15
Status: Approved (design)

## Problem

The existing free-text search ([[product_search_scoring_fixes]],
[[product_search_score_badges_ui]]) scores a leaf by running a character-level
DP subsequence match against `raw_text()` — the node name plus whatever
metadata key/value pairs it was constructed with. This is purely lexical: a
query has to appear, character-by-character, inside the text a node actually
carries. It cannot help a student who searches `"mms spacecraft 1 magnetic
field"` find `mms1/fgm/*` and `mms1/scm/*` when the node's own text only
contains instrument acronyms, or when the query's wording (`"spacecraft"`)
differs from the node's (`"MMS Satellite Number 1"`). Investigation into
Speasy's real CDAWeb inventory confirmed the *raw material* for a better
answer often already exists (dataset descriptions are full prose, e.g. *"Level
2 Flux Gate Magnetometer ... DC Magnetic Field for MMS Satellite Number
1"*) — but no amount of tuning the DP matcher turns "magnetic field" into a
match for text it doesn't literally contain.

We want a way to plug in genuinely semantic matching (starting with a small
local text-embedding model) without hard-wiring "the one embedding method" into
the core — and without risking the GIL/mutex deadlock class this codebase hit
once already ([[gil_lock_ordering_fix]]).

## Goals

- Recall-first but filtered: students who don't know instrument acronyms
  should still find the right products; the ranking must stay trustworthy
  enough to not read as noise.
- A generic extension point for "search methods" — the built-in DP matcher
  plus any number of additional methods (starting with one embedding-based
  method, more later) — addable without touching the aggregation/UI code each
  time.
- Ship the first new method (semantic embeddings) as an **opt-in**, optional
  Python extra (`SciQLopPlots[smart_search]`), toggled on/off, exposed through
  SciQLop's settings rather than a required dependency.
- The embedding model is generic/pretrained (never trained on this project's
  product catalog), downloaded and cached at runtime, with the active model
  swappable via a small API SciQLop's settings UI can drive.
- Scale: expensive work (building a method's internal representation of the
  corpus) must be decoupled from cheap work (scoring one query), so a method
  can start brute-force and later swap its internal indexing strategy without
  changing the interface.
- No regression: with the extra not installed, or the toggle off, behavior is
  byte-for-byte identical to today.

Non-goals: a stable third-party plugin ABI for methods outside this codebase
(the interface is designed to be easy to extend internally; external
registration is not a requirement now — YAGNI); ANN/vector-index
infrastructure (brute-force cosine is trivial at today's corpus sizes; the
interface just avoids foreclosing it later); calling out to a hosted LLM
(network dependency + per-query cost, wrong fit for an interactive desktop
search box — considered and rejected during brainstorming).

## Approach (chosen: Python-side pluggable methods, one C++ overlay hook)

Two options were considered for *where the plugin/aggregation logic lives*:

- **Chosen: aggregation entirely in Python, C++ gets one simple hook.** The
  `SearchMethod` interface, the registry of active methods, and the
  weighted-combination logic all live in the optional `smart_search` Python
  subpackage. The C++ filter models gain exactly one new concept: an
  "external score overlay" — a single `{path_key: score}` map, opaque to them,
  that gets `max()`-blended with the existing DP score. C++ doesn't know how
  many methods contributed to that map or how they were combined.
- **Rejected: native C++ embedding method (onnxruntime in-process, gated by a
  Meson option).** Marginally faster, no Python↔C++ hand-off — but turns an
  opt-in Python extra into an optional C++ dependency with multi-platform
  wheel/CI cost, for a feature explicitly meant to be lightweight and opt-in.
- **Rejected: C++ calls back into a Python-registered scorer per node/query**
  (mirroring `GetDataPyCallable`). Re-enters Python from inside the existing
  batched `process_score_batch` timer loop — closer to the exact shape of the
  prior GIL/mutex deadlock, and far slower than one batched call. The chosen
  design instead has Python push a *finished* result in; C++ never calls back
  into Python during scoring.

Keeping C++ down to a single opaque overlay also means the DP matcher itself
needs zero changes to its own logic — it stays exactly as trusted and tested
as it is today; only the final per-leaf score gains one extra `max()` term.

## Components

### `include/SciQLopPlots/Products/ExternalScoreOverlay.hpp` + `.cpp` (new, shared)

Both filter models need the identical delicate concurrency logic, so it's
factored once rather than duplicated:

```cpp
class ExternalScoreOverlay
{
public:
    void set_scores(QHash<QString, double> scores); // any thread
    void set_enabled(bool enabled);                  // any thread
    double score_for(const QString& path_key) const; // any thread, 0.0 if absent/disabled

private:
    mutable QMutex m_mutex;
    std::shared_ptr<const QHash<QString, double>> m_scores; // swapped, never mutated in place
    std::atomic<bool> m_enabled { false };
};
```

`set_scores` builds a new `QHash` fully off any lock, then only holds
`m_mutex` for the pointer swap itself — the same shared-ptr-snapshot pattern
from [[gil_lock_ordering_fix]] (PR #70). `score_for` takes the lock only long
enough to copy the `shared_ptr`, then reads the (now locally-owned, immutable)
hash without holding anything. Nothing here ever calls into Python, so it's
safe to invoke from a Python background thread with the GIL held, from the
Qt GUI thread, or both concurrently.

### `ProductsTreeFilterModel` / `ProductsFlatFilterModel`

- Each gains an `ExternalScoreOverlay m_external_scores;` member.
- Two new methods delegate straight through: `set_external_scores(QHash<QString,double>)`
  and `set_smart_search_enabled(bool)`.
- The existing per-leaf scoring step (`free_text_score` / `process_score_batch`)
  changes from `score = dp_score` to
  `score = std::max(dp_score, m_external_scores.score_for(path_key))`, where
  `path_key` is the already-computed joined `path()` string (the same
  identity concept `path_text` already uses for DP scoring — no new node-ID
  scheme needed).
- Both new methods are added to `bindings.xml` so Python can call them.

### `SciQLopPlots/smart_search/` (new, optional subpackage — only imported if `SciQLopPlots[smart_search]` is installed)

- `method.py` — `SearchMethod` protocol:
  ```python
  class SearchMethod(Protocol):
      def index(self, nodes: Sequence[NodeSnapshot]) -> None: ...
      def query(self, text: str) -> dict[str, float]: ...  # sparse: absent key = no opinion
  ```
  `NodeSnapshot` is a plain `(path_key: str, raw_text: str)` pair — deliberately
  not a reference to the live C++ node, so `index()`/`query()` never touch
  Shiboken-wrapped objects and never need the GIL to cross back into C++.
- `semantic.py` — `SemanticSearchMethod`, built on `fastembed` (ONNX runtime,
  no torch dependency). Default model: a small general-purpose sentence
  embedding model (`BAAI/bge-small-en-v1.5`, fastembed's lightweight English
  default, ~130MB), downloaded and cached on first use — never trained on or
  aware of this project's product catalog. `index()` embeds every node's
  `raw_text`; `query()` embeds the query text and returns cosine similarity
  per node, rescaled to match the DP matcher's 0-100 range.
- `controller.py` — `SmartSearchController`:
  - Holds the list of active `SearchMethod` instances (initially just
    `SemanticSearchMethod`, if the extra is installed and enabled).
  - Connects to the filter models' existing source-model change signals
    (the same `rowsInserted`/`rowsRemoved`/`modelReset` connections
    [[product_search_score_badges_ui]] already uses to invalidate coverage
    counts) to trigger re-snapshotting + re-`index()` on a background
    `threading.Thread`.
  - On query change, runs `query()` for each active method on a background
    thread, combines results via weighted max (default weight 1.0 — trivial
    with one method today, but the fold is already method-count-agnostic),
    and calls `model.set_external_scores(combined)` — safe to call directly
    from the worker thread since `set_external_scores` only ever swaps a
    pointer under a short lock and never touches Qt/the view.
- `__init__.py` — public surface for SciQLop's settings UI:
  `is_available()`, `is_enabled()` / `set_enabled(bool)`,
  `available_models()` / `get_model()` / `set_model(name)`, and a
  download-progress signal for the settings dialog to show progress instead
  of blocking.

## Data flow

1. Corpus changes → existing structural signals fire → (if smart search
   enabled) `SmartSearchController` snapshots every leaf's `(path_key,
   raw_text)` once (the one place the GIL crosses into the C++ node) →
   background thread calls `index()` on each active method.
2. Query changes → background thread calls `query()` on each active method →
   weighted-max combine → `model.set_external_scores(combined)`.
3. The existing GUI-thread `process_score_batch` timer tick (already async,
   already batched — [[product_search_tree_async_perf_fix]]) reads the latest
   overlay value per leaf via `score_for()` and blends it into the final score
   feeding the existing relevance/coverage badges
   ([[product_search_score_badges_ui]]) — no new rendering path.
4. Toggle off, extra not installed, or model not yet downloaded → overlay
   stays disabled/empty → identical to today's behavior.

## Edge cases

- **Model not downloaded when toggled on**: controller triggers an async
  download with progress; until it completes, behaves as disabled (DP-only
  search keeps working uninterrupted).
- **Download fails (offline, no cache)**: toggle reverts off, error surfaced
  to SciQLop's settings UI, no crash, no effect on baseline search.
- **In-flight index()/query() during a corpus change**: since both operate on
  immutable snapshots, an in-flight call simply finishes against the snapshot
  it received; the change signal has already queued a fresh re-index. One
  possibly-stale round self-corrects on the next update, the same accepted
  pattern as `m_max_score`'s transient-optimism case in
  [[product_search_score_badges_ui]].
- **Concurrent `set_external_scores` (worker thread) and `score_for` (GUI
  thread)**: exactly what `ExternalScoreOverlay`'s shared_ptr-swap exists to
  make safe — this is also the regression test for the
  [[gil_lock_ordering_fix]] bug class (see Testing).
- **Two methods disagree** (one confident, DP scores zero): `max()` lets a
  single confident method surface a result DP would have hidden — the
  intended recall-first behavior.
- **`path_key` collision**: considered and accepted as a known, extremely
  unlikely limitation rather than engineered around now (YAGNI) — leaf paths
  are already the identity concept the DP matcher relies on.

## Bindings

New Python-exposed methods on both filter models: `set_external_scores`,
`set_smart_search_enabled` (+ implicit getter via the overlay's `enabled`
state), added to `bindings.xml`. `ExternalScoreOverlay` itself is an internal
C++ implementation detail, not exposed to Python. The `smart_search`
subpackage is pure Python — no new shiboken bindings of its own.

## Testing (integration/unit, written first)

- **Aggregation logic** (`tests/integration/test_smart_search_controller.py`,
  new, no `fastembed` dependency): fake `SearchMethod` stubs with
  deterministic scores, asserting weighted-max combination, sparse/"no
  opinion" handling, and that disabling/uninstalling short-circuits to a
  no-op.
- **C++ overlay blending** (`tests/integration/test_products_filter.py`,
  extended): call `set_external_scores()` directly with a hand-built dict (no
  Python ML involved), assert final ranking reflects `max(dp_score,
  external_score)`; assert `set_smart_search_enabled(false)` makes the overlay
  inert regardless of what's stored.
- **Concurrency regression test** for the GIL/mutex bug class: a tight loop
  calling `set_external_scores()` from a Python thread while the GUI-thread
  timer concurrently calls the scoring path — asserts no crash/deadlock/hang
  within a timeout, a direct regression test tied to [[gil_lock_ordering_fix]].
- **Real semantic method, end-to-end** (`fastembed` installed in CI, real
  model downloaded — matches [[feedback_ci_deps]]'s "add deps to CI, don't
  skip tests" convention): a small fixture corpus with MMS1 FGM/SCM-style
  descriptive text, asserting a query like `"magnetic field"` surfaces those
  leaves even though the raw acronym text alone wouldn't rank them via DP —
  the direct regression test for this feature's motivating example.
