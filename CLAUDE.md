# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SciQLopPlots is a high-performance scientific plotting library for the [SciQLop](https://github.com/SciQLop/SciQLop) platform. It's a C++20/Qt6 library with Python bindings generated via Shiboken6 (PySide6's binding generator). Built on top of QCustomPlot (via a fork called NeoQCP).

## Build System

Uses **Meson** with `meson-python` as the Python build backend.

### Development Build

```bash
# Configure (debug build)
meson setup build --buildtype=debug

# Configure (debug with optimizations, recommended for profiling)
meson setup build --buildtype=debugoptimized

# Build
meson compile -C build

# Run tests (manual test examples)
meson test -C build
```

### Python Wheel Build

```bash
pip install .
# or for development:
pip install -e . --no-build-isolation
```

### Meson Options

| Option | Type | Default | Description |
|---|---|---|---|
| `trace_refcount` | bool | false | Enable reference count tracing |
| `tracy_enable` | bool | false | Enable Tracy profiling |
| `with_opengl` | bool | true | Enable OpenGL support |

### Build Dependencies

- Qt6 (Core, Widgets, Gui, Svg, PrintSupport, OpenGL)
- PySide6 == 6.11.0 and matching shiboken6/shiboken6_generator
- Python >= 3.10, < 3.15
- numpy
- OpenGL
- C++20 compiler

### Subprojects (Meson wraps)

- **NeoQCP**: Fork of QCustomPlot - the core plotting engine
- **cpp_utils**: C++ utility library
- **hedley**: Compiler portability macros
- **fmt**: Formatting library
- **magic_enum**: Enum reflection
- **tracy**: Optional profiling (when `tracy_enable=true`)

## Architecture

### Source Layout

- `include/SciQLopPlots/` - Public C++ headers
- `src/` - C++ implementation files
- `SciQLopPlots/` - Python package (contains `__init__.py` and binding infrastructure)
- `SciQLopPlots/bindings/` - Shiboken binding definitions
  - `bindings.xml` - Shiboken typesystem XML (defines what gets exposed to Python)
  - `bindings.h` - Master include for bindings generation
  - `helper_scripts/` - Shiboken/build helper scripts
- `tests/manual-tests/` - Python-based example/test scripts (run via `meson test`)

### Key Architectural Layers

**Plot hierarchy** (`include/SciQLopPlots/`):
- `SciQLopPlotInterface` (QFrame base) -> `SciQLopPlot` -> `SciQLopTimeSeriesPlot`, `SciQLopNDProjectionPlot`

**Plotables** (`include/SciQLopPlots/Plotables/`):
- `SciQLopGraphInterface` -> `SciQLopGraphComponent` (individual trace)
- Concrete types: `SciQLopLineGraph`, `SciQLopCurve`, `SciQLopColorMap`, `SciQLopNDProjectionCurves`
- Each plotable has a corresponding **Resampler** (`Resamplers/`) that runs in a worker thread for downsampling large datasets

**Multi-plot containers** (`include/SciQLopPlots/MultiPlots/`):
- `SciQLopMultiPlotPanel` - panel containing multiple vertically stacked plots
- `SciQLopPlotCollection`, `SciQLopPlotContainer` - layout management
- `AxisSynchronizer`, `XAxisSynchronizer`, `TimeAxisSynchronizer` - keep axes aligned across plots
- `VPlotsAlign` - vertical alignment of plot margins

**Items** (`include/SciQLopPlots/Items/`): Overlay items like vertical spans, tracers, straight lines, shapes, text, pixmaps

**Inspector** (`include/SciQLopPlots/Inspector/`): Tree model/view for inspecting and editing plot properties at runtime

**Products** (`include/SciQLopPlots/Products/`): Tree model for browsable data products (drag-and-drop sources)

**Python interface** (`include/SciQLopPlots/Python/`): `PythonInterface.hpp` defines `PyBuffer` and `GetDataPyCallable` types for bridging numpy arrays and Python callbacks to C++

### Python Bindings Pipeline

1. `bindings.xml` defines the typesystem for Shiboken
2. `shiboken-gen.py` / `shiboken-helper.py` invoke shiboken6 to generate C++ wrapper code
3. The generated code is compiled into `SciQLopPlotsBindings` extension module
4. `SciQLopPlots/__init__.py` imports and patches the bindings (adds a unified `plot()` method to plot classes)

### Version Management

Version is tracked in `meson.build` and `SciQLopPlots/__init__.py`, managed via `bumpversion` (configured in `setup.cfg`).
