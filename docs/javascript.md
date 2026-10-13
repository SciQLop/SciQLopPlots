# JavaScript (WebAssembly)

SciQLopPlots also runs in the browser.
The same C++ plots, compiled to WebAssembly, draw lines, scatters and spectrograms that pan and zoom smoothly with millions of points.
A small ES module, `sciqlopplots.mjs`, gives them a JavaScript API.

**[Open the gallery](https://sciqlop.github.io/SciQLopPlots/latest/)** to see it run.
Every example shows its own source.

## Getting it

Each release publishes the library twice:

- **Hosted**, on GitHub Pages, one folder per version:
  `https://sciqlop.github.io/SciQLopPlots/v0.52.0/sciqlopplots.mjs`.
  `latest/` follows the newest release; pin a version for anything that must not change.
- **As a tarball**, `sciqlopplots-wasm-<version>.tar.gz`, attached to the
  [GitHub release](https://github.com/SciQLop/SciQLopPlots/releases), to serve from your own site.

The library is five files that must stay in one folder:
`sciqlopplots.mjs`, `sciqlopplots.js`, `sciqlopplots.wasm`, `qtloader.js` and `coi-serviceworker.js`.

### The page must be cross-origin isolated

SciQLopPlots resamples on web workers that share memory, and browsers only allow that on a
[cross-origin isolated](https://developer.mozilla.org/en-US/docs/Web/API/Window/crossOriginIsolated) page.
Serve the page with these two headers:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Where you cannot set headers (GitHub Pages, for one), load `coi-serviceworker.js` first in the page's `<head>`.
It installs a service worker that adds them, and reloads the page once:

```html
<script src="coi-serviceworker.js"></script>
```

It must be served from your own site, at or above the page's folder.
`load()` throws a clear error when the page is not isolated.

## Quick start

```html
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <script src="coi-serviceworker.js"></script>
</head>
<body>
  <div id="plots" style="height: 500px"></div>
  <script type="module">
    import { load } from './sciqlopplots.mjs';

    const t = Float64Array.from({ length: 10_000 }, (_, i) => 1.6e9 + i);  // seconds, UTC
    const bx = t.map((_, i) => Math.sin(i / 300)), by = t.map((_, i) => Math.cos(i / 300));

    const spz = await load();
    const panel = spz.panel(document.querySelector('#plots'), { timeRange: [t[0], t.at(-1)] });
    panel.addPlot().line(t, [bx, by], { labels: ['Bx', 'By'] });
  </script>
</body>
</html>
```

Drag or scroll to pan in time, Ctrl+scroll to zoom in time, Shift+scroll to zoom the y axis.

## Concepts

- **Time** is in seconds since 1970-01-01 UTC, as `Float64Array`s.
  Single times (ranges, spans) also accept `Date` objects.
- **A panel** fills one element and follows its size: give the element a height.
  Its plots stack vertically and share one time axis.
  A page can hold several panels, each with its own time range and theme.
- **Set the time range before adding plots.**
  A new plot fits its y axis to the data inside the time range.
- **Data** is a typed array (`Float64Array` or `Float32Array`) or a plain number array.
  Several components are given as an array of columns: `[bx, by, bz]`.
  The library copies the data: changing the array afterwards changes nothing until `setData`.
- **Your page owns the data.**
  After a pan or a zoom the panel emits `timeRangeChanged`; fetch what is in view and call `setData`.
  See the [data on demand](https://sciqlop.github.io/SciQLopPlots/latest/gallery/data-on-demand.html) example.

## API

### `load({ base })`

Loads the module and starts it. Returns a promise of a `SciQLopPlots` object.
`base` is the URL of the folder holding the library files; the default is the folder of `sciqlopplots.mjs`.

### `SciQLopPlots`

| Member | Description |
|---|---|
| `version` | The SciQLopPlots version, e.g. `"0.52.0"`. |
| `panel(element, { theme, timeRange })` | A new `Panel` filling `element`. `theme`: `'light'`, `'dark'` or `{ base, colors }` (see `setTheme`). `timeRange`: `[start, stop]`. |

### `Panel`

| Member | Description |
|---|---|
| `addPlot()` | A new `Plot` at the bottom of the panel. |
| `setTimeRange(start, stop)` | Shows this time range on every plot. |
| `timeRange` | `[start, stop]` in view. |
| `on('timeRangeChanged', (start, stop) => …)` | Called after every pan or zoom. |
| `setTheme(base, colors)` | `base`: `'light'` or `'dark'`. `colors` (optional, CSS colours): `background`, `foreground`, `grid`, `subGrid`, `selection`, `legendBackground`, `legendBorder`. |
| `addSpan(start, stop, { color, readOnly, tooltip })` | A `Span` across every plot. |
| `export(format, { width, height })` | A `Blob` of the panel: `'png'` (default), `'jpg'` or `'pdf'`. Width and height default to the panel's size. |
| `clear()` | Removes every plot. |
| `remove()` | Removes the panel and frees its element. |

### `Plot`

| Member | Description |
|---|---|
| `line(t, values, { labels, colors, name })` | A `Graph`: one line per column of `values`. |
| `scatter(t, values, { labels, colors, name })` | A `Graph` drawn with markers. |
| `colormap(t, y, z, { name, yLog, zLog, gradient })` | A `ColorMap` (spectrogram). `y`: the `m` channels, or `n × m` values when they change with time. `z`: `n × m` values, row-major (`z[i * m + j]` is time `i`, channel `j`). `gradient`: `'Jet'`, `'Viridis'`, `'Plasma'`, `'Inferno'`, `'Magma'`, `'Cividis'`, `'Turbo'`, `'Coolwarm'`, … |
| `setAxis(which, { label, log, range })` | `which`: `'x'`, `'y'`, or `'z'` (a spectrogram's colour axis). `range`: `[min, max]`. |
| `axisRange(which)` | `[min, max]` of that axis. |
| `remove()` | Removes the plot from its panel. |

### `Graph` and `ColorMap`

| Member | Description |
|---|---|
| `setData(t, values)` / `setData(t, y, z)` | Replaces the data; same shapes as when created. |
| `setVisible(visible)` | Shows or hides it. |
| `remove()` | Removes it from its plot. |

### `Span`

| Member | Description |
|---|---|
| `range` / `setRange(start, stop)` | The time interval it covers. |
| `on('rangeChanged', (start, stop) => …)` | After the user drags or resizes it. |
| `on('selectionChanged', (selected) => …)` | When the user selects or deselects it. |
| `on('deleteRequested', () => …)` | When the user asks to delete it (Delete or Backspace on a selected span). |
| `remove()` | Removes it. |

Errors (an unknown gradient, a removed object, data of the wrong size) are thrown as exceptions.

## Limits

- **Size**: about 17 MB of WebAssembly, 5 MB once compressed (servers send it brotli or gzip compressed).
  Browsers cache it across pages.
- **Browsers**: it needs WebGL 2 and shared-memory workers (SharedArrayBuffer), which recent Firefox, Chrome, Edge and Safari have. The tests run in Firefox.
- **Data types**: `Float64Array` and `Float32Array` (other arrays are converted to `Float64Array`).
- Fonts are the ones bundled with Qt; page CSS does not style the plots.

## The first API

The first browser build had flat functions on the module object instead of objects
(`spz.addPlot()`, `spz.line(plot, x, y, labels)`, `spz.Buffer.fromArray(...)`, …), loaded with Qt's `qtLoad`
and drawing into the first container element.
They still work, for pages written against them, but new pages should use `sciqlopplots.mjs`.

## Building it

The browser build cross-compiles SciQLopPlots with Emscripten against Qt for WebAssembly (multithread).
The commands are at the top of [`wasm/meson.build`](https://github.com/SciQLop/SciQLopPlots/blob/main/wasm/meson.build):
a meson cross build, then `wasm/build_site.sh` to assemble the library, the gallery and the browser tests,
which `wasm/tests/run.mjs` runs in Firefox.
