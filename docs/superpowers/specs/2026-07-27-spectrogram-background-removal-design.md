# Spectrogram background removal — design

**Date:** 2026-07-27
**Scope:** backend/API only. No knobs, no virtual products, no UI.
**Repos touched:** `SciQLopPlots` (C++ DSP kernels + bindings), `SciQLop` (`user_api.dsp`).

## Motivation

Background (quiet-sun) removal is the canonical first processing step for radio
dynamic spectra: estimate a per-frequency-channel background over time, then
express each sample relative to it. Nothing in the stack provides it today —
`radiospectra` 0.7 dropped the old `subtract_bg` (only the WAVES source keeps a
`background` property), and `SciQLopPlots.dsp` has no reduction along the time
axis for any operator.

It is not a radio-specific operation: MMS/PSP/Cluster energy spectrograms want
the same transform, so the user-facing function belongs in the generic
`SciQLop.user_api.dsp` layer, not in the `sciqlop_radio` plugin.

## Axis direction — why this is a new kernel, not a new `op=`

`reduce` and `reduce_axes` reduce **across columns within a row**: for a
spectrogram that collapses the frequency axis at each time step. A background
needs the opposite direction — one statistic **per channel, down the time
axis**. No operator in the module reduces that way. `Stats.hpp`'s `BlockStats`
is the right direction but is mean/variance/min/max only, is computed per
segment rather than per window, and is not bound to Python.

`rolling_mean` / `rolling_std` *are* along the time axis per column, so the
sliding-background kernel has a direct sibling to mirror.

## 1. C++ kernels — `SciQLopPlots/DSP`

```python
column_percentile(y, q=50.0) -> np.ndarray                    # shape (n_cols,)
rolling_percentile(x, y, window, q=50.0,
                   gap_factor=3.0, has_gaps=True) -> (x_out, y_out)
```

A single percentile parameter subsumes the estimator choice: `q=50` is the
median, `q=5..10` is the robust low-percentile estimator used when bursts fill
much of the window. There is no `estimator=` enum.

### `column_percentile(y, q)`

Deliberately breaks the module's `(x, y) -> (x_out, y_out)` convention. It
reduces time away, so there is no output time axis, and gaps are meaningless to
a percentile — an absent sample is simply not in the population. Requiring an
`x` argument and returning a degenerate one-element time axis would be ceremony.
Returns a bare 1-D array of length `n_cols`.

- Per column: gather into a contiguous buffer (the module's existing
  gather/scatter pattern), drop NaN, `std::nth_element`. O(n) per column.
- Parallel across columns via `Parallel.hpp`.
- An all-NaN column yields `NaN`.
- Goes through the existing `dispatch(y.dtype, ...)`, so float32 (Speasy's
  usual dtype for this data) and float64 both work without a copy.

This is a modest win over `np.nanpercentile` — mostly avoiding the NaN-mask
copy and parallelising across columns. It is included because
`background_subtract` needs it, not because it is a hotspot.

### `rolling_percentile(x, y, window, q, ...)`

Mirrors `rolling_mean` exactly, including gap-awareness through the existing
`apply_stage` segment pipeline (windows never span a gap):

- **centered** (`half = window / 2`), matching `rolling_mean_column`
- **skips NaN** within the window
- **shrinks the window at the edges** rather than emitting fill
- accepts any window size; odd/even is not constrained
- `window` is validated by the existing `check_window_size` (`0 < window <= n_rows`)

**One deliberate deviation:** where `rolling_mean` returns `0` for an all-NaN
window, `rolling_percentile` returns `NaN`. A `0` background is actively harmful
downstream — it silently turns `mode='diff'` into a no-op and makes
`ratio`/`db` divide by zero. Documented in both docstrings so the inconsistency
reads as intentional.

Implementation: sorted window with binary-search insert and erase. O(n·log w)
comparisons plus an O(n·w) memmove of a small contiguous buffer — cache-friendly
for the window sizes in use (~40 ms threaded for 3565×488 at `window=301`).

This is the kernel that earns C++: scipy has no gap-aware rolling percentile,
and `scipy.ndimage.median_filter` over the 2-D array is substantially slower and
gap-blind.

The kernel takes **samples only** and carries no time semantics. Duration
handling lives one layer up.

## 2. User-facing function — `SciQLop.user_api.dsp`

```python
background_subtract(var, *, q=50.0, window=None,
                    mode='diff', gap_factor=3.0) -> SpeasyVariable
```

Plus `arrays.background_subtract(x, y, *, q, window, mode, gap_factor)` for the
raw-numpy path, matching how every other function in that module is paired.

### Parameters

| Param | Meaning |
|---|---|
| `q` | Percentile of the background estimator. `50` = median (default); `5`–`10` when bursts fill much of the window |
| `window` | `None` → one constant background per channel over the whole window. Otherwise a sliding background — see below |
| `mode` | `'diff'` = `S − bg` (default), `'ratio'` = `S / bg`, `'db'` = `10·log10(S / bg)` |
| `gap_factor` | Forwarded to `rolling_percentile`; ignored when `window is None` |

### `window` — samples or duration

```python
window: None | int | timedelta | np.timedelta64
```

- `None` → constant background (`column_percentile`)
- `int` → sliding background, that many **samples**
- `timedelta` / `np.timedelta64` → sliding background, that **duration**

Dispatch is **by type, not by value**. A bare `float` raises `TypeError`
naming the two valid forms. `window=300` and `window=300.0` meaning different
things would be a silent 4× error at e-Callisto's 0.25 s cadence rather than a
visible one.

Duration → samples is `round(seconds / median_dt)` clamped to `[1, n_rows]`,
computed in this Python layer. `median_dt` is the median of `diff(x)`, which is
robust to the inter-file gaps these products carry.

### Output

A new `SpeasyVariable` with the **same time axis and the same frequency axis**
— `rewrap_time_series` already preserves `axes[1:]` — with the name suffixed
`_bgsub`.

`UNITS` handling per mode:

| `mode` | `UNITS` |
|---|---|
| `'diff'` | unchanged (template's) |
| `'ratio'` | `''` (dimensionless) |
| `'db'` | `'dB'` |

`rewrap_time_series` copies the template's meta wholesale, so `ratio` and `db`
must override `UNITS` explicitly or the output is mislabelled.

### Non-positive guard

In `'ratio'` and `'db'`, any sample where `S <= 0` or `bg <= 0` yields **NaN,
not ±inf**. The colormap already hides NaN, whereas an inf destroys the colour
scale. This is silent data loss either way; NaN is the failure mode that stays
visible as a hole rather than blanking the whole plot. `'diff'` needs no guard,
which is part of why it is the default.

## 3. Testing

`SciQLopPlots` — `tests/unit/test_dsp.py`:

- `column_percentile` against `np.nanpercentile` for float32 and float64
- columns containing NaN; an all-NaN column → `NaN`; single-row input
- `q` at 0, 50, 100
- `rolling_percentile` against a numpy reference on gapless data
- `rolling_percentile` across a real gap, against a segment-wise reference
- all-NaN window → `NaN` (the documented deviation from `rolling_mean`)

`SciQLop` — `user_api.dsp` tests:

- constant background removes a per-channel offset exactly (a flat spectrum
  gives 0 in `diff`, 1 in `ratio`, 0 dB in `db`)
- sliding background tracks a linear drift that a constant background cannot
- `window` as `int`, as `timedelta`, and as `np.timedelta64` agree when they
  describe the same span; bare `float` raises `TypeError`
- time axis, frequency axis and metadata survive; `UNITS` correct per mode
- non-positive input yields NaN, not inf, in `ratio` and `db`

## 4. Explicitly out of scope

RFI channel flagging, median rebinning, Stokes I from the I-LOFAR X/Y pair, and
everything in `sciqlop_radio` (knobs, derived products, UI). The transform is a
pure function of `(x, y)`, so each of those remains straightforward to add on
top later.
