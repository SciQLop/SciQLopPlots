# Inspector Font Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose font (family / point size / bold / italic) and color controls in the inspector for axis labels, axis tick labels, plot legends, and text overlay items — the four text-bearing UI elements that already have inspector entries but no styling controls.

**Architecture:** Three layers.
1. **Wrapper surface.** `SciQLopPlotAxis`, `SciQLopPlotLegend`, and `SciQLopTextItem` gain font getters/setters with `*_changed` signals, fronted by their `*Interface` base classes so all subclasses inherit the API and Shiboken sees the signals.
2. **Reusable widget.** A new `FontDelegate` mirrors `ColorDelegate`'s popup-style picker: a single preview button (showing "Aa" rendered in the current font/color) opens a lazily-constructed popup containing a `QFontComboBox`, a size `QSpinBox`, bold and italic `QToolButton`s, and an embedded `ColorDelegate`. The widget exposes two independent signals (`fontChanged(QFont)` and `colorChanged(QColor)`) so font and color edits route to their separate setters on the underlying object.
3. **Delegate wiring.** `SciQLopPlotAxisDelegate`, `SciQLopPlotDelegate`, and a new `SciQLopTextItemDelegate` embed `FontDelegate` instances and bind them to the model with idempotent setters + reverse-path `setFont`/`setColor` calls (the != check inside `FontDelegate` prevents emit loops; an explicit `QSignalBlocker` is unnecessary because of that gate, matching `ColorDelegate`'s style).

**Tech Stack:** C++20, Qt6 (QFont, QFontComboBox, QSpinBox, QToolButton, QColorDialog, QGroupBox), Shiboken6 bindings, meson, Python 3.13 with PySide6 6.11.0, unittest for integration tests under `tests/manual-tests/test_inspector_properties.py` (run via `meson test -C build inspector_properties`).

**Tracking issue:** SciQLop/SciQLopPlots#53.
**Branch:** `font-controls-spec` (already exists on `origin`, contains the spec doc; new commits stack on top).
**Spec:** `docs/superpowers/specs/2026-05-02-text-font-controls-design.md`.
**Handover:** `docs/superpowers/handovers/2026-05-02-font-controls-handover.md`.

## Project conventions to follow on every task

These cost time when forgotten — the implementer must apply them on every task that touches the relevant area:

- **Build:** the build directory `build/` already exists. Compile with the Qt 6.11 toolchain on `PATH`:

  ```bash
  PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build
  ```

  Never reconfigure with `--buildtype=debug` (plain `-O0` makes the test suite unusably slow). `debugoptimized` is the standing default.

- **Q_OBJECT registration:** any new class declared with `Q_OBJECT` must appear in **both** `moc_headers` AND `sources` lists in `SciQLopPlots/meson.build`. Forgetting `moc_headers` produces a clean build that crashes at runtime with `undefined symbol: vtable for <Class>`. Verify after compile with:

  ```bash
  nm -C build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so | grep "vtable for FontDelegate"
  ```

  A line starting with `0000000... d` is good; a line starting with `U` means the moc didn't run.

- **Shiboken regen quirk:** the shiboken `custom_target` only depends on `SciQLopPlots/bindings/bindings.xml`, NOT on the headers it includes. Whenever a Python-exposed interface header changes (anything in `include/SciQLopPlots/Plotables/`, `include/SciQLopPlots/SciQLopPlot*.hpp`, `include/SciQLopPlots/Items/SciQLopTextItem.hpp`), run:

  ```bash
  touch SciQLopPlots/bindings/bindings.xml && \
  PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build
  ```

- **Install (after every successful compile):** copy the bindings `.so` into the SciQLop venv that the integration tests run from:

  ```bash
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
  ```

  Do not `pip install` — just copy the .so.

- **Test invocation:**

  ```bash
  meson test -C build inspector_properties
  ```

  Run from the project root. Do NOT set `QT_QPA_PLATFORM=offscreen` — the test harness uses a real `QApplication` on the user's display.

- **Inspector pattern (idempotent setters + reverse path):**
  - **Setter** (`set_xxx`) early-returns when the new value equals the current one, then mutates state and emits `xxx_changed(value)`. This makes the setter safe to call from both forward (widget→model) and reverse (model→widget) paths without recursion.
  - **Forward path:** `connect(widget, &Widget::xxxChanged, model, &Model::set_xxx);`
  - **Reverse path:** `connect(model, &Model::xxx_changed, widget, &Widget::setXxx);` — the widget's setter must also be idempotent, OR wrap with `QSignalBlocker`. `ColorDelegate::setColor` uses the != gate; `FontDelegate` will mirror that.

- **Push targets:** `origin` is the personal fork (`jeandet/SciQLopPlots`); push commits there. `upstream` is `SciQLop/SciQLopPlots` — never push there unsolicited. Do not open the PR until the user explicitly asks for it.

- **Delegate Python visibility:** delegates created by `DelegateRegistry.instance().create_delegate(obj)` come back typed as `PropertyDelegateBase` from Python's perspective. Don't write tests asserting `type(delegate).__name__ == 'SciQLopXxxDelegate'`. Identify delegates by widget composition (group-box titles, child counts, signal connections).

## File map

### Files created

| Path | Responsibility |
|---|---|
| `include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp` | Reusable font picker widget (preview button + popup) |
| `src/FontDelegate.cpp` | FontDelegate implementation |
| `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp` | Property delegate for `SciQLopTextItem` |
| `src/SciQLopTextItemDelegate.cpp` | TextItem delegate implementation |

### Files modified

| Path | Change |
|---|---|
| `include/SciQLopPlots/SciQLopPlotAxis.hpp` | Add 4 setter pairs + signals on `SciQLopPlotAxisInterface` (label_font, label_color, tick_label_font, tick_label_color); concrete impl declarations on `SciQLopPlotAxis` |
| `src/SciQLopPlotAxis.cpp` | Implement 4 concrete setter+getter pairs bridging to `QCPAxis::{label,tickLabel}{Font,Color}` |
| `include/SciQLopPlots/SciQLopPlotLegendInterface.hpp` | Add font/color setters/getters + signals |
| `include/SciQLopPlots/SciQLopPlotLegend.hpp` | Override declarations |
| `src/SciQLopPlotLegend.cpp` | Bridge to `QCPLegend::setFont` / `setTextColor` |
| `include/SciQLopPlots/Items/SciQLopTextItem.hpp` | Add `set_font` / `font` / `set_font_family` (inline; no .cpp change needed) |
| `src/SciQLopPlotAxisDelegate.cpp` | Move existing label QLineEdit into a `Label` QGroupBox alongside a FontDelegate; add a `Tick labels` QGroupBox containing the existing visibility checkbox + a separate FontDelegate |
| `src/SciQLopPlotDelegate.cpp` | Wrap the existing LegendDelegate in a `Legend` QGroupBox with an additional FontDelegate |
| `src/Registrations.cpp` | Register `SciQLopTextItemDelegate` and add SciQLopTextItem object metadata to TypeRegistry |
| `SciQLopPlots/meson.build` | Add new headers to `moc_headers`, new `.cpp` files to `sources` |
| `SciQLopPlots/bindings/bindings.xml` | Touch (no content change required — typesystem entries already exist for the affected classes) |
| `tests/manual-tests/test_inspector_properties.py` | Add `TestAxisFontControls`, `TestLegendFontControls`, `TestTextItemDelegate`, `TestFontDelegate` |

---

## Phase 1 — Plotable surface (axis / legend / text item)

This phase adds the C++ getters/setters/signals the inspector and Python users will read and write. No widgets yet; tests at this phase exercise the wrappers directly through Python after each compile + bindings refresh.

### Task 1: SciQLopPlotAxisInterface — abstract font/color virtuals + signals

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotAxis.hpp`
- Test: `tests/manual-tests/test_inspector_properties.py` (new `TestAxisFontControls` class — initial smoke test only here)

The four setter/getter pairs go on `SciQLopPlotAxisInterface` (matching the existing `set_label` / `label` pattern with `WARN_ABSTRACT_METHOD` defaults). Signals declared in the existing `signals:` block under the `BINDINGS_H` shim. The concrete implementations land in Task 2.

- [ ] **Step 1: Write the failing Python integration test**

Append to `tests/manual-tests/test_inspector_properties.py` (after the existing `TestPlotAxisDelegate` class ends — locate by searching for `class TestPlotAxisDelegate` and inserting the new class right after the class block ends):

```python
class TestAxisFontControls(unittest.TestCase):
    """Axis label and tick-label font/color setters + signals.

    Exercises the wrapper surface directly (no delegate). The inspector
    delegate is exercised in TestPlotAxisDelegate's added cases.
    """

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y,
            plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line,
            labels=["s"],
        )
        self.axis = self.plot.x_axis()

    def tearDown(self):
        self.panel.deleteLater()

    def test_label_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 14)
        f.setBold(True)
        self.axis.set_label_font(f)
        got = self.axis.label_font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 14)
        self.assertTrue(got.bold())

    def test_label_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.axis.set_label_color(QColor("red"))
        self.assertEqual(self.axis.label_color().name(), "#ff0000")

    def test_tick_label_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 9)
        f.setItalic(True)
        self.axis.set_tick_label_font(f)
        got = self.axis.tick_label_font()
        self.assertEqual(got.family(), "Arial")
        self.assertEqual(got.pointSize(), 9)
        self.assertTrue(got.italic())

    def test_tick_label_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.axis.set_tick_label_color(QColor("#00aa00"))
        self.assertEqual(self.axis.tick_label_color().name(), "#00aa00")

    def test_label_font_signal_fires_once(self):
        from PySide6.QtGui import QFont
        emitted = []
        self.axis.label_font_changed.connect(lambda f: emitted.append(f))
        self.axis.set_label_font(QFont("Courier New", 11))
        self.assertEqual(len(emitted), 1)

    def test_label_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 11)
        self.axis.set_label_font(f)  # baseline
        emitted = []
        self.axis.label_font_changed.connect(lambda x: emitted.append(x))
        self.axis.set_label_font(f)  # same value
        self.assertEqual(len(emitted), 0)

    def test_label_color_change_does_not_fire_font_signal(self):
        from PySide6.QtGui import QColor
        emitted = []
        self.axis.label_font_changed.connect(lambda f: emitted.append(f))
        self.axis.set_label_color(QColor("blue"))
        self.assertEqual(len(emitted), 0,
                         "label_color edit must not fire label_font_changed")
```

- [ ] **Step 2: Run the new tests; verify they fail with AttributeError**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestAxisFontControls|FAIL|ERROR" | head -30
```

Expected: every `TestAxisFontControls.*` case fails with `AttributeError: 'SciQLopPlotAxis' object has no attribute 'set_label_font'` (or similar — `label_font`, `set_label_color`, etc.).

- [ ] **Step 3: Add abstract virtuals + signals on `SciQLopPlotAxisInterface`**

In `include/SciQLopPlots/SciQLopPlotAxis.hpp`, add `#include <QFont>` and `#include <QColor>` near the top (alongside `#include <QObject>`). Then inside the `SciQLopPlotAxisInterface` class body, after the existing `set_tick_labels_visible` virtual (around line 90) and before `set_selected`, insert:

```cpp
    inline virtual void set_label_font(const QFont& font) noexcept
    {
        Q_UNUSED(font);
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_label_color(const QColor& color) noexcept
    {
        Q_UNUSED(color);
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_tick_label_font(const QFont& font) noexcept
    {
        Q_UNUSED(font);
        WARN_ABSTRACT_METHOD;
    }

    inline virtual void set_tick_label_color(const QColor& color) noexcept
    {
        Q_UNUSED(color);
        WARN_ABSTRACT_METHOD;
    }
```

Then in the same class, after the existing `tick_labels_visible() const` getter (around line 126) and before `orientation`, insert:

```cpp
    inline virtual QFont label_font() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QFont();
    }

    inline virtual QColor label_color() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QColor();
    }

    inline virtual QFont tick_label_font() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QFont();
    }

    inline virtual QColor tick_label_color() const noexcept
    {
        WARN_ABSTRACT_METHOD;
        return QColor();
    }
```

Finally, in the existing `signals:` block (under `#ifdef BINDINGS_H ... signals:`), add four lines after `Q_SIGNAL void label_changed(...)`:

```cpp
    Q_SIGNAL void label_font_changed(const QFont& font);
    Q_SIGNAL void label_color_changed(const QColor& color);
    Q_SIGNAL void tick_label_font_changed(const QFont& font);
    Q_SIGNAL void tick_label_color_changed(const QColor& color);
```

- [ ] **Step 4: Add overrides to `SciQLopPlotAxis` (declarations only)**

In the same header, inside the `SciQLopPlotAxis` class body (lines 209–241 region), after the existing `set_tick_labels_visible` override declaration, add:

```cpp
    void set_label_font(const QFont& font) noexcept override;
    void set_label_color(const QColor& color) noexcept override;
    void set_tick_label_font(const QFont& font) noexcept override;
    void set_tick_label_color(const QColor& color) noexcept override;
```

And in the getters section, after `tick_labels_visible() const noexcept override;`, add:

```cpp
    QFont label_font() const noexcept override;
    QColor label_color() const noexcept override;
    QFont tick_label_font() const noexcept override;
    QColor tick_label_color() const noexcept override;
```

(Implementations come in Task 2.)

- [ ] **Step 5: Touch bindings.xml and recompile**

```bash
touch SciQLopPlots/bindings/bindings.xml && \
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build
```

Expected: clean compile. The link step still succeeds because the abstract base methods all have inline bodies.

- [ ] **Step 6: Commit (interim — without the concrete impl yet, tests still fail; that's OK)**

```bash
git add include/SciQLopPlots/SciQLopPlotAxis.hpp tests/manual-tests/test_inspector_properties.py
git commit -m "axis: declare label/tick-label font + color virtuals on interface"
```

### Task 2: Implement concrete font/color setters on SciQLopPlotAxis

**Files:**
- Modify: `src/SciQLopPlotAxis.cpp`
- Test: same `TestAxisFontControls` from Task 1 (now expected to pass)

- [ ] **Step 1: Implement the four setters and four getters**

Append to `src/SciQLopPlotAxis.cpp` (after the existing `set_tick_labels_visible` impl — search for `void SciQLopPlotAxis::set_tick_labels_visible`):

```cpp
void SciQLopPlotAxis::set_label_font(const QFont& font) noexcept
{
    if (!m_axis.isNull() && m_axis->labelFont() != font)
    {
        m_axis->setLabelFont(font);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_font_changed(font);
    }
}

void SciQLopPlotAxis::set_label_color(const QColor& color) noexcept
{
    if (!m_axis.isNull() && m_axis->labelColor() != color)
    {
        m_axis->setLabelColor(color);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT label_color_changed(color);
    }
}

void SciQLopPlotAxis::set_tick_label_font(const QFont& font) noexcept
{
    if (!m_axis.isNull() && m_axis->tickLabelFont() != font)
    {
        m_axis->setTickLabelFont(font);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT tick_label_font_changed(font);
    }
}

void SciQLopPlotAxis::set_tick_label_color(const QColor& color) noexcept
{
    if (!m_axis.isNull() && m_axis->tickLabelColor() != color)
    {
        m_axis->setTickLabelColor(color);
        m_axis->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT tick_label_color_changed(color);
    }
}

QFont SciQLopPlotAxis::label_font() const noexcept
{
    return m_axis.isNull() ? QFont() : m_axis->labelFont();
}

QColor SciQLopPlotAxis::label_color() const noexcept
{
    return m_axis.isNull() ? QColor() : m_axis->labelColor();
}

QFont SciQLopPlotAxis::tick_label_font() const noexcept
{
    return m_axis.isNull() ? QFont() : m_axis->tickLabelFont();
}

QColor SciQLopPlotAxis::tick_label_color() const noexcept
{
    return m_axis.isNull() ? QColor() : m_axis->tickLabelColor();
}
```

- [ ] **Step 2: Recompile + reinstall .so**

```bash
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

Expected: clean compile, `.so` copied.

- [ ] **Step 3: Run TestAxisFontControls; verify all cases pass**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestAxisFontControls|ok|FAIL" | head -20
```

Expected: 7 PASS lines for `test_label_font_round_trip`, `test_label_color_round_trip`, `test_tick_label_font_round_trip`, `test_tick_label_color_round_trip`, `test_label_font_signal_fires_once`, `test_label_font_idempotent_no_emit`, `test_label_color_change_does_not_fire_font_signal`. No FAILs in this test class.

- [ ] **Step 4: Run the full inspector_properties suite to confirm no regressions**

```bash
meson test -C build inspector_properties
```

Expected: all 52+ existing tests still pass (the run summary says `OK`).

- [ ] **Step 5: Commit**

```bash
git add src/SciQLopPlotAxis.cpp
git commit -m "axis: implement label/tick-label font + color setters and getters"
```

### Task 3: SciQLopPlotLegend — font + color surface

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotLegendInterface.hpp`
- Modify: `include/SciQLopPlots/SciQLopPlotLegend.hpp`
- Modify: `src/SciQLopPlotLegend.cpp`
- Test: `tests/manual-tests/test_inspector_properties.py` (new `TestLegendFontControls` class)

The wrapper exposes `set_font` / `font` / `set_color` / `color` — `color` maps to `QCPLegend::setTextColor` (the legend has `font`/`textColor`, not `font`/`color`); we name the wrapper getter `color` to keep it consistent with `SciQLopTextItem::color`.

- [ ] **Step 1: Write the failing test**

Append to `tests/manual-tests/test_inspector_properties.py` (after `TestAxisFontControls`):

```python
class TestLegendFontControls(unittest.TestCase):
    """Legend font + color round-trip and signal isolation."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.legend = self.plot.legend()

    def tearDown(self):
        self.panel.deleteLater()

    def test_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 12)
        f.setItalic(True)
        self.legend.set_font(f)
        got = self.legend.font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 12)
        self.assertTrue(got.italic())

    def test_color_round_trip(self):
        from PySide6.QtGui import QColor
        self.legend.set_color(QColor("#0080ff"))
        self.assertEqual(self.legend.color().name(), "#0080ff")

    def test_font_signal_fires_once(self):
        from PySide6.QtGui import QFont
        emitted = []
        self.legend.font_changed.connect(lambda f: emitted.append(f))
        self.legend.set_font(QFont("Arial", 13))
        self.assertEqual(len(emitted), 1)

    def test_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 13)
        self.legend.set_font(f)
        emitted = []
        self.legend.font_changed.connect(lambda x: emitted.append(x))
        self.legend.set_font(f)
        self.assertEqual(len(emitted), 0)

    def test_color_change_does_not_fire_font_signal(self):
        from PySide6.QtGui import QColor
        emitted = []
        self.legend.font_changed.connect(lambda f: emitted.append(f))
        self.legend.set_color(QColor("magenta"))
        self.assertEqual(len(emitted), 0)
```

- [ ] **Step 2: Run; verify failure**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestLegendFontControls|FAIL|ERROR" | head -20
```

Expected: all `TestLegendFontControls.*` fail with `AttributeError`.

- [ ] **Step 3: Add abstract virtuals + signals on `SciQLopPlotLegendInterface`**

In `include/SciQLopPlots/SciQLopPlotLegendInterface.hpp`, add `#include <QFont>` and `#include <QColor>` near the existing `#include <QObject>`. Then inside the class body (after the existing `set_position` virtual, before the signals block) add:

```cpp
    inline virtual QFont font() const
    {
        WARN_ABSTRACT_METHOD;
        return QFont();
    }

    inline virtual void set_font(const QFont&)
    {
        WARN_ABSTRACT_METHOD;
    }

    inline virtual QColor color() const
    {
        WARN_ABSTRACT_METHOD;
        return QColor();
    }

    inline virtual void set_color(const QColor&)
    {
        WARN_ABSTRACT_METHOD;
    }
```

In the existing `signals:` block, after `Q_SIGNAL void position_changed(...)`, add:

```cpp
    Q_SIGNAL void font_changed(const QFont& font);
    Q_SIGNAL void color_changed(const QColor& color);
```

- [ ] **Step 4: Add overrides on `SciQLopPlotLegend`**

In `include/SciQLopPlots/SciQLopPlotLegend.hpp`, after `void set_position(const QPointF&) override;`, add:

```cpp
    QFont font() const override;
    void set_font(const QFont& font) override;
    QColor color() const override;
    void set_color(const QColor& color) override;
```

- [ ] **Step 5: Implement in `src/SciQLopPlotLegend.cpp`**

Add `#include <layoutelements/layoutelement-legend.h>` near the existing `#include <qcp.h>` (the `QCPLegend` header — verify the include path matches existing usage; alternative is `#include <qcustomplot.h>` which transitively brings it in).

Then append the four method definitions to the end of the file:

```cpp
QFont SciQLopPlotLegend::font() const
{
    return m_legend ? m_legend->font() : QFont();
}

void SciQLopPlotLegend::set_font(const QFont& font)
{
    if (m_legend && m_legend->font() != font)
    {
        m_legend->setFont(font);
        m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT font_changed(font);
    }
}

QColor SciQLopPlotLegend::color() const
{
    return m_legend ? m_legend->textColor() : QColor();
}

void SciQLopPlotLegend::set_color(const QColor& color)
{
    if (m_legend && m_legend->textColor() != color)
    {
        m_legend->setTextColor(color);
        m_legend->parentPlot()->replot(QCustomPlot::rpQueuedReplot);
        Q_EMIT color_changed(color);
    }
}
```

- [ ] **Step 6: Touch bindings.xml + compile + install**

```bash
touch SciQLopPlots/bindings/bindings.xml && \
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

- [ ] **Step 7: Run; verify all TestLegendFontControls pass + no regressions**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestLegendFontControls|FAIL" | head -20
meson test -C build inspector_properties
```

Expected: 5 PASS lines for `TestLegendFontControls.*`, full suite OK.

- [ ] **Step 8: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotLegendInterface.hpp \
        include/SciQLopPlots/SciQLopPlotLegend.hpp \
        src/SciQLopPlotLegend.cpp \
        tests/manual-tests/test_inspector_properties.py
git commit -m "legend: add font + color setters/getters with change signals"
```

### Task 4: SciQLopTextItem — set_font / set_font_family / font

**Files:**
- Modify: `include/SciQLopPlots/Items/SciQLopTextItem.hpp`
- Test: `tests/manual-tests/test_inspector_properties.py` (new `TestTextItemFontControls` class)

This wrapper is header-only inline. Existing API has `set_color`, `set_font_size`, `set_font_color`. We add `set_font(QFont)` / `font()` / `set_font_family(QString)`. Existing methods stay for backward compat.

- [ ] **Step 1: Write the failing test**

Append:

```python
class TestTextItemFontControls(unittest.TestCase):
    """SciQLopTextItem font + font family setters."""

    def setUp(self):
        from SciQLopPlots import SciQLopMultiPlotPanel
        from PySide6.QtCore import QPointF
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        # SciQLopTextItem(plot, text, position, movable=False, coordinates=Pixels)
        from SciQLopPlots import SciQLopTextItem, Coordinates
        self.item = SciQLopTextItem(self.plot, "hello",
                                    QPointF(0.5, 0.5), False,
                                    Coordinates.Data)

    def tearDown(self):
        self.panel.deleteLater()

    def test_set_font_round_trip(self):
        from PySide6.QtGui import QFont
        f = QFont("Courier New", 16)
        f.setBold(True)
        self.item.set_font(f)
        got = self.item.font()
        self.assertEqual(got.family(), "Courier New")
        self.assertEqual(got.pointSize(), 16)
        self.assertTrue(got.bold())

    def test_set_font_family_preserves_size(self):
        # Set initial size via existing setter
        self.item.set_font_size(20.0)
        self.item.set_font_family("Arial")
        self.assertEqual(self.item.font().family(), "Arial")
        self.assertAlmostEqual(self.item.font().pointSizeF(), 20.0, places=1)

    def test_set_font_size_still_works_after_set_font(self):
        # Backward compat: set_font_size keeps working after set_font has been used.
        from PySide6.QtGui import QFont
        self.item.set_font(QFont("Arial", 10))
        self.item.set_font_size(18.0)
        self.assertAlmostEqual(self.item.font_size(), 18.0, places=1)
        self.assertEqual(self.item.font().family(), "Arial")
```

- [ ] **Step 2: Run; verify failure**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestTextItemFontControls|FAIL|ERROR" | head -20
```

Expected: failures with `AttributeError: 'SciQLopTextItem' object has no attribute 'set_font'` (or `font`, `set_font_family`).

- [ ] **Step 3: Add the three methods to `SciQLopTextItem`**

In `include/SciQLopPlots/Items/SciQLopTextItem.hpp`, inside the `SciQLopTextItem` class body, after the existing `font_color()` getter (around line 152), add:

```cpp
    void set_font(const QFont& font)
    {
        qptr_apply(m_item, [font](auto& item) { item->setFont(font); });
    }

    [[nodiscard]] QFont font() const
    {
        return qptr_apply_or(m_item, [](auto& item) { return item->font(); });
    }

    void set_font_family(const QString& family)
    {
        qptr_apply(m_item,
                   [family](auto& item)
                   {
                       auto f = item->font();
                       f.setFamily(family);
                       item->setFont(f);
                   });
    }
```

- [ ] **Step 4: Touch bindings.xml + compile + install**

```bash
touch SciQLopPlots/bindings/bindings.xml && \
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

- [ ] **Step 5: Run; verify all three new tests pass + no regressions**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestTextItemFontControls|FAIL" | head -10
meson test -C build inspector_properties
```

Expected: 3 PASS lines, full suite OK.

- [ ] **Step 6: Commit**

```bash
git add include/SciQLopPlots/Items/SciQLopTextItem.hpp \
        tests/manual-tests/test_inspector_properties.py
git commit -m "text-item: add set_font / font / set_font_family"
```

---

## Phase 2 — Reusable FontDelegate widget

### Task 5: FontDelegate — header + implementation + meson registration

**Files:**
- Create: `include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp`
- Create: `src/FontDelegate.cpp`
- Modify: `SciQLopPlots/meson.build` (add to `moc_headers` and `sources`)
- Test: `tests/manual-tests/test_inspector_properties.py` (new `TestFontDelegate` unit-style class)

`FontDelegate` is a `QPushButton` (mirroring `ColorDelegate`'s base class) whose face displays "Aa" in the current font/color. Clicking it opens a popup `QWidget` with the four controls. The popup is created lazily on first click and reused.

- [ ] **Step 1: Write the failing FontDelegate widget test**

Append to `tests/manual-tests/test_inspector_properties.py`:

```python
class TestFontDelegate(unittest.TestCase):
    """FontDelegate widget surface — getter, setter, signal isolation.

    Driven via the public API (setFont/setColor/font/color + the two
    signals). Internal popup widgets are not exercised here; they're
    indirectly covered by the inspector tests that wire FontDelegate
    to model setters.
    """

    def setUp(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont, QColor
        self.delegate = FontDelegate(QFont("Arial", 10), QColor("black"), None)

    def tearDown(self):
        self.delegate.deleteLater()

    def test_initial_font_and_color(self):
        self.assertEqual(self.delegate.font().family(), "Arial")
        self.assertEqual(self.delegate.font().pointSize(), 10)
        self.assertEqual(self.delegate.color().name(), "#000000")

    def test_set_font_emits_font_changed_only(self):
        from PySide6.QtGui import QFont
        font_emits, color_emits = [], []
        self.delegate.fontChanged.connect(lambda f: font_emits.append(f))
        self.delegate.colorChanged.connect(lambda c: color_emits.append(c))
        self.delegate.setFont(QFont("Courier New", 12))
        self.assertEqual(len(font_emits), 1)
        self.assertEqual(len(color_emits), 0)

    def test_set_color_emits_color_changed_only(self):
        from PySide6.QtGui import QColor
        font_emits, color_emits = [], []
        self.delegate.fontChanged.connect(lambda f: font_emits.append(f))
        self.delegate.colorChanged.connect(lambda c: color_emits.append(c))
        self.delegate.setColor(QColor("#ff00ff"))
        self.assertEqual(len(color_emits), 1)
        self.assertEqual(len(font_emits), 0)

    def test_set_font_idempotent_no_emit(self):
        from PySide6.QtGui import QFont
        f = QFont("Arial", 10)
        emits = []
        self.delegate.fontChanged.connect(lambda x: emits.append(x))
        self.delegate.setFont(f)  # same as initial
        self.assertEqual(len(emits), 0)

    def test_set_color_idempotent_no_emit(self):
        from PySide6.QtGui import QColor
        emits = []
        self.delegate.colorChanged.connect(lambda x: emits.append(x))
        self.delegate.setColor(QColor("black"))  # same as initial
        self.assertEqual(len(emits), 0)
```

- [ ] **Step 2: Run; expected to fail at import (`FontDelegate` not yet exposed)**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestFontDelegate|FAIL|ImportError" | head -10
```

Expected: `ImportError: cannot import name 'FontDelegate'` or every TestFontDelegate.* errors.

- [ ] **Step 3: Create the header `FontDelegate.hpp`**

Write `include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp`:

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
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

#include <QColor>
#include <QFont>
#include <QPointer>
#include <QPushButton>

class QWidget;
class QFontComboBox;
class QSpinBox;
class QToolButton;
class ColorDelegate;

class FontDelegate : public QPushButton
{
    Q_OBJECT

    QFont m_font;
    QColor m_color;

    // Lazily-constructed popup (parented to `this`).
    QPointer<QWidget> m_popup;
    QFontComboBox* m_family = nullptr;
    QSpinBox* m_size = nullptr;
    QToolButton* m_bold = nullptr;
    QToolButton* m_italic = nullptr;
    ColorDelegate* m_color_picker = nullptr;

public:
    FontDelegate(const QFont& font, const QColor& color, QWidget* parent = nullptr);
    virtual ~FontDelegate() = default;

    QFont font() const;
    QColor color() const;

public:
    Q_SLOT void setFont(const QFont& font);
    Q_SLOT void setColor(const QColor& color);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void fontChanged(const QFont& font);
    Q_SIGNAL void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Q_SLOT void toggle_popup();
    void ensure_popup();
    void sync_popup_widgets();
};
```

- [ ] **Step 4: Create the impl `src/FontDelegate.cpp`**

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
-- (header notice as above)
----------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/ColorDelegate.hpp"

#include <QFontComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPoint>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
constexpr int MIN_FONT_SIZE = 4;
constexpr int MAX_FONT_SIZE = 72;
}

FontDelegate::FontDelegate(const QFont& font, const QColor& color, QWidget* parent)
        : QPushButton(parent)
        , m_font(font)
        , m_color(color)
{
    setAutoFillBackground(false);
    setFlat(true);
    setText("Aa");
    setMinimumWidth(48);
    connect(this, &QPushButton::clicked, this, &FontDelegate::toggle_popup);
}

QFont FontDelegate::font() const
{
    return m_font;
}

QColor FontDelegate::color() const
{
    return m_color;
}

void FontDelegate::setFont(const QFont& font)
{
    if (m_font != font)
    {
        m_font = font;
        if (m_popup)
            sync_popup_widgets();
        update();
        Q_EMIT fontChanged(m_font);
    }
}

void FontDelegate::setColor(const QColor& color)
{
    if (m_color != color)
    {
        m_color = color;
        if (m_color_picker)
        {
            QSignalBlocker b(m_color_picker);
            m_color_picker->setColor(color);
        }
        update();
        Q_EMIT colorChanged(m_color);
    }
}

void FontDelegate::toggle_popup()
{
    ensure_popup();
    if (m_popup->isVisible())
    {
        m_popup->hide();
    }
    else
    {
        sync_popup_widgets();
        // Position the popup just below the button.
        m_popup->move(mapToGlobal(QPoint(0, height())));
        m_popup->show();
    }
}

void FontDelegate::ensure_popup()
{
    if (m_popup)
        return;

    m_popup = new QWidget(this, Qt::Popup);
    auto* outer = new QVBoxLayout(m_popup);

    auto* font_row = new QHBoxLayout();
    m_family = new QFontComboBox(m_popup);
    m_family->setCurrentFont(m_font);
    font_row->addWidget(m_family);

    m_size = new QSpinBox(m_popup);
    m_size->setRange(MIN_FONT_SIZE, MAX_FONT_SIZE);
    m_size->setValue(m_font.pointSize() > 0 ? m_font.pointSize() : 10);
    m_size->setSuffix(" pt");
    font_row->addWidget(m_size);
    outer->addLayout(font_row);

    auto* style_row = new QHBoxLayout();
    m_bold = new QToolButton(m_popup);
    m_bold->setText("B");
    m_bold->setCheckable(true);
    m_bold->setChecked(m_font.bold());
    QFont bold_face = m_bold->font();
    bold_face.setBold(true);
    m_bold->setFont(bold_face);
    style_row->addWidget(m_bold);

    m_italic = new QToolButton(m_popup);
    m_italic->setText("I");
    m_italic->setCheckable(true);
    m_italic->setChecked(m_font.italic());
    QFont italic_face = m_italic->font();
    italic_face.setItalic(true);
    m_italic->setFont(italic_face);
    style_row->addWidget(m_italic);
    style_row->addStretch();
    outer->addLayout(style_row);

    auto* color_row = new QHBoxLayout();
    color_row->addWidget(new QLabel("Color"));
    m_color_picker = new ColorDelegate(m_color, m_popup);
    color_row->addWidget(m_color_picker);
    outer->addLayout(color_row);

    auto push_font_from_widgets = [this]()
    {
        QFont f = m_family->currentFont();
        f.setPointSize(m_size->value());
        f.setBold(m_bold->isChecked());
        f.setItalic(m_italic->isChecked());
        setFont(f);
    };

    connect(m_family, &QFontComboBox::currentFontChanged, this, push_font_from_widgets);
    connect(m_size, QOverload<int>::of(&QSpinBox::valueChanged), this, push_font_from_widgets);
    connect(m_bold, &QToolButton::toggled, this, push_font_from_widgets);
    connect(m_italic, &QToolButton::toggled, this, push_font_from_widgets);

    connect(m_color_picker, &ColorDelegate::colorChanged, this, &FontDelegate::setColor);
}

void FontDelegate::sync_popup_widgets()
{
    if (!m_popup)
        return;
    QSignalBlocker bf(m_family);
    QSignalBlocker bs(m_size);
    QSignalBlocker bb(m_bold);
    QSignalBlocker bi(m_italic);
    QSignalBlocker bc(m_color_picker);
    m_family->setCurrentFont(m_font);
    m_size->setValue(m_font.pointSize() > 0 ? m_font.pointSize() : 10);
    m_bold->setChecked(m_font.bold());
    m_italic->setChecked(m_font.italic());
    m_color_picker->setColor(m_color);
}

void FontDelegate::paintEvent(QPaintEvent* event)
{
    QPushButton::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(m_color);
    QFont preview = m_font;
    if (preview.pointSize() <= 0)
        preview.setPointSize(10);
    painter.setFont(preview);
    painter.drawText(rect(), Qt::AlignCenter, "Aa");
}
```

- [ ] **Step 5: Register in `SciQLopPlots/meson.build`**

In the `moc_headers` list, after the existing `Delegates/ColorDelegate.hpp` line (around line 129), add:

```python
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp',
```

In the `sources` list, after the existing `'../src/ColorDelegate.cpp',` line (around line 255), add:

```python
            '../src/FontDelegate.cpp',
```

- [ ] **Step 6: Add the type to `bindings/bindings.xml` so Python sees it**

Search `SciQLopPlots/bindings/bindings.xml` for the existing entry `<object-type name="ColorDelegate"`. After that line, add a sibling line:

```xml
    <object-type name="FontDelegate" parent-management="yes"/>
```

Also ensure `FontDelegate.hpp` is reachable from `bindings/bindings.h`. Open `SciQLopPlots/bindings/bindings.h` and after the `#include` for `ColorDelegate.hpp` (search for `ColorDelegate`), add:

```cpp
#include <SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp>
```

- [ ] **Step 7: Compile + install**

```bash
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

Expected: clean compile.

- [ ] **Step 8: Verify the vtable symbol is defined (Q_OBJECT sanity)**

```bash
nm -C build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so | grep "vtable for FontDelegate"
```

Expected: a line ending in `vtable for FontDelegate` whose first column begins with a hex address and a lowercase letter (typically `d` or `V`). If the line begins with `U`, the moc didn't run — re-check `moc_headers` list.

- [ ] **Step 9: Run TestFontDelegate; verify pass + no regression**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestFontDelegate|FAIL" | head -20
meson test -C build inspector_properties
```

Expected: 5 PASS lines for TestFontDelegate.*, full suite OK.

- [ ] **Step 10: Commit**

```bash
git add include/SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp \
        src/FontDelegate.cpp \
        SciQLopPlots/meson.build \
        SciQLopPlots/bindings/bindings.xml \
        SciQLopPlots/bindings/bindings.h \
        tests/manual-tests/test_inspector_properties.py
git commit -m "delegates: add FontDelegate popup-style font + color picker"
```

---

## Phase 3 — Inspector delegate updates

### Task 6: Axis delegate — Label group with FontDelegate

**Files:**
- Modify: `src/SciQLopPlotAxisDelegate.cpp`
- Test: extend `TestPlotAxisDelegate` in `test_inspector_properties.py`

The existing axis delegate adds a bare `Label` row (line 87–96 of `SciQLopPlotAxisDelegate.cpp`). This task moves the QLineEdit into a `Label` `QGroupBox` and adds a `FontDelegate` next to it.

- [ ] **Step 1: Add a failing test for the Label group's FontDelegate**

In `test_inspector_properties.py`, locate `class TestPlotAxisDelegate(unittest.TestCase):` and append these methods inside the class (preserving indentation):

```python
    def test_label_group_present_with_font_delegate(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        panel, plot = self._make_xy_panel()
        try:
            delegate = make_delegate_for(plot.x_axis())
            box = find_group(delegate, 'Label')
            self.assertIsNotNone(box, "Label group should exist")
            self.assertIsNotNone(box.findChild(QLineEdit),
                                 "Label group should contain the QLineEdit for label text")
            self.assertIsNotNone(box.findChild(FontDelegate),
                                 "Label group should contain a FontDelegate")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            font_widget.setFont(QFont("Courier New", 13))
            self.assertEqual(ax.label_font().family(), "Courier New")
            self.assertEqual(ax.label_font().pointSize(), 13)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            ax.set_label_font(QFont("Arial", 17))
            self.assertEqual(font_widget.font().family(), "Arial")
            self.assertEqual(font_widget.font().pointSize(), 17)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Label')
            font_widget = box.findChild(FontDelegate)
            font_widget.setColor(QColor("#abcdef"))
            self.assertEqual(ax.label_color().name(), "#abcdef")
            delegate.deleteLater()
        finally:
            panel.deleteLater()
```

- [ ] **Step 2: Run; verify failure (no `Label` QGroupBox yet)**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "test_label_(group|font|color)|FAIL" | head -20
```

Expected: 4 failures.

- [ ] **Step 3: Refactor `SciQLopPlotAxisDelegate.cpp` to wrap label in a group**

Add an include at the top of `src/SciQLopPlotAxisDelegate.cpp` (next to the existing `<QLineEdit>` include):

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
```

Replace the existing label-edit block (the lines starting with `// Label edit (all axis types).` and the next `connect(ax, &SciQLopPlotAxisInterface::label_changed, ...)` block — currently lines 86–96) with:

```cpp
    // Label group: text + font/color picker.
    auto* labelBox = new QGroupBox("Label", this);
    auto* labelLayout = new QFormLayout(labelBox);

    auto* labelEdit = new QLineEdit(ax->label(), labelBox);
    labelLayout->addRow("Text", labelEdit);
    connect(labelEdit, &QLineEdit::editingFinished, ax,
            [labelEdit, ax]() { ax->set_label(labelEdit->text()); });
    connect(ax, &SciQLopPlotAxisInterface::label_changed, this,
            [labelEdit](const QString& s)
            {
                if (labelEdit->text() != s)
                    labelEdit->setText(s);
            });

    auto* labelFontDelegate = new FontDelegate(ax->label_font(), ax->label_color(), labelBox);
    labelLayout->addRow("Font", labelFontDelegate);
    connect(labelFontDelegate, &FontDelegate::fontChanged, ax,
            &SciQLopPlotAxisInterface::set_label_font);
    connect(ax, &SciQLopPlotAxisInterface::label_font_changed, labelFontDelegate,
            &FontDelegate::setFont);
    connect(labelFontDelegate, &FontDelegate::colorChanged, ax,
            &SciQLopPlotAxisInterface::set_label_color);
    connect(ax, &SciQLopPlotAxisInterface::label_color_changed, labelFontDelegate,
            &FontDelegate::setColor);

    m_layout->addRow(labelBox);
```

- [ ] **Step 4: Compile + install**

```bash
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

- [ ] **Step 5: Run; verify the new four tests pass + existing TestPlotAxisDelegate cases still pass**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestPlotAxisDelegate|FAIL" | head -30
```

Expected: all `TestPlotAxisDelegate.*` PASS (the existing `find_child(delegate, QLineEdit)`-based tests still work because the QLineEdit is still findable as a descendant).

- [ ] **Step 6: Commit**

```bash
git add src/SciQLopPlotAxisDelegate.cpp tests/manual-tests/test_inspector_properties.py
git commit -m "axis-delegate: group label edit with FontDelegate in 'Label' QGroupBox"
```

### Task 7: Axis delegate — Tick labels group with FontDelegate

**Files:**
- Modify: `src/SciQLopPlotAxisDelegate.cpp`
- Test: extend `TestPlotAxisDelegate` further

The existing tick-labels-visible BooleanDelegate (currently a bare row) moves into a `Tick labels` QGroupBox alongside a separate FontDelegate.

- [ ] **Step 1: Add failing tests**

Append to `class TestPlotAxisDelegate`:

```python
    def test_tick_labels_group_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        panel, plot = self._make_xy_panel()
        try:
            delegate = make_delegate_for(plot.x_axis())
            box = find_group(delegate, 'Tick labels')
            self.assertIsNotNone(box, "Tick labels group should exist")
            self.assertIsNotNone(box.findChild(QCheckBox),
                                 "Tick labels group should contain visibility QCheckBox")
            self.assertIsNotNone(box.findChild(FontDelegate),
                                 "Tick labels group should contain a FontDelegate")
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_tick_label_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Tick labels')
            fd = box.findChild(FontDelegate)
            fd.setFont(QFont("Courier New", 8))
            self.assertEqual(ax.tick_label_font().family(), "Courier New")
            self.assertEqual(ax.tick_label_font().pointSize(), 8)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_tick_label_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            box = find_group(delegate, 'Tick labels')
            fd = box.findChild(FontDelegate)
            ax.set_tick_label_font(QFont("Arial", 11))
            self.assertEqual(fd.font().family(), "Arial")
            self.assertEqual(fd.font().pointSize(), 11)
            delegate.deleteLater()
        finally:
            panel.deleteLater()

    def test_label_and_tick_label_font_delegates_are_distinct(self):
        # Belt-and-braces: setting label font must NOT change tick-label font.
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        panel, plot = self._make_xy_panel()
        try:
            ax = plot.x_axis()
            delegate = make_delegate_for(ax)
            ax.set_label_font(QFont("Times", 18))
            ax.set_tick_label_font(QFont("Arial", 9))
            label_box = find_group(delegate, 'Label')
            tick_box = find_group(delegate, 'Tick labels')
            self.assertEqual(label_box.findChild(FontDelegate).font().family(), "Times")
            self.assertEqual(tick_box.findChild(FontDelegate).font().family(), "Arial")
            delegate.deleteLater()
        finally:
            panel.deleteLater()
```

- [ ] **Step 2: Run; verify failure**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "tick_label|tick_labels_group|distinct|FAIL" | head -20
```

Expected: 4 failures (no `Tick labels` group exists yet).

- [ ] **Step 3: Refactor the tick-labels-visible block in `SciQLopPlotAxisDelegate.cpp`**

The current code (lines 56–63) creates a bare `BooleanDelegate` for `tick_labels_visible` and adds it via `addWidgetWithLabel`. Replace that block with a QGroupBox containing both the visibility checkbox and a FontDelegate:

Find:

```cpp
    auto* axis_visible_delegate = new BooleanDelegate(ax->tick_labels_visible(), this);
    connect(axis_visible_delegate, &BooleanDelegate::value_changed, ax,
            &SciQLopPlotAxisInterface::set_tick_labels_visible);
    connect(ax, &SciQLopPlotAxisInterface::tick_labels_visible_changed,
            axis_visible_delegate, &BooleanDelegate::set_value);
    addWidgetWithLabel(axis_visible_delegate, "Tick labels visible");
```

Replace with:

```cpp
    auto* tickBox = new QGroupBox("Tick labels", this);
    auto* tickLayout = new QFormLayout(tickBox);

    auto* axis_visible_delegate = new BooleanDelegate(ax->tick_labels_visible(), tickBox);
    tickLayout->addRow("Visible", axis_visible_delegate);
    connect(axis_visible_delegate, &BooleanDelegate::value_changed, ax,
            &SciQLopPlotAxisInterface::set_tick_labels_visible);
    connect(ax, &SciQLopPlotAxisInterface::tick_labels_visible_changed,
            axis_visible_delegate, &BooleanDelegate::set_value);

    auto* tickFontDelegate
        = new FontDelegate(ax->tick_label_font(), ax->tick_label_color(), tickBox);
    tickLayout->addRow("Font", tickFontDelegate);
    connect(tickFontDelegate, &FontDelegate::fontChanged, ax,
            &SciQLopPlotAxisInterface::set_tick_label_font);
    connect(ax, &SciQLopPlotAxisInterface::tick_label_font_changed,
            tickFontDelegate, &FontDelegate::setFont);
    connect(tickFontDelegate, &FontDelegate::colorChanged, ax,
            &SciQLopPlotAxisInterface::set_tick_label_color);
    connect(ax, &SciQLopPlotAxisInterface::tick_label_color_changed,
            tickFontDelegate, &FontDelegate::setColor);

    m_layout->addRow(tickBox);
```

- [ ] **Step 4: Compile + install**

```bash
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

- [ ] **Step 5: Run; verify pass + no regression**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestPlotAxisDelegate|FAIL" | head -30
meson test -C build inspector_properties
```

Expected: all four new cases PASS, full suite OK.

- [ ] **Step 6: Commit**

```bash
git add src/SciQLopPlotAxisDelegate.cpp tests/manual-tests/test_inspector_properties.py
git commit -m "axis-delegate: group tick-labels visibility with FontDelegate"
```

### Task 8: Plot delegate — Legend group with FontDelegate

**Files:**
- Modify: `src/SciQLopPlotDelegate.cpp`
- Test: new `TestPlotDelegateLegendFont` class in `test_inspector_properties.py`

`SciQLopPlotDelegate` currently adds the existing `LegendDelegate` (visibility-only widget) as a bare row. We wrap it in a `Legend` `QGroupBox` and add a `FontDelegate` connected to the legend's font/color signals. `LegendDelegate` itself stays unchanged.

- [ ] **Step 1: Failing test**

Append:

```python
class TestPlotDelegateLegendFont(unittest.TestCase):
    """Plot delegate's Legend group: visibility + FontDelegate."""

    def setUp(self):
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.delegate = make_delegate_for(self.plot)

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_legend_group_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        box = find_group(self.delegate, 'Legend')
        self.assertIsNotNone(box, "Legend group should exist on plot delegate")
        self.assertIsNotNone(box.findChild(FontDelegate),
                             "Legend group should contain a FontDelegate")

    def test_legend_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        fd.setFont(QFont("Courier New", 11))
        self.assertEqual(self.plot.legend().font().family(), "Courier New")
        self.assertEqual(self.plot.legend().font().pointSize(), 11)

    def test_legend_font_model_to_widget(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        self.plot.legend().set_font(QFont("Arial", 14))
        self.assertEqual(fd.font().family(), "Arial")
        self.assertEqual(fd.font().pointSize(), 14)

    def test_legend_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        box = find_group(self.delegate, 'Legend')
        fd = box.findChild(FontDelegate)
        fd.setColor(QColor("#123456"))
        self.assertEqual(self.plot.legend().color().name(), "#123456")
```

- [ ] **Step 2: Run; verify failure**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestPlotDelegateLegendFont|FAIL" | head -10
```

Expected: 4 failures (no `Legend` group on plot delegate yet).

- [ ] **Step 3: Modify `SciQLopPlotDelegate.cpp`**

Add include:

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include <QGroupBox>
```

Replace the existing legend wiring (currently lines 41–47):

```cpp
    auto legend = object->legend();
    auto* legend_delegate = new LegendDelegate(legend->is_visible(), this);
    m_layout->addRow(legend_delegate);
    connect(legend_delegate, &LegendDelegate::visibility_changed, legend,
            &SciQLopPlotLegendInterface::set_visible);
    connect(legend, &SciQLopPlotLegendInterface::visibility_changed, legend_delegate,
            &LegendDelegate::set_visible);
```

with:

```cpp
    auto* legend = object->legend();
    auto* legendBox = new QGroupBox("Legend", this);
    auto* legendLayout = new QFormLayout(legendBox);

    auto* legend_delegate = new LegendDelegate(legend->is_visible(), legendBox);
    legendLayout->addRow(legend_delegate);
    connect(legend_delegate, &LegendDelegate::visibility_changed, legend,
            &SciQLopPlotLegendInterface::set_visible);
    connect(legend, &SciQLopPlotLegendInterface::visibility_changed, legend_delegate,
            &LegendDelegate::set_visible);

    auto* legendFont = new FontDelegate(legend->font(), legend->color(), legendBox);
    legendLayout->addRow("Font", legendFont);
    connect(legendFont, &FontDelegate::fontChanged, legend,
            &SciQLopPlotLegendInterface::set_font);
    connect(legend, &SciQLopPlotLegendInterface::font_changed, legendFont,
            &FontDelegate::setFont);
    connect(legendFont, &FontDelegate::colorChanged, legend,
            &SciQLopPlotLegendInterface::set_color);
    connect(legend, &SciQLopPlotLegendInterface::color_changed, legendFont,
            &FontDelegate::setColor);

    m_layout->addRow(legendBox);
```

- [ ] **Step 4: Compile + install + run**

```bash
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
meson test -C build inspector_properties
```

Expected: 4 new TestPlotDelegateLegendFont tests pass; full suite OK.

- [ ] **Step 5: Commit**

```bash
git add src/SciQLopPlotDelegate.cpp tests/manual-tests/test_inspector_properties.py
git commit -m "plot-delegate: wrap legend visibility + font picker in 'Legend' group"
```

### Task 9: SciQLopTextItemDelegate (new) + Registrations.cpp

**Files:**
- Create: `include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp`
- Create: `src/SciQLopTextItemDelegate.cpp`
- Modify: `src/Registrations.cpp` (register `SciQLopTextItem` type + delegate)
- Modify: `SciQLopPlots/meson.build` (moc_headers + sources)
- Test: new `TestTextItemDelegate` class

`SciQLopTextItem` doesn't currently appear as a tracked node in the inspector tree (it's not in `Registrations.cpp`'s TypeRegistry calls). We add it so users can drop a TextItem on a plot and edit its text + font from the inspector. The delegate exposes a `QLineEdit` for text and a `FontDelegate` for font + color.

- [ ] **Step 1: Failing test**

Append:

```python
class TestTextItemDelegate(unittest.TestCase):
    """Inspector delegate for SciQLopTextItem: text + font controls."""

    def setUp(self):
        from SciQLopPlots import SciQLopTextItem, Coordinates
        from PySide6.QtCore import QPointF
        self.panel = SciQLopMultiPlotPanel(synchronize_x=False)
        x = np.linspace(0, 1, 100)
        y = np.sin(x * 6)
        self.plot, _g = self.panel.plot(
            x, y, plot_type=PlotType.BasicXY,
            graph_type=GraphType.Line, labels=["s"],
        )
        self.item = SciQLopTextItem(self.plot, "label",
                                    QPointF(0.5, 0.5), False,
                                    Coordinates.Data)
        self.delegate = make_delegate_for(self.item)
        self.assertIsNotNone(self.delegate, "TextItem delegate should resolve")

    def tearDown(self):
        self.delegate.deleteLater()
        self.panel.deleteLater()

    def test_text_edit_present(self):
        line_edit = self.delegate.findChild(QLineEdit)
        self.assertIsNotNone(line_edit)
        self.assertEqual(line_edit.text(), "label")

    def test_font_delegate_present(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        fd = self.delegate.findChild(FontDelegate)
        self.assertIsNotNone(fd, "TextItem delegate should embed a FontDelegate")

    def test_font_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QFont
        fd = self.delegate.findChild(FontDelegate)
        fd.setFont(QFont("Courier New", 22))
        self.assertEqual(self.item.font().family(), "Courier New")
        self.assertEqual(self.item.font().pointSize(), 22)

    def test_color_widget_to_model(self):
        from SciQLopPlots.SciQLopPlotsBindings import FontDelegate
        from PySide6.QtGui import QColor
        fd = self.delegate.findChild(FontDelegate)
        fd.setColor(QColor("#22aabb"))
        self.assertEqual(self.item.color().name(), "#22aabb")

    def test_text_widget_to_model(self):
        line_edit = self.delegate.findChild(QLineEdit)
        line_edit.setText("hi")
        line_edit.editingFinished.emit()
        self.assertEqual(self.item.text(), "hi")
```

- [ ] **Step 2: Run; verify failure**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestTextItemDelegate|FAIL" | head -20
```

Expected: every case errors at `make_delegate_for(self.item)` returning None or asserting failure (the item has no registered delegate yet).

- [ ] **Step 3: Create `SciQLopTextItemDelegate.hpp`**

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
-- (full GPL header as in other delegates)
----------------------------------------------------------------------------*/
#pragma once
#include "SciQLopPlots/Inspector/PropertyDelegateBase.hpp"

class SciQLopTextItem;

class SciQLopTextItemDelegate : public PropertyDelegateBase
{
    Q_OBJECT

    SciQLopTextItem* item() const;

public:
    using compatible_type = SciQLopTextItem;
    SciQLopTextItemDelegate(SciQLopTextItem* object, QWidget* parent = nullptr);
    virtual ~SciQLopTextItemDelegate() = default;
};
```

- [ ] **Step 4: Create `src/SciQLopTextItemDelegate.cpp`**

```cpp
/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
-- (full GPL header)
----------------------------------------------------------------------------*/
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp"
#include "SciQLopPlots/Inspector/PropertiesDelegates/Delegates/FontDelegate.hpp"
#include "SciQLopPlots/Items/SciQLopTextItem.hpp"

#include <QLineEdit>

SciQLopTextItem* SciQLopTextItemDelegate::item() const
{
    return as_type<SciQLopTextItem>(m_object);
}

SciQLopTextItemDelegate::SciQLopTextItemDelegate(SciQLopTextItem* object, QWidget* parent)
        : PropertyDelegateBase(object, parent)
{
    auto* it = item();

    auto* textEdit = new QLineEdit(it->text(), this);
    m_layout->addRow("Text", textEdit);
    connect(textEdit, &QLineEdit::editingFinished, it,
            [textEdit, it]() { it->set_text(textEdit->text()); });

    auto* fontDelegate = new FontDelegate(it->font(), it->color(), this);
    m_layout->addRow("Font", fontDelegate);
    connect(fontDelegate, &FontDelegate::fontChanged, it, &SciQLopTextItem::set_font);
    connect(fontDelegate, &FontDelegate::colorChanged, it, &SciQLopTextItem::set_color);
    // SciQLopTextItem has no font_changed/color_changed signals (the surface is
    // setter-only); reverse path is therefore omitted. Adding signals to the
    // text item is a follow-up if needed.

    append_inspector_extensions();
}
```

(Note: `SciQLopTextItem` currently has no `text_changed`/`font_changed` signals — adding those is out of scope for this PR. The reverse path from model→widget for the text item is therefore intentionally absent. The forward path is what the inspector needs.)

- [ ] **Step 5: Register in `Registrations.cpp`**

In `src/Registrations.cpp`, add at the top of the existing includes block:

```cpp
#include "SciQLopPlots/Items/SciQLopTextItem.hpp"
```

and:

```cpp
#include "SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp"
```

In `register_all_types()`, after the existing `types.register_type<SciQLopHistogram2D>(...)` block, add:

```cpp
    types.register_type<SciQLopTextItem>({
        .children = [](QObject*) -> QList<QObject*> { return {}; },
        .connect_children = nullptr,
        .set_selected = nullptr,
        .deletable = true,
    });
```

In the delegates registration section at the bottom, after `delegates.register_type<SciQLopPlotAxisDelegate>();`, add:

```cpp
    delegates.register_type<SciQLopTextItemDelegate>();
```

- [ ] **Step 6: Register the new files in `SciQLopPlots/meson.build`**

In `moc_headers`, after the existing `SciQLopPlotAxisDelegate.hpp` line, add:

```python
    project_source_root + '/include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp',
```

In `sources`, after the existing `'../src/SciQLopPlotAxisDelegate.cpp',` line, add:

```python
            '../src/SciQLopTextItemDelegate.cpp',
```

- [ ] **Step 7: Touch bindings.xml + compile + install + verify vtable**

```bash
touch SciQLopPlots/bindings/bindings.xml && \
PATH=/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH meson compile -C build && \
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
nm -C build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so | grep "vtable for SciQLopTextItemDelegate"
```

Expected: clean compile; nm shows a defined vtable line (lowercase `d`/`V` letter, not `U`).

- [ ] **Step 8: Run; verify all TestTextItemDelegate tests pass + no regression**

```bash
meson test -C build inspector_properties -v 2>&1 | grep -E "TestTextItemDelegate|FAIL" | head -20
meson test -C build inspector_properties
```

Expected: 5 PASS lines, full suite OK.

- [ ] **Step 9: Commit**

```bash
git add include/SciQLopPlots/Inspector/PropertiesDelegates/SciQLopTextItemDelegate.hpp \
        src/SciQLopTextItemDelegate.cpp \
        src/Registrations.cpp \
        SciQLopPlots/meson.build \
        tests/manual-tests/test_inspector_properties.py
git commit -m "delegates: add SciQLopTextItemDelegate with text + font controls"
```

---

## Phase 4 — Final integration

### Task 10: End-to-end verification + push

**Files:** none modified — verification + remote push only.

- [ ] **Step 1: Run the full suite once more from a clean test invocation**

```bash
meson test -C build inspector_properties
```

Expected: line count matches the previous green baseline (52 prior + ~25 new ≈ 77 tests). All PASS, summary `OK`.

- [ ] **Step 2: Run all manual-test suite groups to catch unintended breakage outside inspector_properties**

```bash
meson test -C build --suite manual --suite integration
```

Expected: every test passes. (Manual examples open windows; if the test environment is headless the runner will report a different error than functional regressions — investigate before declaring success.)

- [ ] **Step 3: Inspect the resulting commit list**

```bash
git log --oneline origin/main..HEAD
```

Expected: ~10 commits, each scoped to one task. Commit messages are imperative and include the affected layer prefix (axis:, legend:, delegates:, plot-delegate:).

- [ ] **Step 4: Push to origin (the personal fork)**

```bash
git push origin font-controls-spec
```

Do NOT push to `upstream` (the SciQLop org) — wait for the user to ask.

- [ ] **Step 5: Report ready for PR**

Verify the branch is on origin and report to the user that the work is complete and ready to be opened as a PR against `upstream/main` when they say so.

---

## Notes for the implementer

- **If the popup looks unstyled or off-position on first click**, that's expected; the user is happy to land a Plain rendering and iterate visually after merge. Don't spend time tweaking layout if the tests pass.
- **If `QFontComboBox` is slow to construct on first popup open** (>500 ms perceptible delay), note it in the commit but do not optimize speculatively — the popup is constructed once per delegate, then reused.
- **If a test is flaky** (passes some runs, fails others), `make_delegate_for` may be returning before all `addRow` calls have completed widget realization. Add `_app.processEvents()` between delegate creation and `findChild`. Existing tests don't need this so it's unlikely; flag it if it happens.
- **If `set_label_font` triggers a recursive `label_font_changed` emit** (test_label_font_signal_fires_once expects exactly 1, gets 2+), the cause is almost always missing the `m_axis->labelFont() != font` guard in the setter. Re-verify the != check is present.
- **If `Registrations.cpp` won't compile** because `SciQLopTextItem` is not a `QObject`-derived child of the registered hierarchy, check that `SciQLopBoundingRectItemInterface` (the base) inherits `QObject` — it should, given the `Q_OBJECT` on `SciQLopTextItem`. If not, narrow the `register_type<>` call to whatever base class IS `QObject`-derived.
