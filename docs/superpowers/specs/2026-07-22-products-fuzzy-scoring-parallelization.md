# Parallelize the native fuzzy text scorer across products search batches

Date: 2026-07-22
Status: Implemented, NOT YET BUILT/BENCHMARKED — needs `meson subprojects
update` (cpp_utils' `threading/` module is not yet in the vendored copy)
then a normal rebuild before any speedup can be confirmed.

## Context

Measured directly against the user's real 77k-entry product corpus: the
native character-subsequence-DP fuzzy scorer (`free_text_score`/
`subsequence_score`) takes **~30 seconds of wall-clock CPU work** to fully
score a 5-token query against the whole corpus. It's already chunked into
`BATCH_SIZE=200`-leaf batches on a 0ms `QTimer` (the 2026-07-15 async-perf
fix — see `product_search_tree_async_perf_fix` memory) so the UI thread
never freezes solid, but that fix only spreads the *same total* sequential
work across many event-loop turns — it doesn't reduce it. Since typing
restarts the scan from scratch on every keystroke (debounced 150ms,
cancelling whatever was in flight), continuous typing effectively never
lets a query settle at this corpus size. This is what the user reported as
"really slow" typing in product search.

## Fix

`free_text_score()` is a pure, read-only, `const` function with no shared
mutable state — safe to call concurrently for different leaves. Both
`ProductsFlatFilterModel::process_batch()` and
`ProductsTreeFilterModel::process_score_batch()` now:

1. Run `filters_match()` (cheap structured-field compare — provider/type/
   date filters) sequentially first, same as today, to build the list of
   candidates that actually need text scoring.
2. Score all candidates in the *current 200-item batch* in parallel via
   `cpp_utils::threading::parallel_chunks_transform()` (from the
   `cpp_utils` project's own `threading/parallel_chunks.hpp`, not a
   SciQLopPlots-local reimplementation — the user's own general-purpose
   utility, requested explicitly over reaching for the DSP module's
   near-identical private copy). Writes into a preallocated
   `std::vector<int>` at each leaf's own index, so results are
   deterministic regardless of which pool thread processed which leaf.
3. Do the (cheap) per-leaf bookkeeping — signal lookup, merge, model
   mutation, results append — sequentially on the calling (UI) thread, as
   today.

`parallel_chunks_transform()` calls `pool().wait()` internally, blocking
the calling thread until every chunk finishes — this preserves the
existing "one batch = one bounded unit of work, then yield to the event
loop" contract; only *within* a batch does work now run in parallel across
`cpp_utils::threading::pool()` (process-wide, sized to
`hardware_concurrency()`, lazily initialized on first use, shared with the
DSP module's own use of the same underlying `BS::light_thread_pool`
mechanism from the separate `bshoshany-thread-pool` dependency).

**Thread-safety of blocking mid-batch on the query**: `m_pending_query`
(tree model) / the captured `path_text`/`meta_text` (flat model) are only
ever read during the parallel phase. Nothing can mutate them concurrently
because the calling (UI) thread is itself blocked inside
`parallel_chunks_transform()`'s `pool().wait()` for the whole duration —
by construction, no other code runs on the UI thread until the parallel
phase completes.

**Correctness**: `parallel_chunks_transform(input, output, ...)` gives each
worker `output + start` for its own chunk's start offset — every leaf's
score lands at its own deterministic array index, independent of thread
scheduling or completion order. `candidates[idx]`/`fuzzy_scores[idx]`
always correspond to the same leaf. No output reordering, no races.

## Deliberately NOT done here: skip fuzzy scoring under `Override`

Investigated first: when `ScoreMergeStrategy::Override` is active and
`override_signal != "fuzzy"`, the "fuzzy" raw signal is provably never
read by `merge_scores()` — computing it at all is wasted work, and
skipping it would cut the cost far more than parallelizing (near-zero
instead of "corpus size / thread count").

**Not implemented because it has a real correctness hazard that needs more
thought, not a quick patch**: `set_score_merge_strategy()` only triggers a
cheap *remerge* of already-cached raw signals (`remerge_committed()` /
`finalize_batch()`'s per-leaf remerge), not a fresh rescore. If "fuzzy" is
skipped (never computed, never cached) while `Override` is active, then
later code switches strategy *away* from `Override` back to `Max`/`Sum`/
`WeightedSum` *without* also calling `set_query()` again, the remerge would
treat every leaf's "fuzzy" signal as entirely absent — silently wrong,
not just stale. `sidebar_smart_search.py`'s own query-clear path happens to
be safe (an empty query bypasses scoring entirely, verified empirically),
but that's not a general guarantee for every future caller. Revisit this
as a follow-up once there's a clean way to force a full rescore on
strategy changes that need one, or to track "signal was skipped, not
just absent" as a distinct state.

## Correction (after first build+benchmark)

First real-corpus benchmark came back at 17.6s — barely better than the
(flawed, see below) ~30s estimate, on a 16-core machine. Root cause: both
call sites passed `min_chunk_size=32`. `parallel_chunks_transform`'s own
dispatch-plan check is `parallelize = count >= min_chunk_size *
effective_chunks`; with `BATCH_SIZE=200` and a 16-thread pool, that's
`200 >= 32*16=512`, which is false — every single batch silently fell back
to the fully-serial path. Parallelization never actually engaged. Changed
both call sites to `min_chunk_size=0` (always parallelize; `BATCH_SIZE` is
a fixed constant, there's no case here where a 200-item chunk is too small
to be worth it).

Also worth flagging: the original ~30s "baseline" was measured by calling
`SubsequenceMatcher.score()` directly from Python in a loop — that number
includes real Python↔C++ (shiboken) marshalling overhead on every one of
~385,785 calls, which the actual in-process C++ batch loop never pays.
It was never a clean apples-to-apples "before" number for the real
optimization; the honest baseline would have been the pre-parallelization
binary's own real-corpus settle time, not measured before it was
overwritten by the first (buggy) rebuild. Re-verify fresh after this fix.

## Second correction: BATCH_SIZE was tuned for the pre-parallelization world

After the fix above, real settle time was 3.06s (flat) / 0.31s (tree) for
the same 77k-entry query. Comparing the two directly is revealing: both
process the same corpus in the same 386 `BATCH_SIZE=200` batches with the
same parallel compute, yet flat is ~10x slower — the only structural
difference is that flat commits results incrementally every batch
(`beginInsertRows`/`endInsertRows`, growing `m_results`), while tree defers
everything to one final commit (`finish_scoring()`). That gap, plus a
back-of-envelope solve using the two real measurements before/after
parallelization (17.6s serial-broken, 3.06s parallel-fixed, same 386
batches both times) — modeling `total = compute + batches × overhead` —
implies roughly two-thirds of flat's remaining time is fixed per-batch
overhead, not scoring work.

`BATCH_SIZE=200` was sized in the 2026-07-15 async-perf fix purely to
keep each *synchronous* UI-thread tick cheap enough not to freeze the
event loop. That constraint barely applies anymore: a batch's parallel
work now grows very slowly with size (spread across
`cpp_utils::threading::pool()`), so a much bigger batch is still only
tens of milliseconds, while the fixed per-tick cost (QTimer dispatch,
`pool().wait()` synchronization, per-batch model-mutation signals) gets
paid far fewer times. Raised `BATCH_SIZE` 200 → 2000 in both
`ProductsFlatFilterModel.hpp` and `ProductsTreeFilterModel.hpp` (a ~10x
cut in batch count, hence in overhead payments) as a reasoned first
attempt — not a guess, but also not yet re-measured after this specific
change; see below.

## Verification status

**Not yet benchmarked against real code** — needs, in order:
1. `meson subprojects update` (cpp_utils' `threading/parallel_for.hpp` /
   `parallel_chunks.hpp` are already on `cpp_utils`' `origin/main` but not
   yet in the vendored `subprojects/cpp_utils` copy — confirmed via `git
   log origin/main..HEAD` in the standalone cpp_utils checkout showing the
   threading commits are NOT in the "ahead of origin" list, i.e. already
   pushed).
2. Normal rebuild (`meson compile -C build-venv`).
3. Re-run the existing `tests/integration/test_products_filter.py` suite
   (855+ tests) to confirm identical results, not just "didn't crash."
4. Re-benchmark against the real 77k-entry corpus (the same measurement
   method used to find the ~30s baseline) to get a real before/after
   number — expected roughly proportional to core count, not claimed as
   fact until measured.
