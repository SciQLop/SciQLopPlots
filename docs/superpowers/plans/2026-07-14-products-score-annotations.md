# Products Score Annotations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show coverage-tool-style score badges on Products search results — a
colored relevance-% dot on leaves (tree + list view), a plain matched/total
coverage fraction on folders (tree view only) — with zero visual change when
no free-text query is active.

**Architecture:** Two new custom `Qt::ItemDataRole` values, shared via one
header-only constants file, computed as by-products of scans
`ProductsFlatFilterModel` and `ProductsTreeFilterModel` already perform. A new
`QStyledItemDelegate` subclass reads those roles at paint time and draws the
badge; when neither role is present it paints exactly like the stock
delegate. No new Python-exposed API — the roles are reachable from Python via
`model.data(index, role)` (same as `Qt.DisplayRole` etc.), same as any Qt
role.

**Tech Stack:** C++20, Qt6 (QAbstractListModel, QSortFilterProxyModel,
QStyledItemDelegate), Meson build, PySide6/pytest for verification.

**Design doc:** `docs/superpowers/specs/2026-07-14-products-score-annotations-design.md`
(read it first if anything below is ambiguous — this plan implements it verbatim).

## Global Constraints

- Build: `meson compile -C build-venv` with Qt on `PATH` (see CLAUDE.md's
  "Development Build" recipe) — always `--buildtype=debugoptimized`, never
  plain `debug`.
- Run tests from `/tmp` with `PYTHONPATH=<repo>/build-venv`, using the
  project's dedicated `.venv` — never inside the project dir (source package
  shadows the build).
- TDD: write the failing test first, confirm it fails, then implement.
- Verify the **full** integration suite passes (real pass/fail count and
  exit code, not a partial grep) before considering a task done.
- **No bindings.xml changes are needed for this feature** — every new/changed
  method here overrides a method already exposed transitively via an already-
  bound base class (`QAbstractItemModel::data`), or is a private/internal
  member. If a test behaves as though Python can't see a change that should
  be there, the fallback is `touch SciQLopPlots/bindings/bindings.xml` before
  rebuilding (see [[shiboken_regen_quirk]]) — but don't do this pre-emptively.
- Never push to a remote; commit locally only, per the user's standing
  instruction.

---

### Task 1: Shared role constants + relevance % on `ProductsFlatFilterModel`

**Files:**
- Create: `include/SciQLopPlots/Products/ProductsScoreRoles.hpp`
- Modify: `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`
- Modify: `src/ProductsFlatFilterModel.cpp`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Produces: `constexpr int ProductsRelevanceScoreRole` and
  `constexpr int ProductsCoverageRole`, in `ProductsScoreRoles.hpp` — every
  later task includes this header rather than redefining the numbers.
  Python-side, these are just `Qt.UserRole + 10` / `Qt.UserRole + 11` (no
  binding needed — confirm the exact offsets match if you ever change the
  header, the two must stay in sync).
- Produces: `ProductsFlatFilterModel::data(index, ProductsRelevanceScoreRole)`
  → `int` (0–100) when a free-text query is active and has results, else an
  invalid `QVariant` (`None` in Python).

- [ ] **Step 1: Write the failing test**

Open `tests/integration/test_products_filter.py`. Find the existing
`three_tier_model` fixture (leaves `mag_fld_leaf`=score 14, `field_data`=score
10, `unrelated_leaf`=score 6 for query `"mag fld"` — already used by
`TestTreeScoreTiers`). Add the `Qt` import to the existing PySide6.QtCore
import line, then add this test class right after the `TestTreeScoreTiers`
class (before `TestFoldersNeverMatchOnTheirOwnText`):

```python
# add Qt to this existing import line at the top of the file:
# from PySide6.QtCore import QCoreApplication
from PySide6.QtCore import QCoreApplication, Qt


class TestFlatRelevanceScoreRole:
    """Relevance % is relative to the best match in the current result set,
    not an absolute score (see product_search_scoring_fixes memory — raw
    scores are a coarse, query/corpus-dependent scale)."""

    RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole

    def test_relevance_relative_to_best_match(self, qtbot, three_tier_model):
        fm = ProductsFlatFilterModel(three_tier_model)
        fm.set_query(QueryParser.parse("mag fld"))
        flush_events()
        scores = {}
        for i in range(fm.rowCount()):
            idx = fm.index(i, 0)
            scores[fm.data(idx)] = fm.data(idx, self.RELEVANCE_ROLE)
        assert scores["mag_fld_leaf"] == 100
        assert scores["field_data"] == 71
        assert scores["unrelated_leaf"] == 43

    def test_no_relevance_role_without_free_text(self, qtbot, three_tier_model):
        fm = ProductsFlatFilterModel(three_tier_model)
        fm.set_query(QueryParser.parse("provider:test"))
        flush_events()
        assert fm.rowCount() == 3
        for i in range(fm.rowCount()):
            assert fm.data(fm.index(i, 0), self.RELEVANCE_ROLE) is None
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py \
  -k TestFlatRelevanceScoreRole -q
```
Expected: 2 failed (role returns `None`/wrong value — `ProductsRelevanceScoreRole` doesn't exist yet).

- [ ] **Step 3: Create the shared roles header**

`include/SciQLopPlots/Products/ProductsScoreRoles.hpp`:
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
#include <Qt>

// Shared custom item-data roles for the Products search UI's score
// annotations, so ProductsFlatFilterModel, ProductsTreeFilterModel, and
// ProductsScoreDelegate all agree on the same role numbers.
constexpr int ProductsRelevanceScoreRole = Qt::UserRole + 10;
constexpr int ProductsCoverageRole = Qt::UserRole + 11;
```

- [ ] **Step 4: Add the member and include to `ProductsFlatFilterModel.hpp`**

In `include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp`, add the
include after the existing `QueryParser.hpp` include:
```cpp
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
```
And add a new private member right after `QList<ScoredNode> m_results;`:
```cpp
    QList<ScoredNode> m_results;
    int m_max_score = 0;
```

- [ ] **Step 5: Update `rebuild()`, `process_batch()`, and `data()` in `ProductsFlatFilterModel.cpp`**

In `rebuild()`, reset the new member alongside the existing reset (find
`m_results.clear();` inside `beginResetModel()`/`endResetModel()`):
```cpp
    beginResetModel();
    m_results.clear();
    m_max_score = 0;
    endResetModel();
```

In `process_batch()`, find:
```cpp
        auto& [node, full_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;
        int score = free_text_score(full_text);
        if (score > 0)
            batch_results.append({ node, score });
```
Change the last two lines to also track the running max:
```cpp
        auto& [node, full_text] = m_pending_leaves[i];
        if (!filters_match(node))
            continue;
        int score = free_text_score(full_text);
        if (score > 0)
        {
            batch_results.append({ node, score });
            m_max_score = std::max(m_max_score, score);
        }
```

In `data()`, find:
```cpp
QVariant ProductsFlatFilterModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};

    auto* node = m_results[index.row()].node;

    switch (role)
    {
```
Insert a new branch right before the `switch`:
```cpp
QVariant ProductsFlatFilterModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_results.size())
        return {};

    auto* node = m_results[index.row()].node;

    if (role == ProductsRelevanceScoreRole)
    {
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        return qRound(m_results[index.row()].score * 100.0 / m_max_score);
    }

    switch (role)
    {
```

- [ ] **Step 6: Rebuild**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
```
Expected: clean build, no errors (warnings about unrelated pre-existing
`[[nodiscard]]` results are pre-existing noise, ignore them).

- [ ] **Step 7: Run test to verify it passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py \
  -k TestFlatRelevanceScoreRole -q
```
Expected: `2 passed`.

If Python can't see `ProductsRelevanceScoreRole`-driven behavior (e.g. the
role always returns `None` even where it shouldn't), this is plain data
flowing through an already-bound method, not a new binding — check your C++
logic first. Only as a last resort, try
`touch SciQLopPlots/bindings/bindings.xml` then repeat Step 6.

- [ ] **Step 8: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ProductsScoreRoles.hpp \
        include/SciQLopPlots/Products/ProductsFlatFilterModel.hpp \
        src/ProductsFlatFilterModel.cpp \
        tests/integration/test_products_filter.py
git commit -m "feat(products): relevance % role on ProductsFlatFilterModel"
```

---

### Task 2: Relevance % on `ProductsTreeFilterModel`

**Files:**
- Modify: `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`
- Modify: `src/ProductsTreeFilterModel.cpp`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `ProductsRelevanceScoreRole` from `ProductsScoreRoles.hpp` (Task 1).
- Produces: `ProductsTreeFilterModel::data(index, role)` override — for
  `ProductsRelevanceScoreRole`, same semantics as the flat model (Task 1),
  `PARAMETER` leaves only; falls back to `QSortFilterProxyModel::data(index,
  role)` for every other role, so `DisplayRole`/`DecorationRole`/etc. are
  unaffected. Also produces `collect_visible_scores(model, role, parent=None)`
  test helper for reuse in Task 3.

- [ ] **Step 1: Write the failing test**

In `tests/integration/test_products_filter.py`, add this helper next to the
existing `collect_visible_names` helper:
```python
def collect_visible_scores(model, role, parent=None):
    from PySide6.QtCore import QModelIndex
    if parent is None:
        parent = QModelIndex()
    result = {}
    for row in range(model.rowCount(parent)):
        idx = model.index(row, 0, parent)
        result[model.data(idx, Qt.DisplayRole)] = model.data(idx, role)
        result.update(collect_visible_scores(model, role, idx))
    return result
```

Then add this test class after `TestFlatRelevanceScoreRole`:
```python
class TestTreeRelevanceScoreRole:
    RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole

    def test_relevance_relative_to_best_match(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)  # show all three leaves for this check
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["mag_fld_leaf"] == 100
        assert scores["field_data"] == 71
        assert scores["unrelated_leaf"] == 43

    def test_folders_have_no_relevance_role(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["Instrument"] is None
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py \
  -k TestTreeRelevanceScoreRole -q
```
Expected: 2 failed (`ProductsTreeFilterModel` has no `data()` override yet,
so the role is never handled — returns whatever the base class gives, not
the expected values).

- [ ] **Step 3: Add the member, include, and `data()` declaration**

In `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`, add the
include after the existing `QueryParser.hpp` include:
```cpp
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
```
Add the new member after `int m_score_cutoff = 0;`:
```cpp
    int m_score_cutoff = 0;
    int m_max_score = 0;
```
Add the `data()` override declaration in the `public:` section, right after
`int max_score_tiers() const noexcept { return m_max_score_tiers; }`:
```cpp
    int max_score_tiers() const noexcept { return m_max_score_tiers; }

    QVariant data(const QModelIndex& index, int role) const override;

    void setSourceModel(QAbstractItemModel* source_model) override;
```

- [ ] **Step 4: Extend `recompute_score_cutoff()` and add `data()` in `ProductsTreeFilterModel.cpp`**

In `recompute_score_cutoff()`, find:
```cpp
void ProductsTreeFilterModel::recompute_score_cutoff()
{
    m_score_cutoff = 0;

    auto* source = sourceModel();
```
Change to also reset `m_max_score`:
```cpp
void ProductsTreeFilterModel::recompute_score_cutoff()
{
    m_score_cutoff = 0;
    m_max_score = 0;

    auto* source = sourceModel();
```

Further down in the same function, find:
```cpp
    QSet<int> distinct_scores;
    for (auto* node : leaves)
    {
        if (!filters_match(node))
            continue;
        int score = free_text_score(node);
        if (score > 0)
            distinct_scores.insert(score);
    }
```
Change to also track the max:
```cpp
    QSet<int> distinct_scores;
    for (auto* node : leaves)
    {
        if (!filters_match(node))
            continue;
        int score = free_text_score(node);
        if (score > 0)
        {
            distinct_scores.insert(score);
            m_max_score = std::max(m_max_score, score);
        }
    }
```

Add the new `data()` method anywhere in the file (e.g. right after
`filterAcceptsRow`):
```cpp
QVariant ProductsTreeFilterModel::data(const QModelIndex& index, int role) const
{
    if (role == ProductsRelevanceScoreRole)
    {
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() != ProductsModelNodeType::PARAMETER)
            return {};
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        int score = free_text_score(node);
        if (score <= 0)
            return {};
        return qRound(score * 100.0 / m_max_score);
    }
    return QSortFilterProxyModel::data(index, role);
}
```

- [ ] **Step 5: Rebuild**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
```
Expected: clean build.

- [ ] **Step 6: Run test to verify it passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py \
  -k "TestTreeRelevanceScoreRole or TestFlatRelevanceScoreRole" -q
```
Expected: `4 passed`.

- [ ] **Step 7: Run the full products filter test file to check for regressions**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -q
```
Expected: all pass, 0 failed (should be 33: the 28 pre-existing plus 2 from
Task 1 plus 2 from Task 2, plus the new helper doesn't add a test itself —
if the count looks off, read the actual output rather than assuming).

- [ ] **Step 8: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp \
        src/ProductsTreeFilterModel.cpp \
        tests/integration/test_products_filter.py
git commit -m "feat(products): relevance % role on ProductsTreeFilterModel"
```

---

### Task 3: Coverage fraction on folders in `ProductsTreeFilterModel`

**Files:**
- Modify: `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`
- Modify: `src/ProductsTreeFilterModel.cpp`
- Test: `tests/integration/test_products_filter.py`

**Interfaces:**
- Consumes: `ProductsCoverageRole` from `ProductsScoreRoles.hpp` (Task 1);
  `collect_visible_scores` test helper (Task 2).
- Produces: `ProductsTreeFilterModel::data(index, ProductsCoverageRole)` →
  `QString` like `"2/3 (67%)"` for non-`PARAMETER` (folder) nodes; invalid
  `QVariant` for `PARAMETER` leaves or a folder with zero total descendants.

- [ ] **Step 1: Write the failing test**

Add this test class after `TestTreeRelevanceScoreRole`:
```python
class TestTreeCoverageRole:
    COVERAGE_ROLE = Qt.UserRole + 11  # ProductsCoverageRole

    def test_folder_coverage_fraction(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        # default max_score_tiers=2: only mag_fld_leaf(14) and field_data(10)
        # pass the cutoff, unrelated_leaf(6) doesn't -> 2 of 3 total match.
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores["Instrument"] == "2/3 (67%)"

    def test_leaves_have_no_coverage_role(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores["mag_fld_leaf"] is None

    def test_total_count_updates_when_new_leaf_added(self, qtbot, three_tier_model):
        """The `total` half of the coverage fraction must react to structural
        changes on the source model, not just to set_query() -- this is what
        the on_source_structure_changed rewiring in Step 4 is actually for."""
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)  # keep every positive match visible
        fm.set_query(QueryParser.parse("mag fld"))
        scores_before = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores_before["Instrument"] == "3/3 (100%)"

        new_branch = ProductsModelNode("NewBranch")
        new_leaf = ProductsModelNode(
            "brand_new_leaf", "test", {"description": "nothing related"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        new_branch.add_child(new_leaf)
        three_tier_model.add_node(["Instrument"], new_branch)
        flush_events()

        scores_after = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores_after["Instrument"] == "3/4 (75%)"
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py \
  -k TestTreeCoverageRole -q
```
Expected: `test_folder_coverage_fraction` and
`test_total_count_updates_when_new_leaf_added` both FAIL (`scores["Instrument"]`
is `None`, not `"2/3 (67%)"`/`"3/3 (100%)"` — the role isn't handled yet, so
every lookup returns `None`); `test_leaves_have_no_coverage_role` PASSES
trivially for the same reason (that's fine, it'll stay green through the
implementation too — added for completeness/regression protection).

- [ ] **Step 3: Add the coverage members and declarations**

In `include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp`, add the
`QHash` include after the existing includes:
```cpp
#include "SciQLopPlots/Products/QueryParser.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QHash>
#include <QSortFilterProxyModel>
```
Add the `CoverageInfo` struct and member after `int m_max_score = 0;`:
```cpp
    int m_max_score = 0;

    struct CoverageInfo
    {
        int matched = 0;
        int total = 0;
    };
    QHash<ProductsModelNode*, CoverageInfo> m_coverage;
```
Add the two new private method declarations after `void recompute_score_cutoff();`:
```cpp
    void recompute_score_cutoff();
    void recompute_total_leaf_counts();
    void on_source_structure_changed();
```

- [ ] **Step 4: Wire structural-change signals to the new combined slot**

In `src/ProductsTreeFilterModel.cpp`, find `setSourceModel`:
```cpp
void ProductsTreeFilterModel::setSourceModel(QAbstractItemModel* source_model)
{
    if (auto* old = sourceModel())
        disconnect(old, nullptr, this, nullptr);

    QSortFilterProxyModel::setSourceModel(source_model);

    if (source_model)
    {
        connect(source_model, &QAbstractItemModel::rowsInserted, this,
                &ProductsTreeFilterModel::recompute_score_cutoff);
        connect(source_model, &QAbstractItemModel::rowsRemoved, this,
                &ProductsTreeFilterModel::recompute_score_cutoff);
        connect(source_model, &QAbstractItemModel::modelReset, this,
                &ProductsTreeFilterModel::recompute_score_cutoff);
    }
    recompute_score_cutoff();
}
```
Replace it with (the three connections and the final call now go through the
new wrapper, which recomputes totals — a structural property — before
re-deriving the query-dependent tier cutoff and matched counts):
```cpp
void ProductsTreeFilterModel::setSourceModel(QAbstractItemModel* source_model)
{
    if (auto* old = sourceModel())
        disconnect(old, nullptr, this, nullptr);

    QSortFilterProxyModel::setSourceModel(source_model);

    if (source_model)
    {
        connect(source_model, &QAbstractItemModel::rowsInserted, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
        connect(source_model, &QAbstractItemModel::rowsRemoved, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
        connect(source_model, &QAbstractItemModel::modelReset, this,
                &ProductsTreeFilterModel::on_source_structure_changed);
    }
    on_source_structure_changed();
}

void ProductsTreeFilterModel::on_source_structure_changed()
{
    recompute_total_leaf_counts();
    recompute_score_cutoff();
}

void ProductsTreeFilterModel::recompute_total_leaf_counts()
{
    m_coverage.clear();

    auto* source = sourceModel();
    if (!source)
        return;

    QList<ProductsModelNode*> leaves;
    for (int i = 0; i < source->rowCount(); ++i)
    {
        auto idx = source->index(i, 0);
        auto* node = static_cast<ProductsModelNode*>(idx.internalPointer());
        if (node)
            collect_all_leaves(node, leaves);
    }

    for (auto* node : leaves)
        for (auto* ancestor = node->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].total += 1;
}
```

- [ ] **Step 5: Tally matched counts in `recompute_score_cutoff()`**

In `src/ProductsTreeFilterModel.cpp`, find the start of `recompute_score_cutoff()`:
```cpp
void ProductsTreeFilterModel::recompute_score_cutoff()
{
    m_score_cutoff = 0;
    m_max_score = 0;

    auto* source = sourceModel();
```
Add a reset of the matched counts (keeping `total` untouched) right after:
```cpp
void ProductsTreeFilterModel::recompute_score_cutoff()
{
    m_score_cutoff = 0;
    m_max_score = 0;

    for (auto it = m_coverage.begin(); it != m_coverage.end(); ++it)
        it.value().matched = 0;

    auto* source = sourceModel();
```
Find the end of the function:
```cpp
    QList<int> sorted_scores = distinct_scores.values();
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<int>());
    int tier_index = std::min(m_max_score_tiers, static_cast<int>(sorted_scores.size())) - 1;
    m_score_cutoff = sorted_scores[tier_index];
}
```
Add the matched-tallying pass right before the closing brace:
```cpp
    QList<int> sorted_scores = distinct_scores.values();
    std::sort(sorted_scores.begin(), sorted_scores.end(), std::greater<int>());
    int tier_index = std::min(m_max_score_tiers, static_cast<int>(sorted_scores.size())) - 1;
    m_score_cutoff = sorted_scores[tier_index];

    for (auto* node : leaves)
    {
        if (!node_matches(node))
            continue;
        for (auto* ancestor = node->parent_node(); ancestor != nullptr;
             ancestor = ancestor->parent_node())
            m_coverage[ancestor].matched += 1;
    }
}
```

- [ ] **Step 6: Handle `ProductsCoverageRole` in `data()`**

Find the `data()` method added in Task 2:
```cpp
QVariant ProductsTreeFilterModel::data(const QModelIndex& index, int role) const
{
    if (role == ProductsRelevanceScoreRole)
    {
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() != ProductsModelNodeType::PARAMETER)
            return {};
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        int score = free_text_score(node);
        if (score <= 0)
            return {};
        return qRound(score * 100.0 / m_max_score);
    }
    return QSortFilterProxyModel::data(index, role);
}
```
Insert a `ProductsCoverageRole` branch before the final `return`:
```cpp
QVariant ProductsTreeFilterModel::data(const QModelIndex& index, int role) const
{
    if (role == ProductsRelevanceScoreRole)
    {
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() != ProductsModelNodeType::PARAMETER)
            return {};
        if (m_query.free_text_tokens.isEmpty() || m_max_score <= 0)
            return {};
        int score = free_text_score(node);
        if (score <= 0)
            return {};
        return qRound(score * 100.0 / m_max_score);
    }
    if (role == ProductsCoverageRole)
    {
        auto* node = static_cast<ProductsModelNode*>(mapToSource(index).internalPointer());
        if (!node || node->node_type() == ProductsModelNodeType::PARAMETER)
            return {};
        auto it = m_coverage.constFind(node);
        if (it == m_coverage.constEnd() || it.value().total == 0)
            return {};
        int matched = it.value().matched;
        int total = it.value().total;
        return QString("%1/%2 (%3%)")
            .arg(matched).arg(total).arg(qRound(matched * 100.0 / total));
    }
    return QSortFilterProxyModel::data(index, role);
}
```

- [ ] **Step 7: Rebuild**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
```
Expected: clean build.

- [ ] **Step 8: Run test to verify it passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_filter.py -q
```
Expected: all pass (36 total: 33 from Task 2's end state + 3 new). Read the
actual reported count rather than assuming.

- [ ] **Step 9: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ProductsTreeFilterModel.hpp \
        src/ProductsTreeFilterModel.cpp \
        tests/integration/test_products_filter.py
git commit -m "feat(products): coverage fraction role on folders in ProductsTreeFilterModel"
```

---

### Task 4: `ProductsScoreDelegate` + wire into `ProductsView`

**Files:**
- Create: `include/SciQLopPlots/Products/ProductsScoreDelegate.hpp`
- Create: `src/ProductsScoreDelegate.cpp`
- Modify: `include/SciQLopPlots/Products/ProductsView.hpp`
- Modify: `src/ProductsView.cpp`
- Modify: `SciQLopPlots/meson.build` (add new `.cpp` to `sources`)
- Test: new file `tests/integration/test_products_score_delegate.py`

**Interfaces:**
- Consumes: `ProductsRelevanceScoreRole`, `ProductsCoverageRole` from
  `ProductsScoreRoles.hpp` (Task 1); `ProductsView`'s existing private
  `m_tree_view` / `m_list_view` members (reached from tests via
  `view.findChild(QTreeView)` / `view.findChild(QListView)`, same pattern
  already used by `tests/integration/test_search_bar_help.py` and
  `test_products_model_consistency.py` for other private widgets).
- Produces: `ProductsScoreDelegate` (no Python exposure — pure internal
  rendering detail, not added to `bindings.xml`). `ProductsView` gains a
  private `ProductsScoreDelegate* m_score_delegate` member, assigned to both
  `m_tree_view` and `m_list_view` via `setItemDelegate`.

- [ ] **Step 1: Write the failing test**

Create `tests/integration/test_products_score_delegate.py`:
```python
"""ProductsScoreDelegate: colored relevance badges must actually render."""
import pytest
from PySide6.QtCore import Qt
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QListView, QTextEdit

from SciQLopPlots import (
    ParameterType, ProductsModel, ProductsModelNode, ProductsModelNodeType,
    ProductsView,
)


def _distinct_colors(pixmap):
    image = pixmap.toImage().convertToFormat(QImage.Format_ARGB32)
    w, h = image.width(), image.height()
    step = max(1, min(w, h) // 100)
    colors = set()
    for y in range(0, h, step):
        for x in range(0, w, step):
            c = image.pixelColor(x, y)
            colors.add((c.red(), c.green(), c.blue(), c.alpha()))
    return len(colors)


class TestScoreDelegateRendersBadges:
    def test_relevance_badges_add_color_variety(self, qtbot):
        provider = "delegate_test_provider"
        top = ProductsModelNode(
            "mag_fld_leaf", provider, {"description": "clean match"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        mid = ProductsModelNode(
            "field_data", provider,
            {"description": "has magnetic and field words scattered"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        low = ProductsModelNode(
            "unrelated_leaf", provider,
            {"description": "a very long unrelated description that happens to "
                             "contain the letters m a g somewhere and f l d "
                             "somewhere else too purely by accident padded padded "
                             "padded padded padded padded padded padded"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        root = ProductsModelNode("ScoreDelegateRoot")
        root.add_child(top)
        root.add_child(mid)
        root.add_child(low)
        ProductsModel.instance().add_node([], root)

        view = ProductsView()
        qtbot.addWidget(view)
        view.resize(400, 300)
        view.show()
        qtbot.waitExposed(view)

        bar = view.findChild(QTextEdit)
        list_view = view.findChild(QListView)
        assert bar is not None
        assert list_view is not None

        # Filters-only query: has_query is True (so the list view is shown)
        # but there are no free-text tokens, so RelevanceScoreRole is
        # deliberately absent for every row (see product_search_scoring_fixes
        # / the design's edge cases) -- a same-view, no-badge baseline.
        bar.setPlainText(f"provider:{provider}")
        qtbot.waitUntil(lambda: list_view.model().rowCount() == 3, timeout=5000)
        qtbot.wait(100)
        baseline_colors = _distinct_colors(list_view.grab())

        bar.setPlainText("mag fld")
        qtbot.waitUntil(lambda: list_view.model().rowCount() == 3, timeout=5000)
        qtbot.wait(100)
        colored_colors = _distinct_colors(list_view.grab())

        assert colored_colors > baseline_colors
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_score_delegate.py -q
```
Expected: FAIL — `colored_colors > baseline_colors` is false (no delegate
exists yet, so both renders look the same).

- [ ] **Step 3: Create the delegate header**

`include/SciQLopPlots/Products/ProductsScoreDelegate.hpp`:
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
#include <QStyledItemDelegate>

// Paints the score-annotation badges (relevance % dot on leaves, coverage
// fraction on folders) read from ProductsRelevanceScoreRole /
// ProductsCoverageRole. When neither role is present on a given index (e.g.
// no free-text query active), paints exactly like a stock
// QStyledItemDelegate -- no Q_OBJECT needed, this class adds no new
// signals/slots/properties.
class ProductsScoreDelegate : public QStyledItemDelegate
{
public:
    explicit ProductsScoreDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};
```

- [ ] **Step 4: Create the delegate implementation**

`src/ProductsScoreDelegate.cpp`:
```cpp
#include "SciQLopPlots/Products/ProductsScoreDelegate.hpp"
#include "SciQLopPlots/Products/ProductsScoreRoles.hpp"
#include <QFontMetrics>
#include <QPainter>

ProductsScoreDelegate::ProductsScoreDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

void ProductsScoreDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QVariant relevance = index.data(ProductsRelevanceScoreRole);
    QVariant coverage = index.data(ProductsCoverageRole);
    if (!relevance.isValid() && !coverage.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const int right_margin = 8;
    QString text;
    bool draw_dot = false;
    QColor dot_color;

    if (relevance.isValid())
    {
        int percent = relevance.toInt();
        // Continuous green (100%) -> red (0%) gradient in HSV hue space --
        // not fixed traffic-light bands, since this % is relative-to-best,
        // not an absolute pass/fail cutoff.
        dot_color = QColor::fromHsv(int(120 * percent / 100.0), 200, 200);
        text = QString("%1%").arg(percent);
        draw_dot = true;
    }
    else
    {
        text = coverage.toString();
    }

    QFontMetrics fm(option.font);
    int text_width = fm.horizontalAdvance(text);
    QRect text_rect(option.rect.right() - right_margin - text_width, option.rect.top(),
                     text_width, option.rect.height());

    if (draw_dot)
    {
        int dot_diameter = option.rect.height() / 3;
        QRect dot_rect(text_rect.left() - dot_diameter - 4,
                        option.rect.top() + (option.rect.height() - dot_diameter) / 2,
                        dot_diameter, dot_diameter);
        painter->setBrush(dot_color);
        painter->setPen(Qt::NoPen);
        painter->drawEllipse(dot_rect);
    }

    painter->setPen(option.palette.color(QPalette::Text));
    painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignRight, text);

    painter->restore();
}
```

- [ ] **Step 5: Wire the delegate into `ProductsView`**

In `include/SciQLopPlots/Products/ProductsView.hpp`, add a forward
declaration after `class ProductsFlatFilterModel;`:
```cpp
class ProductsFlatFilterModel;
class ProductsScoreDelegate;
struct Query;
```
Add the new member after `ProductsFlatFilterModel* m_flat_filter;`:
```cpp
    ProductsFlatFilterModel* m_flat_filter;
    ProductsScoreDelegate* m_score_delegate;
```

In `src/ProductsView.cpp`, add the include after the existing
`ProductsTreeFilterModel.hpp` include:
```cpp
#include "SciQLopPlots/Products/ProductsTreeFilterModel.hpp"
#include "SciQLopPlots/Products/ProductsScoreDelegate.hpp"
```
Find:
```cpp
    m_list_view = new QListView(this);
    m_list_view->setModel(m_flat_filter);
    m_list_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list_view->setAlternatingRowColors(true);
    m_list_view->setDragEnabled(true);
    m_stack->addWidget(m_list_view);
```
Add the delegate construction and assignment right after:
```cpp
    m_list_view = new QListView(this);
    m_list_view->setModel(m_flat_filter);
    m_list_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list_view->setAlternatingRowColors(true);
    m_list_view->setDragEnabled(true);
    m_stack->addWidget(m_list_view);

    m_score_delegate = new ProductsScoreDelegate(this);
    m_tree_view->setItemDelegate(m_score_delegate);
    m_list_view->setItemDelegate(m_score_delegate);
```

- [ ] **Step 6: Add the new source file to the Meson build**

In `SciQLopPlots/meson.build`, find:
```
            '../src/ProductsView.cpp',
            '../src/icons.cpp'
```
Change to:
```
            '../src/ProductsView.cpp',
            '../src/ProductsScoreDelegate.cpp',
            '../src/icons.cpp'
```

- [ ] **Step 7: Rebuild**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
```
Expected: clean build. If Meson doesn't pick up the new source file, run
`$VENV/bin/meson setup --reconfigure build-venv` first.

- [ ] **Step 8: Run test to verify it passes**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_products_score_delegate.py -q
```
Expected: `1 passed`.

- [ ] **Step 9: Run the full integration suite**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q
```
Expected: all pass (read the actual reported count and exit code — should be
817: 809 from before this feature + 7 from Tasks 1–3 + 1 from Task 4).

- [ ] **Step 10: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Products/ProductsScoreDelegate.hpp \
        src/ProductsScoreDelegate.cpp \
        include/SciQLopPlots/Products/ProductsView.hpp \
        src/ProductsView.cpp \
        SciQLopPlots/meson.build \
        tests/integration/test_products_score_delegate.py
git commit -m "feat(products): render score badges via ProductsScoreDelegate"
```

---

## After all tasks: update memory

Once all four tasks are committed and the full suite is green, update
[[product_search_scoring_fixes]] (or add a new linked memory entry) noting
this UI feature shipped, and update
`/home/jeandet/.claude/projects/-var-home-jeandet-Documents-prog-SciQLopPlots/memory/MEMORY.md`'s
index line accordingly. Do not push to any remote unless explicitly asked.
