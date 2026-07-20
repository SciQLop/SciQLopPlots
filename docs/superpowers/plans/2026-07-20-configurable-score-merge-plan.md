# Configurable Multi-Signal Score Merging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `ExternalScoreOverlay`'s single blended-via-`max()` external
score with a named-signal registry and a configurable merge strategy
(Max/Sum/WeightedSum/Override), so BM25 and future signals (e.g. semantic
search) can each independently plug into Products search and combine with
the native fuzzy scorer in a principled, normalized way.

**Architecture:** A new signal-agnostic `ScoreSignalRegistry` (renamed,
generalized `ExternalScoreOverlay`) holds N named raw-score maps. A new
`ScoreMerge.hpp` free function normalizes each present signal to 0-100
relative to its own max in the current result set, then combines them per
the configured `ScoreMergeStrategy`. Both `ProductsFlatFilterModel` and
`ProductsTreeFilterModel` collect per-leaf raw signals during their existing
async batch-scoring pass, then merge once the batch settles; a cheap
synchronous re-merge path (mirroring the existing `set_max_score_tiers`
pattern) lets strategy/weight/override changes re-rank already-scored
results without re-running the expensive text scorer.

**Tech Stack:** C++20, Qt6 (QHash/QSet/QMutex), Shiboken6 Python bindings,
pytest/pytest-qt integration tests.

## Global Constraints

- BM25 itself is **not** implemented here — it will be a Python `SearchMethod`
  in SciQLop (via `bm25s`), fed in through this registry. Nothing in this
  plan is BM25-specific; the registry and merge logic must stay
  signal-agnostic.
- No back-compat shims for the old `ExternalScoreOverlay`/
  `set_smart_search_enabled` API — this is a clean rename. None of it has
  shipped to remote yet.
- Thread-safety pattern must match the existing GIL/mutex-deadlock fix (PR
  #70): writers build a whole new immutable snapshot off any lock, swap a
  `shared_ptr` under a short-lived mutex; readers copy the `shared_ptr`
  under the same short lock and read lock-free. Never hold the registry
  mutex across a call that could block on the GIL.
- Default `ScoreMergeStrategy` is `Max` (closest to today's `std::max`
  behavior). Default signal weight is `1.0`. Default override signal is
  empty (unset).
- The degenerate single-signal case (no external signals registered) must
  produce **numerically identical** `ProductsRelevanceScoreRole` /
  `ProductsCoverageRole` percentages to today — verified by the ~900
  pre-existing tests in `tests/integration/test_products_filter.py` passing
  **unchanged**. Do not edit those pre-existing exact-percentage assertions;
  if any of them needs a value change, that's a signal the merge math has a
  bug, not that the test was wrong.
- Build: `VENV=$(pwd)/.venv; export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"; export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"; $VENV/bin/meson compile -C build-venv` from the repo root.
- After adding/renaming any Python-exposed method, type, or enum, touch
  `SciQLopPlots/bindings/bindings.xml` before rebuilding (shiboken doesn't
  track header changes automatically), and if the shiboken wrapper fails to
  regenerate after a rename, `rm` the stale
  `build-venv/SciQLopPlots/bindings/**_wrapper.cpp` and
  `$VENV/bin/meson setup --reconfigure build-venv`.
- Run tests from `/tmp` so the source package doesn't shadow the build:
  `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q`.

---

## Task 1: `ScoreMerge.hpp` — merge strategy enum and pure merge function

**Files:**
- Create: `include/SciQLopPlots/Products/ScoreMerge.hpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `SciQLopPlots/bindings/bindings.h`
- Modify: `SciQLopPlots/meson.build`
- Test: `tests/integration/test_score_merge.py`

**Interfaces:**
- Produces: `enum class ScoreMergeStrategy { Max, Sum, WeightedSum, Override }`;
  `std::optional<double> merge_scores(const QHash<QString,double>& raw_signals, ScoreMergeStrategy strategy, const QHash<QString,double>& weights, const QHash<QString,double>& signal_maxes, const QString& override_signal)`;
  Python-bindable `ScoreMerger::merge(...)` static wrapper (test-only surface,
  consumed by Task 1's own tests; Tasks 2/3 call `merge_scores` directly from
  C++, not through this wrapper).

- [ ] **Step 1: Write the failing test**

Create `tests/integration/test_score_merge.py`:

```python
"""Tests for merge_scores() / ScoreMergeStrategy — combining the native
fuzzy score with externally-supplied signals (e.g. BM25, semantic search)."""
import pytest
from SciQLopPlots import ScoreMerger, ScoreMergeStrategy


class TestMaxStrategy:
    def test_picks_higher_normalized_signal(self):
        # fuzzy raw=10 (its own max=10 -> normalized 100), bm25 raw=5 (its
        # own max=20 -> normalized 25): Max picks fuzzy's 100.
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 5.0}, ScoreMergeStrategy.Max,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(100.0)

    def test_absent_signal_excluded_not_zero(self):
        # bm25 is absent for this leaf entirely -- must not count as a
        # literal 0 and drag Max down; only "fuzzy" is considered.
        result = ScoreMerger.merge(
            {"fuzzy": 5.0}, ScoreMergeStrategy.Max,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(50.0)


class TestSumStrategy:
    def test_sums_all_normalized_signals(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 10.0}, ScoreMergeStrategy.Sum,
            {}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        # fuzzy normalized 100 + bm25 normalized 50 = 150
        assert result == pytest.approx(150.0)


class TestWeightedSumStrategy:
    def test_applies_per_signal_weight(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 20.0}, ScoreMergeStrategy.WeightedSum,
            {"fuzzy": 1.0, "bm25": 0.5}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        # fuzzy: 100*1.0 + bm25: 100*0.5 = 150
        assert result == pytest.approx(150.0)

    def test_unweighted_signal_defaults_to_one(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0}, ScoreMergeStrategy.WeightedSum,
            {}, {"fuzzy": 10.0}, "")
        assert result == pytest.approx(100.0)

    def test_zero_weight_excludes_signal(self):
        result = ScoreMerger.merge(
            {"fuzzy": 10.0, "bm25": 20.0}, ScoreMergeStrategy.WeightedSum,
            {"fuzzy": 0.0, "bm25": 1.0}, {"fuzzy": 10.0, "bm25": 20.0}, "")
        assert result == pytest.approx(100.0)


class TestOverrideStrategy:
    def test_uses_only_the_override_signal(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0, "bm25": 5.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0, "bm25": 20.0}, "bm25")
        assert result == pytest.approx(25.0)

    def test_missing_override_signal_is_no_match(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0}, "bm25")
        assert result is None

    def test_unset_override_signal_is_no_match(self):
        result = ScoreMerger.merge(
            {"fuzzy": 1000.0}, ScoreMergeStrategy.Override,
            {}, {"fuzzy": 1000.0}, "")
        assert result is None


def test_empty_raw_signals_is_no_match():
    assert ScoreMerger.merge({}, ScoreMergeStrategy.Max, {}, {}, "") is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_score_merge.py -v`
Expected: FAIL — `ImportError: cannot import name 'ScoreMerger' from 'SciQLopPlots'` (the type doesn't exist yet).

- [ ] **Step 3: Create `include/SciQLopPlots/Products/ScoreMerge.hpp`**

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#include <QHash>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <optional>

enum class ScoreMergeStrategy
{
    Max,
    Sum,
    WeightedSum,
    Override
};

// Combines whichever named raw signals are present for one leaf (the
// native fuzzy scorer is just another entry, under the reserved name
// "fuzzy") into a single score, for one of four strategies:
//
//  - Max/Sum: every present signal, normalized to 0-100 relative to that
//    signal's own max across the current result set (raw scales differ
//    per signal -- subsequence bonus points vs. BM25 vs. cosine similarity
//    -- so combining raw values directly would let whichever signal
//    happens to produce bigger numbers dominate).
//  - WeightedSum: same normalization, each signal scaled by
//    weights.value(name, 1.0) before summing; a weight of 0 excludes that
//    signal entirely (this is how "fuzzy" itself can be excluded from
//    blending).
//  - Override: returns override_signal's normalized value if present,
//    ignoring every other signal and all weights; returns nullopt
//    (no match) if override_signal is absent, unset, or has no value for
//    this leaf -- there is no per-leaf fallback to another signal.
//
// Returns std::nullopt when there is no match at all: raw_signals is
// empty, or (Override only) the designated signal has no value here.
inline std::optional<double> merge_scores(const QHash<QString, double>& raw_signals,
                                           ScoreMergeStrategy strategy,
                                           const QHash<QString, double>& weights,
                                           const QHash<QString, double>& signal_maxes,
                                           const QString& override_signal)
{
    auto normalized = [&](const QString& name, double raw) -> double
    {
        double max_value = signal_maxes.value(name, 0.0);
        return max_value > 0.0 ? (raw * 100.0 / max_value) : 0.0;
    };

    if (strategy == ScoreMergeStrategy::Override)
    {
        auto it = raw_signals.constFind(override_signal);
        if (it == raw_signals.constEnd())
            return std::nullopt;
        return normalized(override_signal, it.value());
    }

    if (raw_signals.isEmpty())
        return std::nullopt;

    double result = 0.0;
    bool any = false;
    for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
    {
        double weight
            = strategy == ScoreMergeStrategy::WeightedSum ? weights.value(it.key(), 1.0) : 1.0;
        if (weight == 0.0)
            continue;
        double n = normalized(it.key(), it.value()) * weight;
        if (strategy == ScoreMergeStrategy::Max)
            result = any ? std::max(result, n) : n;
        else
            result += n;
        any = true;
    }
    return any ? std::optional<double>(result) : std::nullopt;
}

// Python-bindable wrapper for direct unit testing of merge_scores() --
// nothing in SciQLopPlots or SciQLop calls this from production code, it
// exists purely so the strategy math has an isolated test surface, the same
// way SubsequenceMatcher wraps subsequence_score/subsequence_match.
class ScoreMerger
{
public:
    static QVariant merge(const QHash<QString, QVariant>& raw_signals, ScoreMergeStrategy strategy,
                           const QHash<QString, QVariant>& weights,
                           const QHash<QString, QVariant>& signal_maxes,
                           const QString& override_signal)
    {
        auto to_double_hash = [](const QHash<QString, QVariant>& in)
        {
            QHash<QString, double> out;
            out.reserve(in.size());
            for (auto it = in.constBegin(); it != in.constEnd(); ++it)
                out.insert(it.key(), it.value().toDouble());
            return out;
        };

        auto result = merge_scores(to_double_hash(raw_signals), strategy,
                                    to_double_hash(weights), to_double_hash(signal_maxes),
                                    override_signal);
        return result ? QVariant(*result) : QVariant();
    }
};
```

- [ ] **Step 4: Wire into the build and bindings**

In `SciQLopPlots/bindings/bindings.h`, add the include right after the
existing `SubsequenceMatcher.hpp` include (around line 66):

```cpp
#include <SciQLopPlots/Products/SubsequenceMatcher.hpp>
#include <SciQLopPlots/Products/ScoreMerge.hpp>
```

In `SciQLopPlots/bindings/bindings.xml`, add these two lines right after the
existing `<object-type name="SubsequenceMatcher"/>` (around line 997):

```xml
<value-type name="MatchResult"/>
<object-type name="SubsequenceMatcher"/>

<enum-type name="ScoreMergeStrategy"/>
<object-type name="ScoreMerger"/>
```

In `SciQLopPlots/meson.build`, add the new header to the plain `headers`
list right after `SubsequenceMatcher.hpp` (around line 164):

```python
project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
project_source_root+'/include/SciQLopPlots/Products/ScoreMerge.hpp',
```

- [ ] **Step 5: Rebuild**

```bash
touch /var/home/jeandet/Documents/prog/SciQLopPlots/SciQLopPlots/bindings/bindings.xml
VENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv
```
Expected: builds cleanly. If the shiboken wrapper doesn't pick up the new
types, `rm` the stale
`build-venv/SciQLopPlots/bindings/*ScoreMerger_wrapper.cpp` (and any sibling
`*ScoreMergeStrategy*` generated file) and
`$VENV/bin/meson setup --reconfigure /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv`.

- [ ] **Step 6: Run test to verify it passes**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_score_merge.py -v`
Expected: PASS, all 9 tests.

- [ ] **Step 7: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ScoreMerge.hpp \
        SciQLopPlots/bindings/bindings.xml SciQLopPlots/bindings/bindings.h \
        SciQLopPlots/meson.build tests/integration/test_score_merge.py
git commit -m "feat(products): add configurable score-merge strategies (Max/Sum/WeightedSum/Override)"
```

---

## Task 2: Generalize `ExternalScoreOverlay` into `ScoreSignalRegistry` and wire it into `ProductsFlatFilterModel`

**Files:**
- Create: `include/SciQLopPlots/Products/ScoreSignalRegistry.hpp`
- Create: `src/ScoreSignalRegistry.cpp`
- Delete: `include/SciQLopPlots/Products/ExternalScoreOverlay.hpp`
- Delete: `src/ExternalScoreOverlay.cpp`
- Modify: `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`
- Modify: `src/ProductsFlatFilterModel.cpp`
- Modify: `SciQLopPlots/meson.build`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `merge_scores`, `ScoreMergeStrategy` from Task 1's `ScoreMerge.hpp`.
- Produces: `ScoreSignalRegistry` (thread-safe multi-signal store, reused
  verbatim by Task 3); `ProductsFlatFilterModel::set_external_scores(QString,QHash<QString,QVariant>)`,
  `set_signal_enabled(QString,bool)`, `signal_enabled(QString) const`,
  `registered_signals() const`, `set_score_merge_strategy(ScoreMergeStrategy)`,
  `score_merge_strategy() const`, `set_signal_weight(QString,double)`,
  `signal_weight(QString) const`, `set_override_signal(QString)`,
  `override_signal() const`.

- [ ] **Step 1: Update the failing tests first**

In `tests/integration/test_products_filter.py`, replace the entire
`TestExternalScoreOverlay` and `TestExternalScoreOverlayConcurrency` classes
(lines 845-980) with:

```python
class TestScoreSignalRegistry:
    def test_tree_filter_ignores_disabled_signal(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

    def test_tree_filter_blends_enabled_signal(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_flat_filter_blends_enabled_signal(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_signal_enabled_defaults_to_false(self, qtbot):
        fm = ProductsTreeFilterModel()
        assert fm.signal_enabled("bm25") is False

    def test_registered_signals_lists_every_pushed_signal(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_external_scores("bm25", {path_key: 1.0})
        fm.set_external_scores("semantic", {path_key: 1.0})
        assert set(fm.registered_signals()) == {"bm25", "semantic"}

    def test_two_signals_blend_independently_under_max(self, qtbot, overlay_test_model):
        """bm25 alone would surface the leaf; semantic is registered but
        left disabled, and must not suppress bm25's contribution."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_external_scores("semantic", {path_key: 0.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_tree_filter_late_external_scores_trigger_rescoring(self, qtbot, overlay_test_model):
        """Regression test: a slow search method (e.g. an embedding model)
        calls set_external_scores() long after set_query() already
        committed a fuzzy-only scoring pass. The view must pick up the new
        scores immediately, without the caller re-issuing set_query()."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        fm.set_external_scores("bm25", {path_key: 100.0})
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_flat_filter_late_external_scores_trigger_rescoring(self, qtbot, overlay_test_model):
        """Same regression as above, for ProductsFlatFilterModel."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        fm.set_external_scores("bm25", {path_key: 100.0})
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_tree_filter_enabling_signal_alone_triggers_rescoring(self, qtbot, overlay_test_model):
        """Regression: toggling set_signal_enabled() must requery too, not
        only set_external_scores() -- a gap in the old single-signal API."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        fm.set_signal_enabled("bm25", True)
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_tree_filter_background_thread_external_scores_trigger_rescoring(
            self, qtbot, overlay_test_model):
        """Real production call pattern: a Python search controller always
        calls set_external_scores() from a background threading.Thread,
        never the GUI thread. The rescore must still surface via the queued
        invokeMethod trampoline, without deadlocking or racing model state."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        worker = threading.Thread(target=fm.set_external_scores, args=("bm25", {path_key: 100.0}))
        worker.start()
        worker.join(timeout=5)
        assert not worker.is_alive()

        qtbot.waitUntil(lambda: "acronym_only" in collect_visible_names(fm), timeout=5000)

    def test_flat_filter_background_thread_external_scores_trigger_rescoring(
            self, qtbot, overlay_test_model):
        """Same as above, for ProductsFlatFilterModel."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        worker = threading.Thread(target=fm.set_external_scores, args=("bm25", {path_key: 100.0}))
        worker.start()
        worker.join(timeout=5)
        assert not worker.is_alive()

        qtbot.waitUntil(lambda: "acronym_only" in collect_visible_names(fm), timeout=5000)


class TestScoreSignalRegistryConcurrency:
    @pytest.mark.timeout(10)
    def test_concurrent_set_external_scores_does_not_deadlock(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_query(QueryParser.parse("magnetic field"))

        stop = threading.Event()

        def hammer():
            i = 0
            while not stop.is_set():
                fm.set_external_scores("bm25", {path_key: float(i % 100)})
                i += 1

        writer = threading.Thread(target=hammer, daemon=True)
        writer.start()
        try:
            for _ in range(200):
                QCoreApplication.processEvents()
        finally:
            stop.set()
            writer.join(timeout=5)
        assert not writer.is_alive()


class TestMergeStrategyConfiguration:
    def test_weighted_sum_can_zero_out_fuzzy(self, qtbot, overlay_test_model):
        """Zeroing the 'fuzzy' weight means only bm25 decides matches."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_score_merge_strategy(ScoreMergeStrategy.WeightedSum)
        fm.set_signal_weight("fuzzy", 0.0)
        fm.set_query(QueryParser.parse("zzz_no_fuzzy_match_zzz"))
        flush_events()
        # "magnetic_field" (the other leaf) has no bm25 score and fuzzy is
        # zeroed out, so only "acronym_only" (bm25-boosted) should show.
        assert collect_visible_names(fm) == ["acronym_only"]

    def test_override_excludes_leaves_the_override_signal_has_not_scored(
            self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_score_merge_strategy(ScoreMergeStrategy.Override)
        fm.set_override_signal("bm25")
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        # Only the leaf bm25 actually scored is visible -- "magnetic_field"
        # matches under fuzzy alone but bm25 never scored it, so Override
        # treats it as no match, per the "no per-leaf fallback" design.
        assert collect_visible_names(fm) == ["acronym_only"]

    def test_override_with_no_signal_set_shows_nothing(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        fm = ProductsFlatFilterModel(model)
        fm.set_score_merge_strategy(ScoreMergeStrategy.Override)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert collect_visible_names(fm) == []

    def test_changing_strategy_rereanks_without_new_async_pass(self, qtbot, overlay_test_model):
        """set_score_merge_strategy() re-merges already-cached raw signals
        synchronously -- no flush_events() needed, unlike set_query()."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

        fm.set_score_merge_strategy(ScoreMergeStrategy.Override)
        fm.set_override_signal("bm25")
        # No flush_events() -- this must take effect synchronously.
        assert collect_visible_names(fm) == ["acronym_only"]


def test_corpus_snapshot_returns_full_unfiltered_corpus(qtbot, overlay_test_model):
    model, leaf = overlay_test_model
    path_key = ' '.join(leaf.path())
    fm = ProductsFlatFilterModel(model)
    snapshot = fm.corpus_snapshot()
    assert path_key in snapshot
    assert snapshot[path_key] == leaf.raw_text()
```

Also update the import line near the top of the file (around line 7-10) to
pull in the new enum:

```python
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser,
    ScoreMergeStrategy
)
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -k "ScoreSignalRegistry or MergeStrategyConfiguration" -v`
Expected: FAIL — `AttributeError`/`TypeError` on the not-yet-renamed methods
(`set_external_scores` still takes one argument, `set_signal_enabled`
doesn't exist yet, etc).

- [ ] **Step 3: Create `include/SciQLopPlots/Products/ScoreSignalRegistry.hpp`**

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <memory>
#include <optional>

// Holds N independently-named, externally-supplied per-leaf score signals
// (e.g. "bm25", "semantic"), each later blended with the native lexical
// score by ScoreMerge.hpp's merge_scores(). Every method is safe to call
// from any thread: writers build a whole new immutable QHash off any lock,
// then only hold m_mutex long enough to swap a shared_ptr; readers copy the
// shared_ptr under the same short lock and read the (now locally-owned)
// snapshot lock-free. This mirrors the shared_ptr snapshot pattern used to
// fix a prior GIL/mutex deadlock (PR #70) -- the lock is never held across
// a call that could block on the GIL.
class ScoreSignalRegistry
{
public:
    void set_scores(const QString& signal_name, const QHash<QString, QVariant>& scores);
    void set_signal_enabled(const QString& signal_name, bool enabled);
    bool signal_enabled(const QString& signal_name) const;
    QStringList registered_signals() const;

    // Absent != zero -- a leaf a signal hasn't scored yet must not be
    // treated as a genuine zero score by max-tracking or by Override's
    // "no match" rule.
    std::optional<double> score_for(const QString& signal_name, const QString& path_key) const;

private:
    mutable QMutex m_mutex;
    QHash<QString, std::shared_ptr<const QHash<QString, double>>> m_scores;
    QHash<QString, bool> m_enabled;
};
```

- [ ] **Step 4: Create `src/ScoreSignalRegistry.cpp`**

```cpp
#include "SciQLopPlots/Products/ScoreSignalRegistry.hpp"
#include <QMutexLocker>

void ScoreSignalRegistry::set_scores(const QString& signal_name,
                                      const QHash<QString, QVariant>& scores)
{
    auto converted = std::make_shared<QHash<QString, double>>();
    converted->reserve(scores.size());
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
        converted->insert(it.key(), it.value().toDouble());

    QMutexLocker locker(&m_mutex);
    m_scores.insert(signal_name, converted);
}

void ScoreSignalRegistry::set_signal_enabled(const QString& signal_name, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_enabled.insert(signal_name, enabled);
}

bool ScoreSignalRegistry::signal_enabled(const QString& signal_name) const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled.value(signal_name, false);
}

QStringList ScoreSignalRegistry::registered_signals() const
{
    QMutexLocker locker(&m_mutex);
    return m_scores.keys();
}

std::optional<double> ScoreSignalRegistry::score_for(const QString& signal_name,
                                                      const QString& path_key) const
{
    std::shared_ptr<const QHash<QString, double>> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_enabled.value(signal_name, false))
            return std::nullopt;
        snapshot = m_scores.value(signal_name);
    }
    if (!snapshot)
        return std::nullopt;
    auto it = snapshot->constFind(path_key);
    if (it == snapshot->constEnd())
        return std::nullopt;
    return it.value();
}
```

- [ ] **Step 5: Delete the old overlay files**

```bash
git rm include/SciQLopPlots/Products/ExternalScoreOverlay.hpp src/ExternalScoreOverlay.cpp
```

- [ ] **Step 6: Update `SciQLopPlots/meson.build`**

Replace the `ExternalScoreOverlay.hpp` line in the `headers` list (around
line 166) with:

```python
project_source_root+'/include/SciQLopPlots/Products/ScoreSignalRegistry.hpp']
```

Replace `'../src/ExternalScoreOverlay.cpp',` in the `sources` list (around
line 268) with:

```python
'../src/ScoreSignalRegistry.cpp',
```

- [ ] **Step 7: Rewrite `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`**

Change the include (line 25):

```cpp
#include "SciQLopPlots/Products/ScoreSignalRegistry.hpp"
#include "SciQLopPlots/Products/ScoreMerge.hpp"
```

Replace the private member block (the `ScoredNode` struct through
`ExternalScoreOverlay m_external_scores;`, roughly lines 40-58) with:

```cpp
    struct ScoredNode
    {
        ProductsModelNode* node;
        double score;
    };
    QList<ScoredNode> m_results;
    double m_max_score = 0;

    ScoreSignalRegistry m_score_signals;
    ScoreMergeStrategy m_merge_strategy = ScoreMergeStrategy::Max;
    QHash<QString, double> m_signal_weights;
    QString m_override_signal;

    // Per-leaf raw signal scores from the last completed text-scoring
    // pass, kept after settling so set_score_merge_strategy()/
    // set_signal_weight()/set_override_signal() can cheaply re-merge
    // without re-running the DP scorer over the whole corpus -- mirrors
    // ProductsTreeFilterModel's set_max_score_tiers cheap-rerank pattern.
    QHash<ProductsModelNode*, QHash<QString, double>> m_node_raw_signals;
    QHash<QString, double> m_signal_maxes;

    struct LeafEntry
    {
        ProductsModelNode* node;
        QString path_text;
        QString meta_text;
    };
    QList<LeafEntry> m_pending_leaves;
    int m_batch_cursor = 0;
    int m_batch_generation = 0;
    QTimer* m_batch_timer;

    // Scratch state accumulated across batches; committed into
    // m_node_raw_signals/m_signal_maxes only once the whole corpus has
    // been scanned (see finalize_batch()).
    QHash<ProductsModelNode*, QHash<QString, double>> m_pending_raw_signals;
    QHash<QString, double> m_pending_signal_maxes;

    static constexpr int BATCH_SIZE = 200;
```

Replace the `set_external_scores`/`set_smart_search_enabled`/
`smart_search_enabled` block (lines 67-79) with:

```cpp
    void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores)
    {
        m_score_signals.set_scores(signal_name, scores);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    void set_signal_enabled(const QString& signal_name, bool enabled)
    {
        m_score_signals.set_signal_enabled(signal_name, enabled);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    bool signal_enabled(const QString& signal_name) const
    {
        return m_score_signals.signal_enabled(signal_name);
    }
    QStringList registered_signals() const { return m_score_signals.registered_signals(); }

    void set_score_merge_strategy(ScoreMergeStrategy strategy)
    {
        m_merge_strategy = strategy;
        remerge_committed();
    }
    ScoreMergeStrategy score_merge_strategy() const noexcept { return m_merge_strategy; }

    void set_signal_weight(const QString& signal_name, double weight)
    {
        m_signal_weights.insert(signal_name, weight);
        remerge_committed();
    }
    double signal_weight(const QString& signal_name) const
    {
        return m_signal_weights.value(signal_name, 1.0);
    }

    void set_override_signal(const QString& signal_name)
    {
        m_override_signal = signal_name;
        remerge_committed();
    }
    QString override_signal() const { return m_override_signal; }
```

Add a new private method declaration alongside `process_batch`
(find the private section that declares `void rebuild();` /
`void process_batch();` and add):

```cpp
    void finalize_batch();
    void remerge_committed();
    void sort_and_remap_results();
```

- [ ] **Step 8: Rewrite the scoring pipeline in `src/ProductsFlatFilterModel.cpp`**

Update `rebuild()` to also clear the new committed/pending signal caches:

```cpp
void ProductsFlatFilterModel::rebuild()
{
    m_batch_timer->stop();
    ++m_batch_generation;

    beginResetModel();
    m_results.clear();
    m_max_score = 0;
    m_node_raw_signals.clear();
    m_signal_maxes.clear();
    endResetModel();

    m_pending_leaves.clear();
    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();
    m_batch_cursor = 0;

    for (int i = 0; i < m_source->rowCount(); ++i)
    {
        auto idx = m_source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, m_pending_leaves);
    }

    if (!m_pending_leaves.isEmpty())
        m_batch_timer->start();
}
```

Replace `process_batch()` entirely:

```cpp
void ProductsFlatFilterModel::process_batch()
{
    int generation = m_batch_generation;
    int end = std::min(m_batch_cursor + BATCH_SIZE, static_cast<int>(m_pending_leaves.size()));

    QList<ScoredNode> batch_results;
    for (int i = m_batch_cursor; i < end; ++i)
    {
        if (m_batch_generation != generation)
            return;

        auto& [node, path_text, meta_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;

        QHash<QString, double> raw_signals;
        raw_signals.insert(QStringLiteral("fuzzy"),
                            static_cast<double>(free_text_score(path_text, meta_text)));
        for (const auto& signal_name : m_score_signals.registered_signals())
        {
            auto value = m_score_signals.score_for(signal_name, path_text);
            if (value)
                raw_signals.insert(signal_name, *value);
        }

        m_pending_raw_signals.insert(node, raw_signals);
        for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
            m_pending_signal_maxes[it.key()]
                = std::max(m_pending_signal_maxes.value(it.key(), 0.0), it.value());

        // Provisional merge using the running max-so-far, so results keep
        // streaming in progressively during the scan (as today) -- raw
        // scores >0 always normalize to >0 regardless of which max is
        // used, so a leaf shown here never has to be retracted, only
        // re-ranked, once finalize_batch() recomputes with the true maxes.
        auto merged = merge_scores(raw_signals, m_merge_strategy, m_signal_weights,
                                    m_pending_signal_maxes, m_override_signal);
        if (merged && *merged > 0.0)
        {
            batch_results.append({ node, *merged });
            m_max_score = std::max(m_max_score, *merged);
        }
    }

    if (!batch_results.isEmpty())
    {
        int first = m_results.size();
        beginInsertRows(QModelIndex(), first, first + batch_results.size() - 1);
        m_results.append(batch_results);
        endInsertRows();
    }

    m_batch_cursor = end;

    if (m_batch_cursor >= m_pending_leaves.size())
        finalize_batch();
}

void ProductsFlatFilterModel::finalize_batch()
{
    m_batch_timer->stop();

    m_node_raw_signals = m_pending_raw_signals;
    m_signal_maxes = m_pending_signal_maxes;
    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();

    m_max_score = 0;
    for (auto& scored : m_results)
    {
        auto merged = merge_scores(m_node_raw_signals.value(scored.node), m_merge_strategy,
                                    m_signal_weights, m_signal_maxes, m_override_signal);
        scored.score = merged.value_or(0.0);
        m_max_score = std::max(m_max_score, scored.score);
    }

    sort_and_remap_results();
}

// Re-runs merge_scores() over every leaf that has ever passed
// filters_match() for the current query (m_node_raw_signals is a superset
// of m_results: it includes leaves whose merged score is currently 0), using
// the current strategy/weights/override -- no re-scoring of free text
// needed. Unlike finalize_batch(), the row *set* can genuinely change here
// (e.g. switching to Override can shrink or grow which leaves match), so
// this uses a full model reset rather than the persistent-index-preserving
// sort finalize_batch() uses for its narrower "same rows, new order" case.
void ProductsFlatFilterModel::remerge_committed()
{
    if (m_node_raw_signals.isEmpty())
        return;

    beginResetModel();
    m_results.clear();
    m_max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            m_results.append({ it.key(), *merged });
            m_max_score = std::max(m_max_score, *merged);
        }
    }
    std::sort(m_results.begin(), m_results.end(),
              [](const ScoredNode& a, const ScoredNode& b) { return a.score > b.score; });
    endResetModel();
}

void ProductsFlatFilterModel::sort_and_remap_results()
{
    // Sort by score descending — emit layoutChanged so views update order.
    // The layout-change contract requires remapping persistent indexes
    // (selections taken while batches streamed) or they silently retarget
    // to whatever lands on their old row after the sort.
    emit layoutAboutToBeChanged();
    const QModelIndexList old_indexes = persistentIndexList();
    QList<ProductsModelNode*> old_nodes;
    old_nodes.reserve(old_indexes.size());
    for (const auto& idx : old_indexes)
        old_nodes.append(m_results[idx.row()].node);

    std::sort(m_results.begin(), m_results.end(),
              [](const ScoredNode& a, const ScoredNode& b) { return a.score > b.score; });

    if (!old_indexes.isEmpty())
    {
        QHash<ProductsModelNode*, int> new_rows;
        new_rows.reserve(m_results.size());
        for (int row = 0; row < m_results.size(); ++row)
            new_rows.insert(m_results[row].node, row);
        QModelIndexList new_indexes;
        new_indexes.reserve(old_indexes.size());
        for (qsizetype i = 0; i < old_indexes.size(); ++i)
            new_indexes.append(
                index(new_rows.value(old_nodes[i], -1), old_indexes[i].column()));
        changePersistentIndexList(old_indexes, new_indexes);
    }
    emit layoutChanged();
}
```

`free_text_score()` and `data()`/`ProductsRelevanceScoreRole` stay as they
are — `m_results[...].score` and `m_max_score` are now `double` instead of
`int`, but `qRound(score * 100.0 / m_max_score)` already used `100.0`
(double) and works unchanged with a `double` numerator.

- [ ] **Step 9: Rebuild**

```bash
touch /var/home/jeandet/Documents/prog/SciQLopPlots/SciQLopPlots/bindings/bindings.xml
$VENV/bin/meson setup --reconfigure /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv
$VENV/bin/meson compile -C /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv
```
Expected: builds cleanly (the `--reconfigure` picks up the deleted/renamed
source files).

- [ ] **Step 10: Run tests to verify they pass**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -v`
Expected: PASS, all tests including the new `TestScoreSignalRegistry`,
`TestScoreSignalRegistryConcurrency`, and `TestMergeStrategyConfiguration`
classes, **and** every pre-existing test in this file (in particular
`TestFlatRelevanceScoreRole`'s exact-percentage assertions must pass
unchanged).

- [ ] **Step 11: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add -A -- include/SciQLopPlots/Products/ src/ScoreSignalRegistry.cpp \
        src/ExternalScoreOverlay.cpp src/ProductsFlatFilterModel.cpp \
        SciQLopPlots/meson.build tests/integration/test_products_filter.py
git commit -m "feat(products): generalize ExternalScoreOverlay into a named-signal registry, wire configurable merging into ProductsFlatFilterModel"
```

---

## Task 3: Wire `ScoreSignalRegistry` and configurable merging into `ProductsTreeFilterModel`

**Files:**
- Modify: `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`
- Modify: `src/ProductsTreeFilterModel.cpp`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `ScoreSignalRegistry`, `merge_scores`, `ScoreMergeStrategy` (Tasks
  1-2).
- Produces: same public API shape as `ProductsFlatFilterModel` gained in
  Task 2 (`set_external_scores`, `set_signal_enabled`, `signal_enabled`,
  `registered_signals`, `set_score_merge_strategy`, `score_merge_strategy`,
  `set_signal_weight`, `signal_weight`, `set_override_signal`,
  `override_signal`), on `ProductsTreeFilterModel`.

Task 2's rewritten `TestScoreSignalRegistry`/`TestMergeStrategyConfiguration`
test classes already exercise both models' tree-side behavior (each test
method that says "tree" targets `ProductsTreeFilterModel`) — no further test
file edits are needed for the tree side, since Task 2 wrote them ahead of
time expecting both models to expose the same API. Add three tree-specific
tests Task 2 doesn't cover (tiering interaction with merging):

- [ ] **Step 1: Add tree-specific failing tests**

Append to `tests/integration/test_products_filter.py`, inside a new class:

```python
class TestTreeTieringWithMergedScores:
    def test_max_score_tiers_operates_on_merged_scores(self, qtbot, overlay_test_model):
        """set_max_score_tiers() must rank the *merged* score, not the raw
        fuzzy score, so a bm25-boosted leaf can still win a tier slot."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_max_score_tiers(1)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_changing_strategy_on_tree_reranks_without_new_async_pass(
            self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_signal_enabled("bm25", True)
        fm.set_external_scores("bm25", {path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

        fm.set_score_merge_strategy(ScoreMergeStrategy.Override)
        fm.set_override_signal("bm25")
        # No flush_events() -- must take effect synchronously.
        assert collect_visible_names(fm) == ["acronym_only"]
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -k "TestScoreSignalRegistry or MergeStrategyConfiguration or TieringWithMergedScores" -v`
Expected: FAIL — tree-model tests error because `ProductsTreeFilterModel`
still has the old single-signal API from before Task 2's rename (Task 2
only changed `ProductsFlatFilterModel`).

- [ ] **Step 3: Rewrite `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`**

Change the include (line 25):

```cpp
#include "SciQLopPlots/Products/ScoreSignalRegistry.hpp"
#include "SciQLopPlots/Products/ScoreMerge.hpp"
```

Replace the committed-state member block (roughly lines 44-72) with:

```cpp
    Query m_query;
    int m_max_score_tiers = 2;
    double m_score_cutoff = 0;
    double m_max_score = 0;
    QHash<ProductsModelNode*, double> m_node_scores;
    QSet<double> m_distinct_scores;

    struct CoverageInfo
    {
        int matched = 0;
        int total = 0;
    };
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;

    ScoreSignalRegistry m_score_signals;
    ScoreMergeStrategy m_merge_strategy = ScoreMergeStrategy::Max;
    QHash<QString, double> m_signal_weights;
    QString m_override_signal;

    // Raw per-signal scores from the last completed scoring pass, kept
    // after settling so set_score_merge_strategy()/set_signal_weight()/
    // set_override_signal() -- like set_max_score_tiers() already does --
    // can cheaply re-rank without re-running the DP scorer.
    QHash<ProductsModelNode*, QHash<QString, double>> m_node_raw_signals;
    QHash<QString, double> m_signal_maxes;

    // Pending state: in-flight background scoring for a query that hasn't
    // been committed yet. filterAcceptsRow()/data() never read these --
    // the view keeps showing the previous committed results until
    // finish_scoring() swaps pending into committed in one shot.
    Query m_pending_query;
    QList<ProductsModelNode*> m_pending_leaves;
    QHash<ProductsModelNode*, QHash<QString, double>> m_pending_raw_signals;
    QHash<QString, double> m_pending_signal_maxes;
    int m_batch_cursor = 0;
    int m_batch_generation = 0;
    QTimer* m_batch_timer;
```

Replace the `set_external_scores`/`set_smart_search_enabled`/
`smart_search_enabled` block (lines 87-99) with:

```cpp
    void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores)
    {
        m_score_signals.set_scores(signal_name, scores);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    void set_signal_enabled(const QString& signal_name, bool enabled)
    {
        m_score_signals.set_signal_enabled(signal_name, enabled);
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (!m_batch_timer->isActive())
                    set_query(m_query);
            },
            Qt::QueuedConnection);
    }
    bool signal_enabled(const QString& signal_name) const
    {
        return m_score_signals.signal_enabled(signal_name);
    }
    QStringList registered_signals() const { return m_score_signals.registered_signals(); }

    void set_score_merge_strategy(ScoreMergeStrategy strategy)
    {
        m_merge_strategy = strategy;
        remerge_committed();
    }
    ScoreMergeStrategy score_merge_strategy() const noexcept { return m_merge_strategy; }

    void set_signal_weight(const QString& signal_name, double weight)
    {
        m_signal_weights.insert(signal_name, weight);
        remerge_committed();
    }
    double signal_weight(const QString& signal_name) const
    {
        return m_signal_weights.value(signal_name, 1.0);
    }

    void set_override_signal(const QString& signal_name)
    {
        m_override_signal = signal_name;
        remerge_committed();
    }
    QString override_signal() const { return m_override_signal; }
```

Update the `apply_cutoff_and_coverage` declaration and add `remerge_committed`
in the private section:

```cpp
    void apply_cutoff_and_coverage(const QHash<ProductsModelNode*, double>& scores,
                                    const QSet<double>& distinct_scores, double max_score);
    void remerge_committed();
```

- [ ] **Step 4: Rewrite the scoring pipeline in `src/ProductsTreeFilterModel.cpp`**

Update `start_scoring()`'s pending-state reset:

```cpp
void ProductsTreeFilterModel::start_scoring()
{
    m_batch_timer->stop();
    ++m_batch_generation;

    m_pending_raw_signals.clear();
    m_pending_signal_maxes.clear();
    m_pending_leaves.clear();

    auto* source = sourceModel();
    if (source)
    {
        for (int i = 0; i < source->rowCount(); ++i)
        {
            auto idx = source->index(i, 0);
            auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
            if (node)
                collect_all_leaves(node, m_pending_leaves);
        }
    }

    m_batch_cursor = 0;
    if (m_pending_leaves.isEmpty())
        finish_scoring();
    else
        m_batch_timer->start();
}
```

Replace `process_score_batch()`:

```cpp
void ProductsTreeFilterModel::process_score_batch()
{
    int generation = m_batch_generation;
    int end = std::min(m_batch_cursor + BATCH_SIZE, static_cast<int>(m_pending_leaves.size()));

    for (int i = m_batch_cursor; i < end; ++i)
    {
        if (m_batch_generation != generation)
            return;

        auto* node = m_pending_leaves[i];
        if (!filters_match(node, m_pending_query))
            continue;

        QHash<QString, double> raw_signals;
        raw_signals.insert(QStringLiteral("fuzzy"),
                            static_cast<double>(free_text_score(node, m_pending_query)));
        for (const auto& signal_name : m_score_signals.registered_signals())
        {
            auto value = m_score_signals.score_for(signal_name, node->path().join(' '));
            if (value)
                raw_signals.insert(signal_name, *value);
        }

        m_pending_raw_signals.insert(node, raw_signals);
        for (auto it = raw_signals.constBegin(); it != raw_signals.constEnd(); ++it)
            m_pending_signal_maxes[it.key()]
                = std::max(m_pending_signal_maxes.value(it.key(), 0.0), it.value());
    }

    m_batch_cursor = end;

    if (m_batch_cursor >= m_pending_leaves.size())
    {
        m_batch_timer->stop();
        finish_scoring();
    }
}
```

Replace `finish_scoring()`:

```cpp
void ProductsTreeFilterModel::finish_scoring()
{
    beginFilterChange();
    m_query = m_pending_query;
    m_node_raw_signals = m_pending_raw_signals;
    m_signal_maxes = m_pending_signal_maxes;

    QHash<ProductsModelNode*, double> merged_scores;
    QSet<double> distinct_scores;
    double max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            merged_scores.insert(it.key(), *merged);
            distinct_scores.insert(*merged);
            max_score = std::max(max_score, *merged);
        }
    }

    apply_cutoff_and_coverage(merged_scores, distinct_scores, max_score);
    endFilterChange();
}

// Re-runs merge_scores() over every already-scored leaf's cached raw
// signals, then re-applies the tier cutoff -- no re-scoring of free text
// needed, so this stays synchronous even for a large corpus (same
// rationale as set_max_score_tiers()).
void ProductsTreeFilterModel::remerge_committed()
{
    if (m_node_raw_signals.isEmpty())
        return;

    QHash<ProductsModelNode*, double> merged_scores;
    QSet<double> distinct_scores;
    double max_score = 0;
    for (auto it = m_node_raw_signals.constBegin(); it != m_node_raw_signals.constEnd(); ++it)
    {
        auto merged = merge_scores(it.value(), m_merge_strategy, m_signal_weights, m_signal_maxes,
                                    m_override_signal);
        if (merged && *merged > 0.0)
        {
            merged_scores.insert(it.key(), *merged);
            distinct_scores.insert(*merged);
            max_score = std::max(max_score, *merged);
        }
    }

    beginFilterChange();
    apply_cutoff_and_coverage(merged_scores, distinct_scores, max_score);
    endFilterChange();
}
```

Update `apply_cutoff_and_coverage()`'s signature and the one `int`-typed
local inside it (the body is otherwise unchanged: `QList<int>
sorted_scores` and `std::greater<int>()` become `QList<double>` /
`std::greater<double>()`):

```cpp
void ProductsTreeFilterModel::apply_cutoff_and_coverage(
    const QHash<ProductsModelNode*, double>& scores, const QSet<double>& distinct_scores,
    double max_score)
{
    m_node_scores = scores;
    m_distinct_scores = distinct_scores;
    m_max_score = max_score;
    m_score_cutoff = 0;

    for (auto it = m_coverage.begin(); it != m_coverage.end(); ++it)
        it.value().matched = 0;

    if (distinct_scores.isEmpty())
        return;

    QList<double> sorted_scores = distinct_scores.values();
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<double>());
    int tier_index = std::min(m_max_score_tiers, static_cast<int>(sorted_scores.size())) - 1;
    m_score_cutoff = sorted_scores[tier_index];

    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
    {
        if (it.value() < m_score_cutoff)
            continue;
        for (auto* ancestor = it.key()->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].matched += 1;
    }
}
```

`filterAcceptsRow()` and `data()` need no changes beyond type propagation:
`it.value() >= m_score_cutoff` and `qRound(it.value() * 100.0 /
m_max_score)` both already work identically with `double` operands.

- [ ] **Step 5: Rebuild**

```bash
$VENV/bin/meson compile -C /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -v`
Expected: PASS, all tests, including the new
`TestTreeTieringWithMergedScores` class and every pre-existing
`TestTreeRelevanceScoreRole`/`TestTreeCoverageRole`/`TestTreeScoreTiers`
exact-value assertion.

- [ ] **Step 7: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp \
        src/ProductsTreeFilterModel.cpp tests/integration/test_products_filter.py
git commit -m "feat(products): wire configurable score-signal merging into ProductsTreeFilterModel"
```

---

## Task 4: Full-suite verification and cleanup

**Files:**
- No new files. Verification + memory update only.

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces: nothing new — this task's deliverable is a verified, clean
  repo state.

- [ ] **Step 1: Grep for any leftover references to the old API**

```bash
grep -rn "ExternalScoreOverlay\|smart_search_enabled" /var/home/jeandet/Documents/prog/SciQLopPlots --include=*.py --include=*.cpp --include=*.hpp --include=*.xml | grep -v build
```
Expected: no output (empty). If anything shows up outside `build-venv/`,
fix that call site before continuing.

- [ ] **Step 2: Run the full integration suite**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```
Expected: PASS. Read the actual printed pass count and exit code — it
should be at least 946 (the pre-existing count) plus the new tests added in
Tasks 1-3, and 0 failed/errored.

- [ ] **Step 3: Run the DSP/unit suite too, to catch any unrelated fallout**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit -q
```
Expected: PASS, same count as before this branch started.

- [ ] **Step 4: `meson wrap update` per routine project hygiene**

```bash
$VENV/bin/meson wrap update
```
Rebuild and rerun both suites above afterward if this changed anything; if
nothing was bumped, skip the rebuild.

- [ ] **Step 5: Update project memory**

Update
`/home/jeandet/.claude/projects/-var-home-jeandet-Documents-prog-SciQLopPlots/memory/pluggable_smart_search_feature.md`
(or a new memory file linked from `MEMORY.md`) noting: `ExternalScoreOverlay`
is now `ScoreSignalRegistry` (multi-signal, configurable merge strategy —
Max/Sum/WeightedSum/Override), BM25 itself is not implemented here and
remains SciQLop-side future work per the relocation doc, and the merge
math/normalization approach for anyone building the next signal.

- [ ] **Step 6: Final commit (only if Step 4 changed wrap files)**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add subprojects/*.wrap
git commit -m "chore: bump wrapdb dependencies"
```
