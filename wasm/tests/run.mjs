// node wasm/tests/run.mjs <site-dir> [outdir]: runs the test pages of a site made by
// build_site.sh in Firefox (WebDriver BiDi), prints each check, saves a screenshot per page
// in outdir and exits 1 when a check fails.
// Firefox needs a display for WebGL 2: run it under `xvfb-run -a -s "-screen 0 1400x1000x24"`.
import { spawn } from 'node:child_process';
import { createServer } from 'node:http';
import { mkdtempSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { extname, join, normalize } from 'node:path';
import { tmpdir } from 'node:os';

const [site, outdir = mkdtempSync(join(tmpdir(), 'sciqlopplots-tests-'))] = process.argv.slice(2);
// Test pages publish window.testResult; gallery pages (the index's links) pass when they log
// no error. THUMBS=<dir> also saves a picture of each gallery page's .plots[data-thumbnail]
// element, or its first .plots.
const galleryPages = [...readFileSync(join(site, 'index.html'), 'utf8').matchAll(/href="(gallery\/[\w-]+\.html)"/g)]
  .map((match) => match[1]);
const pages = process.env.PAGES?.split(',')
  ?? ['tests/api.html', 'tests/legacy.html', 'tests/gestures.html', 'tests/editor.html', 'index.html', ...galleryPages];
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const types = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript',
  '.wasm': 'application/wasm', '.png': 'image/png', '.css': 'text/css', '.json': 'application/json' };

// The headers threads need (coi-serviceworker.js stands in for them on GitHub Pages).
const server = createServer((req, res) => {
  const path = join(site, normalize(decodeURIComponent(new URL(req.url, 'http://x').pathname)));
  try {
    const body = readFileSync(path.endsWith('/') ? join(path, 'index.html') : path);
    // COI_HEADERS=0 serves like GitHub Pages: no headers, coi-serviceworker.js has to add them.
    const isolation = process.env.COI_HEADERS === '0' ? {}
      : { 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp' };
    res.writeHead(200, { 'Content-Type': types[extname(path)] ?? 'application/octet-stream', ...isolation });
    res.end(body);
  } catch {
    res.writeHead(404).end();
  }
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const origin = `http://127.0.0.1:${server.address().port}`;

const port = 9000 + Math.floor(Math.random() * 1000);
const profile = join(outdir, 'profile');
mkdirSync(profile, { recursive: true });
writeFileSync(join(profile, 'user.js'), ['webgl.force-enabled', 'webgl.enable-webgl2']
  .map((k) => `user_pref("${k}", true);`).concat('user_pref("webgl.disabled", false);').join('\n'));
const firefox = spawn(process.env.FIREFOX ?? 'firefox', ['--no-remote', '--profile', profile,
  `--remote-debugging-port=${port}`, '--width=1400', '--height=1000'],
  { stdio: 'ignore', env: { ...process.env, LIBGL_ALWAYS_SOFTWARE: '1', MOZ_DISABLE_CONTENT_SANDBOX: '1' } });

async function connect() {
  for (let i = 0; i < 100; i++) {
    let ws;
    try {
      ws = new WebSocket(`ws://127.0.0.1:${port}/session`);
      // Firefox can take the connection and never answer it: give up and try again.
      await new Promise((ok, ko) => { ws.addEventListener('open', ok); ws.addEventListener('error', ko); setTimeout(ko, 5000); });
      return ws;
    } catch { ws?.close(); await sleep(300); }
  }
  throw new Error('Firefox did not start');
}

const ws = await connect();
let lastId = 0;
const pending = new Map(), logs = [];
ws.addEventListener('message', (e) => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'log.entryAdded') {
    const frames = m.params.stackTrace?.callFrames ?? [];
    logs.push(`[${m.params.level}] ${m.params.text}` + ((m.params.level === 'error' || /Context is lost/.test(m.params.text)) && frames.length
      ? '\n' + frames.slice(0, 25).map((f) => `      at ${f.functionName} (${f.url.split('/').pop()}:${f.lineNumber})`).join('\n') : ''));
  }
});
const send = (method, params = {}, ms = 30000) => new Promise((resolve) => {
  const id = ++lastId;
  pending.set(id, resolve);
  ws.send(JSON.stringify({ id, method, params }));
  setTimeout(() => { if (pending.delete(id)) resolve({ error: `timeout: ${method}` }); }, ms);
});

await send('session.new', { capabilities: {} });
const context = (await send('browsingContext.getTree')).result.contexts[0].context;
await send('session.subscribe', { events: ['log.entryAdded'] });
// PHONE=1: a phone-sized viewport (the gesture page needs the desktop size). VIEWPORT=WxH: any
// other size, e.g. a tall one to see a whole page in its screenshot.
if (process.env.PHONE)
  await send('browsingContext.setViewport', { context, viewport: { width: 390, height: 844 }, devicePixelRatio: 3 });
else if (process.env.VIEWPORT) {
  const [width, height] = process.env.VIEWPORT.split('x').map(Number);
  await send('browsingContext.setViewport', { context, viewport: { width, height } });
}
const evaluate = async (expression) => (await send('script.evaluate',
  { expression, target: { context }, awaitPromise: false })).result?.result?.value;

// A page asks for real (trusted) input by setting window.inputRequest to a list of WebDriver
// BiDi input sources (mouse, wheel, two touch pointers...); the runner performs it and clears it.
async function performRequestedInput() {
  const json = await evaluate('window.inputRequest ? JSON.stringify(window.inputRequest) : ""');
  if (!json) return;
  await send('input.performActions', { context, actions: JSON.parse(json) });
  await send('input.releaseActions', { context });
  await evaluate('window.inputRequest = null, window.inputDone = (window.inputDone || 0) + 1');
}

async function saveThumbnail(page) {
  const box = JSON.parse(await evaluate(
    `(() => { const plots = document.querySelector('.plots[data-thumbnail]') ?? document.querySelector('.plots');
      plots.scrollIntoView({ block: 'center' });
      const { x, y, width, height } = plots.getBoundingClientRect();
      return JSON.stringify({ x, y, width, height }); })()`));
  const data = (await send('browsingContext.captureScreenshot', { context, clip: { type: 'box', ...box } })).result?.data;
  mkdirSync(process.env.THUMBS, { recursive: true });
  if (data) writeFileSync(join(process.env.THUMBS, page.replace(/^gallery\/|\.html$/g, '') + '.png'), Buffer.from(data, 'base64'));
}

// A gallery page is ready once editor.js has run its example (the index has no example), then
// gets a moment more for the frames and errors that follow.
async function waitForGalleryPage() {
  const hasExample = await evaluate('Boolean(document.querySelector("script[src$=\\"editor.js\\"]"))');
  for (let i = 0; hasExample && i < 60; i++) {
    if (await evaluate('Boolean(window.galleryEditor?.ready)')) break;
    await sleep(500);
  }
  await sleep(1500);
}

let failures = 0;
for (const page of pages) {
  logs.length = 0;
  console.log(`${page}:`);   // before the page runs, so a hang shows where it is
  await send('browsingContext.navigate', { context, url: `${origin}/${page}`, wait: 'complete' });
  let result;
  if (page.startsWith('tests/')) {
    // A frozen page answers no query, and each query waits 30 s: bound the page by the clock.
    const deadline = Date.now() + 120000;
    while (!result && Date.now() < deadline) {
      await performRequestedInput();
      const json = await evaluate('window.testResult ? JSON.stringify(window.testResult) : ""');
      if (json) result = JSON.parse(json); else await sleep(500);
    }
    await sleep(1000);
  } else {
    await waitForGalleryPage();
    // Fonts Firefox's own start page fetches fail without network: not the page's errors.
    const errors = logs.filter((l) => l.startsWith('[error]') && !/^\[error\] downloadable font: .*source: https:\/\/(?!127\.0\.0\.1)/.test(l));
    result = [{ name: 'no error logged', ok: errors.length === 0, error: errors[0] }];
    if (process.env.THUMBS && page.startsWith('gallery/')) await saveThumbnail(page);
  }
  const shot = (await send('browsingContext.captureScreenshot', { context })).result?.data;
  if (shot) writeFileSync(join(outdir, page.replace(/\W/g, '_') + '.png'), Buffer.from(shot, 'base64'));
  if (!result) {
    failures++;
    console.log('  no result (the page did not finish)\n' + logs.slice(-20).map((l) => '  ' + l).join('\n'));
    continue;
  }
  for (const r of result) {
    if (!r.ok) failures++;
    console.log(`  ${r.ok ? 'ok  ' : 'FAIL'} ${r.name}${r.ok ? '' : ': ' + r.error}`);
  }
  const errors = process.env.ALL_LOGS ? logs : logs.filter((l) => l.startsWith('[error]'));
  if (errors.length) console.log(errors.slice(0, 10).map((l) => '  ' + l).join('\n'));
}
console.log(`screenshots in ${outdir}`);
await send('session.end');
firefox.kill();
server.close();
process.exit(failures ? 1 : 0);
