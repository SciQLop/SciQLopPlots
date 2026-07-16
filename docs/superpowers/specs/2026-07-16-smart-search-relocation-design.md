# Smart search: relocate out of SciQLopPlots into a SciQLop core component

Date: 2026-07-16
Status: Draft (architecture direction agreed conversationally; not yet planned/implemented)

## Context

`docs/superpowers/specs/2026-07-15-pluggable-smart-search-design.md` and
`docs/superpowers/plans/2026-07-15-pluggable-smart-search.md` designed and
shipped an opt-in semantic search layer for the Products tree/list search,
living entirely inside SciQLopPlots: a thread-safe C++
`ExternalScoreOverlay` hook on `ProductsTreeFilterModel`/
`ProductsFlatFilterModel`, plus four Python modules
(`smart_search_method.py`, `smart_search_controller.py`,
`smart_search_semantic.py`, `smart_search.py`) implementing a pluggable
`SearchMethod` protocol, a `SmartSearchController` orchestrator, a
`fastembed`-backed embedding method, and a public enable/configure API. All
five tasks are complete, individually reviewed (several needed real fix
rounds for concurrency bugs and false-positive tests, caught and fixed
before merge), and the final whole-branch review found no Critical issues.

That review also flagged three findings that only matter once the feature is
actually turned on (flat-view recall explosion, no query-ordering guard,
reindex-storm risk on incremental corpus loads) — those are tracked as
follow-up work independent of this relocation and are not re-litigated here.

**The problem this doc addresses:** SciQLopPlots is a general-purpose C++/Qt
plotting library. It should not carry an ML dependency (`fastembed` et al.)
or algorithm-specific search logic — that doesn't belong in a plotting
library's dependency surface, regardless of how well-isolated it is as an
"optional extra." The goal is to relocate everything that isn't
intrinsically Qt/C++ plumbing out to SciQLop (the application), while keeping
SciQLopPlots' role reduced to the smallest, genuinely generic primitive.

## Decisions reached (conversationally, this session)

1. **SciQLopPlots keeps only the C++ primitive.** `ExternalScoreOverlay`,
   the three hook methods (`set_external_scores`, `set_smart_search_enabled`,
   `smart_search_enabled`) on both filter models, and
   `ProductsFlatFilterModel::corpus_snapshot()`. Nothing else. This code is
   already domain-agnostic — it has no idea "products" is its only consumer
   today, so nothing here needs to change to support relocation or future
   multi-domain use.
2. **All four Python modules move to SciQLop**, verbatim in spirit — no
   interface changes are needed just *because* of the move itself (moving a
   package and generalizing its interface are independent decisions).
3. **Not a `plugins_sciqlop`-style separate wheel plugin.** SciQLop already
   has a `components/` architecture (e.g. `components/settings/`,
   `components/plotting/ui/product_search_overlay.py`) for core,
   always-loaded functionality, distinct from `plugins/` (in-tree,
   `plugin.json`-manifested) and the `plugins_sciqlop` monorepo (fully
   external, independently-versioned wheels via the `"sciqlop.plugins"`
   entry-point group). Smart search becomes a **core component**, not
   either kind of plugin.
4. **`fastembed` becomes a mandatory dependency of SciQLop itself**, not an
   optional extra. Being a core component means it ships every time — no
   `sciqlop[smart_search]` extra, no lazy-import-and-degrade-gracefully
   pattern at the package level (the existing `is_available()`-style
   degradation in `smart_search.py` was designed around "extra might not be
   installed"; that specific concern goes away once it's mandatory, though
   *runtime* failure handling — model download fails, no network — still
   matters and stays).
5. **The component exposes an API other core code AND plugins can use** —
   and specifically, **multiple independent plugins can register their own
   searchable corpus/domain** through it, not just the Products tree. This
   is the one point that actually changes the existing design: it requires
   generalizing `SmartSearchController` from "hardcoded to one
   Products-shaped sink" into a registry of named domains.

## Target architecture

### Repo boundary

| Stays in SciQLopPlots (C++, no new deps) | Moves to SciQLop (Python, `fastembed` mandatory) |
|---|---|
| `ExternalScoreOverlay` (include/src) | `smart_search_method.py` → `SearchMethod` protocol, `NodeSnapshot` |
| `ProductsTreeFilterModel`/`ProductsFlatFilterModel` hook methods | `smart_search_controller.py` → generalized into a domain registry (see below) |
| `ProductsFlatFilterModel::corpus_snapshot()` | `smart_search_semantic.py` → `SemanticSearchMethod` (fastembed), unchanged |
| | `smart_search.py` → public API, `is_available()`/model-selection semantics adjusted for "always installed" |
| | A new **Products domain adapter** wrapping `ProductsTreeFilterModel`/`ProductsFlatFilterModel`/`ProductsModel` behind the domain interface — this is new code, replacing today's hardcoded coupling inside the controller |
| | A `settings.py` (or extension of an existing one) registering the enable/model-choice UI into SciQLop's `components/settings/` framework |

### The domain registry (the one real design change)

Today, `SmartSearchController.attach(source_model, corpus_model, targets)`
hardcodes: connect to `source_model`'s `rowsInserted`/`rowsRemoved`/
`modelReset`; snapshot via `corpus_model.corpus_snapshot()`; push results via
`target.set_external_scores(...)` on each target. This is exactly right for
one Products-shaped consumer, but a second plugin wanting to index its own
content (a different corpus entirely) can't reuse it without duplicating the
whole class.

Proposed shape — a small `SearchDomain` contract plugins implement, and a
registry replacing the single hardcoded `attach()`:

```
SearchDomain:
    name: str
    snapshot() -> Iterable[NodeSnapshot]        # full corpus, called on reindex
    push_scores(scores: dict[str, float]) -> None   # called with combined results
    # however "corpus changed" is signaled is still an open question — see below
```

The registry (evolved `SmartSearchController`, or a new top-level object that
owns one per domain) would expose something like `register_domain(domain)` /
`unregister_domain(name)`, and internally run the existing
index/query-on-background-thread machinery independently per domain, using
the same registered `SearchMethod`(s) — i.e. one shared embedding model
config serves every domain's corpus, since the model itself has no idea
what domain it's embedding text from.

The Products adapter (built either inside the new component, or inside
`speasy_provider` if that's a better ownership fit — not yet decided) would
be the *first* registered domain: `snapshot()` calls
`ProductsFlatFilterModel.corpus_snapshot()`, `push_scores()` calls
`set_external_scores()` on both filter models, and it internally connects
`ProductsModel`'s structural signals to trigger its own re-snapshot — none
of that logic is new, it's what `SmartSearchController` already does today,
just moved behind the `SearchDomain` interface instead of hardcoded as the
only case the controller knows about.

## Open questions (not yet decided — need a real design pass before planning)

- **Global vs. per-domain enable/model choice.** Is "smart search on/off" and
  "which embedding model" one setting for the whole app, or can each domain
  (each plugin) opt in independently / pick its own model? The current
  `smart_search.py` API (`is_enabled()`/`set_model()`) is global-singleton
  shaped; multi-domain may or may not want to preserve that.
- **Domain lifecycle.** What happens to a registered domain when the
  plugin that registered it unloads (SciQLop plugins can be disabled/
  reloaded at runtime, per the plugin-loader code found during this
  session's investigation)? Does the registry need explicit
  `unregister_domain`, and is it the plugin's responsibility to call it on
  teardown?
- **Change-notification mechanism.** Today's `attach()` binds to Qt-specific
  signal names (`rowsInserted` etc.) because the Products domain happens to
  be Qt-model-shaped. A generic `SearchDomain` can't assume every future
  domain has Qt signals — likely needs either an explicit
  `registry.notify_changed(domain_name)` call the domain owner makes
  manually, or an optional Qt-signal-based convenience path for domains that
  do have one.
- **Where does the Products adapter live** — inside the new smart-search
  component itself (which would need to import/know about `ProductsModel`),
  or inside `speasy_provider` (which already owns Products node
  construction and already imports SciQLopPlots)? Leaning toward
  `speasy_provider`, since that's the existing owner of "how Products nodes
  get built," but not decided.
- **Concurrency at N domains.** The existing design already has known,
  reviewed-but-deferred concerns (I-2: no query-ordering guard; I-3: reindex
  storm on incremental loads) at *one* domain. Multiple domains each running
  independent reindex/query background threads multiplies that surface —
  worth revisiting whether those two follow-ups become prerequisites rather
  than independent follow-ups once multi-domain ships.

## Non-goals (explicitly out of scope for this doc)

- A concrete second domain (e.g. a markdown-docs-with-anchors help search)
  is not being designed here — it was the illustrative example that
  motivated checking whether the architecture *could* generalize, not a
  committed feature. This doc only prepares the seam; building a second
  domain is separate future work.
- The three enabled-path follow-ups from the whole-branch review (flat-view
  recall explosion, query-ordering guard, reindex-storm debounce) are
  tracked in SciQLopPlots' own memory/ledger, not restated here.

## Suggested next step

This relocation + the domain-registry generalization is real new design work
spanning two repos (delete/relocate code in SciQLopPlots; new component +
domain adapter + settings UI in SciQLop) with several open questions above
still unresolved. Recommend a proper brainstorming pass scoped to SciQLop's
side of this (the registry shape, the open questions above) before writing
an implementation plan — this doc captures direction and constraints, not a
buildable spec yet.
