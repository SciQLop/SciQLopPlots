# Search-bar Help Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Document the product-tree search keywords in-place (hover tooltip + `?`-button popup), auto-generated from the known fields, with a public Python API for SciQLop to override the whole help text.

**Architecture:** Help content is owned by `QueryLineEdit` (it already holds the field data). It exposes `set_help_text()` / `help_text()` and keeps its own tooltip in sync. `ProductsView` adds a `?` toolbar button + a lazy `QTextBrowser` popup, and the public `set_search_help()` / `search_help()` override API that delegates to the line edit. The auto-generated body is built by a pure helper from a static builtin-field table plus discovered metadata fields.

**Tech Stack:** C++20, Qt6 Widgets (`QTextEdit`, `QTextBrowser`, `QToolButton`), Shiboken6 bindings, pytest integration tests.

---

## File structure

- `include/SciQLopPlots/Products/QueryLineEdit.hpp` — add help API (override string, getter, setter, signal, private builder).
- `src/QueryLineEdit.cpp` — implement help builder, tooltip sync, signal emission.
- `include/SciQLopPlots/Products/ProductsView.hpp` — add `?` button, popup, public `set_search_help`/`search_help`, `show_help_popup` slot.
- `src/ProductsView.cpp` — wire button/popup, implement public API.
- `SciQLopPlots/bindings/bindings.xml` — touch to force Shiboken regen (no new entry needed; methods are public on the already-exposed `ProductsView`).
- `tests/integration/test_search_bar_help.py` — new integration test (create).

---

## Task 1: Failing integration test for the public help API

**Files:**
- Test: `tests/integration/test_search_bar_help.py` (create)

- [ ] **Step 1: Write the failing test**

```python
"""Tests for the product search-bar help (auto-generated + override)."""
import pytest
from PySide6.QtWidgets import QTextEdit
from SciQLopPlots import (
    ProductsView, ProductsModel, ProductsModelNode,
    ProductsModelNodeType, ParameterType,
)


def test_auto_help_lists_builtin_fields_and_shortcut(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    help_text = view.search_help()
    for field in ("provider", "type", "after", "before"):
        assert field in help_text
    assert "Ctrl+Space" in help_text


def test_set_search_help_overrides_auto(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    assert view.search_help() == "<b>Custom help</b>"


def test_empty_override_reverts_to_auto(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    view.set_search_help("")
    assert "provider" in view.search_help()
    assert view.search_help() != "<b>Custom help</b>"


def test_search_bar_tooltip_reflects_help(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    bar = view.findChild(QTextEdit)
    assert bar is not None
    assert bar.toolTip() == "<b>Custom help</b>"


def test_auto_help_includes_discovered_metadata_key(qtbot):
    model = ProductsModel.instance()
    root = ProductsModelNode("help_test_provider")
    node = ProductsModelNode(
        "HelpProbe", "help_test_provider",
        {"mission": "helptest_mission"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    root.add_child(node)
    model.add_node([], root)

    # ProductsView ctor runs refresh_completions() synchronously over the model,
    # so constructing after populating gives deterministic auto-help.
    view = ProductsView()
    qtbot.addWidget(view)
    assert "mission" in view.search_help()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `PYTHONPATH=build pytest tests/integration/test_search_bar_help.py -v`
Expected: FAIL — `AttributeError: 'ProductsView' object has no attribute 'search_help'`.

---

## Task 2: Help content + tooltip in `QueryLineEdit`

**Files:**
- Modify: `include/SciQLopPlots/Products/QueryLineEdit.hpp`
- Modify: `src/QueryLineEdit.cpp`

- [ ] **Step 1: Add the help API to the header**

In `QueryLineEdit.hpp`, add a member `QString m_help_override;` alongside the
other members (after `QStringList m_product_names;`):

```cpp
    QStringList m_product_names;
    QString m_help_override;
```

In the `public:` section, after `set_known_fields(...)`:

```cpp
    void set_help_text(const QString& html);
    QString help_text() const;

    Q_SIGNAL void helpTextChanged();
```

In the `private:` section, after `QString current_word() const;`:

```cpp
    QString build_help_html() const;
    void refresh_help();
```

- [ ] **Step 2: Implement the help logic in the cpp**

In `src/QueryLineEdit.cpp`, add `#include <QTextDocument>` is **not** needed; the
builder uses only `QString`. Add the implementations at the end of the file
(after `current_word()`):

```cpp
namespace
{
struct BuiltinField
{
    const char* name;
    const char* description;
    const char* example;
};

constexpr BuiltinField builtin_fields[] = {
    { "provider", "data source", "provider:amda" },
    { "type", "parameter kind", "type:vector" },
    { "after", "start date (ISO)", "after:2015-01-01" },
    { "before", "end date (ISO)", "before:2016" },
};
}

QString QueryLineEdit::build_help_html() const
{
    QString html;
    html += "<b>Search products</b><br/>";
    html += "Type words to match product names, and combine with "
            "<i>field:value</i> filters.<br/><br/>";
    html += "<table cellspacing='4'>";

    QSet<QString> builtin_names;
    for (const auto& f : builtin_fields)
    {
        builtin_names.insert(QString::fromLatin1(f.name));
        html += QString("<tr><td><b>%1</b></td><td>%2</td><td><i>%3</i></td></tr>")
                    .arg(f.name, f.description, f.example);
    }
    html += "</table>";

    QStringList metadata_fields;
    for (const auto& name : m_field_names)
    {
        if (!builtin_names.contains(name))
            metadata_fields.append(name);
    }
    if (!metadata_fields.isEmpty())
    {
        metadata_fields.sort();
        html += QString("<br/>Metadata fields: %1").arg(metadata_fields.join(", "));
    }

    html += "<br/><br/><i>Tip: press Ctrl+Space to autocomplete.</i>";
    return html;
}

QString QueryLineEdit::help_text() const
{
    if (!m_help_override.trimmed().isEmpty())
        return m_help_override;
    return build_help_html();
}

void QueryLineEdit::set_help_text(const QString& html)
{
    m_help_override = html;
    refresh_help();
}

void QueryLineEdit::refresh_help()
{
    setToolTip(help_text());
    emit helpTextChanged();
}
```

- [ ] **Step 3: Refresh help whenever completions change**

In `QueryLineEdit::set_completions(...)`, append a call to `refresh_help()` at
the end of the body:

```cpp
void QueryLineEdit::set_completions(const QStringList& field_names,
                                     const QMap<QString, QStringList>& field_values,
                                     const QStringList& product_names)
{
    m_field_names = field_names;
    m_field_values = field_values;
    m_product_names = product_names;
    refresh_help();
}
```

- [ ] **Step 4: Add the missing include**

At the top of `src/QueryLineEdit.cpp`, add `#include <QSet>` with the other
includes (the builder uses `QSet<QString>`).

---

## Task 3: `?` button, popup, and public override API in `ProductsView`

**Files:**
- Modify: `include/SciQLopPlots/Products/ProductsView.hpp`
- Modify: `src/ProductsView.cpp`

- [ ] **Step 1: Declare new members and public API in the header**

In `ProductsView.hpp`, add forward declarations near the top (after the existing
ones):

```cpp
class QTextBrowser;
```

Add private members alongside the existing ones:

```cpp
    QToolButton* m_help_button;
    QTextBrowser* m_help_popup;
```

Add a private slot with the other slots:

```cpp
    Q_SLOT void show_help_popup();
```

In the `public:` section, after the constructor/destructor:

```cpp
    void set_search_help(const QString& html);
    QString search_help() const;
```

- [ ] **Step 2: Add the `?` button to the toolbar**

In `src/ProductsView.cpp`, add `#include <QTextBrowser>` with the other includes.

In the constructor, after the `m_view_toggle` block and before
`m_result_count` is added to the toolbar, create the help button:

```cpp
    m_help_button = new QToolButton(this);
    m_help_button->setText("?");
    m_help_button->setToolTip("Show search syntax help");
    toolbar->addWidget(m_help_button);
```

Initialise the popup pointer to null in the same constructor (before the
`connect` calls):

```cpp
    m_help_popup = nullptr;
```

- [ ] **Step 3: Wire the button and help-changed signal**

In the constructor, after the existing `connect(...)` calls for
`m_view_toggle`, add:

```cpp
    connect(m_help_button, &QToolButton::clicked, this,
            &ProductsView::show_help_popup);
    connect(m_query_line_edit, &QueryLineEdit::helpTextChanged, this, [this]() {
        if (m_help_popup && m_help_popup->isVisible())
            m_help_popup->setHtml(search_help());
    });
```

- [ ] **Step 4: Implement the public API and popup slot**

Add these definitions at the end of `src/ProductsView.cpp`:

```cpp
void ProductsView::set_search_help(const QString& html)
{
    m_query_line_edit->set_help_text(html);
}

QString ProductsView::search_help() const
{
    return m_query_line_edit->help_text();
}

void ProductsView::show_help_popup()
{
    if (!m_help_popup)
    {
        m_help_popup = new QTextBrowser(this);
        m_help_popup->setWindowFlags(Qt::Popup);
        m_help_popup->setOpenExternalLinks(true);
        m_help_popup->setFixedSize(380, 240);
    }
    m_help_popup->setHtml(search_help());
    QPoint pos = m_query_line_edit->mapToGlobal(
        QPoint(0, m_query_line_edit->height()));
    m_help_popup->move(pos);
    m_help_popup->show();
}
```

---

## Task 4: Regenerate bindings, build, install, verify

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.xml` (touch only)

- [ ] **Step 1: Force Shiboken regen**

The new methods are public on the already-exposed `ProductsView`, so no XML
entry is needed — but the meson custom_target does not track header changes, so
touch the typesystem to force regeneration:

Run: `touch SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 2: Compile**

Run: `meson compile -C build`
Expected: builds cleanly; `SciQLopPlotsBindings` relinks.

- [ ] **Step 3: Run the new test**

Run: `PYTHONPATH=build pytest tests/integration/test_search_bar_help.py -v`
Expected: all 5 tests PASS.

(Local alternative matching the user's workflow: copy the freshly built
`build/SciQLopPlots/SciQLopPlotsBindings*.so` into SciQLop's venv site-packages
and run pytest from outside the project dir via that venv — never with
`QT_QPA_PLATFORM=offscreen` locally.)

- [ ] **Step 4: Run the surrounding suites for regressions**

Run: `PYTHONPATH=build pytest tests/integration/test_products_filter.py tests/integration/test_query_parser.py -v`
Expected: PASS (no regressions in the products area).

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Products/QueryLineEdit.hpp src/QueryLineEdit.cpp \
        include/SciQLopPlots/Products/ProductsView.hpp src/ProductsView.cpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_search_bar_help.py
git commit -m "feat(products): documented & settable search-bar help

Auto-generate keyword help from known fields, shown as the search-bar
tooltip and in a ?-button popup; expose ProductsView.set_search_help /
search_help so SciQLop can override the whole help text.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-review notes

- Spec coverage: hover tooltip (Task 2 `refresh_help`→`setToolTip`), `?` popup
  (Task 3), auto-generated from fields (Task 2 `build_help_html`), SciQLop
  whole-text override (Task 3 `set_search_help`), bindings (Task 4), tests
  (Task 1 + Task 4). All spec sections map to a task.
- Type consistency: `set_help_text`/`help_text`/`helpTextChanged`/`build_help_html`/
  `refresh_help` on `QueryLineEdit`; `set_search_help`/`search_help`/
  `show_help_popup`/`m_help_button`/`m_help_popup` on `ProductsView` — names used
  consistently across tasks.
- No placeholders; every code step shows full code.
```
