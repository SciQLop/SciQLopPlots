# Configurable multi-signal score merging (BM25 as an external signal)

Date: 2026-07-20
Status: Draft (design agreed conversationally this session; not yet planned/implemented)

## Context

SciQLop is adding BM25-based ranking to product search. The starting question
was whether BM25 should replace or complement the existing lexical search —
investigation of the actual code answered that and reframed the scope
considerably:

- Products search today combines two things per leaf: a native C++
  character-level fuzzy scorer (`subsequence_score`/`SubsequenceMatcher.hpp`,
  DP-based, no corpus-wide stats needed) and, optionally, one externally
  supplied score via `ExternalScoreOverlay` (built for the since-relocated
  semantic/`fastembed` search — see
  `docs/superpowers/specs/2026-07-16-smart-search-relocation-design.md`).
  The two are combined with `score = std::max(score, external_score)`
  (`ProductsFlatFilterModel.cpp:174`, `ProductsTreeFilterModel.cpp:219`) — a
  raw max over two incomparable scales, not a principled fusion.
- A BM25-pivoted-length-normalization fix for the fuzzy scorer's own
  length-penalty formula was previously proposed and deliberately deferred
  (see `product_search_scoring_fixes` project memory) in favor of a simpler
  PARAMETER-only-node fix.
- Benchmarking during this session's brainstorm confirmed `bm25s` (numpy-only
  dependency, no ML weights download unlike `fastembed`) scores full corpora
  of this size (tens of thousands of leaves) in low milliseconds — orders of
  magnitude faster than `rank_bm25`. At this scale, implementing and
  maintaining a from-scratch BM25 in C++ buys nothing: no dependency-weight
  concern (numpy is already a hard SciQLopPlots dependency) and no
  performance need Python can't meet.

**Decision: BM25 will not be implemented in, or exposed from, SciQLopPlots.**
It becomes another Python-side scoring signal (implemented in SciQLop, using
`bm25s`), computed the same way the semantic `SearchMethod` was designed to
be, and fed into SciQLopPlots through the *existing*
`corpus_snapshot()`/`set_external_scores()` plumbing. Building and wiring the
actual BM25 `SearchMethod` in SciQLop is separate future work (tracked
alongside the still-pending multi-domain registry from the relocation doc)
and is a non-goal of this document.

**What this document designs instead:** today's `ExternalScoreOverlay` only
supports one blended-via-`max()` external signal. To let BM25 and (later)
semantic search coexist, be independently toggled, and combine with the
native fuzzy score in a principled, configurable way — including letting one
signal fully override the others — the overlay and the two filter models'
scoring pipeline need to be generalized. That generalization is the entire
scope of this design.

## Architecture

### `ScoreSignalRegistry` (replaces `ExternalScoreOverlay`)

Same file pair (`include/SciQLopPlots/Products/ScoreSignalRegistry.hpp` +
`src/ScoreSignalRegistry.cpp`), same thread-safety pattern as today's
`ExternalScoreOverlay` (writers build a whole new immutable snapshot off any
lock, then swap a `shared_ptr` under a short-lived mutex; readers copy the
`shared_ptr` under the same short lock and read lock-free — this mirrors the
GIL/mutex deadlock fix from PR #70 and must not regress it).

Generalizes from one `QHash<QString,double>` + one enabled flag to N named
signals:

```cpp
class ScoreSignalRegistry
{
public:
    void set_scores(const QString& signal_name, const QHash<QString, QVariant>& scores);
    void set_signal_enabled(const QString& signal_name, bool enabled);
    bool signal_enabled(const QString& signal_name) const;
    QStringList registered_signals() const;

    // Absent != zero: a leaf a signal hasn't scored yet must not be treated
    // as a genuine zero score by max-tracking or by Override's "no match"
    // rule.
    std::optional<double> score_for(const QString& signal_name, const QString& path_key) const;
};
```

### `ScoreMerge.hpp` (new shared free-function header)

Plain functions, no Qt-model coupling, independently unit-testable — same
spirit as `SubsequenceMatcher.hpp`.

```cpp
enum class ScoreMergeStrategy { Max, Sum, WeightedSum, Override };

// raw_signals: whichever signals are present for this one leaf, keyed by
// signal name (the native fuzzy score is included under the reserved name
// "fuzzy"). Each present signal is normalized to 0-100 relative to that
// signal's own max across the current result set (same convention
// ProductsRelevanceScoreRole already uses) before combining, so Max/Sum/
// WeightedSum never compare raw scores on incomparable scales.
std::optional<double> merge_scores(const QHash<QString, double>& raw_signals,
                                    ScoreMergeStrategy strategy,
                                    const QHash<QString, double>& weights,
                                    const QHash<QString, double>& signal_maxes,
                                    const QString& override_signal);
```

- `Max` / `Sum`: over all present, normalized signals (weight 1.0 each,
  unless overridden via `weights`).
- `WeightedSum`: `sum(weights.value(name, 1.0) * normalized[name])` over
  present signals; a signal with weight 0 is excluded (this is how "fuzzy"
  can be zeroed out of blending entirely, same knob as any other signal).
- `Override`: if `override_signal` is present (and enabled) for this leaf,
  return its normalized score; otherwise return `std::nullopt` regardless of
  what any other signal says. `weights` is not consulted at all in this
  mode — the override signal's normalized score is used as-is. If
  `override_signal` is unset, disabled, or was never registered, every leaf
  resolves to `std::nullopt` (empty result set) — a valid, documented
  configuration state, not a special-cased error.

A signal that produced zero present values across the whole batch is
excluded from `signal_maxes` and from every leaf's merge entirely (it never
contributes a bogus max-of-0 that would divide-by-zero in normalization).

### Filter model integration

`ProductsFlatFilterModel` and `ProductsTreeFilterModel` each own a
`ScoreSignalRegistry`, a `ScoreMergeStrategy`, a `QHash<QString,double>`
weights map, and an override-signal name — mirroring how `max_score_tiers`
is already a per-model settable property today.

Replacing today's `set_external_scores`/`set_smart_search_enabled`/
`smart_search_enabled`:

```cpp
void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores);
void set_signal_enabled(const QString& signal_name, bool enabled);
bool signal_enabled(const QString& signal_name) const;
QStringList registered_signals() const;

void set_score_merge_strategy(ScoreMergeStrategy strategy);
ScoreMergeStrategy score_merge_strategy() const;

void set_signal_weight(const QString& signal_name, double weight);  // default 1.0, applies to "fuzzy" too
double signal_weight(const QString& signal_name) const;

void set_override_signal(const QString& signal_name);   // consulted only when strategy == Override
QString override_signal() const;
```

This is a breaking rename (adds a `signal_name` parameter to
`set_external_scores`, drops `set_smart_search_enabled`/
`smart_search_enabled` outright). Since none of this has shipped to remote
yet, this is a clean rename rather than a back-compat shim.

`ScoreMergeStrategy` is registered in `bindings.xml` as an `<enum-type>`
(same pattern as `ProductsModelNodeType`) so Python sets it directly
(`ScoreMergeStrategy.WeightedSum`, etc). No standalone Python-bindable
wrapper for `merge_scores` itself is needed — unlike `SubsequenceMatcher`,
nothing external ever calls the merge function directly; Python only pushes
signals in and configures the strategy/weights.

## Data flow

**Per-leaf scoring** (unchanged entry point, now records raw per-signal
scores instead of one already-blended int) — for every leaf that passes
`filters_match()` during a batch:

1. Compute the native fuzzy score as today (`free_text_score`) → store under
   `"fuzzy"`.
2. For every currently-enabled registered signal, look up
   `score_for(name, path_key)` — store if present, skip if absent.
3. Update a running per-signal max seen so far this pass.
4. Merge *provisionally*, using the running-so-far maxes, and insert/display
   as today — this preserves the flat model's existing progressive reveal
   during batching.

**Settle pass** (extends the existing end-of-batch block that already
re-sorts, in both models): once the whole corpus has been scanned, re-run
`merge_scores()` for every leaf now that the true final per-signal maxes are
known, update `m_results` scores, re-sort, and emit `layoutChanged` (tree) /
`dataChanged` + the existing sort logic (flat). No new full extra corpus
pass and no new architectural pattern — this reuses the exact commit/settle
machinery the async-perf fix (`product_search_tree_async_perf_fix`) already
built for the tree model, and extends the flat model's existing
end-of-batch re-sort to also re-normalize.

**Override + async gap:** if the override signal hasn't scored a leaf yet
(still computing async, or the leaf was just added), that leaf is excluded
from results — both provisionally and at settle — until the signal actually
produces a score for it. No per-leaf fallback chain.

**Rescore trigger:** `set_scores(name, ...)` and `set_signal_enabled(name,
...)` both go through the existing queued
`QMetaObject::invokeMethod(... set_query(m_query) ...)` re-run path. Today
only `set_external_scores` does this — `set_smart_search_enabled` doesn't,
which is a latent gap; this refactor folds in the fix so toggling any signal
on/off always requeries.

## Error handling & edge cases

- Unknown/unset signal weight → default 1.0, not an error.
- `Override` pointing at a never-registered or disabled signal → empty
  result set. No exception, no assert.
- A signal registered but never enabled never contributes to any strategy
  and is excluded from max-tracking (no divide-by-zero from a bogus 0 max).
- Concurrent `set_scores`/`set_signal_enabled` from a Python thread while a
  batch is scoring: unchanged thread-safety story — snapshot-swap under a
  short lock on the registry side, queued reconnection back onto the
  model's own thread for the rescore, so no cross-thread scoring ever
  happens.
- `WeightedSum` with every weight at 0 → every leaf's merged score is 0 →
  empty result set. Same "empty result set is a valid state" philosophy,
  not special-cased.

## Testing plan

Reproducer-first (TDD) for each piece, as usual:

1. **`ScoreMerge.hpp` unit tests** (pure functions, no Qt model, no
   registry): table-driven over `{Max, Sum, WeightedSum, Override}` ×
   `{all signals present, one signal absent, override signal absent, empty
   weights}`.
2. **`ScoreSignalRegistry` tests**: multi-signal storage/retrieval,
   independent enable/disable per name, thread-safety test extended from
   today's `TestExternalScoreOverlayConcurrency` to N named signals.
3. **Integration tests** (`tests/integration/test_products_filter.py`):
   register two synthetic signals simulating BM25 + a second source, verify
   each strategy end-to-end through Python; verify `Override` excludes
   leaves the override signal hasn't scored; verify the
   provisional-then-settle re-normalization actually changes displayed
   scores/order once late-arriving corpus-wide maxes shift (flat model);
   verify `set_signal_enabled` alone now triggers a requery, using
   `flush_events()` per the existing async-test pattern.
4. Full existing suite (currently 946 tests) must still pass after the
   rename. The single-signal case becomes `WeightedSum` with one signal at
   weight 1.0 — **not** bit-for-bit identical to today's raw `max()`, since
   even the single-signal case now normalizes before combining. This is a
   deliberate, documented behavior change, not a regression.

## Non-goals

- The actual BM25 `SearchMethod` implementation (Python, `bm25s`-backed) is
  SciQLop-side work, not designed here.
- The five open questions from the smart-search relocation doc (global vs.
  per-domain enable/model, domain lifecycle on plugin unload, non-Qt
  change-notification, where the Products adapter lives, concurrency at N
  domains) are unresolved by this document and unaffected by it — this
  design only changes how SciQLopPlots merges whatever named signals
  eventually get registered, not who registers them or how.
- No new signal-specific logic (BM25 or semantic) lives in SciQLopPlots.
  The registry and merge strategies are signal-agnostic by construction.

## Suggested next step

Write an implementation plan for the SciQLopPlots-side change described
here (`ScoreSignalRegistry`, `ScoreMerge.hpp`, the two filter models' API
and batch/settle rework, bindings.xml updates, and the test suite above).
The SciQLop-side BM25 `SearchMethod` and multi-domain registry work remain
separate, tracked alongside the relocation doc's pending items.
