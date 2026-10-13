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

const exportTypes = { png: 'image/png', jpg: 'image/jpeg', pdf: 'application/pdf' };

export class Panel extends Handle {
  constructor(lib, id, element) {
    super(lib, id);
    this.element = element;
  }

  // Plots stack vertically and share the time axis. Set the time range first: a new plot
  // fits its y axis to the data inside it.
  addPlot() {
    return new Plot(this.lib, this.lib.m.addPlot(this.id));
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

// base: where sciqlopplots.js, .wasm and qtloader.js are (default: next to this module).
export async function load({ base = moduleBase } = {}) {
  if (!globalThis.crossOriginIsolated)
    throw new Error('SciQLopPlots needs a cross-origin isolated page (COOP/COEP headers or coi-serviceworker.js)');
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
