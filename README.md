[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
![C++20](https://img.shields.io/badge/Language-C++20-blue.svg)
[![PyPi](https://img.shields.io/pypi/v/sciqlopplots.svg)](https://pypi.python.org/pypi/sciqlopplots)
[![Documentation](https://readthedocs.org/projects/sciqlopplots/badge/?version=latest)](https://sciqlopplots.readthedocs.io/en/latest/)

# SciQLopPlots

A high-performance scientific plotting library built on C++20/Qt6 with Python bindings via Shiboken6 (PySide6). Designed for the [SciQLop](https://github.com/SciQLop/SciQLop) data analysis platform but usable standalone.

**[Try it in your browser](https://sciqlop.github.io/SciQLopPlots/):** the gallery runs the same library as WebAssembly, with examples you can edit and run. The [JavaScript guide](https://sciqlopplots.readthedocs.io/en/latest/javascript.html) shows how to use it in your own pages.

## Features

- **Async resampling** — render millions of points smoothly; data is downsampled in background threads via [NeoQCP](https://github.com/SciQLop/NeoQCP) pipelines
- **Multiple plot types** — time series (line graphs), spectrograms (color maps), 2D histograms, parametric curves, N-D projection curves, waterfall plots
- **Interactive** — pan, zoom, data-driven callbacks, vertical/horizontal/rectangular spans, tracers, straight lines, text, shapes, pixmaps
- **Multi-plot panels** — synchronized axes, aligned margins, drag-and-drop from product trees
- **Interval timelines** — events and instrument modes as bars on named lanes, drawn like a logic analyzer's wave view; overlaps stacked by time or category, or forbidden; editable with the mouse
- **Reactive pipelines** — connect plot properties with `>>` to build live data flows
- **Export** — PDF (vector), PNG, JPG, BMP for both individual plots and panels
- **Busy indicator** — visual feedback when data is loading or being processed
- **Runtime inspector** — tree model/view for inspecting and editing plot properties
- **Signal processing** — gap-aware filters, FFT, spectrograms and rolling statistics on NumPy arrays (`SciQLopPlots.dsp`)
- **In the browser** — the same plots from JavaScript, compiled to WebAssembly ([gallery](https://sciqlop.github.io/SciQLopPlots/), [guide](https://sciqlopplots.readthedocs.io/en/latest/javascript.html))
- **Cross-platform** — Linux, macOS and Windows, on x86-64 and ARM64

## Architecture

![Your Python code or web page goes through the bindings to the C++ library: panels hold plots, plots hold plottables and items. It draws with NeoQCP on Qt 6.](https://raw.githubusercontent.com/SciQLop/SciQLopPlots/main/docs/images/architecture.svg)

### Data flow

![A pan or zoom calls your data function on its own thread, worker threads reduce the points to what the pixels can show, and the main thread draws them.](https://raw.githubusercontent.com/SciQLop/SciQLopPlots/main/docs/images/data-flow.svg)

## Quick start

The **[user guide](https://sciqlopplots.readthedocs.io/en/latest/user-guide.html)** walks through everything below and more: plot types, live data, time series, panels, overlays, pipelines and export. Every example in it is tested. The **[API reference](https://sciqlopplots.readthedocs.io/en/latest/api.html)** lists every class and method.

```bash
pip install SciQLopPlots
```

Wheels exist for Python 3.11 to 3.14. They pin PySide6 to an exact version, so installing replaces any other PySide6 in the environment.

```python
import numpy as np
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot

app = QApplication([])
plot = SciQLopPlot()

# Static data and a data function, both on the same plot
x = np.arange(0, 1000, dtype=np.float64)
y = np.sin(x / 100) * np.cos(x / 10)
plot.plot(x, y, labels=["signal"])

# Data callback (called on pan/zoom with visible range)
def get_data(start, stop):
    x = np.arange(start, stop, dtype=np.float64)
    y = np.column_stack([np.sin(x / 100), np.cos(x / 100)])
    return x, y

plot.plot(get_data, labels=["sin", "cos"])

plot.show()
app.exec()
```

### Reactive pipelines

Connect plot properties with the `>>` operator, optionally through a function:

```python
import numpy as np
from PySide6.QtGui import QColor
from SciQLopPlots import SciQLopPlot, SciQLopVerticalSpan, SciQLopPlotRange

plot = SciQLopPlot()
x = np.linspace(0, 10, 1000)
plot.plot(x, np.sin(x), labels=["sin"])
span = SciQLopVerticalSpan(plot, SciQLopPlotRange(2.0, 4.0), QColor(100, 200, 100, 80),
                           read_only=False, visible=True, tool_tip="")

def describe(event):
    return f"{event.value.start():.2f} .. {event.value.stop():.2f}"

span.on.range >> describe >> span.on.tooltip   # the tooltip follows the span

zoomed = SciQLopPlot()
zoomed.plot(x, np.sin(x), labels=["zoom"])
span.on.range >> zoomed.x_axis().on.range      # a second plot shows the span's interval
```

The function gets an event whose `.value` is the new value. The [user guide](https://sciqlopplots.readthedocs.io/en/latest/user-guide.html#reactive-pipelines) lists the properties you can connect.

To see every feature, try the [browser gallery](https://sciqlop.github.io/SciQLopPlots/), or run the desktop gallery from a source checkout:

```bash
python tests/manual-tests/gallery.py
```

## Runtime tracing

SciQLopPlots ships with a built-in tracer that emits Chrome trace JSON, viewable in
[Perfetto](https://ui.perfetto.dev/), [Speedscope](https://www.speedscope.app/), or
`chrome://tracing`. Always compiled in, runtime-toggled, ~1 ns when off.

Enable for a session, then open the file in Perfetto:

```python
from SciQLopPlots import tracing

with tracing.session("/tmp/sciqlop.json"):
    do_the_slow_thing()         # your code here
```

Annotate your own Python work so it shows next to the C++ zones (`plot.replot`,
`setdata.colormap`, `resample.async_2d`, …). `speasy` and `q` below stand for your own code:

```python
with tracing.zone("speasy.fetch", cat="data", product="amda/mms_fgm"):
    data = speasy.get_data(...)

@tracing.traced("layer.eval", cat="layer")
def render_layer(layer): ...

tracing.counter("queue_depth", q.size(), cat="fetch")
```

You can also auto-enable at process start with the `SCIQLOP_TRACE` env var:

```bash
SCIQLOP_TRACE=/tmp/sciqlop.json python my_script.py
```

When `tracy_enable=true` is also passed at build time, the same `PROFILE_HERE_N`
sites feed both the Chrome JSON tracer and the Tracy live-streaming view.

## Building from source

You need Qt 6.11, a C++20 compiler, libclang (for Shiboken) and, in a virtual environment,
the exact PySide6 version the package pins:

```bash
pip install pyside6==6.11.2 shiboken6==6.11.2 shiboken6-generator==6.11.2 \
    numpy meson meson-python ninja

# Qt's tools must be on PATH
export PATH=/path/to/Qt/6.11.2/gcc_64/bin:$PATH
export PKG_CONFIG_PATH=/path/to/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH

# debugoptimized, not debug: without optimisations the resampling is slow
meson setup build --buildtype=debugoptimized
meson compile -C build

# or build and install it as a Python package
pip install . --no-build-isolation
```

The browser build is described in the [JavaScript guide](https://sciqlopplots.readthedocs.io/en/latest/javascript.html).

### Build options

| Option | Type | Default | Description |
|---|---|---|---|
| `trace_refcount` | bool | false | Enable reference count tracing |
| `tracy_enable` | bool | false | Enable Tracy profiling |
| `with_opengl` | bool | true | Enable OpenGL support |

## Contributing

Fork the repository, make your changes and submit a pull request.
Bug reports and feature requests are welcome.

## Credits

Development is supported by [CDPP](http://www.cdpp.eu/).
We acknowledge support from [Plas@Par](https://www.plasapar.sorbonne-universite.fr).

## Thanks

- [PySide6](https://doc.qt.io/qtforpython-6/index.html) — Qt bindings and Shiboken6 binding generator
- [NeoQCP](https://github.com/SciQLop/NeoQCP) — fork of [QCustomPlot](https://www.qcustomplot.com/) with async pipelines, multi-dtype support, and QRhi rendering
