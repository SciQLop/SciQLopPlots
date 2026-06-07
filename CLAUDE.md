# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SciQLopPlots is a high-performance scientific plotting library for the [SciQLop](https://github.com/SciQLop/SciQLop) platform. It's a C++20/Qt6 library with Python bindings generated via Shiboken6 (PySide6's binding generator). Built on top of QCustomPlot (via a fork called NeoQCP).

## Build System

Uses **Meson** with `meson-python` as the Python build backend.

### Development Build

> **STOP — do not run a bare `meson setup build` / `meson compile -C build`.**
> Qt is not on the default PATH and the stale top-level `build/` dir will trigger
> a doomed reconfigure. Use the dedicated in-tree venv + `build-venv` recipe below.
> Never build against (or `cp .so` into) the SciQLop runtime venv during dev.

```bash
# One-time: dedicated in-tree venv (Qt/PySide6 6.11.1, meson pinned to 1.10.2)
cd /var/home/jeandet/Documents/prog/SciQLopPlots
uv venv .venv --python 3.13
VIRTUAL_ENV=$(pwd)/.venv uv pip install \
  pyside6==6.11.1 shiboken6==6.11.1 shiboken6-generator==6.11.1 \
  numpy meson==1.10.2 ninja meson-python pytest pytest-qt scipy

# Every build: Qt MUST be on PATH (6.11.1 first, 6.11.0 after it for `qsb`)
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson setup build-venv --buildtype=debugoptimized   # first time only
$VENV/bin/meson compile -C build-venv

# Run tests from /tmp so the source pkg doesn't shadow the build (no LD_LIBRARY_PATH!)
cd /tmp && PYTHONPATH=<PROJ>/build-venv $VENV/bin/python -m pytest <PROJ>/tests/integration -q
```

Always `--buildtype=debugoptimized` (never plain `debug`: -O0 makes CPU code unusably slow).
After switching branches, a stale shiboken wrapper can fail to regenerate — `rm` the
offending `build-venv/SciQLopPlots/bindings/.../**_wrapper.cpp` and `meson setup
--reconfigure build-venv` to force a clean regen.

### Python Wheel Build

```bash
pip install .   # produces a wheel; NOT for iterative dev
```

For iterative development use the `build-venv` recipe above and `cp build-venv/SciQLopPlots/*.so build-venv/SciQLopPlots/*.py` into the target venv — never `pip install -e .` (it disturbs the venv).

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

**Runtime tracer** (`include/SciQLopPlots/Tracing.hpp` + `src/Tracing.cpp`): always-compiled-in event logger that emits Chrome trace JSON (Perfetto / Speedscope / chrome://tracing). Toggle via `tracing.enable("path.json") / disable() / flush()` from C++ or Python (`from SciQLopPlots import tracing`), or auto-enable with the `SCIQLOP_TRACE` env var. ~1 ns when off (single relaxed atomic load + branch). `Profiling.hpp`'s `PROFILE_HERE_N` feeds both this tracer and Tracy (when `tracy_enable=true` at build time) — instrument once, view in either tool.

### Python Bindings Pipeline

1. `bindings.xml` defines the typesystem for Shiboken
2. `shiboken-gen.py` / `shiboken-helper.py` invoke shiboken6 to generate C++ wrapper code
3. The generated code is compiled into `SciQLopPlotsBindings` extension module
4. `SciQLopPlots/__init__.py` imports and patches the bindings (adds a unified `plot()` method to plot classes)

### Version Management

Version is tracked in `meson.build` and `SciQLopPlots/__init__.py`, managed via `bumpversion` (configured in `setup.cfg`).
