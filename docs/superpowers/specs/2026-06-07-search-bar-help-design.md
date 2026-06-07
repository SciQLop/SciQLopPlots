# Search-bar help: documented & settable keywords

Date: 2026-06-07
Status: Approved (design)

## Problem

The product-tree search bar (`QueryLineEdit` inside `ProductsView`) supports a
query mini-language — free text plus `field:value` filters over `provider`,
`type`, `after`, `before`, and metadata keys auto-discovered from products.
Today this syntax is essentially undocumented in the UI: discoverability is
limited to the placeholder text and a `Ctrl+Space` completion shortcut most
users never find. We want the keywords documented in-place and a way for the
embedding app (SciQLop) to override the help text.

## Goals

- Show the available fields and syntax as a **hover tooltip** on the search bar.
- Show the same content in a **`?`-button popup** that the user can read and dismiss.
- **Auto-generate** the keyword list from the known fields so it stays in sync.
- Let **SciQLop override the entire help text** from Python with its own string.

Non-goals: per-field descriptions supplied from Python (override is whole-text
only for v1); markdown rendering beyond Qt's rich-text HTML subset; restyling
the existing completion popup.

## Approach (chosen: A)

Help content is owned by `QueryLineEdit` — it already holds the field data
(`m_field_names`, `m_field_values`). `ProductsView` surfaces that content two
ways (tooltip + `?` popup) and exposes the public override API, since
`ProductsView` is the Python-exposed widget (`bindings.xml`).

Rejected alternatives:
- **B** (all help logic in `ProductsView`): splits field data from the object
  that owns it; more plumbing for no benefit.
- **C** (tooltip only): under-delivers — the `?` popup was explicitly requested.

## Components

### `QueryLineEdit` (src/QueryLineEdit.cpp, include/.../QueryLineEdit.hpp)

New members:
- `QString m_help_override;` — empty ⇒ use auto-generated help.
- `void set_help_text(const QString& html);` — set override; `""` (or
  all-whitespace) reverts to auto. Updates the tooltip and emits the signal.
- `QString help_text() const;` — effective help (override if non-blank, else
  auto). Always returns non-empty.
- `Q_SIGNAL void helpTextChanged();`
- `QString build_help_html() const;` (private) — pure builder from a
  file-local static table of builtin fields + discovered `m_field_names` /
  `m_field_values`.

Wiring:
- `set_completions(...)` rebuilds the auto help (only when no override is set),
  calls `setToolTip(help_text())`, and emits `helpTextChanged()`.
- `set_help_text(...)` stores the override, calls `setToolTip(help_text())`,
  emits `helpTextChanged()`.

### `ProductsView` (src/ProductsView.cpp, include/.../ProductsView.hpp)

New members:
- `QToolButton* m_help_button;` — a `?` button added to the existing `toolbar`
  row (always visible, next to the view toggle).
- `QTextBrowser* m_help_popup;` — lazily created, frameless (`Qt::Popup`),
  `setOpenExternalLinks(true)`, parented to `ProductsView`; reused across clicks.
- `void set_search_help(const QString& html);` (public) — delegates to
  `m_query_line_edit->set_help_text(html)`. **Python override entry point.**
- `QString search_help() const;` (public) — `m_query_line_edit->help_text()`.
- `void show_help_popup();` (slot) — builds/positions the popup under the search
  bar, sets its HTML to `search_help()`, shows it.

Wiring:
- `m_help_button` clicked → `show_help_popup()`.
- `m_query_line_edit::helpTextChanged` → refresh popup HTML if currently visible.

## Data flow

1. `refresh_completions()` → `set_completions(...)` → `QueryLineEdit` rebuilds
   auto help (if no override), updates tooltip, emits `helpTextChanged`.
2. SciQLop calls `view.set_search_help(html)` → override stored → tooltip
   updated → `helpTextChanged` emitted.
3. Clicking `?` → `show_help_popup()` renders `search_help()`.

## Auto-generated help content

Built from a file-local static table (data over code):

```
{provider, "data source",      "provider:amda"}
{type,     "parameter kind",   "type:vector"}
{after,    "start date (ISO)", "after:2015-01-01"}
{before,   "end date (ISO)",   "before:2016"}
```

Rendered HTML:
- One-line intro: type words to match product names; combine with `field:value`.
- A `<table>` of builtin field → description → example.
- A line listing discovered metadata field names (those in `m_field_names` not
  in the static table), names only.
- Footer tip: "Press Ctrl+Space to autocomplete."

## Error handling / edge cases

- Override `""` or all-whitespace ⇒ falls back to auto; `help_text()` never empty.
- Popup created lazily, reused; parented for correct lifetime.
- SciQLop-supplied HTML is trusted (same trust level as the rest of the host app).
  Qt renders only its rich-text subset.

## Bindings

`set_search_help` / `search_help` are public methods on the already-exposed
`ProductsView`, so Shiboken auto-exposes them. Per the regen quirk, touch
`bindings.xml` to force regeneration after editing the header.

## Testing (integration, written first)

`tests/integration/test_search_bar_help.py`:
- `set_search_help("custom")` ⇒ `search_help() == "custom"`.
- Empty override reverts to auto; auto text contains every builtin field name
  (`provider`, `type`, `after`, `before`) and "Ctrl+Space".
- After products are loaded, a discovered metadata key appears in `search_help()`.

`build_help_html` is covered transitively via `search_help()`; no separate C++
harness needed.
