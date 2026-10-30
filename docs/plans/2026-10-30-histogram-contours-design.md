# Contours on 2-D histograms (review item 2.12, rest) — design

Approved in chat 2026-10-30. NeoQCP first, then SciQLopPlots.

## Problems found while scoping

1. **Contour labels are invisible on screen.** `QCPColorMap2::drawContourLabels` paints into the
   layer's paint buffer. `QCustomPlot::render` composites, per layer, the paint buffer first and the
   colour-map RHI quad after it (`core.cpp`, "Draw colormap texture quads for this layer"). The quad
   is opaque, so it covers the labels. Exports (`pmNoCaching`) show them; the existing test only
   renders through `toPixmap`.
2. **Export lines and labels are misplaced on a log value axis.** The resampler and the histogram
   binner make log-spaced rows on a log axis (`resample.cpp generateRange`, `BinAxis`), but
   `QCPContourExtractor::extract` maps a grid index to a coordinate linearly
   (`keyRange.lower + i * step`). The GPU lines are right because they live in UV space, stretched
   with the image.
3. **Labels keep old levels after a colour-scale log toggle.** Auto levels depend on the data scale
   type, but the label cache is keyed on the data generation only.

## Design

### 1. One shared contour piece: `QCPContourOverlay` (`src/painting/contour-overlay.{h,cpp}`)

Everything contour-related moves out of `QCPColorMap2` into this class: settings (levels, auto
count, pen, labels on/off), caches and generations, mirroring, GPU line upload, export lines, label
placement and drawing, and the test accessors. It holds a reference to its plottable's
`QCPColormapRenderer`. The plottable calls:

- `dataChanged()` when a new grid arrives,
- `update(grid, painter, imageWasInvalidated, keyAxis, valueAxis)` before `mRenderer.draw`,
- `drawLabels(grid, painter, keyAxis, valueAxis)` after it,
- `releaseGpu()` from `releaseGpuResources`.

`QCPColorMap2` keeps its public contour API as one-line forwards (no API change).
`QCPHistogram2D` gets the same public API.

### 2. One coordinate system: UV, mapped through the image rectangle

Extraction happens once per (grid generation, resolved levels), in UV space (`extractUv`, already
used by the GPU path, already mirrored for reversed axes). Export lines and label positions map UV
to pixels through the same rectangle the renderer draws the image into. That fixes problem 2 for
every axis scale, and removes the second, full-resolution `extract()` pass. The UV extraction keeps
its 300-cell subsampling on screen; exports pass a larger `maxDim` (the grid size) so a printed
figure is not coarser than today.

The label line cache is keyed on the resolved levels as well as the generation (problem 3).

### 3. Labels on the GPU, above the image

`QCPColormapRhiLayer` gets a second textured quad: a transparent, axis-rect-sized overlay image
(device pixels), drawn after the colour-map quad and the contour lines, with the same scissor and
the same pan offset. The two quads share the existing texture pipeline; the per-quad state
(staging image, texture, SRB, vertex buffer, dirty flags) becomes one small struct used twice.

On screen the overlay paints its labels into that image; when exporting, or when the colour map has
no RHI layer (too big for a texture, no RHI), it paints them directly with the painter, as today.

### 4. Histogram contours follow what is shown

`QCPHistogram2D` keeps a "displayed" grid: the binned counts, or a column-normalized copy when
`nColumn` is on, built once per binning or normalization change. That one grid feeds the image, the
colour bounds and the contours, which removes the two hand-written column-sum loops and the
renderer's `NormalizeFn` (no other user). An empty column normalizes to 0, as the image already
shows it; the colour bounds now include that 0 (today they skip empty columns).

### 5. SciQLopPlots

The contour methods and signals move from `SciQLopColorMap` to `SciQLopColorMapBase`, calling the
shared NeoQCP API on whichever plottable the subclass wraps (like `auto_scale_y`). Histogram2d gets
`set_contour_levels`, `set_auto_contour_levels`, pen/colour/width and labels.

## Tests (each written first, failing)

- GPU frame (`grabFramebuffer`, xvfb): a label's pixels differ from the same frame without labels.
- Log value axis: export lines and label positions sit on the GPU line positions (UV mapped).
- Colour-scale log toggle with auto levels: label levels follow `lastContourLevels`.
- Histogram: contours appear (`lastContourLevels` non-empty, UVs present) with and without column
  normalization; with normalization, the levels lie inside the normalized range (<= 1).
- Histogram export draws contour lines (pixel difference with contours off).
- SciQLopPlots: histogram2d contour API round-trip and signals.
- Existing colour-map contour tests stay green unchanged (refactor safety net).

Review MINORs folded in: drop the font-dependent pixel count from the label test (or keep it only
where a font is present); null-check the curve getters' first component; panel theme `destroyed`
handler connected once.
