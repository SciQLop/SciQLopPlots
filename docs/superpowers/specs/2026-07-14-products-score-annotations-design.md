# Products search: score annotations on tree/list rows

Date: 2026-07-14
Status: Approved (design)

## Problem

Following the free-text search scoring fixes ([[product_search_scoring_fixes]] —
best-alignment DP, tree-view score tiers, PARAMETER-only matching), results are
now ranked well, but the ranking itself is invisible to the user: a leaf's row
looks identical whether it's the best match or barely cleared the tier cutoff,
and a folder's row gives no hint of how much of its subtree actually matched.
We want a code-coverage-tool-style annotation — a percentage on files, an
aggregate on folders — so the ranking that already exists becomes visible at a
glance.

## Goals

- Show each matching `PARAMETER` leaf's relevance **relative to the best match
  in the current result set** (100% = top result for this query), in both the
  tree view and the flat list view.
- Show each folder (tree view only — the list view has no folders) a
  **coverage fraction**: how many of its descendant leaves are currently
  visible (matched) out of how many exist in total, e.g. `"12/340 (4%)"`.
- Render the leaf relevance as a colored dot (continuous green→red gradient)
  plus the percentage text; render folder coverage as plain, uncolored text.
- No visual change at all when no free-text query is active (today's plain
  browsing experience is unaffected).

Non-goals: coloring folders (a low match density isn't "bad" the way a low
leaf relevance is — see rejected alternative C below); an absolute/global
relevance scale (rejected — raw scores are a coarse, query/corpus-dependent
scale, exactly why [[product_search_scoring_fixes]] moved to tier-relative
ranking instead of an absolute threshold); a settings toggle to disable the
feature (can be added later if it proves noisy — no evidence of that yet).

## Approach (chosen: colored delegate, leaves-only color)

A new `QStyledItemDelegate` subclass paints the badge; two new custom roles
carry the numbers from the models to the delegate, computed as by-products of
scans the models already perform (no new O(n) walks).

Rejected alternatives:
- **A** (append to `DisplayRole` text, e.g. `"imf_gsm — 100%"`): no model/view
  changes needed, but bakes the number into the same text used for search
  matching, copy, and drag-and-drop; can't be styled, colored, or aligned.
- **B** (second model column): uses Qt's native column rendering, no custom
  delegate — but the tree/list currently expose a single column
  (`setHeaderHidden(true)`), and a real second column is more structural
  change for a plainer result than a delegate.
- **C** (color folders too, same or a distinct color scale): rejected —
  coverage % (match density) and relevance % (match quality) are different
  concepts; coloring both risks the user reading a huge-but-relevant folder's
  low percentage as "this is a bad match" when it's really just a big folder.

## Components

### New: `ProductsScoreRoles.hpp` (include/.../Products/ProductsScoreRoles.hpp, header-only)

Single source of truth for the two custom role constants, so both models and
the delegate agree on the same numbers without duplicating literals:
```cpp
constexpr int ProductsRelevanceScoreRole = Qt::UserRole + 10;
constexpr int ProductsCoverageRole = Qt::UserRole + 11;
```

### `ProductsFlatFilterModel` (src/ProductsFlatFilterModel.cpp, include/.../ProductsFlatFilterModel.hpp)

- `process_batch()` already computes every result's score; track
  `m_max_score` (highest score seen so far), updated incrementally as each
  batch is scored — needed because rows are inserted into the view
  incrementally (`beginInsertRows`/`endInsertRows` per batch), before the
  final sort, so a running max must be available immediately, not only after
  the last batch.
- `data(index, ProductsRelevanceScoreRole)` returns
  `qRound(m_results[row].score * 100.0 / m_max_score)` when
  `!m_query.free_text_tokens.isEmpty() && m_max_score > 0`, else an invalid
  `QVariant{}` (no free-text query ⇒ no meaningful relevance).

### `ProductsTreeFilterModel` (src/ProductsTreeFilterModel.cpp, include/.../ProductsTreeFilterModel.hpp)

- Uses the same `ProductsRelevanceScoreRole` / `ProductsCoverageRole`
  constants from `ProductsScoreRoles.hpp`.
- Extend the existing `recompute_score_cutoff()` full-leaf walk (already
  computes every leaf's score for the tier cutoff) to also track
  `m_max_score` for relevance %, matching the flat model's semantics exactly.
- New `QHash<ProductsModelNode*, CoverageInfo> m_coverage` (`CoverageInfo` =
  `{int matched; int total;}`), populated by a second small recursive walk:
  - `total` — count of descendant `PARAMETER` leaves, independent of the
    query. Only needs recomputing when the *source tree structure* changes,
    not on every `set_query()` — cached separately from `matched`.
  - `matched` — count of descendant leaves currently passing `node_matches()`
    (i.e. exactly what's visible right now) — recomputed on every
    `set_query()`/`set_max_score_tiers()`, same triggers as the tier cutoff.
  - Both walks reuse the existing `collect_all_leaves()` helper.
- `data(index, ProductsRelevanceScoreRole)` — same formula as the flat model,
  for `PARAMETER` nodes only.
- `data(index, ProductsCoverageRole)` — for non-`PARAMETER` nodes only:
  `QString("%1/%2 (%3%)").arg(matched).arg(total).arg(qRound(matched*100.0/total))`,
  guarded against `total == 0`.

### New: `ProductsScoreDelegate` (src/ProductsScoreDelegate.cpp, include/.../ProductsScoreDelegate.hpp)

- `QStyledItemDelegate` subclass, `paint(painter, option, index)` override:
  1. Call `QStyledItemDelegate::paint(...)` for the base name/icon rendering.
  2. Read `index.data(ProductsRelevanceScoreRole)`. If valid: compute a color
     by linearly interpolating HSL hue from green (120°) at 100% down to red
     (0°) at 0%, draw a small filled circle plus right-aligned `"NN%"` text in
     the row's trailing area.
  3. Else read `index.data(ProductsCoverageRole)`. If valid (non-empty
     string): draw it right-aligned, using the palette's normal text color
     (no coloring).
  4. Else (neither role valid — no active free-text query, or a
     non-PARAMETER/non-folder edge case): do nothing further — row looks
     exactly like stock `QStyledItemDelegate` output.
- No painting state kept between calls; safe to share one instance across
  both views.

### `ProductsView` (src/ProductsView.cpp, include/.../ProductsView.hpp)

- New member `ProductsScoreDelegate* m_score_delegate;`, constructed once,
  assigned via `m_tree_view->setItemDelegate(m_score_delegate)` and
  `m_list_view->setItemDelegate(m_score_delegate)`.

## Data flow

1. `on_query_changed(query)` (existing) calls `set_query` on both filter
   models, as today.
2. Each model's existing scoring pass now also derives
   `ProductsRelevanceScoreRole` (both models) and `ProductsCoverageRole` (tree
   model only) as by-products.
3. Qt's view repaints; `ProductsScoreDelegate::paint()` reads the roles per
   row and draws the badge, or draws nothing if the query has no free-text
   tokens.

## Edge cases

- Query with filters but no free-text tokens (e.g. `"provider:cluster"`):
  every passing node's `free_text_score()` is the same flat value (1 per
  existing code), making a relative percentage meaningless — both roles
  return invalid/absent, so no badges render.
- Single result: trivially 100%.
- Ties at the max score: all show 100% (correct — they're joint-best).
- Folder with `total == 0` (shouldn't happen — a folder only exists because
  it has some descendant, but defensively): `CoverageRole` returns an invalid
  `QVariant` rather than dividing by zero.
- Source tree structure changes mid-session (products loaded/removed): the
  existing `setSourceModel` signal connections (rowsInserted/rowsRemoved/
  modelReset → `recompute_score_cutoff`) are extended to also invalidate the
  cached `total` counts in `m_coverage`.
- Flat model streaming: `m_max_score` only reflects batches scored *so far*,
  so a row's displayed percentage can be transiently optimistic (100%) if a
  higher-scoring row is discovered in a later batch. This self-corrects for
  free — the existing final `layoutChanged` emission (once all batches
  finish and the results are sorted) causes the view to re-query `data()` for
  every visible row against the now-final `m_max_score`, the same point
  sorting itself settles today.

## Bindings

No new public Python-exposed API — `ProductsScoreDelegate` is a pure
C++/Qt-internal rendering detail, not exposed via `bindings.xml`. The new
model roles are plain `Qt::ItemDataRole` ints, already reachable from Python
via `model.data(index, role)` the same way `Qt.DisplayRole` etc. are, so no
binding changes are needed there either.

## Testing (integration, written first)

Data layer (`tests/integration/test_products_filter.py`), no rendering
involved:
- `ProductsFlatFilterModel`: build a small fixture with leaves at known
  different scores (reuse the `three_tier_model`-style pattern), assert
  `data(index, ProductsRelevanceScoreRole)` is `100` for the best leaf and the
  correctly rounded fraction for the others.
- `ProductsTreeFilterModel`: same relevance-role assertions; plus a fixture
  with a folder containing a mix of matching/non-matching leaves, asserting
  `data(folder_index, ProductsCoverageRole) == "N/M (P%)"` with hand-computed
  N/M/P.
- Both models: query with filters-only (no free text) ⇒
  `data(index, ProductsRelevanceScoreRole)` is invalid (`QVariant()` /
  `None` from Python) for every row.

Delegate (`tests/integration/test_products_score_delegate.py`, new file):
- Light smoke test only, per [[export_content_test_metric]]'s existing
  convention in this codebase (assert opaque coverage + color variety, not
  pixel-exact output): render a small model through the delegate onto a
  `QPixmap`, assert the badge area isn't uniformly blank and contains more
  than one distinct color when relevance scores differ across rows. No
  pixel-perfect color/position assertions.
