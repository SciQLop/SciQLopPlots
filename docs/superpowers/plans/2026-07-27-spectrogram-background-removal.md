# Spectrogram Background Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add per-channel background removal for 2-D spectrograms — two new time-axis DSP kernels in SciQLopPlots plus one `background_subtract()` function in `SciQLop.user_api.dsp`.

**Architecture:** `SciQLopPlots/DSP/Stats.hpp` gains two column-wise (down the time axis) percentile kernels, bound into the `_sciqlop_dsp` CPython extension. `SciQLop.user_api.dsp` composes them into `background_subtract()`, which estimates a per-frequency-channel background and expresses each sample relative to it. The kernels carry no time semantics; duration→samples conversion happens in the Python layer.

**Tech Stack:** C++20, numpy C API (no pybind/shiboken — `_sciqlop_dsp` is a hand-written CPython extension), Meson, pytest.

**Spec:** `docs/superpowers/specs/2026-07-27-spectrogram-background-removal-design.md`

## Global Constraints

- Two repos. Tasks 1–2 are in `/var/home/jeandet/Documents/prog/SciQLopPlots`. Tasks 3–4 are in `/var/home/jeandet/Documents/prog/SciQLop`.
- Build SciQLopPlots **only** via the in-tree venv recipe below. Never a bare `meson setup build`. Always `--buildtype=debugoptimized`.
- Run SciQLopPlots tests from `/tmp` so the source package doesn't shadow the build.
- Never push. Commit only.
- `q` is a percentile in `[0, 100]`; `q=50` is the median. There is no `estimator=` enum anywhere.
- Percentile interpolation is numpy's default `'linear'`: for a sorted, NaN-free population of size `n`, `idx = q/100 * (n-1)`, `lo = floor(idx)`, result `= a[lo] + (idx-lo) * (a[lo+1] - a[lo])`. Both kernels use exactly this so tests can compare against `np.nanpercentile` directly.
- Both kernels **preserve the input dtype** (float64/float32/int32), matching every other function in the module.
- `mode` values are exactly `'diff'` (default), `'ratio'`, `'db'`.

### Build and test commands (SciQLopPlots, Tasks 1–2)

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv

cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_dsp.py -q
```

### Test command (SciQLop, Tasks 3–4)

Tasks 3–4 import `SciQLopPlots.dsp` from **SciQLop's** venv, so the freshly built extension must be copied there first (this is the project's normal dev install — do not `pip install`):

```bash
cp /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv/SciQLopPlots/_sciqlop_dsp*.so \
   /var/home/jeandet/Documents/prog/SciQLopPlots/build-venv/SciQLopPlots/dsp.py \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/

cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_arrays.py tests/test_dsp_speasy.py -q
```

`QT_QPA_PLATFORM=xcb` is required — these suites segfault in libxkbcommon under native Wayland.

---

## File Structure

| File | Responsibility | Task |
|---|---|---|
| `include/SciQLopPlots/DSP/Stats.hpp` | `detail::column_percentile_value`, `column_percentiles`, `detail::rolling_percentile_column`, `rolling_percentile` stage | 1, 2 |
| `src/DSP/python_module.cpp` | `check_percentile`, `dsp_column_percentile`, `dsp_rolling_percentile`, methods table entries | 1, 2 |
| `SciQLopPlots/dsp.py` | Re-export both new names | 1, 2 |
| `tests/unit/test_dsp.py` | `TestColumnPercentile`, `TestRollingPercentile` | 1, 2 |
| `SciQLop/user_api/dsp/_background.py` | **New.** Window resolution, background estimation, mode application — pure numpy + kernel calls | 3 |
| `SciQLop/user_api/dsp/_arrays.py` | Thin re-export of `background_subtract` | 3 |
| `SciQLop/user_api/dsp/_speasy.py` | `rewrap_time_series` gains `meta_overrides` | 4 |
| `SciQLop/user_api/dsp/__init__.py` | SpeasyVariable-aware `background_subtract` facade | 4 |
| `SciQLop/tests/test_dsp_arrays.py` | Array-level tests | 3 |
| `SciQLop/tests/test_dsp_speasy.py` | SpeasyVariable-level tests | 4 |

No Meson changes: `Stats.hpp` is already pulled in by `DSP.hpp`, and `python_module.cpp` is already in the build.

---

## Task 1: `column_percentile` kernel

Per-channel percentile down the **time** axis. This direction does not exist in the module today (`reduce`/`reduce_axes` go across columns within a row).

**Files:**
- Modify: `include/SciQLopPlots/DSP/Stats.hpp` (add to `namespace detail` before its closing `}`, and a free function before `} // namespace sqp::dsp`)
- Modify: `src/DSP/python_module.cpp` (validation helper, module function, methods table)
- Modify: `SciQLopPlots/dsp.py`
- Test: `tests/unit/test_dsp.py`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `SciQLopPlots.dsp.column_percentile(y: np.ndarray, q: float = 50.0) -> np.ndarray` — returns a 1-D array of length `n_cols` (length 1 for 1-D input), same dtype as `y`. An all-NaN column yields NaN.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/test_dsp.py`:

```python
# ── column_percentile ─────────────────────────────────────────────────────────

class TestColumnPercentile:
    def test_matches_numpy_percentile_2d(self):
        rng = np.random.default_rng(0)
        y = rng.normal(size=(500, 7))
        for q in (0.0, 10.0, 50.0, 90.0, 100.0):
            assert_allclose(column_percentile(y, q), np.percentile(y, q, axis=0), atol=1e-12)

    def test_skips_nan(self):
        y = np.array([[1.0, 10.0], [np.nan, 20.0], [3.0, np.nan], [5.0, 40.0]])
        expected = [np.nanpercentile(y[:, 0], 50.0), np.nanpercentile(y[:, 1], 50.0)]
        assert_allclose(column_percentile(y, 50.0), expected, atol=1e-12)

    def test_all_nan_column_is_nan(self):
        y = np.array([[np.nan, 1.0], [np.nan, 2.0]])
        out = column_percentile(y, 50.0)
        assert np.isnan(out[0])
        assert_allclose(out[1], 1.5, atol=1e-12)

    def test_1d_input_returns_length_one(self):
        y = np.arange(11, dtype=np.float64)
        out = column_percentile(y, 50.0)
        assert out.shape == (1,)
        assert_allclose(out[0], 5.0, atol=1e-12)

    def test_single_row(self):
        y = np.array([[3.0, 7.0]])
        assert_allclose(column_percentile(y, 50.0), [3.0, 7.0], atol=1e-12)

    def test_preserves_float32(self):
        y = np.linspace(0, 1, 100, dtype=np.float32).reshape(50, 2)
        out = column_percentile(y, 50.0)
        assert out.dtype == np.float32

    def test_rejects_out_of_range_q(self):
        y = np.ones((4, 2))
        with pytest.raises(ValueError):
            column_percentile(y, 101.0)
        with pytest.raises(ValueError):
            column_percentile(y, -1.0)
```

Add `column_percentile` to the `from SciQLopPlots.dsp import (...)` block at the top of the file.

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_dsp.py -q
```

Expected: collection error — `ImportError: cannot import name 'column_percentile'`.

- [ ] **Step 3: Add the kernel to `Stats.hpp`**

Inside `namespace detail`, immediately after `block_stats_column` (before `rolling_mean_column`):

```cpp
    // Percentile of one column, down the time axis. NaN values are excluded.
    // Uses numpy's default 'linear' interpolation so results match
    // np.nanpercentile exactly. Returns NaN for an all-NaN column
    // (integer types have no NaN, so that case is unreachable for them).
    template <typename T>
    T column_percentile_value(
        const T* data, std::size_t n_rows, std::size_t n_cols, std::size_t col, double q)
    {
        std::vector<T> buf;
        buf.reserve(n_rows);
        for (std::size_t i = 0; i < n_rows; ++i)
        {
            const T val = data[i * n_cols + col];
            if constexpr (std::is_floating_point_v<T>)
            {
                if (std::isnan(val))
                    continue;
            }
            buf.push_back(val);
        }

        if (buf.empty())
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::numeric_limits<T>::quiet_NaN();
            else
                return T { 0 };
        }

        const double idx = q / 100.0 * static_cast<double>(buf.size() - 1);
        const auto lo = static_cast<std::size_t>(idx);
        const double frac = idx - static_cast<double>(lo);

        std::nth_element(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(lo), buf.end());
        const double a = static_cast<double>(buf[lo]);
        // nth_element leaves everything after `lo` >= buf[lo]; the next order
        // statistic is therefore the minimum of that tail.
        const double b = (lo + 1 < buf.size())
            ? static_cast<double>(*std::min_element(
                  buf.begin() + static_cast<std::ptrdiff_t>(lo) + 1, buf.end()))
            : a;
        return static_cast<T>(a + frac * (b - a));
    }
```

Then, after the `block_stats` free function and before `rolling_mean`:

```cpp
// Per-column percentile down the time axis: one value per column.
// Reduces time away, so there is no output time axis.
template <typename T = double>
auto column_percentiles(const T* data, std::size_t n_rows, std::size_t n_cols, double q)
    -> std::vector<T>
{
    std::vector<T> out(n_cols);
    parallel_for(n_cols, [&](std::size_t col)
        { out[col] = detail::column_percentile_value(data, n_rows, n_cols, col, q); });
    return out;
}
```

Add `#include <algorithm>` and `#include <limits>` to the include block at the top of `Stats.hpp` (it currently has only `<cmath>`, `<cstddef>`, `<vector>`).

- [ ] **Step 4: Add the validation helper to `python_module.cpp`**

Immediately after `check_window_size` (around line 350):

```cpp
bool check_percentile(double q, const char* func_name)
{
    if (!(q >= 0.0 && q <= 100.0))
    {
        // PyErr_Format supports only a subset of printf conversions -- no %g/%f
        // for doubles. Passing one raises SystemError instead of the intended
        // exception, so q is pre-formatted and passed as %s.
        char q_str[32];
        std::snprintf(q_str, sizeof(q_str), "%g", q);
        PyErr_Format(PyExc_ValueError,
            "%s: q must be in [0, 100], got %s", func_name, q_str);
        return false;
    }
    return true;
}
```

This needs `#include <cstdio>` in `python_module.cpp`'s include block.

- [ ] **Step 5: Add the module function to `python_module.cpp`**

Immediately before `PyObject* dsp_rolling_mean(...)`:

```cpp
PyObject* dsp_column_percentile(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* y_obj = nullptr;
    double q = 50.0;

    static const char* kwlist[] = { "y", "q", nullptr };
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "O|d", const_cast<char**>(kwlist), &y_obj, &q))
        return nullptr;

    YArray y;
    if (!y.parse(y_obj))
        return nullptr;
    if (!check_percentile(q, "column_percentile"))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        std::vector<T> vals;
        SQDSP_GIL_RELEASE_BEGIN
        vals = sqp::dsp::column_percentiles<T>(y.typed_data<T>(),
            static_cast<std::size_t>(y.nrows), static_cast<std::size_t>(y.ncols), q);
        SQDSP_GIL_RELEASE_END
        return vec_to_1d(vals);
    });
}
```

The vector is built before any numpy allocation on purpose: `SQDSP_GIL_RELEASE_END` returns `nullptr` on exception, which would leak an array allocated before the guarded block.

- [ ] **Step 6: Register it in the methods table**

In `PyMethodDef methods[]`, immediately before the `"rolling_mean"` entry:

```cpp
    {"column_percentile", reinterpret_cast<PyCFunction>(dsp_column_percentile),
     METH_VARARGS | METH_KEYWORDS,
     "column_percentile(y, q=50.0) -> np.ndarray\n"
     "Per-column percentile down the time axis; one value per column.\n"
     "q=50 is the median. NaN values are excluded; an all-NaN column gives NaN.\n"
     "Matches np.nanpercentile's default 'linear' interpolation. Preserves y dtype.\n"
     "Takes no x and returns no time axis: time is reduced away."},
```

- [ ] **Step 7: Export it from `SciQLopPlots/dsp.py`**

Add `column_percentile` to both the `from ._sciqlop_dsp import (...)` block and `__all__`, placed after `rolling_std`.

- [ ] **Step 8: Rebuild and run the tests**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_dsp.py -q
```

Expected: PASS, and no previously-passing test broken. Read the real pass/fail count and exit code — do not infer from a grep.

- [ ] **Step 9: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/DSP/Stats.hpp src/DSP/python_module.cpp SciQLopPlots/dsp.py tests/unit/test_dsp.py
git commit -m "feat(dsp): add column_percentile, a per-column reduction down the time axis"
```

---

## Task 2: `rolling_percentile` kernel

Sliding per-channel background. Mirrors `rolling_mean` exactly except for the all-NaN-window return value.

**Files:**
- Modify: `include/SciQLopPlots/DSP/Stats.hpp`
- Modify: `src/DSP/python_module.cpp`
- Modify: `SciQLopPlots/dsp.py`
- Test: `tests/unit/test_dsp.py`

**Interfaces:**
- Consumes: `check_percentile(double, const char*)` from Task 1.
- Produces: `SciQLopPlots.dsp.rolling_percentile(x, y, window, q=50.0, gap_factor=3.0, has_gaps=True) -> (x_out, y_out)` — `y_out` has the same shape and dtype as `y`. Centered window, NaN-skipping, shrinking at segment edges. An all-NaN window yields NaN.

- [ ] **Step 1: Write the failing tests**

Append to `tests/unit/test_dsp.py`:

```python
# ── rolling_percentile ────────────────────────────────────────────────────────

def _rolling_percentile_ref(y, window, q):
    """Centered, NaN-skipping, edge-shrinking reference — mirrors the C++
    window bounds exactly (half = window // 2, inclusive on both sides)."""
    half = window // 2
    n = len(y)
    out = np.empty(n, dtype=np.float64)
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n, i + half + 1)
        chunk = y[lo:hi]
        chunk = chunk[~np.isnan(chunk)]
        out[i] = np.percentile(chunk, q) if chunk.size else np.nan
    return out


class TestRollingPercentile:
    def test_matches_reference_gapless(self):
        t = np.arange(200, dtype=np.float64) * 0.01
        y = np.random.default_rng(7).normal(size=200)
        _, got = rolling_percentile(t, y, 21, q=50.0, has_gaps=False)
        assert_allclose(got, _rolling_percentile_ref(y, 21, 50.0), atol=1e-10)

    def test_non_median_q(self):
        t = np.arange(150, dtype=np.float64) * 0.01
        y = np.random.default_rng(8).normal(size=150)
        _, got = rolling_percentile(t, y, 15, q=10.0, has_gaps=False)
        assert_allclose(got, _rolling_percentile_ref(y, 15, 10.0), atol=1e-10)

    def test_windows_never_span_a_gap(self):
        # Two 100-sample segments at dt=1, separated by a 50 s gap. The second
        # segment sits 100 higher, so a gap-spanning window would drag the
        # first segment's tail upward.
        t = np.concatenate([np.arange(100.0), np.arange(100.0) + 150.0])
        y = np.concatenate([np.zeros(100), np.full(100, 100.0)])
        _, got = rolling_percentile(t, y, 21, q=50.0, has_gaps=True)
        ref = np.concatenate([
            _rolling_percentile_ref(y[:100], 21, 50.0),
            _rolling_percentile_ref(y[100:], 21, 50.0),
        ])
        assert_allclose(got, ref, atol=1e-10)

    def test_multicolumn(self):
        t = np.arange(120, dtype=np.float64)
        rng = np.random.default_rng(9)
        y = rng.normal(size=(120, 3))
        _, got = rolling_percentile(t, y, 11, q=50.0, has_gaps=False)
        assert got.shape == y.shape
        for col in range(3):
            assert_allclose(got[:, col], _rolling_percentile_ref(y[:, col], 11, 50.0), atol=1e-10)

    def test_all_nan_window_is_nan_not_zero(self):
        # Deliberate deviation from rolling_mean, which returns 0 here.
        t = np.arange(20, dtype=np.float64)
        y = np.full(20, np.nan)
        y[19] = 1.0
        _, got = rolling_percentile(t, y, 3, q=50.0, has_gaps=False)
        assert np.isnan(got[0])
        assert_allclose(got[19], 1.0, atol=1e-12)

    def test_preserves_float32(self):
        t = np.arange(100, dtype=np.float64)
        y = np.linspace(0, 1, 100, dtype=np.float32)
        _, got = rolling_percentile(t, y, 5, q=50.0, has_gaps=False)
        assert got.dtype == np.float32

    def test_rejects_out_of_range_q(self):
        t = np.arange(10, dtype=np.float64)
        y = np.ones(10)
        with pytest.raises(ValueError):
            rolling_percentile(t, y, 3, q=200.0)
```

Add `rolling_percentile` to the import block at the top of the file.

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  /var/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest \
  /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_dsp.py -q
```

Expected: collection error — `ImportError: cannot import name 'rolling_percentile'`.

- [ ] **Step 3: Add the kernel to `Stats.hpp`**

Inside `namespace detail`, immediately after `rolling_mean_column`:

```cpp
    // Rolling percentile via a sorted sliding window (binary-search insert/erase).
    // Window bounds, NaN handling and edge shrinking match rolling_mean_column
    // exactly. DELIBERATE DEVIATION: where rolling_mean_column writes 0 for an
    // all-NaN window, this writes NaN — a 0 background silently turns a
    // difference into a no-op and makes a ratio divide by zero.
    template <typename T>
    void rolling_percentile_column(const T* in, std::size_t n_rows, std::size_t n_cols,
        std::size_t col, std::size_t window, double q, T* out)
    {
        if (n_cols > 1)
        {
            std::vector<T> col_in(n_rows), col_out(n_rows);
            for (std::size_t i = 0; i < n_rows; ++i)
                col_in[i] = in[i * n_cols + col];

            rolling_percentile_column(col_in.data(), n_rows, 1, 0, window, q, col_out.data());

            for (std::size_t i = 0; i < n_rows; ++i)
                out[i * n_cols + col] = col_out[i];
            return;
        }

        const auto half = window / 2;
        std::vector<double> win;
        win.reserve(std::min(window + 1, n_rows));

        auto insert = [&](std::size_t j)
        {
            const double v = static_cast<double>(in[j]);
            if (std::isnan(v))
                return;
            win.insert(std::upper_bound(win.begin(), win.end(), v), v);
        };
        auto erase = [&](std::size_t j)
        {
            const double v = static_cast<double>(in[j]);
            if (std::isnan(v))
                return;
            auto it = std::lower_bound(win.begin(), win.end(), v);
            if (it != win.end() && *it == v)
                win.erase(it);
        };
        auto current = [&]() -> T
        {
            if (win.empty())
            {
                if constexpr (std::is_floating_point_v<T>)
                    return std::numeric_limits<T>::quiet_NaN();
                else
                    return T { 0 };
            }
            const double idx = q / 100.0 * static_cast<double>(win.size() - 1);
            const auto lo = static_cast<std::size_t>(idx);
            const double frac = idx - static_cast<double>(lo);
            const double a = win[lo];
            const double b = (lo + 1 < win.size()) ? win[lo + 1] : a;
            return static_cast<T>(a + frac * (b - a));
        };

        const auto init_hi = std::min(half + 1, n_rows);
        for (std::size_t j = 0; j < init_hi; ++j)
            insert(j);
        out[0] = current();

        for (std::size_t i = 1; i < n_rows; ++i)
        {
            const auto new_hi = i + half;
            if (new_hi < n_rows)
                insert(new_hi);
            if (i > half)
                erase(i - half - 1);
            out[i] = current();
        }
    }
```

Then, immediately after the `rolling_std` stage at the end of the file (before `} // namespace sqp::dsp`):

```cpp
// Pipeline stage: rolling percentile (q=50 is the median).
template <typename T = double>
auto rolling_percentile(std::size_t window, double q) -> Stage<T>
{
    return [window, q](const std::vector<Segment<T>>& segments) -> std::vector<TimeSeries<T>>
    {
        std::vector<TimeSeries<T>> results(segments.size());
        parallel_for(segments.size(), [&](std::size_t i) {
            const auto& seg = segments[i];
            auto& out = results[i];
            out.x.assign(seg.x.begin(), seg.x.end());
            out.y.resize(seg.y.size());
            out.n_cols = seg.n_cols;
            for (std::size_t col = 0; col < seg.n_cols; ++col)
                detail::rolling_percentile_column(
                    seg.y.data(), seg.x.size(), seg.n_cols, col, window, q, out.y.data());
        });
        return results;
    };
}
```

- [ ] **Step 4: Add the module function to `python_module.cpp`**

Immediately after `dsp_rolling_std`:

```cpp
PyObject* dsp_rolling_percentile(PyObject* /*self*/, PyObject* args, PyObject* kwargs)
{
    PyObject* x_obj = nullptr;
    PyObject* y_obj = nullptr;
    Py_ssize_t window = 51;
    double q = 50.0;
    double gap_factor = 3.0;
    int has_gaps = 1;

    static const char* kwlist[]
        = { "x", "y", "window", "q", "gap_factor", "has_gaps", nullptr };
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOn|ddp", const_cast<char**>(kwlist),
            &x_obj, &y_obj, &window, &q, &gap_factor, &has_gaps))
        return nullptr;

    XArray x;
    YArray y;
    if (!x.parse(x_obj) || !y.parse(y_obj))
        return nullptr;
    if (!check_xy_sizes(x, y) || !check_gap_factor(gap_factor))
        return nullptr;
    if (!check_window_size(window, y.nrows, "rolling_percentile"))
        return nullptr;
    if (!check_percentile(q, "rolling_percentile"))
        return nullptr;

    return dispatch(y.dtype, [&]<typename T>() -> PyObject*
    {
        if (!has_gaps)
        {
            ZeroCopyOutput<T> out;
            if (!out.alloc_like(x, y))
                return static_cast<PyObject*>(nullptr);
            const auto nrows = static_cast<std::size_t>(y.nrows);
            const auto ncols = static_cast<std::size_t>(y.ncols);
            const auto win = static_cast<std::size_t>(window);
            SQDSP_GIL_RELEASE_BEGIN
            for (std::size_t col = 0; col < ncols; ++col)
                sqp::dsp::detail::rolling_percentile_column(
                    y.typed_data<T>(), nrows, ncols, col, win, q, out.y_ptr);
            SQDSP_GIL_RELEASE_END
            return out.to_tuple();
        }
        auto stage = sqp::dsp::rolling_percentile<T>(static_cast<std::size_t>(window), q);
        return apply_stage<T>(x, y, gap_factor, has_gaps, stage);
    });
}
```

- [ ] **Step 5: Register it in the methods table**

In `PyMethodDef methods[]`, immediately after the `"rolling_std"` entry:

```cpp
    {"rolling_percentile", reinterpret_cast<PyCFunction>(dsp_rolling_percentile),
     METH_VARARGS | METH_KEYWORDS,
     "rolling_percentile(x, y, window, q=50.0, gap_factor=3.0, has_gaps=True)"
     " -> (x_out, y_out)\n"
     "Gap-aware centered rolling percentile; q=50 is the median. Preserves y dtype.\n"
     "Unlike rolling_mean, an all-NaN window yields NaN rather than 0."},
```

- [ ] **Step 6: Export it from `SciQLopPlots/dsp.py`**

Add `rolling_percentile` to both the import block and `__all__`, after `column_percentile`.

- [ ] **Step 7: Rebuild and run the tests**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
VENV=$(pwd)/.venv
export PATH="$VENV/bin:/home/jeandet/Qt/6.11.1/gcc_64/bin:/home/jeandet/Qt/6.11.0/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.1/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
$VENV/bin/meson compile -C build-venv
cd /tmp && PYTHONPATH=/var/home/jeandet/Documents/prog/SciQLopPlots/build-venv \
  $VENV/bin/python -m pytest /var/home/jeandet/Documents/prog/SciQLopPlots/tests/unit/test_dsp.py -q
```

Expected: PASS. Read the real pass/fail count and exit code.

- [ ] **Step 8: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/DSP/Stats.hpp src/DSP/python_module.cpp SciQLopPlots/dsp.py tests/unit/test_dsp.py
git commit -m "feat(dsp): add gap-aware rolling_percentile"
```

---

## Task 3: array-level `background_subtract`

**Files:**
- Create: `/var/home/jeandet/Documents/prog/SciQLop/SciQLop/user_api/dsp/_background.py`
- Modify: `/var/home/jeandet/Documents/prog/SciQLop/SciQLop/user_api/dsp/_arrays.py`
- Test: `/var/home/jeandet/Documents/prog/SciQLop/tests/test_dsp_arrays.py`

**Interfaces:**
- Consumes: `SciQLopPlots.dsp.column_percentile(y, q)` (Task 1) and `SciQLopPlots.dsp.rolling_percentile(x, y, window, q=, gap_factor=, has_gaps=)` (Task 2).
- Produces:
  - `SciQLop.user_api.dsp._background.background_subtract(x, y, *, q=50.0, window=None, mode='diff', gap_factor=3.0) -> np.ndarray` — same shape as `y`.
  - `SciQLop.user_api.dsp._background.resolve_window(x, window) -> int | None`
  - `SciQLop.user_api.dsp._background._realign_to_input(x, x_bg, bg) -> np.ndarray`
  - `SciQLop.user_api.dsp.arrays.background_subtract(...)` — same signature, thin delegation.

`_background.py` is a new module rather than logic added to `_arrays.py` because `_arrays.py` documents itself as a thin typed pass-through over `SciQLopPlots.dsp`, and this function composes rather than passes through.

- [ ] **Step 1: Write the failing tests**

Append to `/var/home/jeandet/Documents/prog/SciQLop/tests/test_dsp_arrays.py`:

```python
from datetime import timedelta

import numpy as np
import pytest
from numpy.testing import assert_allclose

from SciQLop.user_api.dsp import arrays as dsp_arrays


def _flat_spectrogram(n_time=200, n_freq=4, dt=1.0):
    """Per-channel constant level: channel k sits at 10**(k+1)."""
    x = np.arange(n_time, dtype=np.float64) * dt
    levels = np.array([10.0 ** (k + 1) for k in range(n_freq)])
    y = np.tile(levels, (n_time, 1))
    return x, y


class TestBackgroundSubtractArrays:
    def test_constant_background_diff_is_zero_on_flat_input(self):
        x, y = _flat_spectrogram()
        out = dsp_arrays.background_subtract(x, y)
        assert_allclose(out, np.zeros_like(y), atol=1e-9)

    def test_constant_background_ratio_is_one_on_flat_input(self):
        x, y = _flat_spectrogram()
        out = dsp_arrays.background_subtract(x, y, mode='ratio')
        assert_allclose(out, np.ones_like(y), atol=1e-9)

    def test_constant_background_db_is_zero_on_flat_input(self):
        x, y = _flat_spectrogram()
        out = dsp_arrays.background_subtract(x, y, mode='db')
        assert_allclose(out, np.zeros_like(y), atol=1e-9)

    def test_burst_survives_low_percentile_background(self):
        x, y = _flat_spectrogram(n_time=200, n_freq=2)
        y = y.copy()
        y[100:110, 0] *= 5.0                       # a burst on channel 0
        out = dsp_arrays.background_subtract(x, y, q=10.0)
        assert out[100:110, 0].min() > 0.0
        assert_allclose(out[:100, 0], 0.0, atol=1e-9)

    def test_sliding_background_removes_drift_that_constant_cannot(self):
        n = 400
        x = np.arange(n, dtype=np.float64)
        drift = np.linspace(0.0, 100.0, n)
        y = (np.full(n, 50.0) + drift).reshape(n, 1)
        const = dsp_arrays.background_subtract(x, y)
        slide = dsp_arrays.background_subtract(x, y, window=31)
        margin = 40                                # skip the shrinking edges
        assert np.abs(slide[margin:-margin]).max() < np.abs(const[margin:-margin]).max() / 10.0

    def test_sliding_background_realigns_across_a_gap(self):
        # rolling_percentile goes through the gap-aware pipeline, which
        # reassembles segments with one extra NaN separator row per gap. The
        # background must still line up row-for-row with the input.
        x = np.concatenate([np.arange(100.0), np.arange(100.0) + 150.0])
        y = np.concatenate([np.full(100, 10.0), np.full(100, 50.0)]).reshape(200, 1)
        out = dsp_arrays.background_subtract(x, y, window=11)
        assert out.shape == y.shape
        assert np.isfinite(out).all()
        assert_allclose(out, 0.0, atol=1e-9)

    def test_window_as_timedelta_matches_equivalent_samples(self):
        n = 300
        x = np.arange(n, dtype=np.float64) * 2.0   # dt = 2 s
        y = np.random.default_rng(3).normal(size=(n, 2)) + 100.0
        by_samples = dsp_arrays.background_subtract(x, y, window=15)
        by_time = dsp_arrays.background_subtract(x, y, window=timedelta(seconds=30))
        assert_allclose(by_samples, by_time, atol=1e-12)

    def test_window_as_numpy_timedelta64_matches_equivalent_samples(self):
        n = 300
        x = np.arange(n, dtype=np.float64) * 2.0
        y = np.random.default_rng(4).normal(size=(n, 2)) + 100.0
        by_samples = dsp_arrays.background_subtract(x, y, window=15)
        by_time = dsp_arrays.background_subtract(x, y, window=np.timedelta64(30, 's'))
        assert_allclose(by_samples, by_time, atol=1e-12)

    def test_bare_float_window_is_rejected(self):
        x, y = _flat_spectrogram()
        with pytest.raises(TypeError, match="int"):
            dsp_arrays.background_subtract(x, y, window=30.0)

    def test_bool_window_is_rejected(self):
        x, y = _flat_spectrogram()
        with pytest.raises(TypeError):
            dsp_arrays.background_subtract(x, y, window=True)

    def test_non_positive_values_give_nan_not_inf(self):
        x, y = _flat_spectrogram(n_time=50, n_freq=1)
        y = y.copy()
        y[10, 0] = 0.0
        y[11, 0] = -5.0
        for mode in ('ratio', 'db'):
            out = dsp_arrays.background_subtract(x, y, mode=mode)
            assert np.isnan(out[10, 0])
            assert np.isnan(out[11, 0])
            assert not np.isinf(out).any()

    def test_diff_needs_no_guard_for_non_positive_values(self):
        x, y = _flat_spectrogram(n_time=50, n_freq=1)
        y = y.copy()
        y[10, 0] = -5.0
        out = dsp_arrays.background_subtract(x, y, mode='diff')
        assert np.isfinite(out).all()

    def test_preserves_float32(self):
        x, y = _flat_spectrogram()
        out = dsp_arrays.background_subtract(x, y.astype(np.float32))
        assert out.dtype == np.float32

    def test_unknown_mode_is_rejected(self):
        x, y = _flat_spectrogram()
        with pytest.raises(ValueError, match="mode"):
            dsp_arrays.background_subtract(x, y, mode='decibels')
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_arrays.py -q -k BackgroundSubtract
```

Expected: `AttributeError: module ... has no attribute 'background_subtract'`.

If instead you get `ImportError: cannot import name 'column_percentile'` from `SciQLopPlots.dsp`, the Task 1–2 build has not been copied into SciQLop's venv — run the `cp` from **Global Constraints → Test command (SciQLop)** first.

- [ ] **Step 3: Create `_background.py`**

```python
"""Per-channel background estimation and removal for 2-D spectrograms.

Composes the ``SciQLopPlots.dsp`` percentile kernels; kept out of
``_arrays.py``, which is a thin pass-through layer.
"""
from __future__ import annotations
from datetime import timedelta
from typing import Optional, Union

import numpy as np

from SciQLopPlots import dsp as _dsp

__all__ = ['background_subtract', 'resolve_window']

Window = Union[None, int, timedelta, np.timedelta64]

_MODES = ('diff', 'ratio', 'db')


def resolve_window(x: np.ndarray, window: Window) -> Optional[int]:
    """Normalize `window` to a sample count.

    `None` stays `None` (constant background). An `int` is a sample count.
    A `timedelta` / `np.timedelta64` is a duration, converted with the median
    sample spacing — robust to the inter-file gaps these products carry.

    Dispatch is by type, not by value: a bare `float` is rejected because
    ``window=300`` and ``window=300.0`` meaning different things is a silent
    error at sub-second cadences, not a visible one.
    """
    if window is None:
        return None
    if isinstance(window, bool):
        raise TypeError("window must be an int (samples) or a timedelta/np.timedelta64 "
                        "(duration), got bool")
    # np.timedelta64 subclasses np.signedinteger, so it must be checked
    # before the plain-int branch or it would be misread as a sample count.
    if isinstance(window, np.timedelta64):
        seconds = float(window / np.timedelta64(1, 's'))
    elif isinstance(window, (int, np.integer)):
        return int(window)
    elif isinstance(window, timedelta):
        seconds = window.total_seconds()
    else:
        raise TypeError("window must be None, an int (samples), or a "
                        f"timedelta/np.timedelta64 (duration), got {type(window).__name__}")

    if x.size < 2:
        raise ValueError("a duration window needs at least 2 samples to infer the cadence")
    median_dt = float(np.median(np.diff(x)))
    if not np.isfinite(median_dt) or median_dt <= 0.0:
        raise ValueError(f"cannot infer a positive sample cadence from x (median dt={median_dt})")
    return int(max(1, min(x.size, round(seconds / median_dt))))


def _realign_to_input(x: np.ndarray, x_bg: np.ndarray, bg: np.ndarray) -> np.ndarray:
    """Drop the gap-separator rows the DSP pipeline inserts.

    Every gap-aware ``Stage<T>`` kernel reassembles its segments with one
    extra NaN row per gap, timestamped at the gap midpoint (see
    ``Pipeline.hpp``'s ``reassemble``). The background would then be longer
    than the data it has to line up with. Separator timestamps are midpoints
    and never occur in `x`, and segment timestamps are copied verbatim, so
    `x` is an exact subsequence of `x_bg` and a searchsorted lookup recovers
    the original rows.
    """
    if bg.shape[0] == x.shape[0]:
        return bg                                   # no gaps, nothing inserted
    return bg[np.searchsorted(x_bg, x)]


def _apply_mode(y: np.ndarray, bg: np.ndarray, mode: str) -> np.ndarray:
    if mode == 'diff':
        return y - bg
    # NaN rather than +-inf where the ratio is undefined: the colormap hides
    # NaN, whereas one inf destroys the colour scale for the whole plot.
    with np.errstate(divide='ignore', invalid='ignore'):
        ratio = np.where((y > 0) & (bg > 0), y / bg, np.nan)
        return ratio if mode == 'ratio' else 10.0 * np.log10(ratio)


def background_subtract(x: np.ndarray, y: np.ndarray, *,
                        q: float = 50.0, window: Window = None,
                        mode: str = 'diff', gap_factor: float = 3.0) -> np.ndarray:
    """Remove a per-channel background from a 2-D spectrogram.

    Parameters
    ----------
    x : np.ndarray
        Time axis, epoch seconds.
    y : np.ndarray
        Values, shape (n_time,) or (n_time, n_freq).
    q : float
        Percentile of the background estimator. 50 is the median (default);
        5-10 is the robust choice when bursts fill much of the window.
    window : None | int | timedelta | np.timedelta64
        `None` estimates one constant background per channel over the whole
        window. Otherwise a sliding background of that many samples, or of
        that duration.
    mode : {'diff', 'ratio', 'db'}
        `S - bg`, `S / bg`, or `10*log10(S / bg)`. `ratio` and `db` yield NaN
        wherever `S <= 0` or `bg <= 0`.
    gap_factor : float
        Gap threshold for the sliding background; ignored when `window` is None.

    Returns
    -------
    np.ndarray
        Same shape and dtype as `y`.
    """
    if mode not in _MODES:
        raise ValueError(f"mode must be one of {_MODES}, got {mode!r}")

    x = np.asarray(x)
    y = np.asarray(y)
    samples = resolve_window(x, window)

    if samples is None:
        bg = _dsp.column_percentile(y, q)          # shape (n_cols,), broadcasts over rows
    else:
        x_bg, bg = _dsp.rolling_percentile(x, y, samples, q=q,
                                           gap_factor=gap_factor, has_gaps=True)
        if y.ndim == 2 and bg.ndim == 1:
            bg = bg.reshape(-1, 1)                  # rolling_percentile drops a size-1 column axis
        bg = _realign_to_input(x, x_bg, bg)
    return _apply_mode(y, bg, mode)
```

- [ ] **Step 4: Add the thin re-export to `_arrays.py`**

Append at the end of `_arrays.py`:

```python
def background_subtract(x: np.ndarray, y: np.ndarray, *,
                        q: float = 50.0, window=None,
                        mode: str = 'diff', gap_factor: float = 3.0) -> np.ndarray:
    """Remove a per-channel background. See ``_background.background_subtract``."""
    return _background.background_subtract(x, y, q=q, window=window,
                                           mode=mode, gap_factor=gap_factor)
```

Add `from . import _background` to the imports at the top, and `'background_subtract'` to `__all__`.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_arrays.py -q
```

Expected: PASS, whole file (not just the new class). Read the real pass/fail count and exit code.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
git add SciQLop/user_api/dsp/_background.py SciQLop/user_api/dsp/_arrays.py tests/test_dsp_arrays.py
git commit -m "feat(dsp): array-level per-channel background removal"
```

---

## Task 4: SpeasyVariable `background_subtract`

**Files:**
- Modify: `/var/home/jeandet/Documents/prog/SciQLop/SciQLop/user_api/dsp/_speasy.py`
- Modify: `/var/home/jeandet/Documents/prog/SciQLop/SciQLop/user_api/dsp/__init__.py`
- Test: `/var/home/jeandet/Documents/prog/SciQLop/tests/test_dsp_speasy.py`

**Interfaces:**
- Consumes: `arrays.background_subtract(x, y, *, q, window, mode, gap_factor)` from Task 3.
- Produces: `SciQLop.user_api.dsp.background_subtract(data, *, q=50.0, window=None, mode='diff', gap_factor=3.0) -> SpeasyVariable`, and `_speasy.rewrap_time_series(..., meta_overrides: Optional[dict] = None)`.

- [ ] **Step 1: Write the failing tests**

Append to `/var/home/jeandet/Documents/prog/SciQLop/tests/test_dsp_speasy.py`:

```python
import numpy as np
import pytest
from numpy.testing import assert_allclose

from speasy.core.data_containers import DataContainer, VariableAxis, VariableTimeAxis
from speasy.products.variable import SpeasyVariable

from SciQLop.user_api import dsp as user_dsp


def _spectrogram_var(n_time=200, n_freq=4, units='sfu'):
    t0 = np.datetime64('2025-07-26T13:00:00', 'ns')
    times = (t0.astype('int64') + np.arange(n_time) * 1_000_000_000).astype('datetime64[ns]')
    freqs = np.linspace(10e6, 240e6, n_freq)
    levels = np.array([10.0 ** (k + 1) for k in range(n_freq)])
    values = np.tile(levels, (n_time, 1))
    return SpeasyVariable(
        axes=[VariableTimeAxis(values=times),
              VariableAxis(name='frequency', values=freqs, meta={'UNITS': 'Hz'})],
        values=DataContainer(values=values, meta={'UNITS': units}, name='ILOFAR'),
        columns=['ILOFAR'],
    )


class TestBackgroundSubtractSpeasy:
    def test_diff_on_flat_input_is_zero(self):
        out = user_dsp.background_subtract(_spectrogram_var())
        assert_allclose(np.asarray(out.values), 0.0, atol=1e-9)

    def test_time_and_frequency_axes_are_preserved(self):
        var = _spectrogram_var()
        out = user_dsp.background_subtract(var)
        assert np.array_equal(out.time, var.time)
        assert_allclose(np.asarray(out.axes[1].values), np.asarray(var.axes[1].values))
        assert np.asarray(out.values).shape == np.asarray(var.values).shape

    def test_name_is_suffixed(self):
        out = user_dsp.background_subtract(_spectrogram_var())
        assert out.name.endswith('_bgsub')

    def test_units_per_mode(self):
        var = _spectrogram_var(units='sfu')
        assert user_dsp.background_subtract(var, mode='diff').meta['UNITS'] == 'sfu'
        assert user_dsp.background_subtract(var, mode='ratio').meta['UNITS'] == ''
        assert user_dsp.background_subtract(var, mode='db').meta['UNITS'] == 'dB'

    def test_sliding_window_accepts_a_duration(self):
        from datetime import timedelta
        var = _spectrogram_var(n_time=300)
        by_samples = user_dsp.background_subtract(var, window=31)
        by_time = user_dsp.background_subtract(var, window=timedelta(seconds=31))
        assert_allclose(np.asarray(by_samples.values), np.asarray(by_time.values), atol=1e-12)

    def test_non_positive_values_give_nan_not_inf(self):
        var = _spectrogram_var(n_time=50, n_freq=1)
        vals = np.asarray(var.values).copy()
        vals[10, 0] = 0.0
        var = SpeasyVariable(axes=list(var.axes),
                             values=DataContainer(values=vals, meta=dict(var.meta),
                                                  name=var.name),
                             columns=var.columns)
        out = np.asarray(user_dsp.background_subtract(var, mode='db').values)
        assert np.isnan(out[10, 0])
        assert not np.isinf(out).any()

    def test_rejects_raw_arrays(self):
        with pytest.raises(TypeError, match="SpeasyVariable"):
            user_dsp.background_subtract(np.ones((10, 2)))
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_speasy.py -q -k BackgroundSubtract
```

Expected: `AttributeError: module 'SciQLop.user_api.dsp' has no attribute 'background_subtract'`.

- [ ] **Step 3: Add `meta_overrides` to `rewrap_time_series`**

In `_speasy.py`, change the signature and the `DataContainer` construction:

```python
def rewrap_time_series(template: SpeasyVariable, values: np.ndarray, *,
                       time_epoch: Optional[np.ndarray] = None,
                       name_suffix: str = "",
                       meta_overrides: Optional[dict] = None) -> SpeasyVariable:
```

Add to the docstring's Parameters section:

```
    meta_overrides : dict or None
        Merged over the template's meta. Transforms that change the physical
        quantity (e.g. a dB ratio) must override ``UNITS`` here, or the output
        carries the input's unit and is mislabelled.
```

and replace the `data = DataContainer(...)` line with:

```python
    meta = dict(template.meta)
    if meta_overrides:
        meta.update(meta_overrides)
    data = DataContainer(values=values, meta=meta,
                         name=template.name + name_suffix)
```

- [ ] **Step 4: Add the facade to `__init__.py`**

Append after the other same-axis transforms (near `rolling_std`):

```python
_BACKGROUND_UNITS = {'diff': None, 'ratio': '', 'db': 'dB'}


@experimental_api()
def background_subtract(data, *, q: float = 50.0, window=None,
                        mode: str = 'diff', gap_factor: float = 3.0):
    """Remove a per-channel background from a spectrogram.

    Estimates a background per frequency channel down the time axis, then
    expresses each sample relative to it. Returns a new SpeasyVariable with the
    same time and frequency axes, suffixed ``_bgsub``.

    ``q`` is the percentile of the estimator (50 = median, the default; 5-10
    when bursts fill much of the window). ``window`` is `None` for a constant
    background, an `int` for that many samples, or a `timedelta` /
    `np.timedelta64` for that duration. ``mode`` is ``'diff'`` (default),
    ``'ratio'`` or ``'db'``; the latter two yield NaN where ``S <= 0`` or
    ``bg <= 0``.

    For raw arrays, use ``SciQLop.user_api.dsp.arrays.background_subtract``.
    """
    if not _is_var(data):
        raise TypeError("background_subtract(data, ...) requires a SpeasyVariable; "
                        "use SciQLop.user_api.dsp.arrays.background_subtract(x, y) "
                        "for arrays.")
    x, y = _sp.unwrap(data)
    y_out = arrays.background_subtract(x, y, q=q, window=window,
                                       mode=mode, gap_factor=gap_factor)
    units = _BACKGROUND_UNITS[mode]
    return _sp.rewrap_time_series(
        data, y_out, name_suffix='_bgsub',
        meta_overrides=None if units is None else {'UNITS': units})
```

Add `'background_subtract'` to `__all__`.

Note `_BACKGROUND_UNITS[mode]` would raise `KeyError` for an unknown mode, but
`arrays.background_subtract` validates `mode` first and raises `ValueError`, so
the lookup is only ever reached with a valid key.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/test_dsp_speasy.py tests/test_dsp_arrays.py -q
```

Expected: PASS, both files. Read the real pass/fail count and exit code.

- [ ] **Step 6: Run the full SciQLop suite for regressions**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
QT_QPA_PLATFORM=xcb .venv/bin/python -m pytest tests/ -q
```

Expected: no new failures relative to the pre-change baseline. If any test fails, check it fails on `git stash` too before treating it as a regression.

- [ ] **Step 7: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLop
git add SciQLop/user_api/dsp/_speasy.py SciQLop/user_api/dsp/__init__.py tests/test_dsp_speasy.py
git commit -m "feat(dsp): SpeasyVariable background_subtract with per-mode units"
```

---

## Out of scope

RFI channel flagging, median rebinning, Stokes I from the I-LOFAR X/Y pair, and everything in `sciqlop_radio` (knobs, derived products, UI). Do not add them to this plan.
