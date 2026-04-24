# GPU-Accelerated Scatter Rendering

## Problem

Scatter plots use `QCPGraph2`/`QCPMultiGraph` with `lsNone` + marker shape. The draw path calls `QCPScatterStyle::drawShape()` per point — one or more QPainter primitive calls (drawEllipse, drawRect, drawPolygon) for each marker. At 1M points this is prohibitively slow.

Additionally, the adaptive sampling (`getOptimizedLineData`) is line-oriented: it produces min/max envelopes per pixel column, which is wrong for scatter data where spatial 2D deduplication is needed.

## Design

Sprite-based instanced rendering, following the existing `QCPPlottableRhiLayer` pattern.

### Marker Sprite

When scatter style changes (shape, size, pen, brush), render the marker once into a small QImage (e.g. 64x64) using existing `QCPScatterStyle::drawShape()` via QPainter. Upload as a 2D RGBA texture. Cache and reuse until style changes.

This gives transparent support for all marker shapes including `ssCustom` pixmaps — no per-shape shader math.

### Per-Instance Vertex Data

3 floats per point without color axis, 4 with:

| Field | Type | Description |
|-------|------|-------------|
| x | float | pixel x coordinate |
| y | float | pixel y coordinate |
| color_value | float | optional scalar in [0,1] mapped through gradient |

Without color axis, the sprite texture carries full RGBA (marker color baked in). With color axis, the sprite provides only the alpha mask and the fragment shader samples a 1D colormap texture for RGB.

### Color Axis (Optional)

- User provides `(x, y, c)` data where `c` is a scalar column
- `c` is normalized to [0,1] by data range (or a user-specified range)
- A `QCPColorGradient` is rendered into a 1D texture (256 or 512 texels)
- Fragment shader: `finalColor = texture(colormap, scalar) * sprite.a`
- If user wants color = f(x), they just repeat x in the c column

### Shaders

`scatter.vert`:
- Quad vertex input: corner offset (2 floats, shared geometry)
- Instance input: x, y, color_value (3-4 floats)
- UBO: viewport width/height, yFlip, dpr, offsetX/Y (pan), markerHalfSize
- Positions each quad corner in NDC from instance (x,y) + corner offset scaled by markerHalfSize

`scatter.frag`:
- Samples sprite texture at UV for alpha mask
- Uniform `useColorAxis` flag:
  - true: sample 1D colormap texture at color_value for RGB, multiply by sprite alpha
  - false: use sprite RGBA directly (color baked into sprite)
- Output premultiplied alpha

### Integration

New `QCPScatterRhiLayer` class in `src/painting/`, same lifecycle pattern as `QCPPlottableRhiLayer`:

- Owns: vertex buffer (dynamic, grows), quad index buffer (static), UBO, sprite texture, colormap texture, pipeline, SRB
- `addScatter(points, scatterStyle, clipRect, colorGradient?)` — called during replot
- `uploadResources(updates, outputSize)` — uploads vertex data + textures when dirty
- `render(cb, outputSize)` — one instanced draw call per scatter entry
- `clear()` — called at start of replot cycle

### Draw Path Changes

In `QCPGraph2::draw()` and `QCPMultiGraph::draw()`, the scatter section:

```
// Current path (per-point QPainter calls):
for (int i = 0; i < lines.size(); i += skip)
    mScatterStyle.drawShape(painter, sx, sy);

// New path:
if (rhi available && !isExportMode) {
    // Build float buffer of (x, y [, color_value]) from lines
    // Call scatterRhiLayer()->addScatter(buffer, mScatterStyle, clipRect)
} else {
    // Existing QPainter loop (export fallback)
}
```

The `coordToPixel` conversion already happened when building `lines`, so the float buffer is just extracting x,y from the QPointF vector.

### Sprite Cache

`QCPScatterStyle` gets a sprite cache:
- Key: (shape, size, pen, brush) tuple
- Value: QImage (rendered marker)
- Invalidated on any style property change
- Shared across all graphs using the same style (pointer-keyed or value-keyed)

### Pan Optimization

Same as `QCPPlottableRhiLayer`: the UBO carries `offsetX/offsetY`. On pan, only the UBO is updated — vertex data stays on GPU. The existing line cache rebuild threshold triggers a full vertex re-upload when panning far enough.

### Export Path

PDF/PNG export uses `pmVectorized`/`pmNoCaching` painter modes. These continue to use the existing QPainter `drawShape` loop — vector export needs real geometry, not rasterized sprites.

### What Changes Where

| File | Change |
|------|--------|
| `src/painting/scatter-rhi-layer.h/cpp` | New: QCPScatterRhiLayer class |
| `src/painting/shaders/scatter.vert/frag` | New: scatter shaders |
| `src/painting/shaders/meson.build` | Add scatter shader QSB targets |
| `src/painting/shaders/embed_shaders.py` | Add scatter shader embedding |
| `src/scatterstyle.h/cpp` | Add sprite cache (renderToImage method) |
| `src/plottables/plottable-graph2.cpp` | Scatter section: RHI path + QPainter fallback |
| `src/plottables/plottable-multigraph.cpp` | Same scatter section change |
| `src/core.h/cpp` | Add mScatterRhiLayer member, wire into render pipeline |
| `tests/benchmark/benchmark.cpp` | Add 1M scatter benchmark |
| `tests/auto/` | Scatter rendering correctness tests |

### Out of Scope

- Per-point variable marker size (uniform size from QCPScatterStyle)
- Per-point variable marker shape
- These are not desirable features

### Benchmark Targets

1M points, measure frame time for pan interaction:
- Baseline: current QPainter path
- GPU: new instanced sprite path
- With/without color axis
- Marker shapes: ssDisc, ssSquare, ssCircle
