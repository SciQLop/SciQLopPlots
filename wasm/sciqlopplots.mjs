// SciQLopPlots in the browser: fast, interactive time-series plots (lines, scatters,
// spectrograms) from SciQLopPlots compiled to WebAssembly.
//
//   import { load } from './sciqlopplots.mjs';
//   const spz = await load();
//   const panel = spz.panel(document.querySelector('#plots'));
//   panel.setTimeRange(t[0], t[t.length - 1]);
//   panel.addPlot().line(t, [bx, by, bz], { labels: ['Bx', 'By', 'Bz'] });
//
// Times are seconds since 1970-01-01 UTC (or Date objects where a single time is taken).
// The page needs cross-origin isolation (COOP/COEP headers, or coi-serviceworker.js): the
// module runs on web workers that share memory.

const moduleBase = new URL('.', import.meta.url).href;

function loadScript(src) {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = src;
    script.onload = resolve;
    script.onerror = () => reject(new Error(`SciQLopPlots: could not load ${src}`));
    document.head.append(script);
  });
}

const seconds = (t) => (t instanceof Date ? t.getTime() / 1000 : t);

function asTypedArray(values) {
  if (values instanceof Float64Array || values instanceof Float32Array) return values;
  return Float64Array.from(values);
}

// Columns side by side, row-major: [x0, y0, z0, x1, y1, z1, ...].
function interleave(columns) {
  const rows = columns[0].length, out = new Float64Array(rows * columns.length);
  columns.forEach((column, j) => {
    for (let i = 0; i < rows; i++) out[i * columns.length + j] = column[i];
  });
  return out;
}

// values: one column (a typed array or a number array), or an array of columns.
function columnsOf(values) {
  const isColumnList = Array.isArray(values) && values.length > 0 && typeof values[0] !== 'number';
  if (!isColumnList) return { data: asTypedArray(values), shape: [values.length] };
  if (values.length === 1) return { data: asTypedArray(values[0]), shape: [values[0].length] };
  return { data: interleave(values), shape: [values[0].length, values.length] };
}

class Handle {
  constructor(lib, id) {
    this.lib = lib;
    this.id = id;
  }

  remove() {
    this.lib.m.remove(this.id);
  }

  // Calls fn with SciQLopPlots buffers made from [typedArray, shape] pairs, then frees them.
  withBuffers(arrays, fn) {
    const buffers = arrays.map(({ data, shape }) => this.lib.m.Buffer.fromArray(data, shape));
    try {
      return fn(...buffers);
    } finally {
      buffers.forEach((b) => b.delete());
    }
  }
}

export class Graph extends Handle {
  // t: times; values: one column or an array of columns (one per component).
  setData(t, values) {
    this.withBuffers([columnsOf(t), columnsOf(values)], (x, y) => this.lib.m.setData(this.id, x, y));
  }

  setVisible(visible) {
    this.lib.m.setVisible(this.id, visible);
  }

  // A step longer than threshold x its neighbours' breaks the line (default 1.5). 0 never
  // breaks it, for keys that are uneven on purpose (min/max resampled data); NaN still does.
  setGapThreshold(threshold) {
    this.lib.m.setGapThreshold(this.id, threshold);
  }

  get gapThreshold() {
    return this.lib.m.gapThreshold(this.id);
  }
}

// A projection's trajectory: a Graph that takes the same four arrays as trajectory().
export class Trajectory extends Graph {
  setData(t, x, y, z) {
    this.withBuffers([t, x, y, z].map(columnsOf), (...b) => this.lib.m.setData(this.id, ...b));
  }
}

// y: the channels (m values), or one row of channels per time (n * m values, row-major).
// z: n * m values, row-major (z[i * m + j] is time i, channel j).
function spectrogramArrays(t, y, z) {
  const n = t.length, m = z.length / n;
  if (!Number.isInteger(m)) throw new Error(`SciQLopPlots: z has ${z.length} values, not a multiple of ${n} times`);
  const yShape = y.length === m ? [m] : [n, m];
  return [columnsOf(t), { data: asTypedArray(y), shape: yShape }, { data: asTypedArray(z), shape: [n, m] }];
}

export class ColorMap extends Handle {
  setData(t, y, z) {
    this.withBuffers(spectrogramArrays(t, y, z), (x, yb, zb) => this.lib.m.setData(this.id, x, yb, zb));
  }

  setVisible(visible) {
    this.lib.m.setVisible(this.id, visible);
  }
}

export class Plot extends Handle {
  // options: labels, colors (CSS colours), name.
  line(t, values, options = {}) {
    return new Graph(this.lib, this.withBuffers([columnsOf(t), columnsOf(values)],
      (x, y) => this.lib.m.line(this.id, x, y, options)));
  }

  scatter(t, values, options = {}) {
    return new Graph(this.lib, this.withBuffers([columnsOf(t), columnsOf(values)],
      (x, y) => this.lib.m.scatter(this.id, x, y, options)));
  }

  // options: name, yLog, zLog, gradient ('Jet', 'Viridis', 'Plasma', ...).
  colormap(t, y, z, options = {}) {
    return new ColorMap(this.lib, this.withBuffers(spectrogramArrays(t, y, z),
      (x, yb, zb) => this.lib.m.colormap(this.id, x, yb, zb, options)));
  }

  // which: 'x', 'y' or 'z' (a spectrogram's colour axis). options: label, log, range ([min, max]).
  setAxis(which, options) {
    this.lib.m.setAxis(this.id, which, options);
  }

  axisRange(which) {
    return this.lib.m.axisRange(this.id, which);
  }

  // A curve drawn in the order of its points, so x need not grow: an orbit, a model outline.
  curve(x, y, options = {}) {
    return new Graph(this.lib, this.withBuffers([columnsOf(x), columnsOf(y)],
      (xb, yb) => this.lib.m.curve(this.id, xb, yb, options)));
  }

  setLegendVisible(visible) {
    this.lib.m.setLegendVisible(this.id, visible);
  }

  // event: 'cursorTimeChanged' (time under the mouse, NaN once it leaves).
  on(event, callback) {
    this.lib.m.on(this.id, event, callback);
    return this;
  }
}

// Side-by-side 2-D projections of a 3-D trajectory, one row of a panel.
export class Projection extends Handle {
  // The i-th pane, a Plot: add model outlines to it with curve().
  pane(index) {
    return new Plot(this.lib, this.lib.m.projectionPane(this.id, index));
  }

  // mode: 'independent', 'linked' (axes showing the same dimension move together, the default)
  // or 'sameScale' (and every axis has the same span, with square panes: a circle stays round).
  setAxes(mode) {
    this.lib.m.setProjectionAxes(this.id, mode);
  }

  // t: times; x, y, z: positions. options: name, timeColors (default true: coloured along time).
  trajectory(t, x, y, z, options = {}) {
    return new Trajectory(this.lib, this.withBuffers([t, x, y, z].map(columnsOf),
      (...b) => this.lib.m.trajectory(this.id, ...b, options)));
  }

  // A static 3-D curve, projected on every pane. options: name, color (CSS).
  referenceCurve(x, y, z, options = {}) {
    return new Graph(this.lib, this.withBuffers([x, y, z].map(columnsOf),
      (...b) => this.lib.m.referenceCurve(this.id, ...b, options)));
  }

  // Marks the trajectory at time t on every pane; NaN (or clearTimeMarker()) removes it.
  setTimeMarker(t) {
    this.lib.m.setTimeMarker(this.id, seconds(t));
  }

  clearTimeMarker() {
    this.lib.m.setTimeMarker(this.id, Number.NaN);
  }
}

// A row of a panel showing a page element: a chart from another library (Plotly, Chart.js,
// D3, ...). The panel lays the element over the row and keeps it there; its own events stay its own.
export class ElementRow extends Handle {
  constructor(lib, id, element) {
    super(lib, id);
    this.element = element;
  }

  // The element goes now; the row once Qt deletes it (it hides at once).
  remove() {
    super.remove();
    this.element.remove();
  }
}

// The layer over a panel that holds its rows' elements, clipped to the panel.
function overlayOf(panel) {
  if (!panel.overlay) {
    const element = panel.element;
    if (getComputedStyle(element).position === 'static') element.style.position = 'relative';
    // Qt stacks its windows with z-indices of their own: kept inside one stacking context, they
    // stay under the layer.
    element.querySelector(':scope > #qt-shadow-container').style.isolation = 'isolate';
    panel.overlay = document.createElement('div');
    panel.overlay.style.cssText = 'position:absolute;inset:0;z-index:1;overflow:hidden;pointer-events:none;';
    element.append(panel.overlay);
  }
  return panel.overlay;
}

// rect: where the row is ({ x, y, width, height }), null while it is hidden, 'removed' once gone.
function placeElement(element, rect) {
  if (rect === 'removed') return element.remove();
  element.style.display = rect ? '' : 'none';
  if (rect)
    Object.assign(element.style, { left: `${rect.x}px`, top: `${rect.y}px`,
      width: `${rect.width}px`, height: `${rect.height}px` });
}

export class Span extends Handle {
  get range() {
    return this.lib.m.spanRange(this.id);
  }

  setRange(start, stop) {
    this.lib.m.setSpanRange(this.id, seconds(start), seconds(stop));
  }

  // event: 'rangeChanged' (start, stop), 'selectionChanged' (selected), 'deleteRequested' ().
  on(event, callback) {
    this.lib.m.on(this.id, event, callback);
    return this;
  }
}

// Pinch zooms time as in SciQLop, around the fingers: a trackpad pinch arrives as Ctrl+wheel
// (each event a scale of exp(deltaY / 100), the browsers' convention), a phone pinch as two touch
// pointers. Both are caught on the panel's element before Qt sees them: Qt's own handling zooms
// in fixed wheel steps, and has no two-finger pinch in the browser.
function installPinch(lib, panel) {
  const element = panel.element;
  let pinch = null;          // { distance, middle } of the last two-finger step
  let dragging = null;       // the touch pointer Qt is dragging with: { target, event }
  const local = (x, y) => {
    const r = element.getBoundingClientRect();
    return [x - r.left, y - r.top];
  };
  const geometry = (touches) => {
    const [[ax, ay], [bx, by]] = [touches[0], touches[1]].map((t) => local(t.clientX, t.clientY));
    return { distance: Math.hypot(bx - ax, by - ay), middle: [(ax + bx) / 2, (ay + by) / 2] };
  };
  const swallow = (event) => { event.stopPropagation(); event.preventDefault(); };
  // Events over a row's element belong to the element (another library's chart).
  const ours = (event) => event.isTrusted && !panel.overlay?.contains(event.target);

  element.addEventListener('wheel', (event) => {
    if (!ours(event) || !event.ctrlKey) return;
    swallow(event);
    const [x, y] = local(event.clientX, event.clientY);
    const lines = event.deltaMode === WheelEvent.DOM_DELTA_LINE ? 33 : 1;
    lib.m.pinchTime(panel.id, x, x, y, Math.exp(event.deltaY * lines / 100));
  }, { capture: true, passive: false });

  // Fingers are read from touch events (one per finger change, with every touch); Qt reads
  // pointer events, which it must not see during a pinch.
  element.addEventListener('touchstart', (event) => {
    if (!ours(event) || event.touches.length < 2 || pinch) return;
    pinch = geometry(event.touches);
    // Qt is dragging with the first finger: release it, or that drag would undo the zoom.
    if (dragging) dragging.target.dispatchEvent(new PointerEvent('pointerup', dragging.event));
    dragging = null;
  }, { capture: true, passive: true });
  element.addEventListener('touchmove', (event) => {
    if (!ours(event) || !pinch || event.touches.length < 2) return;
    const step = geometry(event.touches);
    lib.m.pinchTime(panel.id, pinch.middle[0], step.middle[0], step.middle[1],
                    pinch.distance / step.distance);
    pinch = step;
  }, { capture: true, passive: true });
  const touchEnd = (event) => { if (event.touches.length === 0) pinch = null; };
  element.addEventListener('touchend', touchEnd, true);
  element.addEventListener('touchcancel', touchEnd, true);

  for (const type of ['pointerdown', 'pointermove', 'pointerup', 'pointercancel'])
    element.addEventListener(type, (event) => {
      if (!ours(event) || event.pointerType !== 'touch') return;
      if (pinch) return swallow(event);
      if (type === 'pointerdown') dragging = { target: event.composedPath()[0], event };
      else if (type !== 'pointermove') dragging = null;
    }, true);
}

const exportTypes = { png: 'image/png', jpg: 'image/jpeg', pdf: 'application/pdf' };

export class Panel extends Handle {
  constructor(lib, id, element) {
    super(lib, id);
    this.element = element;
    this.rows = new Map();   // id -> ElementRow
    installPinch(lib, this);
    // Qt re-reads a container's size only on a window resize, not when the element alone
    // changes size (a sidebar hiding, a flex layout).
    this.resizeObserver = new ResizeObserver(() => lib.m.qtResizeContainerElement(element));
    this.resizeObserver.observe(element);
  }

  // Plots stack vertically and share the time axis. Set the time range first: a new plot
  // fits its y axis to the data inside it.
  addPlot() {
    return new Plot(this.lib, this.lib.m.addPlot(this.id));
  }

  // A row showing element, e.g. a chart from another library, laid over the row and resized
  // with it (the panel sets its position, left, top, width and height). options: onTimeRange
  // (start, stop), called with the panel's time range now and after every pan or zoom.
  addElement(element, { onTimeRange } = {}) {
    Object.assign(element.style, { position: 'absolute', boxSizing: 'border-box', pointerEvents: 'auto' });
    overlayOf(this).append(element);
    let row = null;
    const place = (rect) => {
      // An exception must not unwind through Qt's layout, nor through a row's destructor.
      try {
        placeElement(element, rect);
        if (rect === 'removed' && row) this.rows.delete(row.id);
      } catch (error) { console.error(error); }
    };
    const timeRange = onTimeRange && ((start, stop) => {
      try { onTimeRange(start, stop); } catch (error) { console.error(error); }
    });
    row = new ElementRow(this.lib, this.lib.m.addElementRow(this.id, place, timeRange), element);
    this.rows.set(row.id, row);
    return row;
  }

  // What plotAt and drops hand back for a row the panel reports.
  rowHandle({ id, kind }) {
    if (kind === 'element') return this.rows.get(id);
    return new (kind === 'projection' ? Projection : Plot)(this.lib, id);
  }

  // The plot under (x, y), in CSS pixels from the panel element's top-left corner, e.g. where
  // something was dropped: { plot, index } (plot is a Plot, a Projection or an ElementRow), or
  // null between and outside plots.
  plotAt(x, y) {
    const hit = this.lib.m.plotAt(this.id, x, y);
    return hit && { plot: this.rowHandle(hit), index: hit.index };
  }

  // Accepts drops of mimeType (an HTML5 drag whose dataTransfer has that type). While the drag is
  // near a plot's top or bottom edge the panel shows where a new plot would go, as SciQLop does.
  // callback(target, data): target is { plot, index, newPlot }: the plot dropped on, or the new
  // plot the panel made for the drop (newPlot true); data is the dropped string.
  acceptDrops(mimeType, callback) {
    this.lib.m.acceptDrops(this.id, mimeType, (target, data) => {
      // An exception must not unwind through Qt's event handling.
      try { callback({ plot: this.rowHandle(target), index: target.index, newPlot: target.newPlot }, data); }
      catch (error) { console.error(error); }
    });
    return this;
  }

  // A row of projections. options: pairs (one [x, y] of dimension indices per pane, default
  // [[0, 1], [1, 2], [2, 0]]), labels (the dimension names), equalAspect (default true: square
  // panes), ranges (a fixed [min, max] per dimension; equal spans give equal units on both axes),
  // axes (see Projection.setAxes; default 'linked').
  addProjection(options = {}) {
    return new Projection(this.lib, this.lib.m.addProjection(this.id, options));
  }

  setTimeRange(start, stop) {
    this.lib.m.setTimeRange(this.id, seconds(start), seconds(stop));
  }

  get timeRange() {
    return this.lib.m.timeRange(this.id);
  }

  // event: 'timeRangeChanged' (start, stop), after a pan or a zoom. Refetch data from here.
  on(event, callback) {
    this.lib.m.on(this.id, event, callback);
    return this;
  }

  // base: 'light' or 'dark'; colors: background, foreground, grid, subGrid, selection,
  // legendBackground, legendBorder (CSS colours), each optional.
  setTheme(base, colors = {}) {
    this.lib.m.setTheme(this.id, base, colors);
  }

  // A vertical band across every plot. options: color (CSS), readOnly, tooltip.
  addSpan(start, stop, options = {}) {
    return new Span(this.lib, this.lib.m.addSpan(this.id, seconds(start), seconds(stop), options));
  }

  // format: 'png', 'jpg' or 'pdf'. A Blob of the whole panel.
  export(format = 'png', { width = 0, height = 0 } = {}) {
    const bytes = this.lib.m.exportPanel(this.id, format, width, height);
    return new Blob([bytes], { type: exportTypes[format] });
  }

  clear() {
    this.lib.m.clear(this.id);
  }

  remove() {
    super.remove();
    this.resizeObserver.disconnect();
    this.overlay?.remove();
    this.lib.m.qtRemoveContainerElement(this.element);
  }
}

// The Qt screen a container element holds: Qt makes one per element it is given.
function screenOf(module, element) {
  const find = () => element.querySelector(':scope > #qt-shadow-container')?.shadowRoot
    ?.querySelector('.qt-screen')?.id;
  if (!find()) module.qtAddContainerElement(element);
  const name = find();
  if (!name) throw new Error('SciQLopPlots: Qt did not make a screen in this element');
  return name;
}

export class SciQLopPlots {
  constructor(module) {
    this.m = module;
  }

  get version() {
    return this.m.version();
  }

  // A panel filling element (give it a size: the panel follows it).
  panel(element, { theme, timeRange } = {}) {
    const panel = new Panel(this, this.m.createPanel(screenOf(this.m, element)), element);
    if (theme) panel.setTheme(theme.base ?? theme, theme.colors ?? {});
    if (timeRange) panel.setTimeRange(...timeRange);
    return panel;
  }
}

const loaded = new Map();

// base: where sciqlopplots.js, .wasm and qtloader.js are (default: next to this module).
// Qt starts once per page, so later calls share the first one's instance.
export function load({ base = moduleBase } = {}) {
  if (!loaded.has(base))
    loaded.set(base, start(base).catch((error) => { loaded.delete(base); throw error; }));
  return loaded.get(base);
}

async function start(base) {
  if (!globalThis.crossOriginIsolated) {
    // On a first visit coi-serviceworker.js installs itself, then reloads the page: wait for
    // that reload rather than fail on the page that is about to go away. Installing it can
    // take seconds on a slow device; without it no reload comes, so fail at once.
    const coiPending = [...document.scripts].some((s) => /coi-serviceworker(\.min)?\.js$/.test(s.src));
    if (coiPending) await new Promise((resolve) => setTimeout(resolve, 15000));
    throw new Error('SciQLopPlots needs a cross-origin isolated page (COOP/COEP headers or coi-serviceworker.js)');
  }
  if (!globalThis.qtLoad) await loadScript(base + 'qtloader.js');
  if (!globalThis.sciqlopplots_entry) await loadScript(base + 'sciqlopplots.js');
  // Qt starts on a screen of its own; panels get theirs in panel().
  const host = document.createElement('div');
  host.style.cssText = 'position:fixed;left:-10px;top:-10px;width:1px;height:1px;overflow:hidden;';
  document.body.append(host);
  const module = await globalThis.qtLoad({
    locateFile: (file) => base + file,   // Qt's loader drops Emscripten's script directory
    qt: { entryFunction: globalThis.sciqlopplots_entry, containerElements: [host] },
  });
  return new SciQLopPlots(module);
}
