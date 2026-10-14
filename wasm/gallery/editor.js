// Live editing for gallery examples: the page's <script type="text/plain" id="example"> is
// shown in an editor and run as a module. Run (Ctrl/Cmd+Enter) runs the edited code again
// in the same page: Qt loads once, so a run removes the previous run's panels, cancels its
// timers and puts the page's markup back before starting.
import { SciQLopPlots } from '../sciqlopplots.mjs';

const codemirror = 'https://cdnjs.cloudflare.com/ajax/libs/codemirror/5.65.16/';
const example = document.querySelector('#example');
const main = document.querySelector('main');
example.remove();
const markup = main.innerHTML;
const original = dedent(example.textContent);

const run = { panels: new Set(), timers: new Set() };
trackPanels();
globalThis.__galleryTimers = trackedTimers();

const ui = buildUi();
const shared = sharedCode();
const editor = await createEditor(ui.host, shared ?? original);
// A link can carry any code: show it, and let the reader run it once they have read it.
if (shared) showNotice('This code came with the link and has not run. Read it, then press Run.');
else await runCode(original);
// For wasm/tests/editor.html, and for wasm/tests/run.mjs, which waits for the first run.
window.galleryEditor = { run: () => runCode(editor.getValue()), setCode: (code) => editor.setValue(code), ui, ready: true };

// The script sits indented in the page's HTML.
function dedent(text) {
  const lines = text.replace(/^\n/, '').replace(/\s+$/, '').split('\n');
  const indent = Math.min(...lines.filter((line) => line.trim()).map((line) => line.match(/^ */)[0].length));
  return lines.map((line) => line.slice(indent)).join('\n');
}

function sharedCode() {
  const code = new URLSearchParams(location.hash.slice(1)).get('code');
  if (!code) return null;
  try { return new TextDecoder().decode(Uint8Array.from(atob(code), (c) => c.charCodeAt(0))); }
  catch { return null; }
}

function trackPanels() {
  const panel = SciQLopPlots.prototype.panel;
  SciQLopPlots.prototype.panel = function (...args) {
    const created = panel.apply(this, args);
    run.panels.add(created);
    return created;
  };
}

// The example's own timer functions (asModule shadows the globals with them), so a new run
// can cancel what the last one scheduled without touching the editor's timers.
function trackedTimers() {
  const tracked = (start, stop) => (...args) => {
    const id = window[start](...args);
    run.timers.add(() => window[stop](id));
    return id;
  };
  return {
    setTimeout: tracked('setTimeout', 'clearTimeout'),
    setInterval: tracked('setInterval', 'clearInterval'),
    requestAnimationFrame: tracked('requestAnimationFrame', 'cancelAnimationFrame'),
  };
}

function clearPreviousRun() {
  for (const cancel of run.timers) cancel();
  run.timers.clear();
  for (const panel of run.panels) {
    try { panel.remove(); } catch { /* the example removed it itself */ }
  }
  run.panels.clear();
  main.innerHTML = markup;
}

// Relative imports would resolve against the blob: URL, so make them absolute first. The
// prelude stays on line 1 so error line numbers match the editor's.
function asModule(code) {
  const prelude = 'const { setTimeout, setInterval, requestAnimationFrame } = globalThis.__galleryTimers; ';
  const absolute = prelude + code.replace(/(\bfrom\s*|\bimport\s*\(\s*)(['"])(\.{1,2}\/[^'"]+)\2/g,
    (_, keyword, quote, path) => `${keyword}${quote}${new URL(path, location.href).href}${quote}`);
  return URL.createObjectURL(new Blob([absolute], { type: 'text/javascript' }));
}

async function runCode(code) {
  clearPreviousRun();
  ui.notice.hidden = ui.error.hidden = true;
  const url = asModule(code);
  try {
    await import(url);
  } catch (error) {
    showError(error);
  } finally {
    URL.revokeObjectURL(url);
  }
}

function showNotice(text) {
  ui.notice.textContent = text;
  ui.notice.hidden = false;
}

function showError(error) {
  console.error(error);
  // Firefox's stack has no message line; Chrome's starts with it.
  const stack = String(error?.stack ?? '');
  ui.error.textContent = stack.startsWith(String(error)) ? stack : `${error}\n${stack}`.trim();
  ui.error.hidden = false;
}

function buildUi() {
  const section = document.createElement('section');
  section.className = 'editor';
  section.innerHTML = `
    <div class="editor-bar">
      <h3>Source <span class="hint">edit it, then Run (Ctrl+Enter)</span></h3>
      <button type="button" data-action="run">Run</button>
      <button type="button" data-action="reset">Reset</button>
      <button type="button" data-action="share">Copy link</button>
    </div>
    <p class="run-notice" hidden></p>
    <div class="editor-host"></div>
    <pre class="run-error" hidden></pre>`;
  main.after(section);
  section.querySelector('[data-action=run]').onclick = () => runCode(editor.getValue());
  section.querySelector('[data-action=reset]').onclick = () => {
    editor.setValue(original);
    history.replaceState(null, '', location.pathname);
    runCode(original);
  };
  section.querySelector('[data-action=share]').onclick = (event) => share(event.target);
  window.addEventListener('unhandledrejection', (event) => showError(event.reason));
  window.addEventListener('error', (event) => showError(event.error ?? event.message));
  return { host: section.querySelector('.editor-host'), error: section.querySelector('.run-error'),
    notice: section.querySelector('.run-notice') };
}

async function share(button) {
  const bytes = new TextEncoder().encode(editor.getValue());
  const code = btoa(String.fromCharCode(...bytes));
  history.replaceState(null, '', `${location.pathname}#code=${encodeURIComponent(code)}`);
  try { await navigator.clipboard.writeText(location.href); button.textContent = 'Link copied'; }
  catch { button.textContent = 'Link in the address bar'; }
  setTimeout(() => { button.textContent = 'Copy link'; }, 2000);
}

async function createEditor(host, code) {
  try {
    await Promise.all([loadCss(codemirror + 'codemirror.min.css'), loadScript(codemirror + 'codemirror.min.js')]);
    await loadScript(codemirror + 'mode/javascript/javascript.min.js');
  } catch {
    return textareaEditor(host, code);   // offline, or the CDN is blocked: plain text still runs
  }
  const cm = globalThis.CodeMirror(host, {
    value: code, mode: 'javascript', lineNumbers: true, tabSize: 2, indentUnit: 2,
    viewportMargin: Infinity, extraKeys: { 'Ctrl-Enter': () => runCode(cm.getValue()), 'Cmd-Enter': () => runCode(cm.getValue()) },
  });
  return cm;
}

function textareaEditor(host, code) {
  const area = Object.assign(document.createElement('textarea'), { value: code, spellcheck: false });
  area.rows = code.split('\n').length + 1;
  area.addEventListener('keydown', (event) => {
    if (event.key === 'Enter' && (event.ctrlKey || event.metaKey)) { event.preventDefault(); runCode(area.value); }
  });
  host.append(area);
  return { getValue: () => area.value, setValue: (value) => { area.value = value; } };
}

function loadScript(src) {
  return new Promise((resolve, reject) => document.head.append(Object.assign(document.createElement('script'),
    { src, crossOrigin: 'anonymous', onload: resolve, onerror: reject })));
}

function loadCss(href) {
  return new Promise((resolve, reject) => document.head.append(Object.assign(document.createElement('link'),
    { rel: 'stylesheet', href, crossOrigin: 'anonymous', onload: resolve, onerror: reject })));
}
