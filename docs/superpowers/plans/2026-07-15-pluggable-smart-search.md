# Pluggable Smart Search Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an opt-in, extensible "smart search" layer to the Products tree/list search — starting with a local semantic-embedding method — so free-text queries like "magnetic field" can surface `mms1/fgm`/`mms1/scm` via description text the existing DP matcher can't reach, without touching the DP matcher itself or requiring any new hard dependency.

**Architecture:** The existing C++ DP subsequence matcher (`ProductsTreeFilterModel`/`ProductsFlatFilterModel`) gains exactly one new concept — an `ExternalScoreOverlay`, a thread-safe `{path_key: score}` map blended via `max()` into the final per-leaf score. Everything pluggable (the `SearchMethod` interface, a registry, weighted-max combination, and the first concrete method — a `fastembed`-backed semantic embedder) lives in new, optional, pure-Python modules under `SciQLopPlots/`. The Python side computes scores on background threads and pushes finished results into the C++ overlay via a lock-protected shared_ptr swap — mirroring the shared_ptr-snapshot pattern already used to fix a prior GIL/mutex deadlock (PR #70) — so C++ never calls back into Python mid-scoring.

**Tech Stack:** C++20/Qt6 (existing filter models), Meson build, Python 3.10-3.14, `fastembed` (ONNX runtime, no torch) as a new `SciQLopPlots[smart_search]` optional extra, `pytest`/`pytest-qt`/`pytest-timeout` (already test dependencies).

## Global Constraints

- Build only via the in-tree `build-venv` recipe (`meson setup build-venv --buildtype=debugoptimized`; never plain `debug`) — see CLAUDE.md.
- Run tests from `/tmp` with `PYTHONPATH=<repo>/build-venv` so the source tree doesn't shadow the compiled build.
- No new required dependency: `fastembed` must only ever appear under `[project.optional-dependencies].smart_search`, never in `[project].dependencies` or the `test` extra.
- With the `smart_search` extra not installed, or the feature disabled, every code path must behave byte-for-byte identically to today — verified by tests, not assumed.
- Reuse the existing leaf-identity convention everywhere: `node.path().join(' ')` (already used by both filter models for DP scoring).
- Concurrency rule (from the PR #70 GIL/mutex deadlock fix — [[gil_lock_ordering_fix]]): a lock guarding data shared between a Python worker thread and the C++ GUI thread must never be held across a call that can block on the GIL or on Qt/the view. Always snapshot-swap under a short-held lock.
- Follow the project's existing flat-Python-module convention (`dsp.py`, `tracing.py`, `pipeline.py`, `properties.py` all sit directly under `SciQLopPlots/`, not in a subpackage) — the Meson build has no precedent for a nested Python subpackage, so this plan uses flat `smart_search_*.py` module names instead of a `smart_search/` directory, which is a pure file-layout simplification and doesn't change the approved design's public behavior.
- TDD throughout: every task's first step is a failing test.

---

### Task 1: `ExternalScoreOverlay` + wiring into both filter models

**Files:**
- Create: `include/SciQLopPlots/Products/ExternalScoreOverlay.hpp`
- Create: `src/ExternalScoreOverlay.cpp`
- Modify: `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`
- Modify: `src/ProductsTreeFilterModel.cpp`
- Modify: `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`
- Modify: `src/ProductsFlatFilterModel.cpp`
- Modify: `SciQLopPlots/meson.build`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Produces: `ExternalScoreOverlay::set_scores(const QHash<QString, QVariant>&)`, `::set_enabled(bool)`, `::enabled() const`, `::score_for(const QString&) const` — thread-safe from any thread.
- Produces: on both `ProductsTreeFilterModel` and `ProductsFlatFilterModel`: `set_external_scores(const QHash<QString, QVariant>&)`, `set_smart_search_enabled(bool)`, `smart_search_enabled() const`. These are the only symbols later tasks depend on.

- [ ] **Step 1: Write the failing tests**

Append to `tests/integration/test_products_filter.py` (after the existing imports, which already include `uuid`, `pytest`, `QCoreApplication`, `Qt`, `ProductsModel`, `ProductsModelNode`, `ProductsModelNodeType`, `ParameterType`, `ProductsTreeFilterModel`, `ProductsFlatFilterModel`, `QueryParser`):

```python
@pytest.fixture
def overlay_test_model(qtbot):
    """One leaf whose own text has zero relation to the probe query, used to
    prove an external score can surface it when nothing in its own text
    would ever match lexically."""
    model = ProductsModel.instance()
    token = uuid.uuid4().hex[:8]
    root = ProductsModelNode(f"ExternalScoreRoot_{token}")
    leaf = ProductsModelNode(
        "acronym_only", "test",
        {"dataset": "xyz", "description": "totally unrelated text"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    root.add_child(leaf)
    model.add_node([], root)
    yield model, leaf


class TestExternalScoreOverlay:
    def test_tree_filter_ignores_external_scores_when_disabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = leaf.path().join(' ')
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

    def test_tree_filter_blends_external_scores_when_enabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = leaf.path().join(' ')
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_flat_filter_blends_external_scores_when_enabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = leaf.path().join(' ')
        fm = ProductsFlatFilterModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_smart_search_enabled_defaults_to_false(self, qtbot):
        fm = ProductsTreeFilterModel()
        assert fm.smart_search_enabled() is False
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py::TestExternalScoreOverlay -v
```
Expected: FAIL — `AttributeError: 'ProductsTreeFilterModel' object has no attribute 'set_external_scores'` (method doesn't exist yet).

- [ ] **Step 3: Create `ExternalScoreOverlay.hpp`**

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
#include <QVariant>
#include <atomic>
#include <memory>

// Holds an optional, externally-supplied per-leaf score overlay (e.g. from a
// Python-side smart-search method) blended into the built-in lexical score.
// Every method is safe to call from any thread: writers build a whole new
// immutable QHash off any lock, then only hold m_mutex long enough to swap a
// shared_ptr; readers copy the shared_ptr under the same short lock and read
// the (now locally-owned) snapshot lock-free. This mirrors the shared_ptr
// snapshot pattern used to fix a prior GIL/mutex deadlock (PR #70) -- the
// lock is never held across a call that could block on the GIL.
class ExternalScoreOverlay
{
public:
    void set_scores(const QHash<QString, QVariant>& scores);
    void set_enabled(bool enabled) noexcept;
    bool enabled() const noexcept;
    double score_for(const QString& path_key) const;

private:
    mutable QMutex m_mutex;
    std::shared_ptr<const QHash<QString, double>> m_scores;
    std::atomic<bool> m_enabled { false };
};
```

- [ ] **Step 4: Create `src/ExternalScoreOverlay.cpp`**

```cpp
#include "SciQLopPlots/Products/ExternalScoreOverlay.hpp"
#include <QMutexLocker>

void ExternalScoreOverlay::set_scores(const QHash<QString, QVariant>& scores)
{
    auto converted = std::make_shared<QHash<QString, double>>();
    converted->reserve(scores.size());
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it)
        converted->insert(it.key(), it.value().toDouble());

    QMutexLocker locker(&m_mutex);
    m_scores = converted;
}

void ExternalScoreOverlay::set_enabled(bool enabled) noexcept
{
    m_enabled.store(enabled, std::memory_order_relaxed);
}

bool ExternalScoreOverlay::enabled() const noexcept
{
    return m_enabled.load(std::memory_order_relaxed);
}

double ExternalScoreOverlay::score_for(const QString& path_key) const
{
    if (!enabled())
        return 0.0;
    std::shared_ptr<const QHash<QString, double>> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        snapshot = m_scores;
    }
    if (!snapshot)
        return 0.0;
    return snapshot->value(path_key, 0.0);
}
```

- [ ] **Step 5: Wire into `ProductsTreeFilterModel.hpp`**

```
old_string:
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>

new_string:
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include "SciQLopPlots/Products/ExternalScoreOverlay.hpp"
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QVariant>
```

```
old_string:
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;

new_string:
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;
    ExternalScoreOverlay m_external_scores;
```

```
old_string:
    void set_max_score_tiers(int max_tiers);
    int max_score_tiers() const noexcept { return m_max_score_tiers; }

new_string:
    void set_max_score_tiers(int max_tiers);
    int max_score_tiers() const noexcept { return m_max_score_tiers; }

    void set_external_scores(const QHash<QString, QVariant>& scores) { m_external_scores.set_scores(scores); }
    void set_smart_search_enabled(bool enabled) { m_external_scores.set_enabled(enabled); }
    bool smart_search_enabled() const noexcept { return m_external_scores.enabled(); }
```

- [ ] **Step 6: Blend the overlay into `ProductsTreeFilterModel::process_score_batch()`**

In `src/ProductsTreeFilterModel.cpp`:

```
old_string:
        auto* node = m_pending_leaves[i];
        if (!filters_match(node, m_pending_query))
            continue;
        int score = free_text_score(node, m_pending_query);
        if (score > 0)
        {
            m_pending_scores.insert(node, score);
            m_pending_distinct_scores.insert(score);
            m_pending_max_score = std::max(m_pending_max_score, score);
        }

new_string:
        auto* node = m_pending_leaves[i];
        if (!filters_match(node, m_pending_query))
            continue;
        int score = free_text_score(node, m_pending_query);
        int external_score = static_cast<int>(m_external_scores.score_for(node->path().join(' ')));
        score = std::max(score, external_score);
        if (score > 0)
        {
            m_pending_scores.insert(node, score);
            m_pending_distinct_scores.insert(score);
            m_pending_max_score = std::max(m_pending_max_score, score);
        }
```

- [ ] **Step 7: Wire into `ProductsFlatFilterModel.hpp`**

```
old_string:
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QAbstractListModel>
#include <QMimeData>
#include <QTimer>

new_string:
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include "SciQLopPlots/Products/ExternalScoreOverlay.hpp"
#include <QAbstractListModel>
#include <QMimeData>
#include <QTimer>
#include <QVariant>
```

```
old_string:
    QList<ScoredNode> m_results;
    int m_max_score = 0;

new_string:
    QList<ScoredNode> m_results;
    int m_max_score = 0;
    ExternalScoreOverlay m_external_scores;
```

```
old_string:
    void set_query(const Query& query);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

new_string:
    void set_query(const Query& query);

    void set_external_scores(const QHash<QString, QVariant>& scores) { m_external_scores.set_scores(scores); }
    void set_smart_search_enabled(bool enabled) { m_external_scores.set_enabled(enabled); }
    bool smart_search_enabled() const noexcept { return m_external_scores.enabled(); }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
```

- [ ] **Step 8: Blend the overlay into `ProductsFlatFilterModel::process_batch()`**

In `src/ProductsFlatFilterModel.cpp`:

```
old_string:
        auto& [node, path_text, meta_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;
        int score = free_text_score(path_text, meta_text);
        if (score > 0)
        {
            batch_results.append({ node, score });
            m_max_score = std::max(m_max_score, score);
        }

new_string:
        auto& [node, path_text, meta_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;
        int score = free_text_score(path_text, meta_text);
        int external_score = static_cast<int>(m_external_scores.score_for(path_text));
        score = std::max(score, external_score);
        if (score > 0)
        {
            batch_results.append({ node, score });
            m_max_score = std::max(m_max_score, score);
        }
```

- [ ] **Step 9: Add the new file to `SciQLopPlots/meson.build`**

Non-moc headers list (`ExternalScoreOverlay` is a plain class, no `Q_OBJECT`, so it goes here, not in `moc_headers`):

```
old_string:
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp']

new_string:
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp',
         project_source_root+'/include/SciQLopPlots/Products/ExternalScoreOverlay.hpp']
```

Sources list:

```
old_string:
            '../src/ProductsModel.cpp',
            '../src/ProductsNode.cpp',
            '../src/QueryParser.cpp',

new_string:
            '../src/ProductsModel.cpp',
            '../src/ProductsNode.cpp',
            '../src/ExternalScoreOverlay.cpp',
            '../src/QueryParser.cpp',
```

- [ ] **Step 10: Build**

```bash
VENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson setup --reconfigure build-venv
$VENV/bin/meson compile -C build-venv
```
Expected: builds cleanly. If shiboken fails to auto-expose the three new methods (unlikely — `set_max_score_tiers`/`max_score_tiers` are plain public methods exposed the same way with no `bindings.xml` entry), add to `SciQLopPlots/bindings/bindings.xml` right after the existing bare declarations:
```xml
<object-type name="ProductsTreeFilterModel" parent-management="yes">
    <modify-function signature="set_external_scores(QHash&lt;QString,QVariant&gt;)"/>
</object-type>
<object-type name="ProductsFlatFilterModel" parent-management="yes">
    <modify-function signature="set_external_scores(QHash&lt;QString,QVariant&gt;)"/>
</object-type>
```
and re-run the reconfigure + compile commands above.

- [ ] **Step 11: Run tests to verify they pass**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py::TestExternalScoreOverlay -v
```
Expected: PASS (4 tests).

- [ ] **Step 12: Commit**

```bash
git add include/SciQLopPlots/Products/ExternalScoreOverlay.hpp src/ExternalScoreOverlay.cpp \
        include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp src/ProductsTreeFilterModel.cpp \
        include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp src/ProductsFlatFilterModel.cpp \
        SciQLopPlots/meson.build tests/integration/test_products_filter.py
git commit -m "feat(products): add thread-safe external score overlay hook

Blends an optional externally-supplied per-leaf score into the existing
lexical score via max(), gated by set_smart_search_enabled(). The overlay
itself is inert (returns 0.0) unless enabled, so existing search behavior
is unchanged by default."
```

---

### Task 2: Concurrency regression test

**Files:**
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `set_external_scores`, `set_smart_search_enabled` from Task 1.

- [ ] **Step 1: Write the test**

Add `import threading` to the top of `tests/integration/test_products_filter.py` (alongside the existing `import uuid`), then append:

```python
class TestExternalScoreOverlayConcurrency:
    @pytest.mark.timeout(10)
    def test_concurrent_set_external_scores_does_not_deadlock(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = leaf.path().join(' ')
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_query(QueryParser.parse("magnetic field"))

        stop = threading.Event()

        def hammer():
            i = 0
            while not stop.is_set():
                fm.set_external_scores({path_key: float(i % 100)})
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
```

- [ ] **Step 2: Run it**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py::TestExternalScoreOverlayConcurrency -v
```
Expected: PASS within the 10s timeout, no hang/crash. This test should already pass given Task 1's implementation (it's a regression test for the concurrency contract that implementation already establishes) — if it hangs, the bug is in Task 1's locking, not in this test; go back and fix `ExternalScoreOverlay` before proceeding.

- [ ] **Step 3: Commit**

```bash
git add tests/integration/test_products_filter.py
git commit -m "test(products): add concurrency regression test for external score overlay

Hammers set_external_scores() from a background thread while the GUI
thread processes events, guarding against the GIL/mutex deadlock class
fixed in PR #70."
```

---

### Task 3: `corpus_snapshot()` + `SmartSearchController` wiring to real models

**Files:**
- Modify: `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`
- Modify: `src/ProductsFlatFilterModel.cpp`
- Modify: `SciQLopPlots/meson.build`
- Create: `SciQLopPlots/smart_search_method.py`
- Create: `SciQLopPlots/smart_search_controller.py`
- Test: `tests/unit/test_smart_search_controller.py`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `set_external_scores`/`set_smart_search_enabled` (Task 1).
- Produces: `ProductsFlatFilterModel::corpus_snapshot() const -> QHash<QString, QString>` (path_key → raw_text, full unfiltered corpus).
- Produces (Python): `NodeSnapshot(path_key: str, raw_text: str)` (NamedTuple); `SearchMethod` protocol with `index(nodes) -> None` / `query(text) -> dict[str, float]`; `SmartSearchController` with `register_method(method, weight=1.0)`, `clear_methods()`, `attach(source_model, corpus_model, targets)`, `set_enabled(bool)`, `on_query_changed(text)`, `combined_scores(text) -> dict[str, float]`.

- [ ] **Step 1: Write the failing unit tests (pure Python, no Qt, no models)**

Create `tests/unit/test_smart_search_controller.py`:

```python
"""Pure-Python tests for SmartSearchController's registration and
score-combination logic -- no Qt, no fastembed, no product model involved."""
from SciQLopPlots.smart_search_controller import SmartSearchController
from SciQLopPlots.smart_search_method import NodeSnapshot


class FakeMethod:
    def __init__(self, scores):
        self._scores = scores
        self.indexed_nodes = None

    def index(self, nodes):
        self.indexed_nodes = list(nodes)

    def query(self, text):
        return dict(self._scores)


def test_single_method_scores_pass_through():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 80.0, "b": 20.0}))
    assert controller.combined_scores("whatever") == {"a": 80.0, "b": 20.0}


def test_two_methods_combine_via_max():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 30.0, "b": 90.0}))
    controller.register_method(FakeMethod({"a": 70.0, "c": 10.0}))
    assert controller.combined_scores("whatever") == {"a": 70.0, "b": 90.0, "c": 10.0}


def test_weighted_combination():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 100.0}), weight=0.5)
    assert controller.combined_scores("whatever") == {"a": 50.0}


def test_no_methods_registered_returns_empty():
    controller = SmartSearchController()
    assert controller.combined_scores("whatever") == {}


def test_clear_methods_removes_all():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 100.0}))
    controller.clear_methods()
    assert controller.combined_scores("whatever") == {}


def test_index_forwards_nodes_to_every_method():
    controller = SmartSearchController()
    m1, m2 = FakeMethod({}), FakeMethod({})
    controller.register_method(m1)
    controller.register_method(m2)
    nodes = [NodeSnapshot("a", "text a"), NodeSnapshot("b", "text b")]
    controller.index(nodes)
    assert m1.indexed_nodes == nodes
    assert m2.indexed_nodes == nodes
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_smart_search_controller.py -v
```
Expected: FAIL — `ModuleNotFoundError: No module named 'SciQLopPlots.smart_search_controller'`.

- [ ] **Step 3: Create `SciQLopPlots/smart_search_method.py`**

```python
"""Pluggable search-method interface for SciQLopPlots' smart search layer.
Deliberately uses plain (path_key, raw_text) tuples rather than a live
ProductsModelNode reference, so index()/query() never need to touch a
Shiboken-wrapped C++ object (and therefore never need the GIL) on the hot
per-query path -- only when the corpus itself is re-snapshotted."""
from typing import NamedTuple, Protocol, Sequence


class NodeSnapshot(NamedTuple):
    path_key: str
    raw_text: str


class SearchMethod(Protocol):
    def index(self, nodes: Sequence[NodeSnapshot]) -> None:
        ...

    def query(self, text: str) -> dict:
        ...
```

- [ ] **Step 4: Create `SciQLopPlots/smart_search_controller.py`**

```python
"""Combines however many SearchMethod instances are registered into a single
per-leaf score overlay via weighted max. Adding a new method later never
requires touching this combination logic -- see docs/superpowers/specs/
2026-07-15-pluggable-smart-search-design.md."""
import threading
from typing import Sequence

from .smart_search_method import NodeSnapshot, SearchMethod


class SmartSearchController:
    def __init__(self):
        self._methods = []
        self._enabled = False
        self._source_model = None
        self._corpus_model = None
        self._targets = []

    def register_method(self, method: SearchMethod, weight: float = 1.0) -> None:
        self._methods.append((method, weight))

    def clear_methods(self) -> None:
        self._methods.clear()

    def attach(self, source_model, corpus_model, targets: Sequence) -> None:
        """source_model: the ProductsModel emitting rowsInserted/rowsRemoved/
        modelReset. corpus_model: any ProductsFlatFilterModel instance, used
        only for its corpus_snapshot() -- not necessarily a visible view.
        targets: filter model instances (tree and/or flat) that receive the
        combined score overlay via set_external_scores()."""
        self._source_model = source_model
        self._corpus_model = corpus_model
        self._targets = list(targets)
        source_model.rowsInserted.connect(self._reindex)
        source_model.rowsRemoved.connect(self._reindex)
        source_model.modelReset.connect(self._reindex)
        self._reindex()

    def set_enabled(self, enabled: bool) -> None:
        self._enabled = enabled
        for target in self._targets:
            target.set_smart_search_enabled(enabled)
        if enabled:
            self._reindex()

    def _reindex(self, *args) -> None:
        if not self._enabled or self._corpus_model is None:
            return
        snapshot = self._corpus_model.corpus_snapshot()
        nodes = [NodeSnapshot(path_key, raw_text) for path_key, raw_text in snapshot.items()]
        threading.Thread(target=self._reindex_worker, args=(nodes,), daemon=True).start()

    def _reindex_worker(self, nodes) -> None:
        for method, _weight in self._methods:
            method.index(nodes)

    def on_query_changed(self, text: str) -> None:
        if not self._enabled:
            return
        threading.Thread(target=self._query_worker, args=(text,), daemon=True).start()

    def _query_worker(self, text: str) -> None:
        combined = self.combined_scores(text)
        for target in self._targets:
            target.set_external_scores(combined)

    def combined_scores(self, text: str) -> dict:
        combined = {}
        for method, weight in self._methods:
            for path_key, score in method.query(text).items():
                weighted = score * weight
                if weighted > combined.get(path_key, 0.0):
                    combined[path_key] = weighted
        return combined
```

- [ ] **Step 5: Add `corpus_snapshot()` to `ProductsFlatFilterModel`**

In `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`:

```
old_string:
    void set_external_scores(const QHash<QString, QVariant>& scores) { m_external_scores.set_scores(scores); }
    void set_smart_search_enabled(bool enabled) { m_external_scores.set_enabled(enabled); }
    bool smart_search_enabled() const noexcept { return m_external_scores.enabled(); }

new_string:
    void set_external_scores(const QHash<QString, QVariant>& scores) { m_external_scores.set_scores(scores); }
    void set_smart_search_enabled(bool enabled) { m_external_scores.set_enabled(enabled); }
    bool smart_search_enabled() const noexcept { return m_external_scores.enabled(); }

    // Full unfiltered corpus, independent of the current query -- used by
    // SmartSearchController (Python) to (re)index a search method without
    // needing to walk ProductsModelNode itself.
    QHash<QString, QString> corpus_snapshot() const;
```

In `src/ProductsFlatFilterModel.cpp`, add this method right after `rebuild()` (reusing the exact same top-level-row traversal `rebuild()` already uses, confirmed against its real body):

```cpp
QHash<QString, QString> ProductsFlatFilterModel::corpus_snapshot() const
{
    QList<LeafEntry> leaves;
    for (int i = 0; i < m_source->rowCount(); ++i)
    {
        auto idx = m_source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, leaves);
    }
    QHash<QString, QString> snapshot;
    snapshot.reserve(leaves.size());
    for (const auto& leaf : leaves)
        snapshot.insert(leaf.path_text, leaf.meta_text);
    return snapshot;
}
```

- [ ] **Step 6: Add the two new Python files to `SciQLopPlots/meson.build`**

```
old_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py']

new_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'smart_search_method.py', 'smart_search_controller.py']
```

- [ ] **Step 7: Build**

```bash
VENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson setup --reconfigure build-venv
$VENV/bin/meson compile -C build-venv
```
Expected: builds cleanly (same auto-exposure fallback as Task 1 Step 10 applies to `corpus_snapshot()` if needed).

- [ ] **Step 8: Run the unit tests to verify they pass**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_smart_search_controller.py -v
```
Expected: PASS (6 tests).

- [ ] **Step 9: Write the failing end-to-end test (fake method, real models)**

Append to `tests/integration/test_products_filter.py`:

```python
class TestSmartSearchControllerEndToEnd:
    def test_controller_surfaces_leaf_via_fake_method(self, qtbot):
        from SciQLopPlots.smart_search_controller import SmartSearchController

        model = ProductsModel.instance()
        token = uuid.uuid4().hex[:8]
        root = ProductsModelNode(f"E2ERoot_{token}")
        leaf = ProductsModelNode(
            "acronym_only", "test", {"description": "totally unrelated text"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        root.add_child(leaf)
        model.add_node([], root)
        path_key = leaf.path().join(' ')

        tree_fm = ProductsTreeFilterModel()
        tree_fm.setSourceModel(model)
        corpus_fm = ProductsFlatFilterModel(model)

        class FakeSemanticMethod:
            def index(self, nodes):
                self.nodes = list(nodes)

            def query(self, text):
                if text != "magnetic field":
                    return {}
                return {n.path_key: 100.0 for n in self.nodes if n.path_key == path_key}

        controller = SmartSearchController()
        controller.register_method(FakeSemanticMethod())
        controller.set_enabled(True)
        controller.attach(model, corpus_fm, [tree_fm])

        tree_fm.set_query(QueryParser.parse("magnetic field"))
        controller.on_query_changed("magnetic field")

        qtbot.waitUntil(lambda: "acronym_only" in collect_visible_names(tree_fm), timeout=5000)
```

- [ ] **Step 10: Run to verify it fails, then passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py::TestSmartSearchControllerEndToEnd -v
```
Expected: passes once Steps 3-8 are in place (this is the integration proof they all fit together — if it fails, check `corpus_snapshot()` binding exposure first, per the Task 1/3 fallback).

- [ ] **Step 11: Commit**

```bash
git add include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp src/ProductsFlatFilterModel.cpp \
        SciQLopPlots/meson.build SciQLopPlots/smart_search_method.py SciQLopPlots/smart_search_controller.py \
        tests/unit/test_smart_search_controller.py tests/integration/test_products_filter.py
git commit -m "feat(products): add SmartSearchController and corpus_snapshot()

Generic, method-count-agnostic registry that combines any number of
SearchMethod implementations via weighted max and pushes the result into
the C++ external score overlay from a background thread. No concrete
search method yet -- proven end-to-end with a fake one."
```

---

### Task 4: `SemanticSearchMethod` (fastembed) + packaging + CI

**Files:**
- Create: `SciQLopPlots/smart_search_semantic.py`
- Modify: `SciQLopPlots/meson.build`
- Modify: `pyproject.toml`
- Modify: `.github/workflows/test.yml`
- Test: `tests/integration/test_smart_search_semantic.py`

**Interfaces:**
- Consumes: `NodeSnapshot`, `SmartSearchController`, `corpus_snapshot()` (Task 3).
- Produces: `SemanticSearchMethod(model_name: str = DEFAULT_MODEL)` implementing `SearchMethod`; `SemanticSearchMethod.model_name` property.

- [ ] **Step 1: Add the `smart_search` extra to `pyproject.toml`**

```
old_string:
[project.optional-dependencies]
test = ["pytest", "pytest-qt", "pytest-timeout", "hypothesis", "python-dateutil", "numpy"]

new_string:
[project.optional-dependencies]
test = ["pytest", "pytest-qt", "pytest-timeout", "hypothesis", "python-dateutil", "numpy"]
smart_search = ["fastembed"]
```

- [ ] **Step 2: Install it into CI so the real model gets exercised**

In `.github/workflows/test.yml`:

```
old_string:
          pip install ".[test]"
          pip install scipy

new_string:
          pip install ".[test,smart_search]"
          pip install scipy
```

- [ ] **Step 3: Write the failing end-to-end test**

Create `tests/integration/test_smart_search_semantic.py`:

```python
"""End-to-end test for the real fastembed-backed SemanticSearchMethod --
requires the `smart_search` extra installed. Downloads the default model on
first run (network access required then), cached locally afterward."""
import uuid

import pytest
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser
)
from SciQLopPlots.smart_search_controller import SmartSearchController
from SciQLopPlots.smart_search_semantic import SemanticSearchMethod
from test_products_filter import collect_visible_names

pytest.importorskip("fastembed")


@pytest.mark.timeout(120)
def test_magnetic_field_query_surfaces_fgm_and_scm_by_description(qtbot):
    model = ProductsModel.instance()
    token = uuid.uuid4().hex[:8]
    root = ProductsModelNode(f"MMS1_{token}")

    fgm = ProductsModelNode(
        "mms1_fgm_b_gse_srvy_l2", "test",
        {"description": "Level2 Flux Gate Magnetometer Combined Fast/Slow "
                         "Survey DC Magnetic Field for MMS Satellite Number 1"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    scm = ProductsModelNode(
        "mms1_scm_acb_gse_scsrvy_srvy_l2", "test",
        {"description": "Level 2 Search Coil Magnetometer AC Magnetic Field "
                         "Survey (32S/s) Data"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    unrelated = ProductsModelNode(
        "mms1_edp_dce_gse_fast_l2", "test",
        {"description": "Level 2 Electric Field Double Probe Fast Survey Data"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    root.add_child(fgm)
    root.add_child(scm)
    root.add_child(unrelated)
    model.add_node([], root)

    tree_fm = ProductsTreeFilterModel()
    tree_fm.setSourceModel(model)
    corpus_fm = ProductsFlatFilterModel(model)

    controller = SmartSearchController()
    controller.register_method(SemanticSearchMethod())
    controller.set_enabled(True)
    controller.attach(model, corpus_fm, [tree_fm])

    tree_fm.set_query(QueryParser.parse("magnetic field"))
    controller.on_query_changed("magnetic field")

    def fgm_and_scm_visible():
        names = collect_visible_names(tree_fm)
        return "mms1_fgm_b_gse_srvy_l2" in names and "mms1_scm_acb_gse_scsrvy_srvy_l2" in names

    qtbot.waitUntil(fgm_and_scm_visible, timeout=60000)
```

- [ ] **Step 4: Run to verify it fails**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_smart_search_semantic.py -v
```
Expected: FAIL — `ModuleNotFoundError: No module named 'SciQLopPlots.smart_search_semantic'`.

- [ ] **Step 5: Install `fastembed` in the dev venv**

```bash
VIRTUAL_ENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv \
  uv pip install fastembed
```

- [ ] **Step 6: Create `SciQLopPlots/smart_search_semantic.py`**

```python
"""Generic, pretrained local text-embedding search method -- never trained on
or aware of this project's product catalog. Requires the optional
SciQLopPlots[smart_search] extra (fastembed) to be installed."""
from typing import Sequence

from .smart_search_method import NodeSnapshot

DEFAULT_MODEL = "BAAI/bge-small-en-v1.5"


class SemanticSearchMethod:
    def __init__(self, model_name: str = DEFAULT_MODEL):
        from fastembed import TextEmbedding

        self._model_name = model_name
        self._model = TextEmbedding(model_name=model_name)
        self._path_keys = []
        self._embeddings = None

    @property
    def model_name(self) -> str:
        return self._model_name

    def index(self, nodes: Sequence[NodeSnapshot]) -> None:
        import numpy as np

        self._path_keys = [n.path_key for n in nodes]
        texts = [n.raw_text for n in nodes]
        if not texts:
            self._embeddings = None
            return
        self._embeddings = np.array(list(self._model.embed(texts)))

    def query(self, text: str) -> dict:
        import numpy as np

        if self._embeddings is None or not self._path_keys:
            return {}
        query_vec = next(self._model.embed([text]))
        norms = np.linalg.norm(self._embeddings, axis=1) * np.linalg.norm(query_vec)
        norms[norms == 0] = 1.0
        cosine = (self._embeddings @ query_vec) / norms
        return {
            path_key: float(max(0.0, sim)) * 100.0
            for path_key, sim in zip(self._path_keys, cosine)
        }
```

- [ ] **Step 7: Add it to `SciQLopPlots/meson.build`**

```
old_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'smart_search_method.py', 'smart_search_controller.py']

new_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'smart_search_method.py', 'smart_search_controller.py', 'smart_search_semantic.py']
```

- [ ] **Step 8: Rebuild**

```bash
VENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
$VENV/bin/meson compile -C build-venv
```

- [ ] **Step 9: Run to verify it passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_smart_search_semantic.py -v
```
Expected: PASS (downloads the ~130MB default model on first run — allow extra time; cached afterward).

- [ ] **Step 10: Commit**

```bash
git add SciQLopPlots/smart_search_semantic.py SciQLopPlots/meson.build pyproject.toml \
        .github/workflows/test.yml tests/integration/test_smart_search_semantic.py
git commit -m "feat(products): add fastembed-backed SemanticSearchMethod

Generic pretrained sentence-embedding method (default: BAAI/bge-small-en-v1.5,
via fastembed/ONNX, no torch), shipped as the opt-in SciQLopPlots[smart_search]
extra. Proves the motivating example end-to-end: a 'magnetic field' query
surfaces FGM/SCM leaves via their description text alone."
```

---

### Task 5: Public API surface + graceful degradation

**Files:**
- Create: `SciQLopPlots/smart_search.py`
- Modify: `SciQLopPlots/meson.build`
- Test: `tests/unit/test_smart_search_public_api.py`

**Interfaces:**
- Consumes: `SmartSearchController` (Task 3), `SemanticSearchMethod` (Task 4).
- Produces (this is the surface SciQLop's settings UI drives): `is_available() -> bool`, `available_models() -> list[str]`, `get_model() -> str`, `set_model(name: str) -> None` (raises `ValueError` on an unknown name), `is_enabled() -> bool`, `set_enabled(enabled: bool, on_ready=None, on_error=None) -> None` (asynchronous — `on_ready()`/`on_error(exc)` are called from a background thread, same cross-thread contract as `SmartSearchController`), `controller() -> SmartSearchController`.

- [ ] **Step 1: Write the failing unit tests**

Create `tests/unit/test_smart_search_public_api.py`:

```python
"""Tests for the public smart_search API surface -- degradation paths in
particular, since these must never let a failure break baseline search."""
import builtins
import threading
import time

import pytest
import SciQLopPlots.smart_search as smart_search


@pytest.fixture(autouse=True)
def reset_smart_search_state():
    smart_search._controller = None
    smart_search._enabled = False
    smart_search._model_name = smart_search.AVAILABLE_MODELS[0]
    yield
    smart_search._controller = None
    smart_search._enabled = False
    smart_search._model_name = smart_search.AVAILABLE_MODELS[0]


def _simulate_fastembed_missing(monkeypatch):
    real_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name == "fastembed":
            raise ImportError("simulated: fastembed not installed")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)


def test_is_available_reflects_fastembed_import(monkeypatch):
    _simulate_fastembed_missing(monkeypatch)
    assert smart_search.is_available() is False


def test_set_enabled_true_without_fastembed_reports_error_and_stays_disabled(monkeypatch):
    _simulate_fastembed_missing(monkeypatch)
    errors = []
    done = threading.Event()

    def on_error(exc):
        errors.append(exc)
        done.set()

    smart_search.set_enabled(True, on_error=on_error)
    assert done.wait(timeout=5)
    assert len(errors) == 1
    assert not smart_search.is_enabled()


def test_set_model_rejects_unknown_name():
    with pytest.raises(ValueError):
        smart_search.set_model("not-a-real-model")


def test_set_enabled_false_is_always_safe_even_when_never_enabled():
    smart_search.set_enabled(False)
    assert not smart_search.is_enabled()


def test_available_models_is_non_empty_and_get_model_defaults_to_first():
    models = smart_search.available_models()
    assert len(models) > 0
    assert smart_search.get_model() == models[0]
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_smart_search_public_api.py -v
```
Expected: FAIL — `ModuleNotFoundError: No module named 'SciQLopPlots.smart_search'`.

- [ ] **Step 3: Create `SciQLopPlots/smart_search.py`**

```python
"""Public API for SciQLop's settings UI to drive the optional smart-search
feature. Safe to import unconditionally -- never imports fastembed at module
import time, only lazily inside is_available()/set_enabled()."""
import threading

from .smart_search_controller import SmartSearchController

AVAILABLE_MODELS = [
    "BAAI/bge-small-en-v1.5",
    "sentence-transformers/all-MiniLM-L6-v2",
]

_controller = None
_model_name = AVAILABLE_MODELS[0]
_enabled = False


def is_available() -> bool:
    try:
        import fastembed  # noqa: F401
    except ImportError:
        return False
    return True


def available_models() -> list:
    return list(AVAILABLE_MODELS)


def get_model() -> str:
    return _model_name


def set_model(name: str) -> None:
    global _model_name
    if name not in AVAILABLE_MODELS:
        raise ValueError(f"Unknown smart-search model: {name!r}. Available: {AVAILABLE_MODELS}")
    _model_name = name
    if _controller is not None and is_enabled():
        _rebuild_controller_method()


def is_enabled() -> bool:
    return _enabled


def controller() -> SmartSearchController:
    global _controller
    if _controller is None:
        _controller = SmartSearchController()
    return _controller


def set_enabled(enabled: bool, on_ready=None, on_error=None) -> None:
    """Asynchronous. Disabling is always synchronous and always succeeds.
    Enabling loads/downloads the model on a background thread; on_ready()/
    on_error(exc) are called from that thread when it finishes -- callers
    driving Qt UI must marshal back to the GUI thread themselves (e.g. via a
    Qt signal), the same cross-thread contract SmartSearchController uses."""
    global _enabled
    if not enabled:
        _enabled = False
        if _controller is not None:
            _controller.set_enabled(False)
        return

    if not is_available():
        if on_error is not None:
            on_error(RuntimeError("fastembed is not installed"))
        return

    def worker():
        global _enabled
        try:
            _rebuild_controller_method()
        except Exception as exc:
            if on_error is not None:
                on_error(exc)
            return
        _enabled = True
        controller().set_enabled(True)
        if on_ready is not None:
            on_ready()

    threading.Thread(target=worker, daemon=True).start()


def _rebuild_controller_method() -> None:
    from .smart_search_semantic import SemanticSearchMethod

    c = controller()
    c.clear_methods()
    c.register_method(SemanticSearchMethod(_model_name))
```

- [ ] **Step 4: Add it to `SciQLopPlots/meson.build`**

```
old_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'smart_search_method.py', 'smart_search_controller.py', 'smart_search_semantic.py']

new_string:
sciqlopplots_python_sources = ['__init__.py', 'event.py', 'pipeline.py', 'properties.py', 'dsp.py', 'tracing.py', 'smart_search_method.py', 'smart_search_controller.py', 'smart_search_semantic.py', 'smart_search.py']
```

- [ ] **Step 5: Rebuild**

```bash
VENV=/var/home/jeandet/Documents/prog/SciQLopPlots/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
$VENV/bin/meson compile -C build-venv
```

- [ ] **Step 6: Run to verify tests pass**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_smart_search_public_api.py -v
```
Expected: PASS (5 tests).

- [ ] **Step 7: Run the full test suite to confirm no regressions**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```
Expected: all tests pass, exit code 0. Read the actual pass/fail count — do not infer success from a partial grep.

- [ ] **Step 8: Commit**

```bash
git add SciQLopPlots/smart_search.py SciQLopPlots/meson.build tests/unit/test_smart_search_public_api.py
git commit -m "feat(products): expose smart_search public API for SciQLop settings

is_available()/is_enabled()/set_enabled()/available_models()/get_model()/
set_model() -- the complete surface SciQLop's settings UI needs to offer
smart search as a toggle. Enabling degrades gracefully (reports an error,
leaves baseline DP search untouched) when fastembed isn't installed or the
model fails to load."
```
