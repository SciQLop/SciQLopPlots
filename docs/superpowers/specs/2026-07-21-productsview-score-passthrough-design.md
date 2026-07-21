# ProductsView external-score passthrough (closing the BM25-into-the-sidebar-tree gap)

Date: 2026-07-21
Status: Draft (design agreed conversationally this session; implementing directly given urgency)

## Context

The 2026-07-20 score-merge generalization (`docs/superpowers/specs/2026-07-20-configurable-score-merge-design.md`)
gave `ProductsTreeFilterModel` and `ProductsFlatFilterModel` a full N-signal
external-score API (`set_external_scores`, `set_signal_enabled`,
`set_score_merge_strategy`, `set_signal_weight`, `set_override_signal`,
`registered_signals`). That work is correct and complete on those two
classes.

What was missed: `ProductsView` — the widget SciQLop actually mounts as its
sidebar Products browser (`self.productTree = ProductsView(self)` in
SciQLop's `mainwindow.py`) — builds its own **private**
`ProductsTreeFilterModel`/`ProductsFlatFilterModel` internally
(`ProductsView.cpp:64,78`) and has never exposed them, or any scoring hook,
to Python. Its entire public surface is `set_search_help()`/`search_help()`.
So the new signal API, while real and correct, is unreachable from the one
UI surface it needs to reach: the sidebar tree is the actual "product tree
search" the whole BM25 effort was for.

This was never in scope of any prior design (2026-07-15, 2026-07-16,
2026-07-20 docs — none mention `ProductsView`), and SciQLop's own smart
search to date only ever wired into a separate, independently-constructed
`ProductsFlatFilterModel` inside `product_search_overlay.py` (the
empty-panel popup), not the sidebar. That's the seam this document closes.

## Design

`ProductsView` keeps both internal filter models in sync already (it calls
`set_query` on both from `on_query_changed`). The passthrough follows the
same pattern: every scoring setter forwards to both `m_tree_filter` and
`m_flat_filter` so switching between tree/list view mid-session never loses
signal state; every getter reads `m_tree_filter` (the two are always kept
configured identically by construction, so either is a valid source of
truth).

### `ProductsView.hpp` additions

```cpp
#include "SciQLopPlots/Products/ScoreMerge.hpp"
...
public:
    // Emitted whenever the query bar's free-text tokens change (filter-only
    // syntax like "provider:x" is excluded, same split QueryParser already
    // does) -- lets a Python search controller (re)dispatch an async
    // external-scoring query (e.g. BM25) without polling or reparsing the
    // query bar itself.
    Q_SIGNAL void free_text_query_changed(const QStringList& tokens);

    void set_external_scores(const QString& signal_name, const QHash<QString, QVariant>& scores);
    void set_signal_enabled(const QString& signal_name, bool enabled);
    bool signal_enabled(const QString& signal_name) const;
    QStringList registered_signals() const;

    void set_score_merge_strategy(ScoreMergeStrategy strategy);
    ScoreMergeStrategy score_merge_strategy() const;

    void set_signal_weight(const QString& signal_name, double weight);
    double signal_weight(const QString& signal_name) const;

    void set_override_signal(const QString& signal_name);
    QString override_signal() const;
```

### `ProductsView.cpp`

- Every setter forwards to `m_tree_filter` then `m_flat_filter`. Getters read
  `m_tree_filter` only.
- `on_query_changed` gains one line, `emit free_text_query_changed(query.free_text_tokens);`,
  emitted unconditionally (including when tokens are empty) so a Python
  controller can clear a stale external-score set when the user clears the
  query — mirroring how `product_search_overlay.py._on_text_changed` already
  clears results below its own minimum-query-length threshold.

### Bindings

`bindings.xml` already registers `ProductsView` as a blanket
`<object-type name="ProductsView" parent-management="yes"/>` with no
per-method `<modify-function>` — the same registration style that already
auto-exposed the new methods on `ProductsTreeFilterModel`/
`ProductsFlatFilterModel` in the 0.31.0 release with zero XML changes. No
XML edits needed, only the customary `touch bindings.xml` after editing the
header so meson's dependency tracking notices ([[shiboken_regen_quirk]]).

## Non-goals

- No change to `ProductsTreeFilterModel`/`ProductsFlatFilterModel` — their
  API is already correct.
- No SciQLop-side wiring in this document (that's the follow-up: connect
  `ProductsView.free_text_query_changed` to `smart_search.query()` and pipe
  the result into `set_external_scores("smart_search", scores)`, the same
  shape `product_search_overlay.py` already proved out).
- No change to `merge_scores`/`ScoreMergeStrategy` semantics.

## Testing plan

Extend `tests/integration/test_products_filter.py` (reuses the existing
`overlay_test_model` fixture) with a new `TestProductsViewScorePassthrough`
class:

1. `set_external_scores`/`set_signal_enabled` on the view surface a leaf in
   the list view exactly like the equivalent `ProductsTreeFilterModel`-level
   test, driven by typing into the view's query bar (`QTextEdit.setPlainText`,
   the existing pattern from `test_products_score_delegate.py`).
2. `registered_signals()`/`signal_enabled()`/`signal_weight()`/
   `override_signal()` reflect what was set.
3. `free_text_query_changed` emits with the parsed free-text tokens (not
   filter-only tokens) when the query bar changes, and emits an empty list
   when the query is cleared.
4. Setting scores before vs. after typing a query both work (mirrors the
   existing "late external scores trigger rescoring" coverage at the
   individual-model level, exercised here through the view).
