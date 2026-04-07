"""Benchmarks comparing C++ DSP functions against numpy/scipy equivalents.

Run with: python tests/perf/bench_dsp.py

Uses has_gaps=False to bypass gap detection overhead for fair comparison
of pure computation speed against numpy/scipy.
"""
import time
import numpy as np
from scipy import signal as sp_signal
import pandas as pd

from SciQLopPlots.dsp import (
    interpolate_nan,
    resample,
    fir_filter,
    iir_sos,
    filtfilt,
    sosfiltfilt,
    fft,
    spectrogram,
    rolling_mean,
    rolling_std,
    reduce,
)


def bench(fn, *args, warmup=2, rounds=10, **kwargs):
    for _ in range(warmup):
        fn(*args, **kwargs)
    times = []
    for _ in range(rounds):
        t0 = time.perf_counter()
        fn(*args, **kwargs)
        times.append(time.perf_counter() - t0)
    return np.median(times)


def make_signal(n, fs=1000.0):
    t = np.arange(n, dtype=np.float64) / fs
    y = np.sin(2 * np.pi * 5 * t) + 0.5 * np.sin(2 * np.pi * 20 * t)
    return t, y, fs


def make_3col(n):
    t = np.arange(n, dtype=np.float64) * 0.001
    rng = np.random.default_rng(42)
    y = np.column_stack([
        np.sin(2 * np.pi * 2 * t),
        0.5 * np.cos(2 * np.pi * 3 * t),
        rng.normal(size=n),
    ])
    return t, y


NO_GAPS = dict(has_gaps=False)


def run_benchmarks():
    sizes = [10_000, 100_000, 1_000_000]
    results = []

    for n in sizes:
        tag = f"{n//1000}k" if n < 1_000_000 else f"{n//1_000_000}M"
        t, y, fs = make_signal(n)
        t3, y3 = make_3col(n)

        # -- interpolate_nan --
        y_nan = y.copy()
        rng = np.random.default_rng(42)
        y_nan[rng.choice(n, n // 100, replace=False)] = np.nan
        t_nan = t.copy()

        cpp_time = bench(interpolate_nan, t_nan, y_nan, 1)

        def numpy_interp_nan():
            r = y_nan.copy()
            nans = np.isnan(y_nan)
            r[nans] = np.interp(t_nan[nans], t_nan[~nans], y_nan[~nans])
            return r
        py_time = bench(numpy_interp_nan)
        results.append(("interpolate_nan", tag, cpp_time, py_time))

        # -- resample --
        dt = np.median(np.diff(t))
        t_jit = t + rng.uniform(-dt * 0.2, dt * 0.2, size=n)
        t_jit = np.sort(t_jit)

        # Pass target_dt to skip median computation in C++
        cpp_time = bench(resample, t_jit, y, target_dt=dt, **NO_GAPS)

        def numpy_resample():
            t_u = np.arange(t_jit[0], t_jit[-1], dt)
            return t_u, np.interp(t_u, t_jit, y)
        py_time = bench(numpy_resample)
        results.append(("resample", tag, cpp_time, py_time))

        # -- FIR filter --
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)
        cpp_time = bench(fir_filter, t, y, coeffs, **NO_GAPS)
        py_time = bench(sp_signal.lfilter, coeffs, 1.0, y)
        results.append(("fir_filter", tag, cpp_time, py_time))

        # -- IIR SOS --
        sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')
        cpp_time = bench(iir_sos, t, y, sos, **NO_GAPS)
        py_time = bench(sp_signal.sosfilt, sos, y)
        results.append(("iir_sos", tag, cpp_time, py_time))

        # -- filtfilt (zero-phase FIR) --
        cpp_time = bench(filtfilt, t, y, coeffs, **NO_GAPS)
        py_time = bench(sp_signal.filtfilt, coeffs, 1.0, y)
        results.append(("filtfilt", tag, cpp_time, py_time))

        # -- sosfiltfilt (zero-phase IIR) --
        cpp_time = bench(sosfiltfilt, t, y, sos, **NO_GAPS)
        py_time = bench(sp_signal.sosfiltfilt, sos, y)
        results.append(("sosfiltfilt", tag, cpp_time, py_time))

        # -- FFT --
        cpp_time = bench(fft, t, y, **NO_GAPS)

        def numpy_fft():
            freqs = np.fft.rfftfreq(n, d=1.0 / fs)
            mag = np.abs(np.fft.rfft(y)) / n
            return freqs, mag
        py_time = bench(numpy_fft)
        results.append(("fft", tag, cpp_time, py_time))

        # -- spectrogram --
        cpp_time = bench(spectrogram, t, y, 0, 256, 128, **NO_GAPS)
        py_time = bench(sp_signal.spectrogram, y, fs=fs, nperseg=256, noverlap=128)
        results.append(("spectrogram", tag, cpp_time, py_time))

        # -- rolling_mean --
        cpp_time = bench(rolling_mean, t, y, 51, **NO_GAPS)

        def numpy_rolling_mean():
            kernel = np.ones(51) / 51
            return np.convolve(y, kernel, mode='same')
        py_time = bench(numpy_rolling_mean)
        results.append(("rolling_mean", tag, cpp_time, py_time))

        # -- rolling_std --
        cpp_time = bench(rolling_std, t, y, 51, **NO_GAPS)
        s = pd.Series(y)
        py_time = bench(lambda: s.rolling(51, center=True).std().to_numpy())
        results.append(("rolling_std", tag, cpp_time, py_time))

        # -- reduce (norm) --
        cpp_time = bench(reduce, t3, y3, 'norm', **NO_GAPS)
        py_time = bench(np.linalg.norm, y3, axis=1)
        results.append(("reduce_norm", tag, cpp_time, py_time))

    # Print results
    print(f"\n{'Function':<20} {'Size':>6} {'C++ (ms)':>10} {'Python (ms)':>12} {'Speedup':>8}")
    print("-" * 60)
    for name, tag, cpp_t, py_t in results:
        speedup = py_t / cpp_t if cpp_t > 0 else float('inf')
        print(f"{name:<20} {tag:>6} {cpp_t*1000:>10.3f} {py_t*1000:>12.3f} {speedup:>7.1f}x")


def run_multicol_benchmarks():
    """Benchmark multi-component (3-col) data — typical for vector products (Bx,By,Bz)."""
    sizes = [10_000, 100_000, 1_000_000]
    results = []
    ncols = 3

    for n in sizes:
        tag = f"{n//1000}k" if n < 1_000_000 else f"{n//1_000_000}M"
        fs = 1000.0
        t = np.arange(n, dtype=np.float64) / fs
        y = np.column_stack([
            np.sin(2 * np.pi * 5 * t),
            0.5 * np.sin(2 * np.pi * 10 * t),
            0.3 * np.sin(2 * np.pi * 20 * t),
        ])
        rng = np.random.default_rng(42)
        dt = 1.0 / fs
        t_jit = t + rng.uniform(-dt * 0.2, dt * 0.2, size=n)
        t_jit = np.sort(t_jit)

        # -- FIR filter (3 cols) --
        coeffs = sp_signal.firwin(65, 10.0, fs=fs)
        cpp_time = bench(fir_filter, t, y, coeffs, **NO_GAPS)

        def scipy_fir_3col():
            return np.column_stack([sp_signal.lfilter(coeffs, 1.0, y[:, c]) for c in range(ncols)])
        py_time = bench(scipy_fir_3col)
        results.append(("fir_filter_3col", tag, cpp_time, py_time))

        # -- IIR SOS (3 cols) --
        sos = sp_signal.butter(4, 10.0, fs=fs, output='sos')
        cpp_time = bench(iir_sos, t, y, sos, **NO_GAPS)

        def scipy_iir_3col():
            return np.column_stack([sp_signal.sosfilt(sos, y[:, c]) for c in range(ncols)])
        py_time = bench(scipy_iir_3col)
        results.append(("iir_sos_3col", tag, cpp_time, py_time))

        # -- filtfilt (3 cols) --
        cpp_time = bench(filtfilt, t, y, coeffs, **NO_GAPS)

        def scipy_filtfilt_3col():
            return np.column_stack([sp_signal.filtfilt(coeffs, 1.0, y[:, c]) for c in range(ncols)])
        py_time = bench(scipy_filtfilt_3col)
        results.append(("filtfilt_3col", tag, cpp_time, py_time))

        # -- sosfiltfilt (3 cols) --
        cpp_time = bench(sosfiltfilt, t, y, sos, **NO_GAPS)

        def scipy_sosfiltfilt_3col():
            return np.column_stack([sp_signal.sosfiltfilt(sos, y[:, c]) for c in range(ncols)])
        py_time = bench(scipy_sosfiltfilt_3col)
        results.append(("sosfiltfilt_3col", tag, cpp_time, py_time))

        # -- rolling_mean (3 cols) --
        cpp_time = bench(rolling_mean, t, y, 51, **NO_GAPS)

        def numpy_rmean_3col():
            kernel = np.ones(51) / 51
            return np.column_stack([np.convolve(y[:, c], kernel, mode='same') for c in range(ncols)])
        py_time = bench(numpy_rmean_3col)
        results.append(("rolling_mean_3col", tag, cpp_time, py_time))

        # -- rolling_std (3 cols) --
        cpp_time = bench(rolling_std, t, y, 51, **NO_GAPS)

        def pandas_rstd_3col():
            return pd.DataFrame(y).rolling(51, center=True).std().to_numpy()
        py_time = bench(pandas_rstd_3col)
        results.append(("rolling_std_3col", tag, cpp_time, py_time))

        # -- resample (3 cols) --
        cpp_time = bench(resample, t_jit, y, target_dt=dt, **NO_GAPS)

        def numpy_resample_3col():
            t_u = np.arange(t_jit[0], t_jit[-1], dt)
            return t_u, np.column_stack([np.interp(t_u, t_jit, y[:, c]) for c in range(ncols)])
        py_time = bench(numpy_resample_3col)
        results.append(("resample_3col", tag, cpp_time, py_time))

        # -- reduce (3 cols) --
        cpp_time = bench(reduce, t, y, 'norm', **NO_GAPS)
        py_time = bench(np.linalg.norm, y, axis=1)
        results.append(("reduce_norm_3col", tag, cpp_time, py_time))

    print(f"\n{'Function':<25} {'Size':>6} {'C++ (ms)':>10} {'Python (ms)':>12} {'Speedup':>8}")
    print("-" * 65)
    for name, tag, cpp_t, py_t in results:
        speedup = py_t / cpp_t if cpp_t > 0 else float('inf')
        print(f"{name:<25} {tag:>6} {cpp_t*1000:>10.3f} {py_t*1000:>12.3f} {speedup:>7.1f}x")


if __name__ == "__main__":
    print("=== Single column ===")
    run_benchmarks()
    print("\n=== 3 columns (vector product) ===")
    run_multicol_benchmarks()
