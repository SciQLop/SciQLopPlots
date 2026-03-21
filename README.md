[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++20](https://img.shields.io/badge/Language-C++20-blue.svg)]()
[![PyPi](https://img.shields.io/pypi/v/sciqlopplots.svg)](https://pypi.python.org/pypi/sciqlopplots)
[![Coverage](https://codecov.io/gh/SciQLop/SciQLopPlots/coverage.svg?branch=main)](https://codecov.io/gh/SciQLop/SciQLopPlots/branch/main)

# SciQLopPlots

A high-performance scientific plotting library built on C++20/Qt6 with Python bindings via Shiboken6 (PySide6). Designed for the [SciQLop](https://github.com/SciQLop/SciQLop) data analysis platform but usable standalone.

## Features

- **Async resampling** — render millions of points smoothly; data is downsampled in background threads via [NeoQCP](https://github.com/SciQLop/NeoQCP) pipelines
- **Multiple plot types** — time series (line graphs), spectrograms (color maps), histograms, parametric curves
- **Interactive** — pan, zoom, data-driven callbacks, vertical/horizontal spans, tracers
- **Multi-plot panels** — synchronized axes, aligned margins, drag-and-drop from product trees
- **Export** — PDF (vector), PNG, JPG, BMP for both individual plots and panels
- **Busy indicator** — visual feedback when data is loading or being processed
- **Cross-platform** — Linux, macOS, Windows

## Quick start

```bash
pip install SciQLopPlots
```

```python
import numpy as np
from PySide6.QtWidgets import QApplication
from SciQLopPlots import SciQLopPlot

app = QApplication([])
plot = SciQLopPlot()

# Static data
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

Connect plot properties with the `>>` operator to build live data pipelines:

```python
from SciQLopPlots import SciQLopPlot

plot = SciQLopPlot()
graph = plot.plot(lambda start, stop: ..., labels=["signal"])

# Axis range changes automatically feed a transform, which pushes data to the graph
plot.x_axis.on.range >> get_data >> graph.on.data

# Direct property forwarding (no transform)
span.on.range >> plot.x_axis.on.range

# Chain multiple steps
source.on.range >> transform >> target.on.data
```

The pipeline handles signal connection, call-style detection (positional args vs `Event` object), and automatic disconnection when objects are destroyed.

Run the gallery for a full feature showcase:

```bash
python tests/manual-tests/gallery.py
```

## Building from source

Requires Qt6, PySide6 == 6.10.2, a C++20 compiler, and Meson.

```bash
# Development build
meson setup build --buildtype=debug
meson compile -C build

# Install as editable Python package
pip install -e . --no-build-isolation
```

## Contributing

Fork the repository, make your changes and submit a pull request.
Bug reports and feature requests are welcome.

## Credits

Development is supported by [CDPP](http://www.cdpp.eu/).
We acknowledge support from [Plas@Par](https://www.plasapar.sorbonne-universite.fr).

## Thanks

- [PySide6](https://doc.qt.io/qtforpython-6/index.html) — Qt bindings and Shiboken6 binding generator
- [NeoQCP](https://github.com/SciQLop/NeoQCP) — fork of [QCustomPlot](https://www.qcustomplot.com/) with async pipelines, multi-dtype support, and QRhi rendering
