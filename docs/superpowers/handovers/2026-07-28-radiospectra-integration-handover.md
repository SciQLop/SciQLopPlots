# Handover — radiospectra integration for spectrogram processing

**Date:** 2026-07-28
**Prerequisite work:** DONE and merged. This handover covers what comes next.

## Where things stand

The **backend is finished, merged and installed**. What remains is exposing it to users
through `sciqlop_radio`, plus three more processing operations that were deliberately
deferred.

| Repo | `main` | Pushed? |
|---|---|---|
| `/var/home/jeandet/Documents/prog/SciQLopPlots` | `59f0a1f` | **No** — 14 ahead of `origin/main` |
| `/var/home/jeandet/Documents/prog/SciQLop` | `d0d23217` | **No** — 5 ahead of `origin/main` |
| `/var/home/jeandet/Documents/prog/plugins_sciqlop` | `13dca48` | **No** |

Nothing is pushed. Pushing is always an explicit request from the user.

## What already exists (do not rebuild)

```python
from SciQLop.user_api import dsp

dsp.background_subtract(var, *, q=50.0, window=None, mode='diff', gap_factor=3.0)
dsp.arrays.background_subtract(x, y, *, q=50.0, window=None, mode='diff', gap_factor=3.0)
```

- `q` — estimator percentile. `50` = median (default); `5`–`10` when bursts fill much of the window.
- `window` — `None` = one constant background over the whole window; `int` = that many **samples**;
  `timedelta`/`np.timedelta64` = that **duration**. A bare `float` raises `TypeError` by design.
  Zero/negative duration raises `ValueError`.
- `mode` — `'diff'` (default, `UNITS` unchanged), `'ratio'` (`UNITS=''`), `'db'` (`UNITS='dB'`).
  `ratio`/`db` yield NaN, never ±inf, where `S ≤ 0` or `bg ≤ 0`.
- Output keeps the input's **time and frequency axes**, name suffixed `_bgsub`.

Backed by two new kernels in `SciQLopPlots.dsp`: `column_percentile(y, q)` and gap-aware
`rolling_percentile(x, y, window, q, gap_factor, has_gaps)` — the module's first reductions
running **down the time axis per column**.

Source of truth:
- Spec: `docs/superpowers/specs/2026-07-27-spectrogram-background-removal-design.md`
- Plan: `docs/superpowers/plans/2026-07-27-spectrogram-background-removal.md`
- Kernels: `include/SciQLopPlots/DSP/Stats.hpp`, `src/DSP/python_module.cpp`
- Python: `SciQLop/user_api/dsp/_background.py`, `_arrays.py`, `__init__.py`

Verified on real I-LOFAR data (3565×488): per-channel spread of the time-average drops
**4.3e7 → 0.31 dB**, i.e. the receiver bandpass is flattened. That is the visible symptom in
the user's original screenshots — the saturated red band across the top of the Y plot.

## The open decision: how users drive it

This was discussed and **deliberately deferred**, not settled. Three options were put to the
user; they chose to build the backend first and decide the UX later.

**(a) Knobs on the existing radio products.** Add `background`/`window`/`mode` knobs to the
radio VPs; the fetch callback applies them. Cheapest, live-adjustable. Downsides: mutates the
product in place (no raw-vs-processed side by side) and re-runs the transform on every pan.

**(b) Derived virtual products.** `radio/ilofar/X` stays raw; `radio/ilofar/X (bg-subtracted)`
is a separate VP. Composable, raw and processed stackable in one panel. Doubles fetch cost
unless a shared cache is wired.

**(c) Display-time layer on the colormap.** Best interactivity, biggest build, fights the
existing resampler.

Recommendation at the time: **(a) first**, because `lofar.py` already carries working `Beam`/`SAP`
knobs, and (b) becomes easy afterwards since the transform is already a pure function. Confirm
with the user before building — this is their call, not a settled question.

### Trap if you implement (a)

`sciqlop_radio/lofar.py` has a load-bearing comment about this. SciQLop introspects the callback
with `inspect.signature(callback, eval_str=True)`, which re-evaluates stringified
`Annotated[int, Knob(...)]` annotations **in the defining module's globals**. A lazy import of
`Knob` inside the factory function is invisible to that eval and raises `NameError` at
VP-registration time. `Knob` must be imported at module top level, with the headless-CI
`ImportError` fallback stub that `lofar.py` already demonstrates.

Note also that `continuous.py`'s `_build_callback` currently returns `_callback(start, stop)`
with no knob parameters at all — unlike `lofar.py`'s `lofar(start, stop, beam=0, sap=0)`.
Adding knobs to the continuous sources means changing that signature, and the `ContinuousSource`
registry entries feed every stream built by the dock via `make_stream_source`, so the change
reaches e-Callisto/RSTN streams too, not just I-LOFAR.

## Remaining processing operations (deferred, in rough priority order)

1. **RFI channel flagging.** Per-channel variance/kurtosis threshold, sigma-clipping, or a
   median filter along frequency. I-LOFAR mode 357 has permanently-hot channels. Needs a
   `sigma_clip` kernel or equivalent; `column_percentile` already gives robust per-channel stats
   to build a threshold from.
2. **Median rebinning** along time or frequency. Distinct from the colormap's mean-binning
   resample — median survives spikes. Reuses the same percentile machinery.
3. **Stokes I from the I-LOFAR X/Y pair.** `I = X + Y`, and crude linear polarisation
   `(X − Y)/(X + Y)`. Nearly free: both polarisations are already separate registered products
   (`radio/ilofar/X`, `radio/ilofar/Y` in `CONTINUOUS_SOURCES`). Needs a VP that fetches two
   sources and combines them — check whether SciQLop's virtual-product API supports deriving
   from two other products before designing this.

Placement rule established during the design and worth keeping: **generic spectrogram
operations belong in `SciQLop.user_api.dsp`** (MMS/PSP/Cluster energy spectrograms want the same
things), **hot generic numerics in `SciQLopPlots/DSP`**, and **only instrument policy in
`sciqlop_radio`** (per-source defaults, RFI channel lists, knob exposure). Putting numerics in
the plugin would mean re-implementing a robust median in Python over 3565×488 arrays per fetch,
and no other spectrogram in SciQLop could use it.

## Four traps already paid for — don't rediscover them

1. **`PyErr_Format` does not support `%g`/`%f`** for doubles. It raises `SystemError` instead of
   your intended exception. Pre-format with `snprintf` and pass `%s`.
2. **`np.timedelta64` is a subclass of `np.signedinteger`.** `isinstance(w, (int, np.integer))`
   catches a `timedelta64` and silently reads a *duration* as a *sample count*. Test the
   duration branch first.
3. **Every gap-aware `Stage<T>` kernel returns `n + G` rows, not `n`.** `reassemble()`
   (`Pipeline.hpp:65-109`) inserts one NaN separator row per gap, timestamped at the gap
   midpoint — a deliberate line-break marker for plotting, shared by `rolling_mean`/`rolling_std`.
   `_background._realign_to_input` strips them via `bg[np.searchsorted(x_bg, x)]`, which is exact
   because `split_segments` partitions `x` without dropping samples and segment timestamps are
   copied verbatim.
4. **`rolling_percentile` collapses a size-1 column axis** (`(n,1) → (n,)`), on both the gapped
   and gapless branches. Single-channel data needs a reshape guard before broadcasting.

Traps 3 and 4 produce silently wrong output rather than exceptions, and neither was visible to a
single-file review — they only surfaced across the C++/Python boundary.

## Environment — read before running anything

**Build SciQLopPlots** (never a bare `meson setup build`; Qt is not on the default PATH):

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv          # --buildtype=debugoptimized, never plain debug
```

**Install into SciQLop's venv** (never `pip install -e .`):

```bash
SRC=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv/SciQLopPlots
DST=/var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots
cp "$SRC"/*.so "$SRC"/*.py "$DST"/ && rm -rf "$DST"/__pycache__
```

**Tests:**

```bash
# SciQLopPlots — run from /tmp so the source package doesn't shadow the build
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/integration -q      # 1041 passed

# SciQLop DSP
cd /var/home/jeandet/Documents/prog/SciQLop && \
  QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_arrays.py tests/test_dsp_speasy.py -q   # 45 passed

# sciqlop_radio plugin
cd /var/home/jeandet/Documents/prog/plugins_sciqlop/sciqlop_radio && \
  QT_QPA_PLATFORM=xcb /var/home/jeandet/Documents/prog/SciQLop/.venv/bin/python \
  -m pytest sciqlop_radio/tests/ -q -m "not live"                          # 186 passed, 5 skipped
```

`QT_QPA_PLATFORM=xcb` is **required** for the SciQLop and plugin suites — they hard-crash in
libxkbcommon under native Wayland. Never use `offscreen`.

**Known pre-existing failure, not yours:** the full SciQLop `tests/` directory aborts with
SIGABRT in an unrelated `tscat_catalogs`/watchdog fixture teardown. Reproduces on a baseline
without any of this work. Run the DSP-targeted suites instead.

## Real data available offline

`~/.cache/sciqlop_radio/` holds cached I-LOFAR BST files for 2025-07-26 (hourly:
`20250726_{0858,1000,1100,1200,1300,1400,1500,1600}*_bst_00{X,Y}.dat`) plus e-Callisto
GREENLAND FITS. You can exercise the whole pipeline with no network:

```python
import sys; sys.path.insert(0, '/var/home/jeandet/Documents/prog/plugins_sciqlop/sciqlop_radio')
from pathlib import Path
from sciqlop_radio.reader import open_spectrogram
from sciqlop_radio.plot import spectrogram_to_speasy_variable
from SciQLop.user_api import dsp

v = spectrogram_to_speasy_variable(open_spectrogram(
    Path.home()/'.cache/sciqlop_radio/20250726_130037_bst_00X.dat'))
out = dsp.background_subtract(v, mode='db')
```

I-LOFAR mode 357 shape is `(3565, 488)`, 1 s cadence, **ascending** 10.5–244.5 MHz, three
frequency bands with real gaps between them (that's the instrument, not a bug). Files are
hourly with a ~36 s gap between consecutive files.

## Recently fixed nearby — context, not work

`plugins_sciqlop` commit `13dca48` fixed `_rows_overlapping` in `sciqlop_radio/continuous.py`:
it approximated a file's coverage as `[start, next_row_start)` while running **before** the
X/Y polarisation filter, and I-LOFAR ships both polarisations under the same `Start Time`. Every
row but the last of each same-timestamp group got a zero-width coverage and was dropped, so the
X product silently lost the file containing the window's start. Coverage now runs to the next
**distinct** start time. If you touch that trimming code, keep that invariant.

## Suggested first move

Ask the user which UX option (a/b/c) they want before building anything — it was explicitly left
open. If they pick (a), start from `sciqlop_radio/lofar.py`'s `Knob` pattern and mind the
`eval_str` trap above.
