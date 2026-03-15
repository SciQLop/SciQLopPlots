# SciQLopHistogram2D Wrapper Design

## Summary

Wrap NeoQCP's `QCPHistogram2D` plottable in SciQLopPlots, exposing a 2D histogram that takes raw scatter data (x, y) and bins it into a colormap display with async background binning.

## API

### Class: `SciQLopHistogram2D` (inherits `SciQLopColorMapInterface`)

**Data:**
- `set_data(PyBuffer x, PyBuffer y)` — 1D scatter arrays; NeoQCP bins asynchronously
- `data() -> QList<PyBuffer>` — returns current x, y buffers

**Histogram-specific:**
- `set_bins(int key_bins, int value_bins)` / `key_bins()` / `value_bins()`
- `set_normalization(SciQLopHistogram2D::Normalization)` / `normalization()`
- Enum `Normalization { None, Column }` — maps to `QCPHistogram2D::nNone`, `QCPHistogram2D::nColumn`

**Color (inherited from SciQLopColorMapInterface):**
- `set_gradient()`, `set_data_range()`, `set_data_scale_type()`, `set_color_scale()`
- `rescale_data_range()`

### Plot Creation

`SciQLopPlot::add_histogram2d(name, key_bins=100, value_bins=100)` — creates histogram with color scale axis.

### Data Lifetime

Same aliasing `shared_ptr` pattern as `SciQLopColorMap`: PyBuffers stored alongside the data source so numpy memory stays alive while the async pipeline holds a reference.

## Scope

- No function variant (deferred — will use observer pattern later)
- No resampler needed (NeoQCP's async pipeline handles binning)
- No delegate (inspector shows basic properties from `SciQLopColorMapInterface`)

## Files

| Action | File |
|--------|------|
| Create | `include/SciQLopPlots/Plotables/SciQLopHistogram2D.hpp` |
| Create | `src/SciQLopHistogram2D.cpp` |
| Modify | `include/SciQLopPlots/SciQLopPlot.hpp` — add `add_histogram2d` |
| Modify | `src/SciQLopPlot.cpp` — implement `add_histogram2d` |
| Modify | `SciQLopPlots/bindings/bindings.xml` — register type |
| Modify | `SciQLopPlots/bindings/bindings.h` — add include |
| Modify | `SciQLopPlots/meson.build` — add to build |
| Modify | `src/Registrations.cpp` — register inspector type |
| Create | `tests/manual-tests/histogram2d.py` — gallery example |
| Create | test file for the wrapper |
