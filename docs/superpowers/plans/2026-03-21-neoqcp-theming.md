# NeoQCP Theming Passthrough Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose NeoQCP's QCPTheme API (light/dark presets + custom color roles) through SciQLopPlots to C++ and Python users.

**Architecture:** Thin `SciQLopTheme` QObject wrapper around `QCPTheme`, following the `SciQLopOverlay` pattern. Virtual `set_theme()`/`theme()` on `SciQLopPlotInterface`, overridden in `SciQLopPlot`. Panel-level theme propagation with `plot_added` auto-application.

**Tech Stack:** C++20, Qt6, Shiboken6, Meson

**Spec:** `docs/superpowers/specs/2026-03-21-neoqcp-theming-design.md`

---

### Task 1: Create SciQLopTheme wrapper class

**Files:**
- Create: `include/SciQLopPlots/SciQLopTheme.hpp`

- [ ] **Step 1: Create the header file**

Follow the `SciQLopOverlay` pattern — a QObject wrapping a `QPointer<QCPTheme>`, with snake_case forwarding methods.

```cpp
#pragma once
#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <theme.h>

class SciQLopTheme : public QObject
{
    Q_OBJECT

    QPointer<QCPTheme> m_theme;

    // Private constructor from existing QCPTheme
    explicit SciQLopTheme(QCPTheme* theme, QObject* parent = nullptr);

public:
    virtual ~SciQLopTheme() override = default;

    static SciQLopTheme* light(QObject* parent = nullptr);
    static SciQLopTheme* dark(QObject* parent = nullptr);

    QCPTheme* qcp_theme() const { return m_theme; }

    QColor background() const;
    QColor foreground() const;
    QColor grid() const;
    QColor sub_grid() const;
    QColor selection() const;
    QColor legend_background() const;
    QColor legend_border() const;

    void set_background(const QColor& color);
    void set_foreground(const QColor& color);
    void set_grid(const QColor& color);
    void set_sub_grid(const QColor& color);
    void set_selection(const QColor& color);
    void set_legend_background(const QColor& color);
    void set_legend_border(const QColor& color);

    QString busy_indicator_symbol() const;
    qreal busy_fade_alpha() const;
    int busy_show_delay_ms() const;
    int busy_hide_delay_ms() const;

    void set_busy_indicator_symbol(const QString& symbol);
    void set_busy_fade_alpha(qreal alpha);
    void set_busy_show_delay_ms(int ms);
    void set_busy_hide_delay_ms(int ms);

#ifdef BINDINGS_H
#define Q_SIGNAL
signals:
#endif
    Q_SIGNAL void changed();
};
```

- [ ] **Step 2: Commit**

```bash
git add include/SciQLopPlots/SciQLopTheme.hpp
git commit -m "feat: add SciQLopTheme header wrapping QCPTheme"
```

---

### Task 2: Implement SciQLopTheme

**Files:**
- Create: `src/SciQLopTheme.cpp`

- [ ] **Step 1: Write the implementation**

All getters check `m_theme` pointer (QPointer null-safety), all setters forward to QCPTheme. Factories create a `QCPTheme` via `QCPTheme::light()`/`QCPTheme::dark()` and wrap it.

```cpp
#include <SciQLopPlots/SciQLopTheme.hpp>

SciQLopTheme::SciQLopTheme(QCPTheme* theme, QObject* parent)
    : QObject(parent), m_theme(theme)
{
    if (m_theme)
    {
        m_theme->setParent(this);
        connect(m_theme, &QCPTheme::changed, this, &SciQLopTheme::changed);
    }
}

SciQLopTheme* SciQLopTheme::light(QObject* parent)
{
    return new SciQLopTheme(QCPTheme::light(), parent);
}

SciQLopTheme* SciQLopTheme::dark(QObject* parent)
{
    return new SciQLopTheme(QCPTheme::dark(), parent);
}

// -- Color getters --
QColor SciQLopTheme::background() const { return m_theme ? m_theme->background() : QColor(); }
QColor SciQLopTheme::foreground() const { return m_theme ? m_theme->foreground() : QColor(); }
QColor SciQLopTheme::grid() const { return m_theme ? m_theme->grid() : QColor(); }
QColor SciQLopTheme::sub_grid() const { return m_theme ? m_theme->subGrid() : QColor(); }
QColor SciQLopTheme::selection() const { return m_theme ? m_theme->selection() : QColor(); }
QColor SciQLopTheme::legend_background() const { return m_theme ? m_theme->legendBackground() : QColor(); }
QColor SciQLopTheme::legend_border() const { return m_theme ? m_theme->legendBorder() : QColor(); }

// -- Color setters --
void SciQLopTheme::set_background(const QColor& c) { if (m_theme) m_theme->setBackground(c); }
void SciQLopTheme::set_foreground(const QColor& c) { if (m_theme) m_theme->setForeground(c); }
void SciQLopTheme::set_grid(const QColor& c) { if (m_theme) m_theme->setGrid(c); }
void SciQLopTheme::set_sub_grid(const QColor& c) { if (m_theme) m_theme->setSubGrid(c); }
void SciQLopTheme::set_selection(const QColor& c) { if (m_theme) m_theme->setSelection(c); }
void SciQLopTheme::set_legend_background(const QColor& c) { if (m_theme) m_theme->setLegendBackground(c); }
void SciQLopTheme::set_legend_border(const QColor& c) { if (m_theme) m_theme->setLegendBorder(c); }

// -- Busy indicator getters --
QString SciQLopTheme::busy_indicator_symbol() const { return m_theme ? m_theme->busyIndicatorSymbol() : QString(); }
qreal SciQLopTheme::busy_fade_alpha() const { return m_theme ? m_theme->busyFadeAlpha() : 0.3; }
int SciQLopTheme::busy_show_delay_ms() const { return m_theme ? m_theme->busyShowDelayMs() : 500; }
int SciQLopTheme::busy_hide_delay_ms() const { return m_theme ? m_theme->busyHideDelayMs() : 500; }

// -- Busy indicator setters --
void SciQLopTheme::set_busy_indicator_symbol(const QString& s) { if (m_theme) m_theme->setBusyIndicatorSymbol(s); }
void SciQLopTheme::set_busy_fade_alpha(qreal a) { if (m_theme) m_theme->setBusyFadeAlpha(a); }
void SciQLopTheme::set_busy_show_delay_ms(int ms) { if (m_theme) m_theme->setBusyShowDelayMs(ms); }
void SciQLopTheme::set_busy_hide_delay_ms(int ms) { if (m_theme) m_theme->setBusyHideDelayMs(ms); }
```

- [ ] **Step 2: Commit**

```bash
git add src/SciQLopTheme.cpp
git commit -m "feat: implement SciQLopTheme wrapper"
```

---

### Task 3: Wire up Meson build system

**Files:**
- Modify: `SciQLopPlots/meson.build`

- [ ] **Step 1: Add SciQLopTheme to moc_headers**

In `SciQLopPlots/meson.build`, add to the `moc_headers` list (after the `SciQLopOverlay.hpp` entry at line 137):

```python
project_source_root + '/include/SciQLopPlots/SciQLopTheme.hpp',
```

- [ ] **Step 2: Add SciQLopTheme.cpp to sources**

In the `sources` list, add after `'../src/SciQLopPlotAxis.cpp'` (line 170):

```python
'../src/SciQLopTheme.cpp',
```

- [ ] **Step 3: Build to verify**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
meson setup build --buildtype=debug --wipe 2>&1 | tail -5
meson compile -C build 2>&1 | tail -20
```

Expected: compiles and links without errors.

- [ ] **Step 4: Commit**

```bash
git add SciQLopPlots/meson.build
git commit -m "build: add SciQLopTheme to meson build"
```

---

### Task 4: Add set_theme/theme to SciQLopPlotInterface and SciQLopPlot

**Files:**
- Modify: `include/SciQLopPlots/SciQLopPlotInterface.hpp`
- Modify: `include/SciQLopPlots/SciQLopPlot.hpp`
- Modify: `src/SciQLopPlot.cpp`

- [ ] **Step 1: Add virtual methods to SciQLopPlotInterface**

Add forward declaration of `SciQLopTheme` at the top of `SciQLopPlotInterface.hpp` (after existing includes). Add virtual methods following the `WARN_ABSTRACT_METHOD` pattern used by `time_axis()`, `legend()`, etc.:

```cpp
// Forward declaration (near top, after includes)
class SciQLopTheme;

// In the class body, near the other virtual getters/setters:
inline virtual void set_theme(SciQLopTheme* theme)
{
    Q_UNUSED(theme);
    WARN_ABSTRACT_METHOD;
}

inline virtual SciQLopTheme* theme() const
{
    WARN_ABSTRACT_METHOD;
    return nullptr;
}
```

- [ ] **Step 2: Add override to SciQLopPlot**

In `include/SciQLopPlots/SciQLopPlot.hpp`:

1. Add `#include "SciQLopPlots/SciQLopTheme.hpp"` to includes
2. Add `QPointer<SciQLopTheme> m_theme;` to member variables (in the `protected:` section, after `m_overlay`)
3. Add override declarations:

```cpp
void set_theme(SciQLopTheme* theme) override;
SciQLopTheme* theme() const override { return m_theme; }
```

- [ ] **Step 3: Implement set_theme in SciQLopPlot.cpp**

Add `#include <SciQLopPlots/SciQLopTheme.hpp>` to includes. Connect to the theme's `destroyed` signal to reset when the theme is deleted externally:

```cpp
void SciQLopPlot::set_theme(SciQLopTheme* theme)
{
    if (m_theme)
        disconnect(m_theme, nullptr, this, nullptr);

    m_theme = theme;

    if (theme && theme->qcp_theme())
    {
        m_impl->setTheme(theme->qcp_theme());
        connect(theme, &QObject::destroyed, this, [this]() { m_impl->setTheme(nullptr); });
    }
    else
    {
        m_impl->setTheme(nullptr);
    }
}
```

- [ ] **Step 4: Build to verify compilation**

```bash
meson compile -C build 2>&1 | tail -20
```

Expected: compiles without errors.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/SciQLopPlotInterface.hpp include/SciQLopPlots/SciQLopPlot.hpp src/SciQLopPlot.cpp
git commit -m "feat: add set_theme/theme to plot interface and SciQLopPlot"
```

---

### Task 5: Add theme propagation to SciQLopMultiPlotPanel

**Files:**
- Modify: `include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp`
- Modify: `src/SciQLopMultiPlotPanel.cpp`

- [ ] **Step 1: Add declarations to SciQLopMultiPlotPanel.hpp**

1. Add forward declaration `class SciQLopTheme;` near the top
2. Add member variables in the `private:` section:

```cpp
QPointer<SciQLopTheme> m_theme;
QMetaObject::Connection m_theme_connection;
```

3. Add public methods:

```cpp
void set_theme(SciQLopTheme* theme);
SciQLopTheme* theme() const { return m_theme; }
```

- [ ] **Step 2: Implement set_theme in SciQLopMultiPlotPanel.cpp**

Add `#include <SciQLopPlots/SciQLopTheme.hpp>` to includes. Follow the `set_span_creation_enabled` pattern for iteration and `plot_added` connection:

```cpp
void SciQLopMultiPlotPanel::set_theme(SciQLopTheme* theme)
{
    // Disconnect previous plot_added connection
    if (m_theme_connection)
    {
        disconnect(m_theme_connection);
        m_theme_connection = {};
    }

    m_theme = theme;
    if (theme)
        theme->setParent(this);

    for (auto& p : plots())
    {
        if (auto* sp = dynamic_cast<SciQLopPlot*>(p.data()))
            sp->set_theme(theme);
    }

    if (theme)
    {
        m_theme_connection = connect(this, &SciQLopMultiPlotPanel::plot_added, this,
            [this](SciQLopPlotInterface* plot)
            {
                if (m_theme)
                {
                    if (auto* sp = dynamic_cast<SciQLopPlot*>(plot))
                        sp->set_theme(m_theme);
                }
            });
    }
}
```

- [ ] **Step 3: Build to verify compilation**

```bash
meson compile -C build 2>&1 | tail -20
```

Expected: compiles without errors.

- [ ] **Step 4: Commit**

```bash
git add include/SciQLopPlots/MultiPlots/SciQLopMultiPlotPanel.hpp src/SciQLopMultiPlotPanel.cpp
git commit -m "feat: add theme propagation to SciQLopMultiPlotPanel"
```

---

### Task 6: Add Python bindings

**Files:**
- Modify: `SciQLopPlots/bindings/bindings.h`
- Modify: `SciQLopPlots/bindings/bindings.xml`

- [ ] **Step 1: Add include to bindings.h**

Add after the existing `#include <SciQLopPlots/SciQLopOverlay.hpp>` line:

```cpp
#include <SciQLopPlots/SciQLopTheme.hpp>
```

- [ ] **Step 2: Add SciQLopTheme to bindings.xml**

Add the `object-type` entry after the existing `SciQLopOverlay` entry (around line 311):

```xml
<object-type name="SciQLopTheme" parent-management="yes">
    <modify-function signature="qcp_theme()const" remove="all"/>
</object-type>
```

- [ ] **Step 3: Add set_theme to SciQLopPlotInterface in bindings.xml**

Inside the existing `<object-type name="SciQLopPlotInterface" ...>` block, add:

```xml
<modify-function signature="set_theme(SciQLopTheme*)">
    <modify-argument index="1">
        <parent index="this" action="add"/>
    </modify-argument>
</modify-function>
```

- [ ] **Step 4: Add set_theme to SciQLopMultiPlotPanel in bindings.xml**

Inside the existing `<object-type name="SciQLopMultiPlotPanel" ...>` block, add:

```xml
<modify-function signature="set_theme(SciQLopTheme*)">
    <modify-argument index="1">
        <parent index="this" action="add"/>
    </modify-argument>
</modify-function>
```

- [ ] **Step 5: Build the Python bindings**

```bash
meson compile -C build 2>&1 | tail -30
```

Expected: shiboken generates wrappers and compiles without errors.

- [ ] **Step 6: Commit**

```bash
git add SciQLopPlots/bindings/bindings.h SciQLopPlots/bindings/bindings.xml
git commit -m "feat: expose SciQLopTheme in Python bindings"
```

---

### Task 7: Verify Python exports and add manual test

**Files:**
- Possibly modify: `SciQLopPlots/__init__.py`
- Create: `tests/manual-tests/test_theming.py`

- [ ] **Step 1: Check if SciQLopTheme is auto-exported**

`SciQLopPlots/__init__.py` uses `from .SciQLopPlotsBindings import *` which should auto-export `SciQLopTheme`. Verify:

```bash
QT_QPA_PLATFORM=offscreen python -c "from SciQLopPlots import SciQLopTheme; t = SciQLopTheme.dark(); print(t.background().name())"
```

Expected: prints `#1e1e1e`. If not, add `SciQLopTheme` to the explicit imports in `__init__.py`.

- [ ] **Step 2: Write the test script**

Follow the `gallery.py` pattern — create a panel with plots, add toggle buttons:

```python
"""SciQLopPlots Theming Demo — toggle between light and dark themes."""
import sys
import numpy as np
from PySide6.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QPushButton, QHBoxLayout
from PySide6.QtGui import QColor

from SciQLopPlots import (
    SciQLopMultiPlotPanel, SciQLopTheme,
    PlotType, SciQLopPlotRange,
)


def sample_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([np.sin(x / 50), np.cos(x / 30)])
    return x, y


def main():
    app = QApplication(sys.argv)
    win = QMainWindow()
    central = QWidget()
    layout = QVBoxLayout(central)

    panel = SciQLopMultiPlotPanel(synchronize_x=False, synchronize_time=True)
    panel.plot(sample_data, labels=["sin", "cos"], plot_type=PlotType.TimeSeries)
    panel.plot(sample_data, labels=["sin", "cos"], plot_type=PlotType.TimeSeries)

    dark_theme = SciQLopTheme.dark()
    light_theme = SciQLopTheme.light()
    is_dark = [False]

    def toggle():
        is_dark[0] = not is_dark[0]
        panel.set_theme(dark_theme if is_dark[0] else light_theme)
        btn.setText("Switch to Light" if is_dark[0] else "Switch to Dark")

    btn_layout = QHBoxLayout()
    btn = QPushButton("Switch to Dark")
    btn.clicked.connect(toggle)
    btn_layout.addWidget(btn)

    # Per-plot override demo: get the first plot from the panel's plots list
    custom_theme = SciQLopTheme.dark()
    custom_theme.set_background(QColor("#1a3a1a"))
    custom_theme.set_selection(QColor("#00ff88"))

    def apply_custom():
        plots = panel.plots()
        if plots:
            plots[0].set_theme(custom_theme)

    btn2 = QPushButton("Custom Theme on Plot 0")
    btn2.clicked.connect(apply_custom)
    btn_layout.addWidget(btn2)

    layout.addLayout(btn_layout)
    layout.addWidget(panel)
    win.setCentralWidget(central)
    win.resize(900, 600)
    win.show()

    panel.set_x_axis_range(SciQLopPlotRange(0, 500))

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run the test**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
QT_QPA_PLATFORM=xcb python tests/manual-tests/test_theming.py
```

Expected: window opens with two plots, buttons toggle themes.

- [ ] **Step 4: Commit**

```bash
git add tests/manual-tests/test_theming.py
git commit -m "test: add theming manual test with light/dark toggle"
```
